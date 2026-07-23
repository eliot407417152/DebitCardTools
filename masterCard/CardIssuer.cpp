#include "CardIssuer.h"

CardIssuer::CardIssuer(SCARDHANDLE cardHandle, SCARDHANDLE isamHandle)
    : m_cardHandle(cardHandle), m_isamHandle(isamHandle)
{
}

// ─────────────────────────────────────────────────────────────────────────────
// 輔助：send（不需要回傳值 / 需要回傳值）— 與 CardWasher 相同做法
// ─────────────────────────────────────────────────────────────────────────────
bool CardIssuer::sendCheck(SCARDHANDLE hCard, const QString& apdu,
    const QString& stepDesc, const LogFn& log)
{
    QString dummy;
    return sendCheck(hCard, apdu, stepDesc, log, dummy);
}

bool CardIssuer::sendCheck(SCARDHANDLE hCard, const QString& apdu,
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
bool CardIssuer::execute(const QString& manualC2Data, const LogFn& log)
{
    if (m_cardHandle == 0 || m_isamHandle == 0) {
        logErr(log, "Error: 卡片控制代碼無效（卡片或 ISAM 為 0）");
        return false;
    }
    if (manualC2Data.trimmed().isEmpty()) {
        logErr(log, "Error: 00C2 Update Record Ciphered 資料未輸入（自行輸入欄位為空）");
        return false;
    }

    logInfo(log, "====== 💳 [發卡] 啟動 ======");

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

    // 4. Generate Terminal Authentication Code（ISAM）(BC3)：82 08 21 00 Lc(自動) DataIn=1+2 Le=08
    QString bc3Data0 = readBinData + randCardData;
    QString bc3Lc = QString("%1").arg(bc3Data0.length() / 2, 2, 16, QChar('0')).toUpper();
    QString bc3Apdu = "82082100" + bc3Lc + bc3Data0 + "08";
    QString bc3Resp;
    if (!sendCheck(m_isamHandle, bc3Apdu, "4. Generate Terminal Authentication Code", log, bc3Resp)) return false;
    QString bc3Data = bc3Resp.left(qMax(0, bc3Resp.length() - 4));
    logInfo(log, "✔ 4. Generate Terminal Authentication Code 完成（BC3）：" + bc3Data);

    // 5. Terminal Authentication（卡片）：00 82 00 18 Lc(自動) DataIn=BC3
    QString taLc = QString("%1").arg(bc3Data.length() / 2, 2, 16, QChar('0')).toUpper();
    QString taApdu = "00820018" + taLc + bc3Data;
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

    // 8. Set Session Key（ISAM）(SK)：82 0A 22 00 Lc(自動) DataIn=1.Read Binary回應+7.Set Session Key(卡片)回應
    QString sskIsamData = readBinData + sskCardData;
    QString sskIsamLc = QString("%1").arg(sskIsamData.length() / 2, 2, 16, QChar('0')).toUpper();
    QString sskIsamApdu = "820A2200" + sskIsamLc + sskIsamData;
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

    // ── 建立發卡應用檔案：00C4 / 00C6 / 00D6 / 00D7 / 00C8（一般情形）─────────
    logInfo(log, "── 建立應用檔案 00C4 ──");
    if (!createAppFile("00C4", "00C4050005000400001C901802040200", "24", readBinData, log)) return false;

    logInfo(log, "── 建立應用檔案 00C6 ──");
    if (!createAppFile("00C6", "00C6800005000400001CB01800120400", "26", readBinData, log)) return false;

    logInfo(log, "── 建立應用檔案 00D6 ──");
    if (!createAppFile("00D6", "00D6800005000400001CB01800130400", "27", readBinData, log)) return false;

    logInfo(log, "── 建立應用檔案 00D7 ──");
    if (!createAppFile("00D7", "00D7800005000400001CB01800140400", "28", readBinData, log)) return false;

    logInfo(log, "── 建立應用檔案 00C8 ──");
    if (!createAppFile("00C8", "00C8080005000400001CB01800150400", "29", readBinData, log)) return false;

    // ── 建立發卡應用檔案：00C5（特例，Get Working Key With SK）───────────────
    logInfo(log, "── 建立應用檔案 00C5（特例）──");
    if (!createAppFile00C5("25", log)) return false;

    // ── 建立發卡應用檔案：00C2（特例，無 KID，Update 資料自行輸入）───────────
    logInfo(log, "── 建立應用檔案 00C2（特例）──");
    if (!createAppFile00C2(manualC2Data, log)) return false;

    // ── 建立個人化資料檔案（僅 Create File）───────────────────────────────────
    logInfo(log, "── 建立個人化資料檔案 ──");
    struct DataFile { const char* no; const char* data; };
    static const DataFile dataFiles[] = {
        { "0001", "000100000D0005000034000000000000" },
        { "0002", "000200008A000500008A000000000000" },
        { "1001", "100100000D00050000CE000000000000" },
        { "1002", "100200000D00050000E0000000000000" },
        { "1003", "100300000D0005000042000000000000" },
        { "1080", "10800000820080000484D06E00120000" },
        { "1090", "10900000000080000484D06E00150000" },
        { "1102", "11020000820080000048D01800130000" },
        { "1103", "11030000820080000048D01800140000" },
        { "007F", "007F0000000005000006000000000000" },
        { "2F00", "2F000000050005000014000000000000" },
    };
    for (const auto& df : dataFiles) {
        if (!createDataFile(df.no, df.data, log)) return false;
    }

    // ── 寫入個人化資料內容（0001 / 0002 / 1001 / 1002）─────────────────────────
    logInfo(log, "── 寫入個人化資料（0001）──");
    if (!writeDataRecords("0001", QStringList{
        "011311223344112233445566778899000120300101",
        "020E4131323334353637383919910101",
        "030700000100000101",
        }, log)) return false;

    logInfo(log, "── 寫入個人化資料（0002）──");
    if (!writeDataRecords("0002", QStringList{
        "010420260101",
        "02140011223344556677889900112233445566778899",
        "0336001122334455667788990011223344556677889900112233445566778899001122334455667788990011223344556677889911223344",
        }, log)) return false;

    logInfo(log, "── 寫入個人化資料（1001）──");
    if (!writeDataRecords("1001", QStringList{
        "01081122334455667788",
        "021E001122334455667788990011223344556677889900112233445566778899",
        "031000112233445566778899112233445566",
        "041000112233445566778899112233445566",
        "051000112233445566778899112233445566",
        "061000112233445566778899112233445566",
        "071000112233445566778899112233445566",
        "081000112233445566778899112233445566",
        "091000112233445566778899112233445566",
        "0A1000112233445566778899112233445566",
        }, log)) return false;

    logInfo(log, "── 寫入個人化資料（1002）──");
    if (!writeDataRecords("1002", QStringList{
        "0118001122334455667788990011223344556677889911223344",
        "0218001122334455667788990011223344556677889911223344",
        "0318001122334455667788990011223344556677889911223344",
        "0418001122334455667788990011223344556677889911223344",
        "0518001122334455667788990011223344556677889911223344",
        "0618001122334455667788990011223344556677889911223344",
        "0718001122334455667788990011223344556677889911223344",
        "0818001122334455667788990011223344556677889911223344",
        }, log)) return false;

    logInfo(log, "🎉 ====== ✔ 發卡完成 ======");
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 單一金鑰檔案處理：Get Div Key With SK（ISAM）→ Select（卡片）→ Update Record Ciphered（卡片）
//   （00C3 / 00C1 / 00C0 專用，邏輯與 CardWasher::processKeyFile 相同）
// ─────────────────────────────────────────────────────────────────────────────
bool CardIssuer::processKeyFile(const QString& isamP1,
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

// ─────────────────────────────────────────────────────────────────────────────
// 建立發卡應用檔案（一般情形）：
//   Create File → Get Div Key With SK（ISAM）→ Select File → Update Record Ciphered
// ─────────────────────────────────────────────────────────────────────────────
bool CardIssuer::createAppFile(const QString& efid,
    const QString& createDataIn,
    const QString& kid,
    const QString& readBinData,
    const LogFn& log)
{
    // Create File（卡片）：00 E0 00 00 Lc(自動) DataIn=createDataIn
    QString cfLc = QString("%1").arg(createDataIn.length() / 2, 2, 16, QChar('0')).toUpper();
    if (!sendCheck(m_cardHandle, "00E00000" + cfLc + createDataIn,
        QString("Create File %1").arg(efid), log))
        return false;
    logInfo(log, QString("✔ Create File %1 完成。").arg(efid));

    // Get Div Key With SK（ISAM）：82 0B KID 00 Lc(自動) DataIn=Read Binary回應 Le=00
    QString dkLc = QString("%1").arg(readBinData.length() / 2, 2, 16, QChar('0')).toUpper();
    QString dkApdu = "820B" + kid + "00" + dkLc + readBinData + "00";
    QString dkResp;
    if (!sendCheck(m_isamHandle, dkApdu, QString("Get Div Key With SK（%1）").arg(efid), log, dkResp))
        return false;
    QString dkData = dkResp.left(qMax(0, dkResp.length() - 4));
    logInfo(log, QString("✔ Get Div Key With SK（%1）完成：%2").arg(efid, dkData));

    // Select File（卡片）：00 A4 02 00 Lc=02 DataIn=EFID
    if (!sendCheck(m_cardHandle, "00A4020002" + efid, QString("Select File %1").arg(efid), log))
        return false;
    logInfo(log, QString("✔ Select File %1 完成。").arg(efid));

    // Update Record Ciphered（卡片）：00 E4 01 04 Lc(自動) DataIn=Get Div Key With SK 回應
    QString urcLc = QString("%1").arg(dkData.length() / 2, 2, 16, QChar('0')).toUpper();
    if (!sendCheck(m_cardHandle, "00E40104" + urcLc + dkData,
        QString("Update Record Ciphered（%1）").arg(efid), log))
        return false;
    logInfo(log, QString("✔ Update Record Ciphered（%1）完成。").arg(efid));

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 建立發卡應用檔案 — 00C5 特例：
//   Create File → Get Working Key With SK（ISAM，無 Lc / DataIn）→
//   Select File → Update Record Ciphered
// ─────────────────────────────────────────────────────────────────────────────
bool CardIssuer::createAppFile00C5(const QString& kid, const LogFn& log)
{
    const QString efid = "00C5";
    const QString createDataIn = "00C5800005000400001CB01800110100";

    // Create File（卡片）
    QString cfLc = QString("%1").arg(createDataIn.length() / 2, 2, 16, QChar('0')).toUpper();
    if (!sendCheck(m_cardHandle, "00E00000" + cfLc + createDataIn,
        QString("Create File %1").arg(efid), log))
        return false;
    logInfo(log, QString("✔ Create File %1 完成。").arg(efid));

    // Get Working Key With SK（ISAM）：82 0C KID 00，無 Lc、無 DataIn，Le=00
    QString wkApdu = "820C" + kid + "0000";
    QString wkResp;
    if (!sendCheck(m_isamHandle, wkApdu, "Get Working Key With SK（00C5）", log, wkResp))
        return false;
    QString wkData = wkResp.left(qMax(0, wkResp.length() - 4));
    logInfo(log, "✔ Get Working Key With SK（00C5）完成：" + wkData);

    // Select File（卡片）
    if (!sendCheck(m_cardHandle, "00A4020002" + efid, QString("Select File %1").arg(efid), log))
        return false;
    logInfo(log, QString("✔ Select File %1 完成。").arg(efid));

    // Update Record Ciphered（卡片）：DataIn = Get Working Key With SK 回應
    QString urcLc = QString("%1").arg(wkData.length() / 2, 2, 16, QChar('0')).toUpper();
    if (!sendCheck(m_cardHandle, "00E40104" + urcLc + wkData,
        QString("Update Record Ciphered（%1）").arg(efid), log))
        return false;
    logInfo(log, QString("✔ Update Record Ciphered（%1）完成。").arg(efid));

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 建立發卡應用檔案 — 00C2 特例：
//   Create File → Select File → Update Record Ciphered（資料自行輸入）
// ─────────────────────────────────────────────────────────────────────────────
bool CardIssuer::createAppFile00C2(const QString& manualData, const LogFn& log)
{
    const QString efid = "00C2";
    const QString createDataIn = "00C2050007000400000C900803020100";

    // Create File（卡片）
    QString cfLc = QString("%1").arg(createDataIn.length() / 2, 2, 16, QChar('0')).toUpper();
    if (!sendCheck(m_cardHandle, "00E00000" + cfLc + createDataIn,
        QString("Create File %1").arg(efid), log))
        return false;
    logInfo(log, QString("✔ Create File %1 完成。").arg(efid));

    // Select File（卡片）
    if (!sendCheck(m_cardHandle, "00A4020002" + efid, QString("Select File %1").arg(efid), log))
        return false;
    logInfo(log, QString("✔ Select File %1 完成。").arg(efid));

    // Update Record Ciphered（卡片）：DataIn 自行輸入
    QString urcLc = QString("%1").arg(manualData.length() / 2, 2, 16, QChar('0')).toUpper();
    if (!sendCheck(m_cardHandle, "00E40104" + urcLc + manualData,
        QString("Update Record Ciphered（%1）").arg(efid), log))
        return false;
    logInfo(log, QString("✔ Update Record Ciphered（%1）完成。").arg(efid));

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 建立個人化資料檔案：僅 Create File，無 Select / Update
//   createDataIn 為完整 16 bytes DataIn（已含檔案編號 2 bytes 前綴 + 14 bytes
//   參數），直接依其實際長度計算 Lc 後送出，不再另行拼接檔案編號。
// ─────────────────────────────────────────────────────────────────────────────
bool CardIssuer::createDataFile(const QString& fileNo, const QString& createDataIn, const LogFn& log)
{
    QString cfLc = QString("%1").arg(createDataIn.length() / 2, 2, 16, QChar('0')).toUpper();
    if (!sendCheck(m_cardHandle, "00E00000" + cfLc + createDataIn,
        QString("Create File %1").arg(fileNo), log))
        return false;
    logInfo(log, QString("✔ Create File %1 完成。").arg(fileNo));
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 寫入個人化資料內容（0001 / 0002 / 1001 / 1002 專用）：
//   Select File（卡片）→ 依序對每個 block 執行 Write Record（卡片）
//   Write Record：00 D2 00 05 Lc(自動) DataIn=block（[Block ID][Length][資料]）
// ─────────────────────────────────────────────────────────────────────────────
bool CardIssuer::writeDataRecords(const QString& fileNo, const QStringList& blocks, const LogFn& log)
{
    // Select File（卡片）：00 A4 02 00 Lc=02 DataIn=fileNo
    if (!sendCheck(m_cardHandle, "00A4020002" + fileNo, QString("Select File %1").arg(fileNo), log))
        return false;
    logInfo(log, QString("✔ Select File %1 完成。").arg(fileNo));

    for (int i = 0; i < blocks.size(); ++i) {
        const QString& blockData = blocks.at(i);
        QString lc = QString("%1").arg(blockData.length() / 2, 2, 16, QChar('0')).toUpper();
        QString apdu = "00D20005" + lc + blockData;
        if (!sendCheck(m_cardHandle, apdu,
            QString("Write Record %1 block %2").arg(fileNo).arg(i + 1), log))
            return false;
        logInfo(log, QString("✔ %1 block %2 寫入完成：%3").arg(fileNo).arg(i + 1).arg(blockData));
    }

    return true;
}
