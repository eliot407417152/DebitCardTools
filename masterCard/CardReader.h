#pragma once
#include <string>
#include <vector>
#include <winscard.h>
#include "Hex.h"
#include "Logger.h"

class CardReader {
public:
    ~CardReader() { disconnect(); }

    void setLog(Logger* log, const std::string& label) { log_ = log; label_ = label; }

    static std::vector<std::string> list();
    bool connect(const std::string& name);

    // 借用外部已連線的 handle(例如 ReaderManager 管理的連線)。
    // 借用時本物件不擁有該連線:disconnect() / 解構都不會呼叫 SCardDisconnect。
    void attach(SCARDHANDLE h);

    void disconnect();
    bool isConnected() const { return card_ != 0; }

    // 送出 APDU。成功回 true,data 為回應資料(不含 SW),sw 為狀態碼。
    // 只有通訊失敗才回 false,SW 不是 9000 由呼叫端判斷。
    bool transmit(const Bytes& cmd, Bytes& data, uint16_t& sw);

private:
    void log(const std::string& s) { if (log_) log_->add(label_ + " " + s); }

    SCARDCONTEXT ctx_ = 0;
    SCARDHANDLE  card_ = 0;
    DWORD        proto_ = 0;
    bool         owned_ = true;   // false = 借用外部 handle,不負責斷線
    Logger* log_ = nullptr;
    std::string  label_;
};