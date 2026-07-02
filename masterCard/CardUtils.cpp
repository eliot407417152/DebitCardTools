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
