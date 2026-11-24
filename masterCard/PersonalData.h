#pragma once
#include <set>
#include <string>
#include <utility>
#include <vector>

// 個人化資料檔案的欄位定義(純 C++,不含 Qt)
namespace personal {

    struct Field {
        std::string name;   // 資料名稱
        std::string type;   // PB / AN / N / B
        int minLen;         // 長度(bytes)
        int maxLen;         // 固定長度時 maxLen == minLen
    };

    struct Record {
        int rdId;                   // RD ID;-1 代表此檔案沒有 RD ID
        std::vector<Field> fields;
    };

    // 卡片上的認證種類
    enum class Auth { C0, C2, C3, C4 };

    // 存取條件(寫入 / 更新 / 讀取 / 新增檔案):
    //   never = true            → 此動作不允許(例如只能讀取的檔案)
    //   anyOf 是空的            → FREE,不需要認證
    //   anyOf = {{C0},{C3}}      → C0 或 C3
    //   anyOf = {{C2,C4}}        → C2 且 C4
    struct AccessRule {
        bool never = false;
        std::vector<std::vector<Auth>> anyOf;
    };

    bool allowed(const AccessRule& rule, const std::set<Auth>& done);   // 目前已完成的認證是否滿足條件
    std::string describe(const AccessRule& rule);                       // "C0 或 C3"、"C2 且 C4"
    std::string authName(Auth a);                                       // "C0"

    struct FileDef {
        FileDef(std::string n, std::string t, std::vector<Record> r)
            : name(std::move(n)), title(std::move(t)), records(std::move(r)) {
        }

        std::string name;           // EF ID,例如 "0001"
        std::string title;          // 檔案說明(可為空)
        std::vector<Record> records;

        AccessRule create;   // 新增檔案(Create File)
        AccessRule read;     // 讀取
        AccessRule update;   // 更新
        AccessRule write;    // 寫入
    };

    const std::vector<FileDef>& allFiles();
    const FileDef* findFile(const std::string& name);

} // namespace personal