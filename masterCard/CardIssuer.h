#pragma once

#include "CardUtils.h"
#include <winscard.h>

// ─────────────────────────────────────────────────────────────────────────────
// CardIssuer — 發卡
//
//   對「卡片」與銀行 ISAM 執行完整發卡流程，步驟 1~17 與 CardWasher（洗卡）
//   完全相同（建立 Session Key、處理 00C3 / 00C1 / 00C0 三個金鑰檔案），
//   之後接續建立發卡所需的應用檔案與個人化資料檔案：
//
//     1~8.   建立 Session Key（同洗卡）
//     9~17.  00C3 / 00C1 / 00C0（同洗卡，00C0 若不存在會自動 Create File）
//     18~.   依序處理 00C4 / 00C6 / 00D6 / 00D7 / 00C8：
//              Create File → Get Div Key With SK（ISAM，P1=KID）→
//              Select File（卡片）→ Update Record Ciphered（卡片）
//     特例 00C5：Create File → Get Working Key With SK（ISAM，P1=KID，
//              無 Lc / DataIn）→ Select File → Update Record Ciphered
//     特例 00C2：Create File → Select File → Update Record Ciphered
//              （此檔案無 KID、無 Get Div Key 步驟，Update 資料需自行輸入，
//              由呼叫端以 manualC2Data 參數提供）
//     最後：依序 Create File 建立個人化資料檔（0001 / 0002 / 1001 / 1002 /
//              1003 / 1080 / 1090 / 1102 / 1103 / 007F / 2F00），僅執行
//              Create File，不執行 Select / Update。
//     再之後：針對 0001 / 0002 / 1001 / 1002 這 4 個檔案，各自 Select File
//              後依序 Write Record 寫入多筆個人化資料（每筆 DataIn 格式為
//              [Block ID 1 byte][長度 1 byte][資料]，Lc 依實際長度自動計算）。
//
//   【實作說明】
//     比照 CardWasher 的作法，所有 APDU 的 Lc 一律依「實際組出的 DataIn
//     長度」自動計算。個人化資料檔的 Create File DataIn 為完整 16 bytes
//     （已含檔案編號 2 bytes 前綴 + 14 bytes 參數），直接依文件所列數值
//     使用，Lc 計算結果為 10h。
// ─────────────────────────────────────────────────────────────────────────────
class CardIssuer
{
public:
    CardIssuer(SCARDHANDLE cardHandle, SCARDHANDLE isamHandle);

    // 執行完整發卡流程
    //   manualC2Data：00C2 Update Record Ciphered 所需之自行輸入資料（HEX 字串）。
    //                 對應文件中「DataIN:自行輸入」，因無法由本流程自動推導，
    //                 需由呼叫端（UI）提供。
    bool execute(const QString& manualC2Data, const LogFn& log);

private:
    SCARDHANDLE m_cardHandle;
    SCARDHANDLE m_isamHandle;

    bool sendCheck(SCARDHANDLE hCard, const QString& apdu,
        const QString& stepDesc, const LogFn& log);
    bool sendCheck(SCARDHANDLE hCard, const QString& apdu,
        const QString& stepDesc, const LogFn& log, QString& respOut);

    // 依序處理單一金鑰檔案（00C3 / 00C1 / 00C0，同 CardWasher 的作法）：
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

    // 建立發卡應用檔案（一般情形，適用 00C4 / 00C6 / 00D6 / 00D7 / 00C8）：
    //   Create File（卡片，DataIn=createDataIn）→
    //   Get Div Key With SK（ISAM，P1=kid，DataIn=readBinData）→
    //   Select File（卡片）→ Update Record Ciphered（卡片）
    bool createAppFile(const QString& efid,
        const QString& createDataIn,
        const QString& kid,
        const QString& readBinData,
        const LogFn& log);

    // 建立發卡應用檔案 — 00C5 特例：
    //   Create File（卡片）→ Get Working Key With SK（ISAM，P1=kid，
    //   無 Lc / DataIn）→ Select File（卡片）→ Update Record Ciphered（卡片）
    bool createAppFile00C5(const QString& kid, const LogFn& log);

    // 建立發卡應用檔案 — 00C2 特例：
    //   Create File（卡片）→ Select File（卡片）→
    //   Update Record Ciphered（卡片，DataIn=manualData 自行輸入）
    bool createAppFile00C2(const QString& manualData, const LogFn& log);

    // 建立個人化資料檔案：僅 Create File，無 Select / Update
    bool createDataFile(const QString& fileNo, const QString& createDataIn, const LogFn& log);

    // 寫入個人化資料內容（0001 / 0002 / 1001 / 1002 專用）：
    //   Select File（卡片）→ 依序對每個 block 執行 Write Record（卡片）
    //   blocks 內每一筆字串皆已是完整 DataIn（[Block ID 1B][Length 1B][資料]），
    //   Lc 依該筆 DataIn 實際長度自動計算，不採用文件裡標示的 Lc 數字。
    bool writeDataRecords(const QString& fileNo, const QStringList& blocks, const LogFn& log);
};
