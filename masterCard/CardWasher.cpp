#include "CardWasher.h"

CardWasher::CardWasher(SCARDHANDLE cardHandle, SCARDHANDLE isamHandle)
    : m_cardHandle(cardHandle), m_isamHandle(isamHandle)
{
}

// ─────────────────────────────────────────────────────────────────────────────
// 輔助：send（不需要回傳值）
// ─────────────────────────────────────────────────────────────────────────────
bool CardWasher::sendCheck(SCARDHANDLE hCard, const QString& apdu,
    const QString& stepDesc, const LogFn& log)
{
    QString dummy;
    return sendCheck(hCard, apdu, stepDesc, log, dummy);
}

// 輔助：send（需要回傳值）
bool CardWasher::sendCheck(SCARDHANDLE hCard, const QString& apdu,
    const QString& stepDesc, const LogFn& log, QString& respOut)
{
    bool ok = false;
    respOut = CardUtils::sendApdu(hCard, apdu, ok, log);
    if (!ok || respOut.right(4) != "9000") {
        logErr(log, QString("Error: [%1] 失敗 (SW=%2)").arg(stepDesc, respOut.right(4)));
        return false;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 主流程
// ─────────────────────────────────────────────────────────────────────────────
bool CardWasher::execute(const LogFn& log)
{
    if (m_cardHandle == 0 || m_isamHandle == 0) {
        logErr(log, "Error: 卡片控制代碼無效（卡片或 ISAM 為 0）");
        return false;
    }

    logInfo(log, "====== 🧼 [洗卡] 啟動 ======");

    // 1. Read Binary（卡片）：00 B0 00 04 Le=08
    QString readBinResp;
    if (!sendCheck(m_cardHandle, "00B0000408", "1. Read Binary", log, readBinResp)) return false;
    QString readBinData = readBinResp.left(qMax(0, readBinResp.length() - 4));
    logInfo(log, "✔ 1. Read Binary 完成：" + readBinData);

    // 2. Generate Random Number（卡片）：00 86 00 00 Le=08
    QString randCardResp;
    if (!sendCheck(m_cardHandle, "0086000008", "2. Generate Random Number（卡片）", log, randCardResp)) return false;
    QString randCardData = randCardResp.left(qMax(0, randCardResp.length() - 4));
    logInfo(log, "✔ 2. Generate Random Number（卡片）完成：" + randCardData);

    // 3. Select ISAM Object：82 A4 00 00 Lc=02 DataIn=9500 Le=0E
    if (!sendCheck(m_isamHandle, "82A400000295000E", "3. Select ISAM Object", log)) return false;
    logInfo(log, "✔ 3. Select ISAM Object 完成。");

    // 4. Generate Terminal Authentication Code（ISAM）(MC3)：82 08 11 00 Lc(自動) DataIn=1+2 Le=08
    QString mc3Data0 = readBinData + randCardData;
    QString mc3Lc = QString("%1").arg(mc3Data0.length() / 2, 2, 16, QChar('0')).toUpper();
    QString mc3Apdu = "82081100" + mc3Lc + mc3Data0 + "08";
    QString mc3Resp;
    if (!sendCheck(m_isamHandle, mc3Apdu, "4. Generate Terminal Authentication Code", log, mc3Resp)) return false;
    QString mc3Data = mc3Resp.left(qMax(0, mc3Resp.length() - 4));
    logInfo(log, "✔ 4. Generate Terminal Authentication Code 完成（MC3）：" + mc3Data);

    // 5. Terminal Authentication（卡片）：00 82 00 18 Lc(自動) DataIn=MC3
    QString taLc = QString("%1").arg(mc3Data.length() / 2, 2, 16, QChar('0')).toUpper();
    QString taApdu = "00820018" + taLc + mc3Data;
    if (!sendCheck(m_cardHandle, taApdu, "5. Terminal Authentication", log)) return false;
    logInfo(log, "✔ 5. Terminal Authentication 成功。");

    // 6. Generate Random Number（ISAM）：8F 06 00 00 Le=08
    QString randIsamResp;
    if (!sendCheck(m_isamHandle, "8F86000008", "6. Generate Random Number（ISAM）", log, randIsamResp)) return false;
    QString randIsamData = randIsamResp.left(qMax(0, randIsamResp.length() - 4));
    logInfo(log, "✔ 6. Generate Random Number（ISAM）完成：" + randIsamData);

    // 7. Set Session Key（卡片）(SK)：00 84 00 80 Lc(自動) DataIn=6 Le=08
    QString sskCardLc = QString("%1").arg(randIsamData.length() / 2, 2, 16, QChar('0')).toUpper();
    QString sskCardApdu = "00840080" + sskCardLc + randIsamData + "08";
    QString sskCardResp;
    if (!sendCheck(m_cardHandle, sskCardApdu, "7. Set Session Key（卡片）", log, sskCardResp)) return false;
    QString sskCardData = sskCardResp.left(qMax(0, sskCardResp.length() - 4));
    logInfo(log, "✔ 7. Set Session Key（卡片）成功：" + sskCardData);

    // 8. Set Session Key（ISAM）(SK)：82 0A 12 00 Lc(自動) DataIn=1.Read Binary回應+7.Set Session Key(卡片)回應
    QString sskIsamData = readBinData + sskCardData;
    QString sskIsamLc = QString("%1").arg(sskIsamData.length() / 2, 2, 16, QChar('0')).toUpper();
    QString sskIsamApdu = "820A1200" + sskIsamLc + sskIsamData;
    if (!sendCheck(m_isamHandle, sskIsamApdu, "8. Set Session Key（ISAM）", log)) return false;
    logInfo(log, "✔ 8. Set Session Key（ISAM）成功。");

    // 9~11. 00C3
    logInfo(log, "── 處理金鑰檔案 00C3 ──");
    if (!processKeyFile("21", "00C3", readBinData, false, QString(), log)) return false;

    // 12~14. 00C1
    logInfo(log, "── 處理金鑰檔案 00C1 ──");
    if (!processKeyFile("22", "00C1", readBinData, false, QString(), log)) return false;

    // 15~17. 00C0（若不存在需先 Create File）
    logInfo(log, "── 處理金鑰檔案 00C0 ──");
    if (!processKeyFile("23", "00C0", readBinData, true,
        "00C0040005000400001C901802010200", log)) return false;

    logInfo(log, "🎉 ====== ✔ 洗卡完成 ======");
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 單一金鑰檔案處理：Get Div Key With SK（ISAM）→ Select（卡片）→ Update Record Ciphered（卡片）
// ─────────────────────────────────────────────────────────────────────────────
bool CardWasher::processKeyFile(const QString& isamP1,
    const QString& efid,
    const QString& readBinData,
    bool allowCreate,
    const QString& createDataIn,
    const LogFn& log)
{
    // Get Div Key With SK（ISAM）：82 0B P1 00 Lc(自動) DataIn=Read Binary回應 Le=00
    QString dkLc = QString("%1").arg(readBinData.length() / 2, 2, 16, QChar('0')).toUpper();
    QString dkApdu = "820B" + isamP1 + "00" + dkLc + readBinData + "00";
    QString dkResp;
    if (!sendCheck(m_isamHandle, dkApdu, QString("Get Div Key With SK（%1）").arg(efid), log, dkResp))
        return false;
    QString dkData = dkResp.left(qMax(0, dkResp.length() - 4));
    logInfo(log, QString("✔ Get Div Key With SK（%1）完成：%2").arg(efid, dkData));

    // Select EFID（卡片）：00 A4 02 00 Lc=02 DataIn=EFID
    QString selApdu = "00A4020002" + efid;
    bool selOk = false;
    QString selResp;
    {
        bool ok = false;
        selResp = CardUtils::sendApdu(m_cardHandle, selApdu, ok, log);
        selOk = ok && selResp.right(4) == "9000";
    }

    if (!selOk) {
        QString sw = selResp.right(4);
        if (allowCreate && sw == "6602") {
            logInfo(log, QString("⚠️ Select %1 回應 SW=6602（檔案不存在），執行 Create File...").arg(efid));

            // Create File：00 E0 00 00 Lc=10 DataIn=createDataIn
            if (!sendCheck(m_cardHandle, "00E0000010" + createDataIn,
                QString("Create File %1").arg(efid), log))
                return false;
            logInfo(log, QString("✔ Create File %1 完成，重新執行 Select...").arg(efid));

            if (!sendCheck(m_cardHandle, selApdu, QString("Select %1（重試）").arg(efid), log))
                return false;
        }
        else {
            logErr(log, QString("Error: [Select %1] 失敗 (SW=%2)").arg(efid, sw));
            return false;
        }
    }
    logInfo(log, QString("✔ Select %1 完成。").arg(efid));

    // Update Record Ciphered（卡片）：00 E4 01 04 Lc(自動) DataIn=Get Div Key With SK 回應
    QString urcLc = QString("%1").arg(dkData.length() / 2, 2, 16, QChar('0')).toUpper();
    QString urcApdu = "00E40104" + urcLc + dkData;
    if (!sendCheck(m_cardHandle, urcApdu, QString("Update Record Ciphered（%1）").arg(efid), log))
        return false;
    logInfo(log, QString("✔ Update Record Ciphered（%1）完成。").arg(efid));

    return true;
}
