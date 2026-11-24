#include "PersonalData.h"

namespace personal {

    std::string authName(Auth a) {
        switch (a) {
        case Auth::C0: return "C0";
        case Auth::C2: return "C2";
        case Auth::C3: return "C3";
        default:       return "C4";
        }
    }

    bool allowed(const AccessRule& rule, const std::set<Auth>& done) {
        if (rule.never) return false;
        if (rule.anyOf.empty()) return true;            // FREE
        for (const auto& all : rule.anyOf) {            // 任一組「全部都完成」就通過
            bool ok = true;
            for (Auth a : all) ok = ok && done.count(a) > 0;
            if (ok) return true;
        }
        return false;
    }

    std::string describe(const AccessRule& rule) {
        if (rule.never) return "不允許";
        if (rule.anyOf.empty()) return "不需認證";
        std::string s;
        for (size_t i = 0; i < rule.anyOf.size(); ++i) {
            if (i) s += " 或 ";
            for (size_t j = 0; j < rule.anyOf[i].size(); ++j) {
                if (j) s += " 且 ";
                s += authName(rule.anyOf[i][j]);
            }
        }
        return s;
    }

    // 各檔案的存取條件
    static AccessRule orOf(std::initializer_list<Auth> l) {
        AccessRule r;
        for (Auth a : l) r.anyOf.push_back({ a });
        return r;
    }
    static AccessRule andOf(std::initializer_list<Auth> l) {
        AccessRule r;
        r.anyOf.push_back(std::vector<Auth>(l));
        return r;
    }
    static AccessRule never() { AccessRule r; r.never = true; return r; }
    static AccessRule freeRule() { return AccessRule(); }

    static void applyRules(FileDef& f) {
        f.create = orOf({ Auth::C0, Auth::C3 });   // 文件沒有規定,比照「寫入」(請依實際需求調整)
        f.read = freeRule();                        // 所有檔案讀取都 FREE

        const std::string& n = f.name;
        if (n == "0001" || n == "1001" || n == "1002" || n == "1003") {
            f.update = orOf({ Auth::C0, Auth::C3, Auth::C4 });
            f.write = orOf({ Auth::C0, Auth::C3 });
        }
        else if (n == "0002") {
            f.update = andOf({ Auth::C2, Auth::C4 });
            f.write = orOf({ Auth::C0, Auth::C3 });
        }
        else if (n == "007F") {
            f.update = freeRule();
            f.write = orOf({ Auth::C0, Auth::C3 });
        }
        else {                                    // 1080 / 1090 / 1102 / 1103:只讀取,不寫入也不更新
            f.update = never();
            f.write = never();
        }
    }

    static std::vector<FileDef> build() {
        std::vector<FileDef> v;

        v.push_back({ "0001", "", {
            { 0x01, { {"發卡單位代號", "PB", 4, 4}, {"帳戶號碼", "PB", 10, 10},
                      {"卡片序號", "PB", 1, 1},   {"卡片失效日期", "PB", 4, 4} } },
            { 0x02, { {"持卡人身分證統一編號", "AN", 10, 10}, {"持卡人出生日期", "PB", 4, 4} } },
            { 0x03, { {"國別碼", "N", 3, 3}, {"幣別碼", "N", 3, 3}, {"金額冪次", "N", 1, 1} } },
        } });

        v.push_back({ "0002", "", {
            { 0x01, { {"卡片啟用日期", "PB", 4, 4} } },
            { 0x02, { {"金融卡磁條第二軌資料", "PB", 20, 20} } },
            { 0x03, { {"金融卡磁條第三軌資料", "PB", 54, 54} } },
        } });

        {   // 1001 卡片基本資料檔:RD ID 03 ~ 0A 為帳/卡號 1~8
            FileDef f{ "1001", "卡片基本資料檔", {
                { 0x01, { {"發卡單位代號", "AN", 8, 8} } },
                { 0x02, { {"晶片金融卡備註欄", "B", 30, 30} } },
            } };
            for (int i = 1; i <= 8; ++i)
                f.records.push_back({ 0x02 + i, { {"帳/卡號 " + std::to_string(i), "AN", 16, 16} } });
            v.push_back(f);
        }

        {   // 1002 轉入帳號資料檔:RD ID 01 ~ 08
            FileDef f{ "1002", "轉入帳號資料檔", {} };
            for (int i = 1; i <= 8; ++i)
                f.records.push_back({ i, { {"帳/卡號 " + std::to_string(i), "AN", 24, 24} } });
            v.push_back(f);
        }

        v.push_back({ "1003", "", {
            { 0x01, { {"發卡單位代號", "AN", 8, 8} } },
            { 0x02, { {"晶片金融卡備註欄", "B", 30, 30} } },
            { 0x03, { {"帳/卡號 1", "AN", 16, 16} } },
        } });

        // 以下四個檔案文件上沒有 RD ID:一筆記錄 = 各欄位依序串接
        v.push_back({ "1080", "交易授權明細記錄", { { -1, {
            {"交易資料明細", "AN", 102, 102}, {"交易授權驗證碼", "B", 8, 128} } } } });
        v.push_back({ "1090", "小額支付專用交易授權明細記錄", { { -1, {
            {"交易資料明細", "AN", 102, 102}, {"交易授權驗證碼", "B", 8, 128} } } } });
        v.push_back({ "1102", "One Time Password授權明細記錄檔", { { -1, {
            {"輸入資料", "AN", 8, 8}, {"OTP 驗證碼", "B", 8, 8} } } } });
        v.push_back({ "1103", "One Time Password授權明細記錄檔", { { -1, {
            {"輸入資料", "AN", 8, 8}, {"OTP 驗證碼", "B", 8, 8} } } } });

        v.push_back({ "007F", "", {
            { 0x01, { {"卡片最後一次異常狀態碼", "B", 2, 2} } },
        } });
        for (auto& f : v) applyRules(f);
        return v;
    }

    const std::vector<FileDef>& allFiles() {
        static const std::vector<FileDef> list = build();
        return list;
    }

    const FileDef* findFile(const std::string& name) {
        for (const auto& f : allFiles())
            if (f.name == name) return &f;
        return nullptr;
    }

} // namespace personal