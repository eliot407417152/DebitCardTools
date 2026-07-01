#pragma once

#define _OSSL_DEPRECATED_ALLOW_
#pragma warning(disable: 4996)

#include <QString>
#include <QByteArray>
#include <QStringList>
#include <functional>
#include <winscard.h>
#include <openssl/des.h>

// ─────────────────────────────────────────────────────────────────────────────
// 客戶資料結構
// ─────────────────────────────────────────────────────────────────────────────
struct CustomerData {
    QString name;
    QString c3, c3Code;
    QString c1, c1Code;
    QString exchangeCode;
    QString codeA, codeB;
    QString pinA, pinB;

    bool isValid() const {
        return !c3.isEmpty() && !c1.isEmpty() && !codeA.isEmpty() && !codeB.isEmpty();
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Log 回呼型別 (msg, isError=false)
// ─────────────────────────────────────────────────────────────────────────────
using LogFn = std::function<void(const QString& msg, bool isError)>;

// 單參數便利包裝（避免每次呼叫都要帶 false）
inline void logInfo(const LogFn& fn, const QString& msg) { fn(msg, false); }
inline void logErr(const LogFn& fn, const QString& msg) { fn(msg, true); }

// ─────────────────────────────────────────────────────────────────────────────
// CardUtils — 靜態工具集合
// ─────────────────────────────────────────────────────────────────────────────
class CardUtils
{
public:
    // ── APDU 傳輸 ────────────────────────────────────────────────────────────
    // 傳送 APDU 指令並回傳 HEX 字串，ok 指示通訊是否成功
    static QString sendApdu(SCARDHANDLE hCard,
        const QString& apduHex,
        bool& ok,
        const LogFn& log);

    // ── 密碼學 ───────────────────────────────────────────────────────────────
    // 3DES-ECB 加密：key/data 皆為 HEX 字串，回傳加密後 HEX 字串
    static QString  encrypt3DES(const QString& hexKey, const QString& hexData);

    // 3DES-ECB 加密：key/data 皆為 QByteArray，回傳加密後 QByteArray
    static QByteArray encrypt3DES(const QByteArray& key, const QByteArray& data);

    // XOR 兩段等長 QByteArray
    static QByteArray xorBytes(const QByteArray& a, const QByteArray& b);

    // ── 格式轉換 ─────────────────────────────────────────────────────────────
    static QByteArray hexToBytes(const QString& hex);
    static QString    bytesToHex(const QByteArray& bytes);

    // 補齊 QByteArray 至 8/16/24 bytes (填 0xFF)
    static QByteArray padKeyToValid(const QByteArray& key, const QString& label, const LogFn& log);

    // ── ISAM CD Key 對照表 ───────────────────────────────────────────────────
    // 根據 ISAM 回傳前 2 bytes 選取對應的基礎衍生 CD Key
    static QString lookupIsamCdKey(const QString& responseHex);
};
