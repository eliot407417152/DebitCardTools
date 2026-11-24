#include "CardPersonal.h"

static std::string hex1(int v) { return toHex(Bytes{ static_cast<uint8_t>(v) }); }

bool CardPersonal::selectFile(const personal::FileDef& f) {
    Bytes fid, out;
    fromHex(f.name, fid);
    return run(icc_, "Select File " + f.name, apdu(0x00, 0xA4, 0x02, 0x00, fid), out);
}

// Write Record:00 D2 00 05 Lc  [RD ID][長度][資料]
// 沒有 RD ID 的檔案:資料直接送出
bool CardPersonal::write(const personal::FileDef& f, const std::vector<RecordInput>& recs) {
    if (recs.empty()) { fail("Nothing to write", "沒有要寫入的項目"); return false; }
    if (!selectFile(f)) return false;

    Bytes out;
    for (const auto& r : recs) {
        Bytes d;
        if (r.rdId >= 0) {
            d.push_back(static_cast<uint8_t>(r.rdId));
            d.push_back(static_cast<uint8_t>(r.data.size()));
        }
        d.insert(d.end(), r.data.begin(), r.data.end());
        if (d.empty() || d.size() > 255) { fail("Record length out of range", "資料長度超出範圍"); return false; }

        const std::string label = "Write Record " + f.name + (r.rdId >= 0 ? " RD " + hex1(r.rdId) : "");
        if (!run(icc_, label, apdu(0x00, 0xD2, 0x00, 0x05, d), out)) return false;
    }
    return true;
}

// Update Record:00 D2 RecN 04 L [資料]
// RecN = RD ID;沒有 RD ID 的檔案則使用指定的記錄編號
bool CardPersonal::update(const personal::FileDef& f, const std::vector<RecordInput>& recs) {
    if (recs.empty()) { fail("Nothing to update", "沒有要更新的項目"); return false; }
    if (!selectFile(f)) return false;

    Bytes out;
    for (const auto& r : recs) {
        if (r.data.empty() || r.data.size() > 255) { fail("Record length out of range", "資料長度超出範圍"); return false; }
        const uint8_t recN = static_cast<uint8_t>(r.rdId >= 0 ? r.rdId : r.recNo);
        const std::string label = "Update Record " + f.name + " " + (r.rdId >= 0 ? "RD " : "Rec ") + hex1(recN);
        if (!run(icc_, label, apdu(0x00, 0xD2, recN, 0x04, r.data), out)) return false;
    }
    return true;
}

// Read All Record:00 B2 01 05 + Le(000000h)
// 回應格式(有 RD ID 的檔案):[RD ID][長度][資料] 重複
bool CardPersonal::read(const personal::FileDef& f, std::vector<RecordData>& recs, Bytes& raw) {
    recs.clear();
    raw.clear();
    if (!selectFile(f)) return false;
    if (!run(icc_, "Read All Record " + f.name, Bytes{ 0x00, 0xB2, 0x01, 0x05, 0x00, 0x00, 0x00 }, raw)) return false;

    const bool hasRd = !f.records.empty() && f.records[0].rdId >= 0;
    if (!hasRd) {   // 沒有 RD ID:整段當作一筆原始資料
        if (!raw.empty()) recs.push_back({ -1, raw });
        return true;
    }

    size_t i = 0;
    while (i + 2 <= raw.size()) {
        const int rd = raw[i], len = raw[i + 1];
        if (rd == 0x00 && len == 0x00) break;   // 後面是補的 00,結束
        if (i + 2 + len > raw.size()) break;    // 長度超出,資料不完整
        recs.push_back({ rd, Bytes(raw.begin() + i + 2, raw.begin() + i + 2 + len) });
        i += 2 + len;
    }
    return true;
}