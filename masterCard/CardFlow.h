#pragma once
#include <string>
#include "CardReader.h"

// 洗卡、發卡共用的基底:送指令、組 APDU、共同流程(純 C++,不含 Qt)
class CardFlow {
public:
    CardFlow(CardReader& isam, CardReader& icc, Logger& log)
        : isam_(isam), icc_(icc), log_(log) {
    }

    // 給使用者看的失敗原因(不含狀態碼);狀態碼與指令細節只寫在 Log
    const std::string& lastError() const { return userErr_; }

protected:
    // 組 APDU:有 data 就自動加 Lc,le >= 0 就加 Le
    static Bytes apdu(uint8_t cla, uint8_t ins, uint8_t p1, uint8_t p2,
        const Bytes& data = {}, int le = -1);
    static Bytes cat(Bytes a, const Bytes& b);
    static std::string swString(uint16_t sw);   // 給 Log 用:"SW=6610 (...)"
    static std::string swReason(uint16_t sw);   // 給使用者看:"權限不足,請先認證端末設備"
    void fail(const std::string& technical, const std::string& reason);   // technical 只進 Log,reason 給使用者

    // swOut == nullptr:SW 必須是 9000,否則視為失敗
    // swOut != nullptr:只要通訊成功就回 true,SW 交給呼叫端判斷
    bool run(CardReader& r, const std::string& name, const Bytes& cmd,
        Bytes& out, uint16_t* swOut = nullptr);
    bool checkDiv(const Bytes& div);

    // 洗卡、發卡相同,只差 P1 的流程
    bool doAuthTerminal(uint8_t p1, uint8_t p2 = 0x18);   // 步驟 1~5(Step 4 的 P1、Step 5 的 P2)
    bool doCreateSessionKey(uint8_t p1);  // 步驟 6~8(Step 8 的 P1)
    bool updateByDivKey(int firstStep, uint8_t p1, uint8_t fileHi, uint8_t fileLo, const Bytes& div);

    CardReader& isam_;
    CardReader& icc_;
    Logger& log_;
    std::string err_;       // 技術細節(Log 用)
    std::string userErr_;   // 使用者看的原因
};