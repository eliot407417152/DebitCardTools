#pragma once

#include "CardUtils.h"
#include <winscard.h>

// ─────────────────────────────────────────────────────────────────────────────
// 檢查結果
// ─────────────────────────────────────────────────────────────────────────────
struct KeyCheckResult
{
    bool    success = false;   // 最終比對是否通過（FISC OK）
    QString icid;              // 卡片 ICID
    QString lastStatus;        // 最後一次外部認證的狀態碼（passcode 比對對象）
    QString listFile;          // List File 回傳內容（若卡片有回傳）
    bool    listFileMismatch = false; // 有帶入 expectedListFile 時才有意義
};

// ─────────────────────────────────────────────────────────────────────────────
// IsamKeyChecker — 對應 C# CheckKey()：
//   1) 先以目前選定的 C3 對卡片做一次外部認證（DoC3Aauthen）
//   2) 若通過，改用 C1 建立 Session Key 並覆寫 C3（UpdateC3wSession）
//   3) 再做一次外部認證，比對結果 = 「檢查 KEY 是否正確」的最終結論
// ─────────────────────────────────────────────────────────────────────────────
class IsamKeyChecker
{
public:
    explicit IsamKeyChecker(SCARDHANDLE cardHandle);

    // 執行完整檢查流程；expectedListFile 留空表示不比對 List File 內容
    KeyCheckResult execute(const CustomerData& data, const QString& expectedListFile, const LogFn& log);

private:
    SCARDHANDLE m_handle;

    // 對應 DoC3Aauthen()：回傳最後一次外部認證的狀態碼（passcode 比對對象）
    // 過程中會填入 icidOut / listFileOut；若卡片未回應資料（例如 Get Challenge 失敗），icidOut 會被清空
    QString doC3Authen(const CustomerData& data, QString& icidOut, QString& listFileOut,
        QString& divKeyC3Out, const LogFn& log);

    // 對應 UpdateC3wSession()：用 C1 建立 Session Key，並將 C3 以 Session Key 覆寫回卡片
    bool updateC3WithSession(const CustomerData& data, const QString& icid,
        const QString& divKeyC3, const LogFn& log);

    // 輔助：傳送 APDU，回傳完整回應（含 SW），ok 表示通訊層是否成功
    QString transmit(const QString& apdu, const QString& stepDesc, const LogFn& log, bool& ok);
};
