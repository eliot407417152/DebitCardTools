#pragma once

#include "CardUtils.h"
#include <winscard.h>

// ─────────────────────────────────────────────────────────────────────────────
// MasterCardUpdater — 製作母卡（Exchange Key 寫入 + 雙向認證 + 資料寫入）
//
// 製作母卡請直接呼叫 makeMasterCard()，一次完成：
//   0. writeExchangeKey() — ISAM 寫入 Exchange Key（KID=10）
//        Select ISAM Object → Get Key Check Code（對照 CD Key）
//        → Write Key  CLA:82 INS:03 P1:10 P2:02 Lc:1E
//          Data = EKey + CCode + Limit(FFFFFFFF)
//          EKey  = CD Key 3DES 加密 LOD 的 Exchange Code
//          CCode = Exchange Code 的檢查碼（3DES-ECB，Data=0000000000000000，取前 4 碼）
//   1. authenticate()   — 雙向認證，成功後才能寫入
//   2. writeData()      — 依序寫入 C8, C9, 0001, 0002
//
// 寫入流程：
//   C8 / C9  (AEF — PIN)
//     ├ Create File
//     ├ Select File
//     └ Write Record  [01][08][8 bytes PIN]
//
//   0001 / 0002  (DEF — Encrypted Key + Check Code)
//     ├ Create File
//     ├ Select File
//     ├ Get Encrypted Key  (→ ISAM: 82 06 10 P2 00)
//     ├ Write Record #1   [01][18][24 bytes EncKey]
//     ├ Get Key Check Code (→ ISAM: 82 07 P2 00 06) 取前 2 bytes
//     └ Write Record #2   [02][02][2 bytes CheckCode]
// ─────────────────────────────────────────────────────────────────────────────
class MasterCardUpdater
{
public:
    MasterCardUpdater(SCARDHANDLE masterHandle, SCARDHANDLE isamHandle);

    // 製作母卡（完整流程）：寫入 Exchange Key → 雙向認證 → 寫入 C8/C9/0001/0002
    //   使用 data.exchangeCode / data.ccode / data.pinA / data.pinB
    bool makeMasterCard(const CustomerData& data, const LogFn& log);

    // 雙向認證（makeMasterCard 內部使用；單獨呼叫時須先於 writeData）
    bool authenticate(const LogFn& log);

    // 寫入全部 EF（C8, C9, 0001, 0002）
    bool writeData(const CustomerData& data,
        const QString& pinA,
        const QString& pinB,
        const LogFn& log);

    // ─────────────────────────────────────────────────────────────────────
    // 母卡轉換 — 拿製作好的母卡，跟銀行卡的 ISAM 做母卡轉換
    //
    //   1. Write Key (ISAM)：CLA:82 INS:03 P1:00(KID) P2:08(KT) 固定值
    //        先執行 Select ISAM Object（CLA:82 INS:A4 P1:00 P2:00 Lc:02 DataIn:9500 Le:0E）
    //        EKey = ISAM CD Key 3DES 加密 Exchange Key
    //               （CD Key 透過 Get Key Check Code 取前 2 bytes 對照 CardUtils::lookupIsamCdKey 取得）
    //        CCode = 客戶資料 ccode
    //        Limit = FFFFFFFF
    //   2. Verify PIN A（母卡，P2 = 01 左移 3 位 = 08）
    //   3. Verify PIN B（母卡，P2 = 02 左移 3 位 = 10）
    //   4~7. 針對 0001（KT=03, KID=11）與 0002（KT=04, KID=12）：
    //        Select EFID → Read Record r01 / r02 → Store Manufacturer Key
    // ─────────────────────────────────────────────────────────────────────
    bool convertToBankIsam(const CustomerData& data, const LogFn& log);

private:
    SCARDHANDLE m_masterHandle;
    SCARDHANDLE m_isamHandle;

    // ── ISAM 寫入 Exchange Key（製作母卡的第一步）──────────────────────────
    //   Select ISAM Object → Get Key Check Code（取得 CD Key）→ Write Key（P1=10 P2=02）
    bool writeExchangeKey(const CustomerData& data, const LogFn& log);

    // ── AEF（C8 / C9）──────────────────────────────────────────────────────
    bool writeAEF(const QString& label,
        const QString& efid,
        const QString& ac1,
        const QString& ac2,
        const QString& ac3,
        quint8         keyIndex,
        const QByteArray& pinData,
        const LogFn& log);

    // ── DEF（0001 / 0002）──────────────────────────────────────────────────
    bool writeDEF(const QString& label,
        const QString& efid,
        quint8         isamP2,    // Get Encrypted Key / Check Code 的 P2
        const LogFn& log);

    // ── 母卡轉換：單一金鑰區塊（0001 或 0002）處理 ─────────────────────────
    //   Select EFID(母卡) → Read Record r01 / r02(母卡) → Store Manufacturer Key(ISAM)
    bool convertOneKeyBlock(const QString& efid,
        const QString& kt,          // Store Manufacturer Key 的 KT
        const QString& storeKeyKid, // Store Manufacturer Key 的 P2（KID，例如 11 / 12）
        const LogFn& log);

    // ── 共用輔助 ────────────────────────────────────────────────────────────
    bool    sendCheck(SCARDHANDLE hCard, const QString& apdu,
        const QString& stepDesc, const LogFn& log);
    bool    sendCheck(SCARDHANDLE hCard, const QString& apdu,
        const QString& stepDesc, const LogFn& log, QString& respOut);

    // 組 Create File DataIn (16 bytes)
    static QString buildCreateDataIn(const QString& efid,
        const QString& ac1,
        const QString& ac2,
        const QString& ac3,
        const QString& sob,
        quint8 tof,
        quint8 sor,
        quint8 rc,
        quint8 keyIndex,
        quint8 tok,
        quint8 rfu = 0x00);

    // 組 Write Record APDU
    static QString buildWriteRecord(quint8 recordId,
        const QByteArray& payload);
};