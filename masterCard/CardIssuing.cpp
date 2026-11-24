#include "CardIssuing.h"

using Kind = CardIssuing::Kind;

const std::vector<CardIssuing::Entry>& CardIssuing::extraEntries() {
    static const std::vector<Entry> list = {
        // 基碼
        { "B00C2", "00C2050007000400000C900803020100", -1,   Kind::Pin },
        { "B00C4", "00C4050005000400001C901802040200", 0x24, Kind::DivKey },
        { "B00C5", "00C5800005000400001CB01800110100", 0x25, Kind::WorkingKey },
        { "B00C6", "00C6800005000400001CB01800120400", 0x26, Kind::DivKey },
        { "B00D6", "00D6800005000400001CB01800130400", 0x27, Kind::DivKey },
        { "B00D7", "00D7800005000400001CB01800140400", 0x28, Kind::DivKey },
        { "B00C8", "00C8080005000400001CB01800150400", 0x29, Kind::DivKey },
        // 個人化資料(只有 Create File)
        { "0001", "000100000D0005000034000000000000", -1, Kind::CreateOnly },
        { "0002", "000200008A000500008A000000000000", -1, Kind::CreateOnly },
        { "1001", "100100000D00050000CE000000000000", -1, Kind::CreateOnly },
        { "1002", "100200000D00050000E0000000000000", -1, Kind::CreateOnly },
        { "1003", "100300000D0005000042000000000000", -1, Kind::CreateOnly },
        { "1080", "10800000820080000484D06E00120000", -1, Kind::CreateOnly },
        { "1090", "10900000000080000484D06E00150000", -1, Kind::CreateOnly },
        { "1102", "11020000820080000048D01800130000", -1, Kind::CreateOnly },
        { "1103", "11030000820080000048D01800140000", -1, Kind::CreateOnly },
        { "007F", "007F0000000005000006000000000000", -1, Kind::CreateOnly },
    };
    return list;
}

// 與洗卡差別:Step 4 的 P1 = 21,Step 8 的 P1 = 22
bool CardIssuing::authTerminal() { return doAuthTerminal(0x21, 0x18); }
bool CardIssuing::authTerminalC0() { return doAuthTerminal(0x23, 0x08); }
bool CardIssuing::authTerminalC4() { return doAuthTerminal(0x24, 0x20); }

bool CardIssuing::verifyPin(const Bytes& pin) {
    if (pin.size() != 8) { fail("Verify PIN data must be 8 bytes", "認證 PIN 碼必須是 8 bytes"); return false; }
    Bytes out;
    return run(icc_, "Verify PIN (C2)", apdu(0x00, 0x20, 0x00, 0x10, pin), out);
}
bool CardIssuing::createSessionKey() { return doCreateSessionKey(0x22); }

bool CardIssuing::updateC3(const Bytes& div) { return updateByDivKey(9, 0x21, 0x00, 0xC3, div); }
bool CardIssuing::updateC1(const Bytes& div) { return updateByDivKey(12, 0x22, 0x00, 0xC1, div); }

bool CardIssuing::updateC0(const Bytes& div) {
    if (!checkDiv(div)) return false;
    Bytes key, tmp;
    if (!run(icc_, "15 Select 00C0", apdu(0x00, 0xA4, 0x02, 0x00, { 0x00, 0xC0 }), tmp))  return false;
    if (!run(isam_, "16 Get Div Key With SK (ISAM)", apdu(0x82, 0x0B, 0x23, 0x00, div, 0x00), key))     return false;
    if (!run(icc_, "17 Update Record Cipher", apdu(0x00, 0xE4, 0x01, 0x04, key), tmp))           return false;
    return true;
}

bool CardIssuing::checkExtraInput(const Entry& e, const Bytes& in) {
    if (e.kind == Kind::DivKey && !checkDiv(in)) return false;
    if (e.kind == Kind::Pin && (in.empty() || in.size() > 250)) {
        fail("PIN length must be 1~250 bytes", "PIN碼長度必須是 1~250 bytes");
        return false;
    }
    return true;
}

// 寫入內容(不含 Create File),新增與修改共用
bool CardIssuing::writeExtra(const Entry& e, const Bytes& in) {
    Bytes create;
    fromHex(e.createHex, create);
    const Bytes fid = { create[0], create[1] };
    const std::string& n = e.name;
    Bytes out, key;

    switch (e.kind) {
    case Kind::DivKey:
        if (!run(isam_, "Get Div Key With SK " + n, apdu(0x82, 0x0B, (uint8_t)e.kid, 0x00, in, 0x00), key)) return false;
        if (!run(icc_, "Select File " + n, apdu(0x00, 0xA4, 0x02, 0x00, fid), out))               return false;
        if (!run(icc_, "Update Record Cipher " + n, apdu(0x00, 0xE4, 0x01, 0x04, key), out))               return false;
        return true;

    case Kind::WorkingKey:
        if (!run(isam_, "Get Working Key With SK " + n, apdu(0x82, 0x0C, (uint8_t)e.kid, 0x00, {}, 0x00), key)) return false;
        if (!run(icc_, "Select File " + n, apdu(0x00, 0xA4, 0x02, 0x00, fid), out))                 return false;
        if (!run(icc_, "Update Record Cipher " + n, apdu(0x00, 0xE4, 0x01, 0x04, key), out))                 return false;
        return true;

    case Kind::Pin: {
        if (!run(icc_, "Select File " + n, apdu(0x00, 0xA4, 0x02, 0x00, fid), out)) return false;
        Bytes d = { 0x01, static_cast<uint8_t>(in.size()) };   // 01 XX PIN,Lc 會自動 = PIN長度 + 2
        d.insert(d.end(), in.begin(), in.end());
        if (!run(icc_, "Write Record " + n, apdu(0x00, 0xD2, 0x00, 0x05, d), out)) return false;
        return true;
    }

    case Kind::CreateOnly:
        break;
    }
    fail("This item has no content to write", "此項目沒有可寫入的內容");
    return false;
}

bool CardIssuing::addExtra(size_t index, const Bytes& in) {
    const Entry& e = extraEntries().at(index);
    if (!checkExtraInput(e, in)) return false;   // 先檢查輸入,避免送了一半才發現資料不對

    Bytes create, out;
    fromHex(e.createHex, create);
    if (!run(icc_, "Create File " + e.name, apdu(0x00, 0xE0, 0x00, 0x00, create), out)) return false;
    if (e.kind == Kind::CreateOnly) return true;
    return writeExtra(e, in);
}

bool CardIssuing::modifyExtra(size_t index, const Bytes& in) {
    const Entry& e = extraEntries().at(index);
    if (e.kind == Kind::CreateOnly) {
        fail("This item cannot be modified", "此項目不能修改");
        return false;
    }

    // B00C2 的修改:Select File 00C2 -> Update Record(P1 = 記錄編號 01、P2 = 04,Lc = Data 長度 8)
    if (e.kind == Kind::Pin) {
        if (in.size() != 8) { fail("Update Record data must be 8 bytes", "PIN碼必須是 8 bytes"); return false; }
        Bytes create, out;
        fromHex(e.createHex, create);
        const Bytes fid = { create[0], create[1] };
        if (!run(icc_, "Select File " + e.name, apdu(0x00, 0xA4, 0x02, 0x00, fid), out)) return false;
        return run(icc_, "Update Record " + e.name, apdu(0x00, 0xD2, 0x01, 0x04, in), out);
    }

    if (!checkExtraInput(e, in)) return false;
    return writeExtra(e, in);
}

// ---------- List File ----------
static uint16_t fidOf(const CardIssuing::Entry& e) {
    return static_cast<uint16_t>(std::stoi(e.createHex.substr(0, 4), nullptr, 16));
}

// 解析 List File 的回應,取出檔案 ID。
// ※ 回應格式尚未確認,目前的假設(要調整就改這個函式):
//    - 長度是 16 的倍數:每 16 bytes 一筆,前 2 bytes 為檔案 ID(與 Create File 的資料格式相同)
//    - 其他長度:把每相鄰 2 bytes 都當成可能的檔案 ID
static std::set<uint16_t> parseFileIds(const Bytes& r) {
    std::set<uint16_t> ids;
    if (!r.empty() && r.size() % 16 == 0) {
        for (size_t i = 0; i + 1 < r.size(); i += 16)
            ids.insert(static_cast<uint16_t>((r[i] << 8) | r[i + 1]));
    }
    else {
        for (size_t i = 0; i + 1 < r.size(); ++i)
            ids.insert(static_cast<uint16_t>((r[i] << 8) | r[i + 1]));
    }
    return ids;
}

bool CardIssuing::refreshFileList() {
    clearFileList();
    Bytes out;
    uint16_t sw = 0;
    const Bytes cmd = { 0x00, 0xAA, 0x00, 0x00, 0x00, 0x00, 0x00 };   // Le = 000000h (Extended Le)

    if (!run(icc_, "List File", cmd, out, &sw)) return false;
    if (sw != 0x9000 && sw != 0x6282) {
        fail("List File: " + swString(sw), swReason(sw));
        return false;
    }

    files_ = parseFileIds(out);
    loaded_ = true;

    std::string found;
    for (size_t i = 0; i < extraEntries().size(); ++i)
        if (hasFile(i)) found += " " + extraEntries()[i].name;
    log_.add("List File: items already on card:" + (found.empty() ? std::string(" (none)") : found));
    return true;
}

bool CardIssuing::hasFile(size_t index) const {
    return loaded_ && files_.count(fidOf(extraEntries().at(index))) > 0;
}