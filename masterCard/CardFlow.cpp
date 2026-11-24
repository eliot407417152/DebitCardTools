#include "CardFlow.h"

Bytes CardFlow::apdu(uint8_t cla, uint8_t ins, uint8_t p1, uint8_t p2,
    const Bytes& data, int le) {
    Bytes c{ cla, ins, p1, p2 };
    if (!data.empty()) {
        c.push_back(static_cast<uint8_t>(data.size()));
        c.insert(c.end(), data.begin(), data.end());
    }
    if (le >= 0) c.push_back(static_cast<uint8_t>(le));
    return c;
}

Bytes CardFlow::cat(Bytes a, const Bytes& b) {
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

// 狀態碼 + 已知狀態碼的說明
std::string CardFlow::swString(uint16_t sw) {
    char b[16];
    std::snprintf(b, sizeof b, "SW=%04X", sw);
    std::string s = b;
    switch (sw) {
    case 0x6610: s += " (沒有權限 請認證端末設備)"; break;
    case 0x6602: s += " (Record not found / Incorrect type of EF)"; break;
    default: break;
    }
    return s;
}

// 使用者看得懂的原因
std::string CardFlow::swReason(uint16_t sw) {
    if ((sw & 0xFFF0) == 0x63C0)   // Verify PIN 錯誤:低 4 bits 是剩餘次數
        return "PIN碼錯誤,剩餘 " + std::to_string(sw & 0x000F) + " 次嘗試";
    switch (sw) {
    case 0x6610: return "權限不足,請先認證端末設備";
    case 0x6602: return "找不到記錄,或檔案類型不正確";
    default:     return "卡片回應錯誤,詳細代碼請查看 Log";
    }
}

void CardFlow::fail(const std::string& technical, const std::string& reason) {
    err_ = technical;
    userErr_ = reason;
    log_.add("!!! " + technical);
}

bool CardFlow::run(CardReader& r, const std::string& name, const Bytes& cmd,
    Bytes& out, uint16_t* swOut) {
    log_.add("--- Step " + name);
    uint16_t sw = 0;
    if (!r.transmit(cmd, out, sw)) {
        fail("Step " + name + ": communication error", "與讀卡機通訊失敗");
        return false;
    }
    if (swOut) { *swOut = sw; return true; }
    if (sw != 0x9000) {
        fail("Step " + name + ": " + swString(sw), swReason(sw));
        return false;
    }
    return true;
}

bool CardFlow::checkDiv(const Bytes& div) {
    if (div.size() != 8) {
        fail("Diversification data must be 8 bytes", "多樣化資料必須是 8 bytes");
        return false;
    }
    return true;
}

// ---------- 1~5 認證端末設備 ----------
bool CardFlow::doAuthTerminal(uint8_t p1, uint8_t p2) {
    Bytes readBin, rnd, tmp, mac;

    if (!run(icc_, "1 Read Binary", apdu(0x00, 0xB0, 0x00, 0x04, {}, 0x08), readBin)) return false;
    if (!run(icc_, "2 Generate Random Number", apdu(0x00, 0x86, 0x00, 0x00, {}, 0x08), rnd))     return false;
    if (!run(isam_, "3 Select ISAM Object", apdu(0x82, 0xA4, 0x00, 0x00, { 0x95, 0x00 }, 0x0E), tmp)) return false;
    if (!run(isam_, "4 Generate Terminal Authentication Code",
        apdu(0x82, 0x08, p1, 0x00, cat(readBin, rnd), 0x08), mac))                    return false;
    if (!run(icc_, "5 Terminal Authentication", apdu(0x00, 0x82, 0x00, p2, mac), tmp))         return false;
    return true;
}

// ---------- 6~8 建立交談基碼 ----------
bool CardFlow::doCreateSessionKey(uint8_t p1) {
    Bytes readBin, isamRnd, sk, tmp;

    // 步驟 8 需要 Read Binary 回應;此功能可單獨執行,所以這裡重新讀一次(同一張卡,內容相同)
    if (!run(icc_, "1 Read Binary (for step 8)", apdu(0x00, 0xB0, 0x00, 0x04, {}, 0x08), readBin)) return false;
    if (!run(isam_, "6 Generate Random Number (ISAM)", apdu(0x8F, 0x86, 0x00, 0x00, {}, 0x08), isamRnd)) return false;
    if (!run(icc_, "7 Set Session Key (ICC)", apdu(0x00, 0x84, 0x00, 0x80, isamRnd, 0x08), sk)) return false;
    if (!run(isam_, "8 Set Session Key (ISAM)", apdu(0x82, 0x0A, p1, 0x00, cat(readBin, sk)), tmp)) return false;
    return true;
}

// ---------- Get Div Key -> Select -> Update Record Cipher ----------
bool CardFlow::updateByDivKey(int s, uint8_t p1, uint8_t fHi, uint8_t fLo, const Bytes& div) {
    if (!checkDiv(div)) return false;
    Bytes key, tmp;
    auto n = [](int i) { return std::to_string(i); };

    if (!run(isam_, n(s) + " Get Div Key With SK (ISAM)", apdu(0x82, 0x0B, p1, 0x00, div, 0x00), key)) return false;
    if (!run(icc_, n(s + 1) + " Select File", apdu(0x00, 0xA4, 0x02, 0x00, { fHi, fLo }), tmp)) return false;
    if (!run(icc_, n(s + 2) + " Update Record Cipher", apdu(0x00, 0xE4, 0x01, 0x04, key), tmp))       return false;
    return true;
}