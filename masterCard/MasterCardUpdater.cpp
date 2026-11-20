#include "MasterCardUpdater.h"
#include <QRegularExpression>

MasterCardUpdater::MasterCardUpdater(SCARDHANDLE masterHandle, SCARDHANDLE isamHandle)
    : m_masterHandle(masterHandle), m_isamHandle(isamHandle)
{
}

// ─────────────────────────────────────────────────────────────────────────────
// 製作母卡（完整流程）
//   1. ISAM 寫入 Exchange Key（Select ISAM Object → Get Key Check Code → Write Key）
//   2. 母卡雙向認證
//   3. 寫入 C8 / C9 / 0001 / 0002
// ─────────────────────────────────────────────────────────────────────────────
bool MasterCardUpdater::makeMasterCard(const CustomerData& data, const LogFn& log)
{
    if (m_masterHandle == 0 || m_isamHandle == 0) {
        logErr(log, "Error: 卡片控制代碼無效（Master 或 ISAM 為 0）");
        return false;
    }

    logInfo(log, "====== 🔮 [Master] 啟動製作母卡流程 ======");

    if (!writeExchangeKey(data, log)) return false;
    if (!authenticate(log)) return false;
    if (!writeData(data, data.pinA, data.pinB, log)) return false;

    logInfo(log, "🎉 ====== ✔ 母卡製作完成 ======");
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// ISAM 寫入 Exchange Key
//   1. Select ISAM Object          82 A4 00 00 02 95 02 0E
//   2. Get Key Check Code          82 07 00 00 07 → 取前 4 碼對照 CD Key（例：656B）
//   3. Write Key                   82 03 10 02 1E [EKey + CCode + Limit]
//        EKey  = CD Key 3DES 加密 Exchange Code（24 bytes）
//        CCode = Exchange Code 的檢查碼（2 bytes）
//        Limit = FFFFFFFF（4 bytes）
// ─────────────────────────────────────────────────────────────────────────────
bool MasterCardUpdater::writeExchangeKey(const CustomerData& data, const LogFn& log)
{
    logInfo(log, "── [ISAM] 寫入 Exchange Key ──");

    // 先檢查資料，避免組出長度不對的 Write Key
    const QRegularExpression hexRe("^[0-9A-F]+$");
    const QString exchangeKey = data.exchangeCode.trimmed().toUpper();
    const QString ccode = data.ccode.trimmed().toUpper();
    if (exchangeKey.length() != 48 || !hexRe.match(exchangeKey).hasMatch()) {
        logErr(log, "Error: Exchange Code 必須是 48 個 HEX 字元（K1+K2+K3，各 16 碼）");
        return false;
    }
    if (ccode.length() != 4 || !hexRe.match(ccode).hasMatch()) {
        logErr(log, "Error: CCode（Exchange Code 檢查碼）必須是 4 個 HEX 字元");
        return false;
    }

    // 1. Select ISAM Object
    if (!sendCheck(m_isamHandle, "82A400000295020E", "Select ISAM Object", log)) return false;

    // 2. Get Key Check Code → 取得此 ISAM 的 CD Key
    QString kccResp;
    if (!sendCheck(m_isamHandle, "8207000007", "Get Key Check Code", log, kccResp)) return false;

    const QString cdKey = CardUtils::lookupIsamCdKey(kccResp);
    if (cdKey.isEmpty()) {
        logErr(log, QString("Error: 無法對照 CD Key 類型 (回應前4碼: %1)").arg(kccResp.left(4)));
        return false;
    }
    logInfo(log, QString("✔ CD Key 對照成功（檢索碼 %1）").arg(kccResp.left(4).toUpper()));

    // 3. EKey = CD Key 3DES 加密 Exchange Code
    const QString eKey = CardUtils::encrypt3DES(cdKey, exchangeKey);
    if (eKey.length() != 48) {
        logErr(log, "Error: Exchange Key 加密失敗（請確認 CD Key / Exchange Code 格式）");
        return false;
    }
    logInfo(log, "✔ EKey 計算完成：" + eKey);

    // 4. Write Key：CLA:82 INS:03 P1:10 P2:02 Lc:自動計算（EKey+CCode+Limit = 30 bytes = 1E）
    const QString wkData = eKey + ccode + "FFFFFFFF";
    const QString wkLc = QString("%1").arg(wkData.length() / 2, 2, 16, QChar('0')).toUpper();
    const QString wkApdu = "8203" "10" "02" + wkLc + wkData;
    if (!sendCheck(m_isamHandle, wkApdu, "Write Key (Exchange Key)", log)) return false;

    logInfo(log, "✔ Exchange Key 寫入 ISAM 成功。");
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 雙向認證
// ─────────────────────────────────────────────────────────────────────────────
bool MasterCardUpdater::authenticate(const LogFn& log)
{
    if (m_masterHandle == 0 || m_isamHandle == 0) {
        logErr(log, "Error: 卡片控制代碼無效（Master 或 ISAM 為 0）");
        return false;
    }

    logInfo(log, "====== 🔐 [Master] 啟動 C3 雙向認證 ======");

    QString resp;

    // 1. 從母卡讀取 Div-Data (8 bytes)
    if (!sendCheck(m_masterHandle, "00B0000408", "Read Binary (Div-Data)", log, resp)) return false;
    QString divData = resp.left(resp.length() - 4);
    logInfo(log, "✔ Div-Data: " + divData);

    // 2. 從母卡取得隨機數 Rn (8 bytes)
    if (!sendCheck(m_masterHandle, "0086000008", "Generate Random Number", log, resp)) return false;
    QString rnData = resp.left(16);
    logInfo(log, "✔ Rn: " + rnData);

    // 3. 選擇 ISAM 安全環境
    if (!sendCheck(m_isamHandle, "82A400000295020E", "ISAM Select Object", log)) return false;

    // 4. 請 ISAM 計算終端認證碼
    //    CLA:82 INS:08 P1:05 P2:00 Lc:10(16bytes) DataIn:[DivData+Rn] Le:08
    QString dataIn16 = divData + rnData;  // 16 bytes = 32 HEX chars
    QString cmdGetAuth = "82080500" + QString("%1").arg(dataIn16.length() / 2, 2, 16, QChar('0')).toUpper()
        + dataIn16 + "08";
    if (!sendCheck(m_isamHandle, cmdGetAuth, "ISAM Get TerminalAuthCode", log, resp)) return false;
    QString authCode = resp.left(16);
    logInfo(log, "✔ AuthCode: " + authCode);

    // 5. 將認證碼送回母卡驗證
    if (!sendCheck(m_masterHandle, "0082001808" + authCode, "Master External Auth", log)) return false;

    logInfo(log, "🎉 ====== ✔ 母卡認證成功，權限已解鎖！ ======");
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 寫入全部 EF
// ─────────────────────────────────────────────────────────────────────────────
bool MasterCardUpdater::writeData(const CustomerData& data,
    const QString& pinA,
    const QString& pinB,
    const LogFn& log)
{
    logInfo(log, "====== 📝 [Master] 啟動母卡寫入流程 ======");

    if (m_masterHandle == 0 || m_isamHandle == 0) {
        logErr(log, "Error: 卡片控制代碼無效");
        return false;
    }

    // PIN 資料（各 8 bytes，不足補 FF）
    QByteArray pinABytes = CardUtils::hexToBytes(pinA);
    QByteArray pinBBytes = CardUtils::hexToBytes(pinB);
    pinABytes = pinABytes.leftJustified(8, static_cast<char>(0xFF)).left(8);
    pinBBytes = pinBBytes.leftJustified(8, static_cast<char>(0xFF)).left(8);

    // ── C8：PIN A ─────────────────────────────────────────────────────────
    logInfo(log, "── [C8] 寫入 PIN A ──");
    if (!writeAEF("C8", "00C8",
        "8200", "0500", "8400",  // AC1 AC2 AC3
        0x01,                    // keyIndex
        pinABytes, log)) return false;

    // ── C9：PIN B ─────────────────────────────────────────────────────────
    logInfo(log, "── [C9] 寫入 PIN B ──");
    if (!writeAEF("C9", "00C9",
        "8100", "0600", "8400",  // AC1 AC2 AC3
        0x02,                    // keyIndex
        pinBBytes, log)) return false;

    // ── 0001：C3 ─────────────────────────────────────────────────────────
    logInfo(log, "── [0001] 寫入 C3 Encrypted Key + Check Code ──");
    if (!writeDEF("0001", "0001", 0x05, log)) return false;

    // ── 0002：C1 ─────────────────────────────────────────────────────────
    logInfo(log, "── [0002] 寫入 C1 Encrypted Key + Check Code ──");
    if (!writeDEF("0002", "0002", 0x06, log)) return false;

    logInfo(log, "🎉 ====== ✔ 母卡所有 EF 寫入完畢 ======");
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// AEF 寫入流程（C8 / C9）
//   Create File → Select File → Write Record [01][08][PIN 8bytes]
// ─────────────────────────────────────────────────────────────────────────────
bool MasterCardUpdater::writeAEF(const QString& label,
    const QString& efid,
    const QString& ac1,
    const QString& ac2,
    const QString& ac3,
    quint8         keyIndex,
    const QByteArray& pinData,
    const LogFn& log)
{
    // Create File DataIn (16 bytes):
    // EFID AC1 AC2 AC3 SoB(000C) ToF(90) SoR(08) RC(02) Index ToK(01) RFU(00)
    QString createDataIn = buildCreateDataIn(
        efid, ac1, ac2, ac3,
        "000C",             // SoB: 8 bytes data + 4 bytes header = 12 = 0x0C
        0x90,               // ToF: AEF
        0x08,               // SoR: record length = 8
        0x02,               // RC:  error tolerance
        keyIndex,           // Index
        0x01,               // ToK: PIN
        0x00                // RFU
    );
    QString createApdu = "00E0000010" + createDataIn;

    // 1. Create File
    bool ok = false;
    QString resp = CardUtils::sendApdu(m_masterHandle, createApdu, ok, log);
    if (!ok) { log("Error: [" + label + "] Create File 通訊失敗", true); return false; }
    QString sw = resp.right(4);
    if (sw == "9000")
        logInfo(log, "✔ [" + label + "] Create File 成功");
    else if (sw == "6602")
        logInfo(log, "⚠️ [" + label + "] 檔案已存在，繼續 Select...");
    else {
        logErr(log, "Error: [" + label + "] Create File SW=" + sw);
        return false;
    }

    // 2. Select File
    if (!sendCheck(m_masterHandle, "00A4020002" + efid.toUpper(),
        "[" + label + "] Select File", log)) return false;

    // 3. Write Record: [01][08][PIN 8bytes]
    //    Lc = 1(ID) + 1(Len) + 8(PIN) = 10 = 0x0A
    QByteArray writePayload;
    writePayload.append(static_cast<char>(0x01));           // Record ID
    writePayload.append(static_cast<char>(pinData.size())); // Data length
    writePayload.append(pinData);
    QString writeApdu = QString("00D20005%1").arg(
        QString("%1").arg(writePayload.size(), 2, 16, QChar('0')).toUpper())
        + CardUtils::bytesToHex(writePayload);
    if (!sendCheck(m_masterHandle, writeApdu, "[" + label + "] Write Record", log)) return false;

    logInfo(log, "✔ [" + label + "] 寫入完成");
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// DEF 寫入流程（0001 / 0002）
//   Create File → Select File
//   → Get Encrypted Key (ISAM)
//   → Write Record #1 [01][18][24 bytes EncKey]
//   → Get Key Check Code (ISAM) → 取前 2 bytes
//   → Write Record #2 [02][02][2 bytes CheckCode]
// ─────────────────────────────────────────────────────────────────────────────
bool MasterCardUpdater::writeDEF(const QString& label,
    const QString& efid,
    quint8         isamP2,
    const LogFn& log)
{
    // Create File DataIn (16 bytes):
    // EFID AC1(8300) AC2(8000) AC3(8400) SoB(0022) ToF(00) SoR(00) RC(00) Index(00) ToK(00) RFU(00)
    // SoB: 24bytes EncKey + 4 header + 2bytes CheckCode + 4 header = 34 = 0x22
    QString createDataIn = buildCreateDataIn(
        efid, "8300", "8000", "8400",
        "0022",     // SoB
        0x00,       // ToF: DEF
        0x00,       // SoR: not used
        0x00,       // RC:  not used
        0x00,       // Index: not used
        0x00,       // ToK: not used
        0x00        // RFU
    );
    QString createApdu = "00E0000010" + createDataIn;

    // 1. Create File
    bool ok = false;
    QString resp = CardUtils::sendApdu(m_masterHandle, createApdu, ok, log);
    if (!ok) { log("Error: [" + label + "] Create File 通訊失敗", true); return false; }
    QString sw = resp.right(4);
    if (sw == "9000")
        logInfo(log, "✔ [" + label + "] Create File 成功");
    else if (sw == "6602")
        logInfo(log, "⚠️ [" + label + "] 檔案已存在，繼續 Select...");
    else {
        logErr(log, "Error: [" + label + "] Create File SW=" + sw);
        return false;
    }

    // 2. Select File
    if (!sendCheck(m_masterHandle, "00A4020002" + efid.toUpper(),
        "[" + label + "] Select File", log)) return false;

    // 3. Get Encrypted Key（對 ISAM）
    //    CLA:82 INS:06 P1:10 P2:isamP2 Le:00
    QString p2Hex = QString("%1").arg(isamP2, 2, 16, QChar('0')).toUpper();
    QString getEncKeyApdu = "820610" + p2Hex + "00";
    QString encKeyResp;
    if (!sendCheck(m_isamHandle, getEncKeyApdu,
        "[" + label + "] Get Encrypted Key", log, encKeyResp)) return false;

    // 取回傳資料（去掉末尾 SW 4碼）= 24 bytes = 48 HEX 字元
    QString encKeyHex = encKeyResp.left(encKeyResp.length() - 4);
    if (encKeyHex.length() < 48) {
        logErr(log, "Error: [" + label + "] Get Encrypted Key 回傳長度不足 (" + encKeyHex + ")");
        return false;
    }
    encKeyHex = encKeyHex.left(48);  // 確保只取 24 bytes
    logInfo(log, "✔ [" + label + "] Encrypted Key: " + encKeyHex);

    // 4. Write Record #1: [01][18][24 bytes EncKey]
    //    Lc = 1(ID) + 1(Len) + 24(EncKey) = 26 = 0x1A
    QByteArray encKeyBytes = CardUtils::hexToBytes(encKeyHex);
    QByteArray wr1Payload;
    wr1Payload.append(static_cast<char>(0x01));              // Record ID = 1
    wr1Payload.append(static_cast<char>(encKeyBytes.size())); // Data length = 0x18
    wr1Payload.append(encKeyBytes);
    QString wr1Apdu = "00D20005"
        + QString("%1").arg(wr1Payload.size(), 2, 16, QChar('0')).toUpper()
        + CardUtils::bytesToHex(wr1Payload);
    if (!sendCheck(m_masterHandle, wr1Apdu,
        "[" + label + "] Write Record #1 (EncKey)", log)) return false;

    // 5. Get Key Check Code（對 ISAM）
    //    CLA:82 INS:07 P1:P2 P2:00 Le:06
    QString getCheckApdu = "8207" + p2Hex + "0006";
    QString checkResp;
    if (!sendCheck(m_isamHandle, getCheckApdu,
        "[" + label + "] Get Key Check Code", log, checkResp)) return false;

    // 取前 2 bytes（4 HEX 字元），規格說明回傳 XX XX FF FF FF FF
    QString checkHex = checkResp.left(4);
    logInfo(log, "✔ [" + label + "] Key Check Code (前2bytes): " + checkHex);

    // 6. Write Record #2: [02][02][2 bytes CheckCode]
    //    Lc = 1(ID) + 1(Len) + 2(CheckCode) = 4 = 0x04
    QByteArray checkBytes = CardUtils::hexToBytes(checkHex);
    QByteArray wr2Payload;
    wr2Payload.append(static_cast<char>(0x02));               // Record ID = 2
    wr2Payload.append(static_cast<char>(checkBytes.size()));  // Data length = 0x02
    wr2Payload.append(checkBytes);
    QString wr2Apdu = "00D20005"
        + QString("%1").arg(wr2Payload.size(), 2, 16, QChar('0')).toUpper()
        + CardUtils::bytesToHex(wr2Payload);
    if (!sendCheck(m_masterHandle, wr2Apdu,
        "[" + label + "] Write Record #2 (CheckCode)", log)) return false;

    logInfo(log, "✔ [" + label + "] 寫入完成");
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 母卡轉換 — 拿製作好的母卡，跟銀行卡的 ISAM 做母卡轉換
// ─────────────────────────────────────────────────────────────────────────────
bool MasterCardUpdater::convertToBankIsam(const CustomerData& data, const LogFn& log)
{
    if (m_masterHandle == 0 || m_isamHandle == 0) {
        logErr(log, "Error: 卡片控制代碼無效（Master 或 ISAM 為 0）");
        return false;
    }

    logInfo(log, "====== 🔄 [母卡轉換] 啟動 ======");

    // 1-a. Select ISAM Object（CLA:82 INS:A4 P1:00 P2:00 Lc:02 DataIn:9500 Le:0E）
    if (!sendCheck(m_isamHandle, "82A400000295000E", "Select ISAM Object", log)) return false;

    // 1-b. Get Key Check Code（ISAM）：取得 CD Key 對照用回應
    QString kccResp;
    if (!sendCheck(m_isamHandle, "8207000007", "Get Key Check Code", log, kccResp)) return false;

    QString cdKey = CardUtils::lookupIsamCdKey(kccResp);
    if (cdKey.isEmpty()) {
        logErr(log, QString("Error: 無法對照 CD Key 類型 (回應前4碼: %1)").arg(kccResp.left(4)));
        return false;
    }
    logInfo(log, "✔ CD Key 對照成功。");

    // 1-c. 用 CD Key 對 Exchange Key 做 3DES 加密，取得 EKey
    QString exchangeKey = data.exchangeCode.trimmed().toUpper();
    if (exchangeKey.isEmpty()) {
        logErr(log, "Error: 客戶資料缺少 Exchange Code");
        return false;
    }
    QString eKey = CardUtils::encrypt3DES(cdKey, exchangeKey);
    if (eKey.isEmpty()) {
        logErr(log, "Error: Exchange Key 加密失敗（請確認 CD Key / Exchange Code 格式）");
        return false;
    }
    logInfo(log, "✔ EKey 計算完成：" + eKey);

    // 1-d. Write Key（ISAM）：DataIn = EKey + CCode + Limit(FFFFFFFF)
    QString ccode = data.ccode.trimmed().toUpper();
    if (ccode.isEmpty()) {
        logErr(log, "Error: 客戶資料缺少 CCode");
        return false;
    }
    QString wkData = eKey + ccode + "FFFFFFFF";
    QString wkLc = QString("%1").arg(wkData.length() / 2, 2, 16, QChar('0')).toUpper();
    // P1(KID)=20、P2(KT)=08 為固定值
    QString wkApdu = "8203" "20" "08" + wkLc + wkData;
    if (!sendCheck(m_isamHandle, wkApdu, "Write Key", log)) return false;
    logInfo(log, "✔ Write Key 成功。");

    // 2. Verify PIN A（母卡，P2 = 01 左移 3 位 = 08）
    QByteArray pinABytes = CardUtils::hexToBytes(data.pinA);
    pinABytes = pinABytes.leftJustified(8, static_cast<char>(0xFF)).left(8);
    QString pinAApdu = "0020000808" + CardUtils::bytesToHex(pinABytes);
    if (!sendCheck(m_masterHandle, pinAApdu, "Verify PIN A", log)) return false;
    logInfo(log, "✔ PIN A 驗證成功。");

    // 3. Verify PIN B（母卡，P2 = 02 左移 3 位 = 10）
    QByteArray pinBBytes = CardUtils::hexToBytes(data.pinB);
    pinBBytes = pinBBytes.leftJustified(8, static_cast<char>(0xFF)).left(8);
    QString pinBApdu = "0020001008" + CardUtils::bytesToHex(pinBBytes);
    if (!sendCheck(m_masterHandle, pinBApdu, "Verify PIN B", log)) return false;
    logInfo(log, "✔ PIN B 驗證成功。");

    // 4~7. 0001（KT=03, KID=11）與 0002（KT=04, KID=12）
    if (!convertOneKeyBlock("0001", "03", "11", log)) return false;
    if (!convertOneKeyBlock("0002", "04", "12", log)) return false;

    logInfo(log, "🎉 ====== ✔ 母卡轉換完成 ======");
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 母卡轉換 — 單一金鑰區塊（0001 / 0002）
//   Select EFID(母卡) → Read Record r01 / r02(母卡) → Store Manufacturer Key(ISAM)
// ─────────────────────────────────────────────────────────────────────────────
bool MasterCardUpdater::convertOneKeyBlock(const QString& efid,
    const QString& kt,
    const QString& storeKeyKid,
    const LogFn& log)
{
    QString efidU = efid.toUpper();

    // Select EFID（母卡）
    if (!sendCheck(m_masterHandle, "00A4020002" + efidU,
        "[" + efidU + "] Select File", log)) return false;

    // Read Record r01（母卡）
    QString r1Resp;
    if (!sendCheck(m_masterHandle, "00B2010400",
        "[" + efidU + "] Read Record r01", log, r1Resp)) return false;
    QString r1Data = r1Resp.left(qMax(0, r1Resp.length() - 4));
    if (r1Data.length() <= 4) {
        logErr(log, "Error: [" + efidU + "] Read Record r01 資料長度不足 (" + r1Data + ")");
        return false;
    }
    // 前 2 碼(ID) + 2 碼(長度) 為 header，之後才是實際 EKey 內容
    QString eKeyExtracted = r1Data.mid(4);
    logInfo(log, "✔ [" + efidU + "] r01 取得 EKey：" + eKeyExtracted);

    // Read Record r02（母卡）
    QString r2Resp;
    if (!sendCheck(m_masterHandle, "00B2020400",
        "[" + efidU + "] Read Record r02", log, r2Resp)) return false;
    QString r2Data = r2Resp.left(qMax(0, r2Resp.length() - 4));
    if (r2Data.length() <= 4) {
        logErr(log, "Error: [" + efidU + "] Read Record r02 資料長度不足 (" + r2Data + ")");
        return false;
    }
    QString ccodeExtracted = r2Data.mid(4);
    logInfo(log, "✔ [" + efidU + "] r02 取得 CCode：" + ccodeExtracted);

    // Store Manufacturer Key（ISAM）：CLA:82 INS:09 P1:ExKID(20) P2:KID Data = KT+EKey+CCode+Limit
    QString smkData = kt.toUpper() + eKeyExtracted + ccodeExtracted + "FFFFFFFF";
    QString smkLc = QString("%1").arg(smkData.length() / 2, 2, 16, QChar('0')).toUpper();
    QString smkApdu = "820920" + storeKeyKid.toUpper().rightJustified(2, '0').right(2) + smkLc + smkData;
    if (!sendCheck(m_isamHandle, smkApdu,
        "[" + efidU + "] Store Manufacturer Key", log)) return false;

    logInfo(log, "✔ [" + efidU + "] Store Manufacturer Key 完成。");
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 組 Create File DataIn (固定 16 bytes = 32 HEX)
// ─────────────────────────────────────────────────────────────────────────────
QString MasterCardUpdater::buildCreateDataIn(const QString& efid,
    const QString& ac1,
    const QString& ac2,
    const QString& ac3,
    const QString& sob,
    quint8 tof,
    quint8 sor,
    quint8 rc,
    quint8 keyIndex,
    quint8 tok,
    quint8 rfu)
{
    auto byte2hex = [](quint8 b) {
        return QString("%1").arg(b, 2, 16, QChar('0')).toUpper();
        };

    return efid.toUpper().leftJustified(4, '0').left(4)
        + ac1.toUpper().leftJustified(4, '0').left(4)
        + ac2.toUpper().leftJustified(4, '0').left(4)
        + ac3.toUpper().leftJustified(4, '0').left(4)
        + sob.toUpper().leftJustified(4, '0').left(4)
        + byte2hex(tof)
        + byte2hex(sor)
        + byte2hex(rc)
        + byte2hex(keyIndex)
        + byte2hex(tok)
        + byte2hex(rfu);
}

// ─────────────────────────────────────────────────────────────────────────────
// sendCheck
// ─────────────────────────────────────────────────────────────────────────────
bool MasterCardUpdater::sendCheck(SCARDHANDLE hCard, const QString& apdu,
    const QString& stepDesc, const LogFn& log)
{
    QString dummy;
    return sendCheck(hCard, apdu, stepDesc, log, dummy);
}

bool MasterCardUpdater::sendCheck(SCARDHANDLE hCard, const QString& apdu,
    const QString& stepDesc, const LogFn& log,
    QString& respOut)
{
    bool ok = false;
    respOut = CardUtils::sendApdu(hCard, apdu, ok, log);
    if (!ok || !respOut.endsWith("9000")) {
        logErr(log, QString("Error: [%1] 失敗 (SW=%2)").arg(stepDesc).arg(respOut.right(4)));
        return false;
    }
    return true;
}