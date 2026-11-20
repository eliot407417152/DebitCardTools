#include "CardUtils.h"

// ─────────────────────────────────────────────────────────────────────────────
// APDU 傳輸
// ─────────────────────────────────────────────────────────────────────────────
QString CardUtils::sendApdu(SCARDHANDLE hCard, const QString& apduHex, bool& ok, const LogFn& log)
{
    ok = false;
    if (hCard == 0) return {};

    QByteArray send = QByteArray::fromHex(apduHex.toLatin1());
    BYTE        recv[260];
    DWORD       recvLen = sizeof(recv);
    SCARD_IO_REQUEST ioReq = *SCARD_PCI_T1;

    LONG ret = SCardTransmit(hCard, &ioReq,
        reinterpret_cast<LPCBYTE>(send.constData()), send.length(),
        nullptr, recv, &recvLen);
    if (ret != SCARD_S_SUCCESS) {
        log(QString("PCSC 底層通訊錯誤: 0x%1").arg(ret, 0, 16), true);
        return {};
    }

    QString resp = QString(QByteArray(reinterpret_cast<const char*>(recv), recvLen).toHex()).toUpper();
    log(">> " + apduHex, false);
    log("<< " + resp, false);

    ok = true;
    return resp;
}

// ─────────────────────────────────────────────────────────────────────────────
// 3DES-ECB 加密 (HEX 介面)
// ─────────────────────────────────────────────────────────────────────────────
QString CardUtils::encrypt3DES(const QString& hexKey, const QString& hexData)
{
    return bytesToHex(encrypt3DES(hexToBytes(hexKey), hexToBytes(hexData)));
}

// ─────────────────────────────────────────────────────────────────────────────
// 3DES-ECB 加密 (Bytes 介面)
// ─────────────────────────────────────────────────────────────────────────────
QByteArray CardUtils::encrypt3DES(const QByteArray& key, const QByteArray& data)
{
    if (key.isEmpty() || data.isEmpty()) return {};

    QByteArray k = key;
    if (k.size() < 24) k.append(24 - k.size(), '\0');

    DES_cblock k1, k2, k3;
    memcpy(&k1, k.constData(), 8);
    memcpy(&k2, k.constData() + 8, 8);
    memcpy(&k3, k.constData() + 16, 8);

    DES_key_schedule ks1, ks2, ks3;
    DES_set_key_unchecked(&k1, &ks1);
    DES_set_key_unchecked(&k2, &ks2);
    DES_set_key_unchecked(&k3, &ks3);

    QByteArray result;
    for (int i = 0; i < data.size(); i += 8) {
        DES_cblock in{}, out{};
        int chunk = qMin(8, data.size() - i);
        memcpy(&in, data.constData() + i, chunk);
        DES_ecb3_encrypt(&in, &out, &ks1, &ks2, &ks3, DES_ENCRYPT);
        result.append(reinterpret_cast<const char*>(out), 8);
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// XOR
// ─────────────────────────────────────────────────────────────────────────────
QByteArray CardUtils::xorBytes(const QByteArray& a, const QByteArray& b)
{
    int len = qMin(a.size(), b.size());
    QByteArray result(len, '\0');
    for (int i = 0; i < len; ++i)
        result[i] = a[i] ^ b[i];
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// 格式轉換
// ─────────────────────────────────────────────────────────────────────────────
QByteArray CardUtils::hexToBytes(const QString& hex)
{
    return QByteArray::fromHex(hex.trimmed().remove(' ').toUtf8());
}

// 是否為非空、純 HEX 字元的字串
static bool isHexString(const QString& s)
{
    if (s.isEmpty()) return false;
    for (QChar c : s) {
        const bool ok = (c >= '0' && c <= '9') ||
            (c >= 'A' && c <= 'F') ||
            (c >= 'a' && c <= 'f');
        if (!ok) return false;
    }
    return true;
}

QString CardUtils::xorHex(const QString& hexA, const QString& hexB)
{
    const QString a = hexA.trimmed().remove(' ');
    const QString b = hexB.trimmed().remove(' ');
    if (a.length() != b.length() || a.length() % 2 != 0) return {};
    if (!isHexString(a) || !isHexString(b)) return {};
    return bytesToHex(xorBytes(hexToBytes(a), hexToBytes(b)));
}

QString CardUtils::calcKeyCheckCode(const QString& hexKey)
{
    const QString key = hexKey.trimmed().remove(' ').toUpper();
    if (key.length() != 48 || !isHexString(key)) return {};
    // 3DES-ECB 對單一 8 bytes 區塊，IV 不影響結果（ECB 不使用 IV）
    const QString out = encrypt3DES(key, QStringLiteral("0000000000000000"));
    return out.left(4);
}

QString CardUtils::bytesToHex(const QByteArray& bytes)
{
    return QString(bytes.toHex()).toUpper();
}

// 補齊金鑰長度至 8 / 16 / 24 bytes
QByteArray CardUtils::padKeyToValid(const QByteArray& key, const QString& label, const LogFn& log)
{
    QByteArray k = key;
    int len = k.size();
    int target = (len <= 8) ? 8 : (len <= 16) ? 16 : (len <= 24) ? 24 : -1;

    if (target < 0) {
        k = k.left(24);
        log("⚠️ " + label + " 超過 24 bytes，已截斷至 24 bytes。", false);
        return k;
    }
    if (len != target) {
        log(QString("⚠️ %1 長度 %2 bytes 不合規，補齊至 %3 bytes (0xFF)。")
            .arg(label).arg(len).arg(target), false);
        k = k.leftJustified(target, static_cast<char>(0xFF));
    }
    return k;
}

// ─────────────────────────────────────────────────────────────────────────────
// ISAM CD Key 對照表
// ─────────────────────────────────────────────────────────────────────────────
QString CardUtils::lookupIsamCdKey(const QString& responseHex)
{
    // 取前 4 個 HEX 字元（= 2 bytes）做比對
    QString code = responseHex.trimmed().remove(' ').left(4).toUpper();

    if (code == "656B") return "111111111111111122222222222222223333333333333333";
    if (code == "C396") return "010203040506070811121314151617182122232425262728";
    if (code == "E427") return "EF295380F1029C073D581C021FB39DCE10B50F439041E61F";
    if (code == "8CA6") return "000000000000000000000000000000000000000000000000";
    if (code == "BE0A") return "000000000000000011111111111111112222222222222222";

    return {};   // 未知型別 → 回傳空字串
}

// ─────────────────────────────────────────────────────────────────────────────
// 「檢查卡片 KEY」功能專用（比照 DESFuc.cs 邏輯）
// ─────────────────────────────────────────────────────────────────────────────

// 單一 DES 加密／解密一個 8 bytes 區塊，ECB 模式
QString CardUtils::desSingleBlockHex(const QString& hexKey16, const QString& hexData16, bool encrypt)
{
    QByteArray keyBytes = hexToBytes(hexKey16);
    QByteArray dataBytes = hexToBytes(hexData16);
    if (keyBytes.size() < 8 || dataBytes.size() < 8) return {};

    DES_cblock key{};
    memcpy(&key, keyBytes.constData(), 8);

    DES_key_schedule ks;
    DES_set_key_unchecked(&key, &ks);

    DES_cblock in{}, out{};
    memcpy(&in, dataBytes.constData(), 8);
    DES_ecb_encrypt(&in, &out, &ks, encrypt ? DES_ENCRYPT : DES_DECRYPT);

    return bytesToHex(QByteArray(reinterpret_cast<const char*>(out), 8));
}

// 單一 DES（8 bytes key）以 ECB 模式逐區塊加密／解密任意長度資料
// 對應 DESFuc.DesEncrypt / DesDecrypt (cbc=false)：同一把 key 對每個 8 bytes 區塊各自運算
QString CardUtils::desHex(const QString& hexKey16, const QString& hexData, bool encrypt)
{
    QString key = hexKey16.left(16);
    QString result;
    for (int i = 0; i + 16 <= hexData.length(); i += 16) {
        QString block = hexData.mid(i, 16);
        result += desSingleBlockHex(key, block, encrypt);
    }
    return result;
}

// 3DES（key 長度 16/32/48 hex 皆可）逐區塊 EDE 運算，cbc=false
// 對應 DESFuc.Encrypt3DES：
//   key 長度 32 hex → 補成 key + key 前 16 hex（共 48 hex）
//   針對每個 16 hex（8 bytes）資料區塊，依序用各段 key 做 encrypt/decrypt/encrypt...（j 為偶數 encrypt，奇數 decrypt）
QString CardUtils::tripleDesHex(const QString& hexKey, const QString& hexData)
{
    QString key = hexKey;
    if (key.length() == 32)
        key = key + key.left(16);

    int dataCount = hexData.length() / 16;
    int keyCount = key.length() / 16;
    if (dataCount <= 0 || keyCount <= 0) return {};

    QString result;
    for (int i = 0; i < dataCount; ++i) {
        QString tempData = hexData.mid(i * 16, 16);
        for (int j = 0; j < keyCount; ++j) {
            QString tempKey = key.mid(j * 16, 16);
            tempData = desSingleBlockHex(tempKey, tempData, (j % 2 == 0));
        }
        result += tempData;
    }
    return result;
}

// Key Diversification：對應 DESFuc.DivKey
// 主金鑰每 16 hex（8 bytes）切一段，各段各自用單一 DES「解密」input，再串接起來
QString CardUtils::divKey(const QString& hexMasterKey, const QString& hexInput)
{
    int count = hexMasterKey.length() / 16;
    QString result;
    for (int i = 0; i < count; ++i) {
        QString segment = hexMasterKey.mid(i * 16, 16);
        result += desSingleBlockHex(segment, hexInput.left(16), /*encrypt=*/false);
    }
    return result;
}

// Session Key 計算：對應 DESFuc.GetSessionKey
// sessionKey = tripleDesHex( tripleDesHex(key, iv, r1), iv, r2 )
QString CardUtils::getSessionKey(const QString& hexKey, const QString& hexR1, const QString& hexR2)
{
    QString inner = tripleDesHex(hexKey, hexR1);
    return tripleDesHex(inner, hexR2);
}

// 產生指定 bytes 數的隨機 HEX 字串（對應 DESFuc.GetRandom，預設 8 bytes）
QString CardUtils::generateRandomHex(int numBytes)
{
    QString result;
    for (int i = 0; i < numBytes; ++i) {
        quint32 b = QRandomGenerator::global()->bounded(256);
        result += QString("%1").arg(b, 2, 16, QChar('0')).toUpper();
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// 「加密工具」頁籤專用：通用 DES / 3DES 對稱加解密（ECB / CBC）
// ─────────────────────────────────────────────────────────────────────────────

// 單一 8 bytes 區塊運算（key 長度 16/32/48 hex，encrypt/decrypt 互為反運算）
QString CardUtils::desBlockCipher(const QString& hexKey, const QString& hexBlock16, bool encrypt)
{
    if (hexBlock16.length() != 16) return {};

    QString key = hexKey;
    if (key.length() == 32)          // 2-key 3DES → K1,K2,K1
        key = key + key.left(16);

    int keyCount = key.length() / 16;
    if (keyCount != 1 && keyCount != 3) return {};   // 只接受 16 / 32 / 48 hex

    QString data = hexBlock16;

    if (keyCount == 1) {
        // 單一 DES：encrypt 參數直接對應加密/解密
        return desSingleBlockHex(key, data, encrypt);
    }

    // 3DES：加密＝K1 Encrypt → K2 Decrypt → K3 Encrypt（EDE）
    //       解密＝K3 Decrypt → K2 Encrypt → K1 Decrypt（反序、每段方向相反，即 DED）
    if (encrypt) {
        for (int j = 0; j < keyCount; ++j) {
            QString segKey = key.mid(j * 16, 16);
            bool segEncrypt = (j % 2 == 0);   // j=0:E  j=1:D  j=2:E
            data = desSingleBlockHex(segKey, data, segEncrypt);
        }
    }
    else {
        for (int j = keyCount - 1; j >= 0; --j) {
            QString segKey = key.mid(j * 16, 16);
            bool segEncrypt = !(j % 2 == 0);  // 反序且方向相反：j=2→D j=1→E j=0→D
            data = desSingleBlockHex(segKey, data, segEncrypt);
        }
    }
    return data;
}

// ECB 模式：逐區塊各自獨立運算
QString CardUtils::desEcb(const QString& hexKey, const QString& hexData, bool encrypt)
{
    if (hexData.isEmpty() || hexData.length() % 16 != 0) return {};

    QString result;
    for (int i = 0; i + 16 <= hexData.length(); i += 16) {
        QString block = desBlockCipher(hexKey, hexData.mid(i, 16), encrypt);
        if (block.isEmpty()) return {};
        result += block;
    }
    return result;
}

// CBC 模式：區塊間以前一區塊密文（第一區塊用 IV）鏈結
QString CardUtils::desCbc(const QString& hexKey, const QString& hexIV, const QString& hexData, bool encrypt)
{
    if (hexData.isEmpty() || hexData.length() % 16 != 0) return {};

    QString iv = hexIV.trimmed();
    if (iv.isEmpty()) iv = QString("0000000000000000");
    if (iv.length() != 16) return {};

    QString result;
    QString prevCipherBlock = iv;   // 加密：前一區塊密文；解密：前一區塊密文（即本次輸入的前一段）

    for (int i = 0; i + 16 <= hexData.length(); i += 16) {
        QString block = hexData.mid(i, 16);

        if (encrypt) {
            QString xored = bytesToHex(xorBytes(hexToBytes(block), hexToBytes(prevCipherBlock)));
            QString cipher = desBlockCipher(hexKey, xored, true);
            if (cipher.isEmpty()) return {};
            result += cipher;
            prevCipherBlock = cipher;
        }
        else {
            QString decryptedRaw = desBlockCipher(hexKey, block, false);
            if (decryptedRaw.isEmpty()) return {};
            QString plain = bytesToHex(xorBytes(hexToBytes(decryptedRaw), hexToBytes(prevCipherBlock)));
            result += plain;
            prevCipherBlock = block;   // 本區塊密文成為下一區塊的鏈結值
        }
    }
    return result;
}