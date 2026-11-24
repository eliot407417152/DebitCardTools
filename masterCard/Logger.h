#pragma once
#include <chrono>
#include <cstdio>
#include <ctime>
#include <functional>
#include <string>
#include <vector>

class Logger {
public:
    std::function<void(const std::string&)> onLine;   // 每新增一行就通知(UI 用)

    void add(const std::string& msg) {
        std::string line = timestamp() + " " + msg;
        lines_.push_back(line);
        if (onLine) onLine(line);
    }
    void clear() { lines_.clear(); }
    std::string text() const {
        std::string s;
        for (const auto& l : lines_) s += l + "\n";
        return s;
    }

private:
    static std::string timestamp() {
        using namespace std::chrono;
        auto now = system_clock::now();
        int ms = static_cast<int>(duration_cast<milliseconds>(now.time_since_epoch()).count() % 1000);
        std::time_t t = system_clock::to_time_t(now);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        char buf[32];
        std::snprintf(buf, sizeof buf, "[%02d:%02d:%02d.%03d]", tm.tm_hour, tm.tm_min, tm.tm_sec, ms);
        return buf;
    }
    std::vector<std::string> lines_;
};