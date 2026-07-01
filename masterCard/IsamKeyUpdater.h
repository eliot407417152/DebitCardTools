#pragma once

#include "CardUtils.h"
#include <winscard.h>

// ─────────────────────────────────────────────────────────────────────────────
// IsamKeyUpdater — 負責將 C3 / C1 金鑰寫入 ISAM 卡
// ─────────────────────────────────────────────────────────────────────────────
class IsamKeyUpdater
{
public:
    explicit IsamKeyUpdater(SCARDHANDLE isamHandle);

    // 執行完整更新流程；透過 log 回呼輸出進度
    bool execute(const CustomerData& data, const LogFn& log);

private:
    SCARDHANDLE m_handle;

    // 輔助：傳送指令並檢查回傳必須是 9000，否則記錄錯誤並回傳 false
    bool send(const QString& apdu, const QString& stepDesc, const LogFn& log);
    bool send(const QString& apdu, const QString& stepDesc, const LogFn& log, QString& respOut);
};
