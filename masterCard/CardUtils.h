#pragma once

#include <QString>
#include <QByteArray>
#include <QStringList>
#include <QRandomGenerator>
#include <functional>
#include <winscard.h>
#include "SimpleDes.h"   // 內建 DES/3DES（不再依賴 OpenSSL）

// ─────────────────────────────────────────────────────────────────────────────
// 客戶資料結構
// ─────────────────────────────────────────────────────────────────────────────
struct CustomerData {
    QString name;
    QString c3, c3Code;
    QString c1, c1Code;
    QString exchangeCode;
    QString ccode;          // 母卡轉換用 CCode（手動填入）
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

    // XOR 兩段等長 HEX 字串，回傳 HEX；長度不同、為空或含非 HEX 字元時回傳空字串
    static QString xorHex(const QString& hexA, const QString& hexB);

    // 金鑰檢查碼：以 3DES-ECB（IV=0）加密 8 bytes 全 0，取輸出前 4 個 HEX 字元。
    // hexKey 必須是 48 個 HEX 字元（K1+K2+K3，各 16 碼），否則回傳空字串。
    // 用於 Code A / Code B 的檢查碼，以及 Exchange Code 的 CCode。
    static QString calcKeyCheckCode(const QString& hexKey);

    // ── 格式轉換 ─────────────────────────────────────────────────────────────
    static QByteArray hexToBytes(const QString& hex);
    static QString    bytesToHex(const QByteArray& bytes);

    // 補齊 QByteArray 至 8/16/24 bytes (填 0xFF)
    static QByteArray padKeyToValid(const QByteArray& key, const QString& label, const LogFn& log);

    // ── ISAM CD Key 對照表 ───────────────────────────────────────────────────
    // 根據 ISAM 回傳前 2 bytes 選取對應的基礎衍生 CD Key
    static QString lookupIsamCdKey(const QString& responseHex);

    // ─────────────────────────────────────────────────────────────────────
    // 「檢查卡片 KEY」功能專用（比照 DESFuc.cs 的演算法邏輯，純 HEX 字串介面）
    // ─────────────────────────────────────────────────────────────────────
    // 單一 DES（8 bytes key）加密／解密一個 8 bytes 區塊，ECB 模式
    static QString desSingleBlockHex(const QString& hexKey16, const QString& hexData16, bool encrypt);

    // 單一 DES（8 bytes key）以 ECB 模式逐區塊加密／解密任意長度資料（對應 DESFuc.DesEncrypt / DesDecrypt，cbc=false）
    static QString desHex(const QString& hexKey16, const QString& hexData, bool encrypt);

    // 3DES（key 長度 16/32/48 hex 皆可）逐區塊 EDE 運算，cbc=false（對應 DESFuc.Encrypt3DES）
    // key 長度 16 hex：等同單一 DES 加密；key 長度 48 hex：標準三把金鑰 EDE 3DES
    static QString tripleDesHex(const QString& hexKey, const QString& hexData);

    // Key Diversification（對應 DESFuc.DivKey）：以 8 bytes input 對主金鑰逐段做單一 DES 解密後串接
    static QString divKey(const QString& hexMasterKey, const QString& hexInput);

    // Session Key 計算（對應 DESFuc.GetSessionKey）
    static QString getSessionKey(const QString& hexKey, const QString& hexR1, const QString& hexR2);

    // 產生指定 bytes 數的隨機 HEX 字串（對應 DESFuc.GetRandom，預設 8 bytes）
    static QString generateRandomHex(int numBytes = 8);

    // ─────────────────────────────────────────────────────────────────────
    // 「加密工具」頁籤專用：通用 DES / 3DES 對稱加解密（ECB / CBC）
    // 皆為純 HEX 字串介面，key / data 長度需為區塊大小的整數倍，否則回傳空字串
    // ─────────────────────────────────────────────────────────────────────
    // 單一 8 bytes 區塊運算：
    //   key 長度 16 hex → 單一 DES
    //   key 長度 32 hex → 2-key 3DES（K1,K2,K1）
    //   key 長度 48 hex → 3-key 3DES（K1,K2,K3）
    // encrypt=true 時做 E-D-E（3DES）或單純 Encrypt（DES）；
    // encrypt=false 時做對應反向的 D-E-D（3DES）或 Decrypt（DES），確保加解密互為反運算
    static QString desBlockCipher(const QString& hexKey, const QString& hexBlock16, bool encrypt);

    // ECB 模式：資料逐 8 bytes 區塊各自獨立運算（區塊之間互不影響）
    // hexData 長度需為 16（HEX 字元，即 8 bytes）的整數倍，否則回傳空字串
    static QString desEcb(const QString& hexKey, const QString& hexData, bool encrypt);

    // CBC 模式：需搭配 8 bytes IV（16 個 HEX 字元；hexIV 空白時視為全 0）
    // 加密：本區塊明文先與「前一區塊密文（第一區塊用 IV）」XOR，再做區塊加密
    // 解密：本區塊密文先做區塊解密，再與「前一區塊密文（第一區塊用 IV）」XOR 還原明文
    static QString desCbc(const QString& hexKey, const QString& hexIV, const QString& hexData, bool encrypt);
};