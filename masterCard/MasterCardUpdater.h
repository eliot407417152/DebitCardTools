#pragma once

#include "CardUtils.h"
#include <winscard.h>

// ─────────────────────────────────────────────────────────────────────────────
// MasterCardUpdater — 母卡雙向認證 & 資料寫入
//
// 正確使用順序：
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

    // 雙向認證（須先呼叫）
    bool authenticate(const LogFn& log);

    // 寫入全部 EF（C8, C9, 0001, 0002）
    bool writeData(const CustomerData& data,
        const QString& pinA,
        const QString& pinB,
        const LogFn& log);

private:
    SCARDHANDLE m_masterHandle;
    SCARDHANDLE m_isamHandle;

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
