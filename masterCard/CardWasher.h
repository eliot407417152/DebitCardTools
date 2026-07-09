#pragma once

#include "CardUtils.h"
#include <winscard.h>

// ─────────────────────────────────────────────────────────────────────────────
// CardWasher — 洗卡
//
//   對「卡片」（實際發卡標的，與母卡不同）與銀行 ISAM 執行完整洗卡流程：
//     1. Read Binary（卡片）
//     2. Generate Random Number（卡片）
//     3. Select ISAM Object
//     4. Generate Terminal Authentication Code（ISAM）→ MC3
//     5. Terminal Authentication（卡片）
//     6. Generate Random Number（ISAM）
//     7. Set Session Key（卡片）
//     8. Set Session Key（ISAM）
//     9~11.  00C3：Get Div Key With SK（ISAM）→ Select（卡片）→ Update Record Ciphered（卡片）
//     12~14. 00C1：同上
//     15~17. 00C0：同上（若 Select 回應 6602，代表檔案不存在，會自動 Create File 後重試一次）
//
//   【實作說明】
//     文件裡部分步驟標示的 Lc 與實際 DataIn 長度對不上（例如步驟 8 標示 Lc:08，
//     但 DataIn 是兩段 8 bytes 回應相接、實際應為 16 bytes；步驟 9/12/16 標示
//     Lc:00，但 DataIn 明確有內容），為避免組出錯誤的 APDU，本類別一律依「實際
//     組出的 DataIn 長度」自動計算 Lc，不採用文件裡標示的 Lc 數字。
// ─────────────────────────────────────────────────────────────────────────────
class CardWasher
{
public:
    CardWasher(SCARDHANDLE cardHandle, SCARDHANDLE isamHandle);

    // 執行完整洗卡流程
    bool execute(const LogFn& log);

private:
    SCARDHANDLE m_cardHandle;
    SCARDHANDLE m_isamHandle;

    bool sendCheck(SCARDHANDLE hCard, const QString& apdu,
        const QString& stepDesc, const LogFn& log);
    bool sendCheck(SCARDHANDLE hCard, const QString& apdu,
        const QString& stepDesc, const LogFn& log, QString& respOut);

    // 依序處理單一金鑰檔案（00C3 / 00C1 / 00C0）：
    //   Get Div Key With SK（ISAM，P1=isamP1）→ Select EFID（卡片）
    //   → Update Record Ciphered（卡片）
    //   若 Select 回應 SW=6602（檔案不存在）且 allowCreate 為 true，
    //   會先以 createDataIn 執行 Create File，再重新 Select 一次。
    bool processKeyFile(const QString& isamP1,
        const QString& efid,
        const QString& readBinData,
        bool allowCreate,
        const QString& createDataIn,
        const LogFn& log);
};
