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
