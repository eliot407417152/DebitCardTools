#include "CardWasher.h"

bool CardWasher::authTerminal() { return doAuthTerminal(0x11); }
bool CardWasher::createSessionKey() { return doCreateSessionKey(0x12); }

bool CardWasher::updateIssuerAuthKey(const Bytes& div) { return updateByDivKey(9, 0x21, 0x00, 0xC3, div); }
bool CardWasher::updateCardRandomKey(const Bytes& div) { return updateByDivKey(12, 0x22, 0x00, 0xC1, div); }

// ---------- 15~17 新增發卡單位主控基碼 ----------
bool CardWasher::addMasterKey(const Bytes& div) {
    if (!checkDiv(div)) return false;
    Bytes out, key;
    uint16_t sw = 0;
    const Bytes select = apdu(0x00, 0xA4, 0x02, 0x00, { 0x00, 0xC0 });

    if (!run(icc_, "15 Select 00C0", select, out, &sw)) return false;

    if (sw == 0x6602) {   // 檔案不存在 -> Create File 後再 Select 一次
        const Bytes fileInfo = { 0x00, 0xC0, 0x04, 0x00, 0x05, 0x00, 0x04, 0x00,
                                 0x00, 0x1C, 0x90, 0x18, 0x02, 0x01, 0x02, 0x00 };
        if (!run(icc_, "15-1 Create File 00C0", apdu(0x00, 0xE0, 0x00, 0x00, fileInfo), out)) return false;
        if (!run(icc_, "15-2 Select 00C0", select, out))                                  return false;
    }
    else if (sw != 0x9000) {
        fail("Step 15 Select 00C0: " + swString(sw), swReason(sw));
        return false;
    }

    if (!run(isam_, "16 Get Div Key With SK (ISAM)", apdu(0x82, 0x0B, 0x23, 0x00, div, 0x00), key)) return false;
    if (!run(icc_, "17 Update Record Cipher", apdu(0x00, 0xE4, 0x01, 0x04, key), out))       return false;
    return true;
}