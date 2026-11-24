#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using Bytes = std::vector<uint8_t>;

// {0x00,0xB0} -> "00 B0"
inline std::string toHex(const Bytes& b) {
    std::string s;
    char t[4];
    for (size_t i = 0; i < b.size(); ++i) {
        std::snprintf(t, sizeof t, i ? " %02X" : "%02X", b[i]);
        s += t;
    }
    return s;
}

// "00 b0" / "00B0" -> {0x00,0xB0};含非法字元或奇數長度回傳 false
inline bool fromHex(const std::string& s, Bytes& out) {
    out.clear();
    int hi = -1;
    for (char c : s) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
        int v;
        if (c >= '0' && c <= '9')      v = c - '0';
        else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
        else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
        else return false;
        if (hi < 0) hi = v;
        else { out.push_back(static_cast<uint8_t>(hi << 4 | v)); hi = -1; }
    }
    return hi < 0;
}