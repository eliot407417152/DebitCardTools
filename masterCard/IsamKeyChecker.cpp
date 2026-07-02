#include "IsamKeyChecker.h"

// ─────────────────────────────────────────────────────────────────────────────
// 說明：
// 本檔案將 C# 版 CheckKey() / DoC3Aauthen() / UpdateC3wSession() 的邏輯搬移過來。
//
// 【重要假設，如與實際規格不符請告知調整】
//   passcode（C# 原碼中用來比對外部認證是否成功的值）在此假設為 "9000"，
//   因為 External Authenticate (0082...) 指令本身不帶 Le，卡片只會回傳
//   狀態碼（SW），並不會有額外資料欄位，故 C# 版 resp[0] 實質上就是 SW。
//   若貴公司的卡片規格中 passcode 並非 "9000"，請告知，我再調整
//   kPasscode 這個常數即可，不需更動其他邏輯。
// ─────────────────────────────────────────────────────────────────────────────
static const QString kPasscode = "9000";

IsamKeyChecker::IsamKeyChecker(SCARDHANDLE cardHandle)
    : m_handle(cardHandle)
{
}

// ─────────────────────────────────────────────────────────────────────────────
// 輔助：傳送 APDU，回傳完整回應（含 SW）
// ─────────────────────────────────────────────────────────────────────────────
QString IsamKeyChecker::transmit(const QString& apdu, const QString& stepDesc, const LogFn& log, bool& ok)
{
    QString resp = CardUtils::sendApdu(m_handle, apdu, ok, log);
    if (!ok) {
        logErr(log, QString("Error: [%1] 通訊失敗").arg(stepDesc));
    }
    return resp;
}

// ─────────────────────────────────────────────────────────────────────────────
// 對應 DoC3Aauthen()
// ─────────────────────────────────────────────────────────────────────────────
QString IsamKeyChecker::doC3Authen(const CustomerData& data, QString& icidOut, QString& listFileOut,
    QString& divKeyC3Out, const LogFn& log)
{
    icidOut.clear();
    listFileOut.clear();
    divKeyC3Out.clear();

    bool ok = false;

    // 1) Select ISAM Object（FISC AID）
    QString resp = transmit("00A4040008A00000017295000100", "Select ISAM Object", log, ok);
    QString sw = resp.right(4);
    if (!ok || sw != "9000") {
        logErr(log, QString("Error: [Select ISAM Object] 失敗 (SW=%1)").arg(sw));
        return sw.isEmpty() ? QString("ERROR") : sw;
    }

    // 2) List File（取得 List File 內容，卡片可能無回傳資料）
    resp = transmit("00AA0000000000", "List File", log, ok);
    if (ok) {
        sw = resp.right(4);
        QString dataPart = resp.left(qMax(0, resp.length() - 4));
        listFileOut = dataPart;   // 卡片若無資料，dataPart 會是空字串（對應 C# listfile = string.Empty）
    }

    // 3) Read Binary（取得 ICID，offset 04h，Le=08h）
    resp = transmit("00B0000408", "Read ICID", log, ok);
    sw = resp.right(4);
    if (!ok || sw != "9000") {
        logErr(log, QString("Error: [Read ICID] 失敗 (SW=%1)").arg(sw));
        return sw.isEmpty() ? QString("ERROR") : sw;
    }
    icidOut = resp.left(qMax(0, resp.length() - 4));
    if (icidOut.isEmpty()) {
        logErr(log, "Error: 卡片未回傳 ICID");
        return sw;
    }

    // 4) Get Challenge（取得亂數 ICCRan）
    resp = transmit("0086000008", "Get Challenge", log, ok);
    QString dataPart = resp.left(qMax(0, resp.length() - 4));
    sw = resp.right(4);
    if (!ok || dataPart.isEmpty()) {
        // 對應 C#：resp.Count==1（無資料） → ICID 清空，直接回傳狀態碼
        icidOut.clear();
        logErr(log, QString("Error: [Get Challenge] 卡片無回應資料 (SW=%1)").arg(sw));
        return sw.isEmpty() ? QString("ERROR") : sw;
    }
    QString iccRan = dataPart;

    // 5) 用 C3 對卡片 ICID 做 Key Diversification
    divKeyC3Out = CardUtils::divKey(data.c3, icidOut);
    if (divKeyC3Out.isEmpty()) {
        logErr(log, "Error: C3 金鑰衍生失敗（請確認 C3 金鑰格式）");
        return QString("KEY_ERROR");
    }

    // 6) 用衍生金鑰對 ICCRan 做 3DES 運算，取得 Rn
    QString rn = CardUtils::tripleDesHex(divKeyC3Out, iccRan);
    if (rn.isEmpty()) {
        logErr(log, "Error: Rn 運算失敗");
        return QString("CALC_ERROR");
    }

    // 7) External Authenticate（外部認證，回應僅有 SW，無資料）
    QString cmdExtAuth = QString("0082001808%1").arg(rn);
    resp = transmit(cmdExtAuth, "External Authenticate", log, ok);
    if (!ok) return QString("ERROR");

    return resp.right(4);   // 回傳狀態碼，即 C# 版的 terminaloutput
}

// ─────────────────────────────────────────────────────────────────────────────
// 對應 UpdateC3wSession()
// ─────────────────────────────────────────────────────────────────────────────
bool IsamKeyChecker::updateC3WithSession(const CustomerData& data, const QString& icid,
    const QString& divKeyC3, const LogFn& log)
{
    bool ok = false;

    // 1) 產生 8 bytes 亂數 R1，並取得 Session Key 建立回應
    QString r1 = CardUtils::generateRandomHex(8);
    // 注意：不可寫成 QString("0084008008%1" "08").arg(r1)
    //      因為 %1 後面緊接著數字 "08" 會被 Qt 誤判成佔位符 %10，導致組出來的
    //      HEX 字串少一碼、長度變奇數，APDU 直接送壞（曾實際觸發 SW=6D00）。
    //      這裡改用純字串串接，避開 arg() 的佔位符解析歧義。
    QString cmdGetSession = QString("0084008008") + r1 + "08";
    QString resp = transmit(cmdGetSession, "Get Session Key", log, ok);
    QString sw = resp.right(4);
    if (!ok || sw != "9000") {
        logErr(log, QString("Error: [Get Session Key] 失敗 (SW=%1)").arg(sw));
        return false;
    }
    QString icSessionRes = resp.left(qMax(0, resp.length() - 4));
    if (icSessionRes.isEmpty()) {
        logErr(log, "Error: 卡片未回傳 Session 回應資料");
        return false;
    }

    // 2) 用 C1 對 ICID 做 Key Diversification，並計算 Session Key
    QString divKeyC1 = CardUtils::divKey(data.c1, icid);
    if (divKeyC1.isEmpty()) {
        logErr(log, "Error: C1 金鑰衍生失敗（請確認 C1 金鑰格式）");
        return false;
    }
    QString sessionKey = CardUtils::getSessionKey(divKeyC1, r1, icSessionRes);
    if (sessionKey.isEmpty()) {
        logErr(log, "Error: Session Key 運算失敗");
        return false;
    }

    // 3) Select「C3」金鑰物件（比照原 C# 版行為，不特別檢查 SW，僅確認通訊層成功）
    transmit("00A402000200C3", "Select C3 Object", log, ok);
    if (!ok) return false;

    // 4) 用 Session Key 解密先前的 C3 衍生金鑰，取得 WSK
    QString wsk = CardUtils::desHex(sessionKey, divKeyC3, /*encrypt=*/false);
    if (wsk.isEmpty()) {
        logErr(log, "Error: WSK 運算失敗");
        return false;
    }

    // 5) 將 WSK 寫回卡片（Update Binary，Lc 依 WSK 長度自動換算）
    QString lcHex = QString("%1").arg(wsk.length() / 2, 2, 16, QChar('0')).toUpper();
    QString cmdUpdate = QString("00E40104%1%2").arg(lcHex, wsk);
    resp = transmit(cmdUpdate, "Update C3 with Session Key", log, ok);
    sw = resp.right(4);
    if (!ok || sw != "9000") {
        logErr(log, QString("Error: [Update C3 with Session Key] 失敗 (SW=%1)").arg(sw));
        return false;
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 對應 CheckKey()
// ─────────────────────────────────────────────────────────────────────────────
KeyCheckResult IsamKeyChecker::execute(const CustomerData& data, const QString& expectedListFile, const LogFn& log)
{
    KeyCheckResult result;

    if (m_handle == 0) {
        logErr(log, "Error: 無效的卡片控制代碼");
        return result;
    }

    logInfo(log, "====== [檢查卡片 KEY] 啟動 ======");

    QString icid, listFile, divKeyC3;
    QString status = doC3Authen(data, icid, listFile, divKeyC3, log);

    if (status != kPasscode) {
        if (!icid.isEmpty()) {
            logErr(log, QString("FISC [KO]：ICID=%1  Error=%2").arg(icid, status));
        }
        else {
            logErr(log, QString("Error：%1").arg(status));
        }
        result.icid = icid;
        result.lastStatus = status;
        result.success = false;
        return result;
    }

    logInfo(log, "✔ 第一次外部認證通過，開始以 C1 建立 Session Key 並覆寫 C3...");

    if (!updateC3WithSession(data, icid, divKeyC3, log)) {
        result.icid = icid;
        result.lastStatus = "UPDATE_FAILED";
        result.success = false;
        return result;
    }

    // 覆寫完成後，再做一次外部認證以驗證最終結果
    QString icid2, listFile2, divKeyC3_2;
    status = doC3Authen(data, icid2, listFile2, divKeyC3_2, log);

    result.icid = icid2.isEmpty() ? icid : icid2;
    result.lastStatus = status;
    result.listFile = listFile2;

    if (status == kPasscode) {
        if (!expectedListFile.isEmpty()) {
            if (listFile2 == expectedListFile) {
                logInfo(log, QString("FISC [OK]：ICID=%1").arg(result.icid));
                result.success = true;
            }
            else {
                logErr(log, QString("FISC [KO]：ICID=%1，List File 不相符").arg(result.icid));
                result.listFileMismatch = true;
                result.success = false;
            }
        }
        else {
            logInfo(log, QString("FISC [OK]：ICID=%1").arg(result.icid));
            result.success = true;
        }
    }
    else {
        logErr(log, QString("FISC [KO]：ICID=%1  Error=%2").arg(result.icid, status));
        result.success = false;
    }

    logInfo(log, result.success ? "🎉 ====== [檢查卡片 KEY] 通過 ======"
        : "❌ ====== [檢查卡片 KEY] 未通過 ======");

    return result;
}
