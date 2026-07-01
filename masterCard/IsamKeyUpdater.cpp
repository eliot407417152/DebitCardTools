#include "IsamKeyUpdater.h"

IsamKeyUpdater::IsamKeyUpdater(SCARDHANDLE isamHandle)
    : m_handle(isamHandle)
{
}

// ─────────────────────────────────────────────────────────────────────────────
// 主流程
// ─────────────────────────────────────────────────────────────────────────────
bool IsamKeyUpdater::execute(const CustomerData& data, const LogFn& log)
{
    if (m_handle == 0) {
        logErr(log, "Error: 無效的 ISAM 卡片控制代碼");
        return false;
    }

    logInfo(log, "====== [ISAM] 啟動金鑰更新流程 ======");

    // 1. Select ISAM Object
    if (!send("82A400000295020E", "Select ISAM Object", log)) return false;

    // 2. 查詢 CD Key 屬性
    QString resp;
    if (!send("8207000007", "Get CD Key 屬性", log, resp)) return false;

    QString cdKey = CardUtils::lookupIsamCdKey(resp);
    if (cdKey.isEmpty()) {
        logErr(log, QString("Error: 無法對照 CD Key 類型 (卡片回傳前4碼: %1)").arg(resp.left(4)));
        return false;
    }
    logInfo(log, "✔ CD Key 對齊成功。");

    // 3. 寫入 C3 金鑰
    if (!send("8207050007", "C3 前置指令", log)) return false;

    QString eKeyC3 = CardUtils::encrypt3DES(cdKey, data.c3);
    QString cmdC3 = QString("820305031E%1%2FFFFFFFF")
        .arg(eKeyC3)
        .arg(data.c3Code.rightJustified(4, '0').left(4));
    if (!send(cmdC3, "Write C3", log)) return false;
    logInfo(log, "✔ C3 金鑰寫入成功。");

    // 4. 寫入 C1 金鑰
    if (!send("8207060007", "C1 前置指令", log)) return false;

    QString eKeyC1 = CardUtils::encrypt3DES(cdKey, data.c1);
    QString cmdC1 = QString("820306041E%1%2FFFFFFFF")
        .arg(eKeyC1)
        .arg(data.c1Code.rightJustified(4, '0').left(4));
    if (!send(cmdC1, "Write C1", log)) return false;
    logInfo(log, "✔ C1 金鑰寫入成功。");

    logInfo(log, "🎉 ====== [ISAM] C3 & C1 全部寫入完畢 ======");
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 輔助：send (不需要回傳值)
// ─────────────────────────────────────────────────────────────────────────────
bool IsamKeyUpdater::send(const QString& apdu, const QString& stepDesc, const LogFn& log)
{
    QString dummy;
    return send(apdu, stepDesc, log, dummy);
}

// 輔助：send (需要回傳值)
bool IsamKeyUpdater::send(const QString& apdu, const QString& stepDesc, const LogFn& log, QString& respOut)
{
    bool ok = false;
    respOut = CardUtils::sendApdu(m_handle, apdu, ok, log);
    if (!ok || respOut.right(4) != "9000") {
        logErr(log, QString("Error: [%1] 失敗 (SW=%2)").arg(stepDesc).arg(respOut.right(4)));
        return false;
    }
    return true;
}
