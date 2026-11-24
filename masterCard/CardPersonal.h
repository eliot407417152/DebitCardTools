#pragma once
#include <vector>
#include "CardFlow.h"
#include "PersonalData.h"

// 個人化資料:寫入(Write Record)、查看(Read All Record)、更新(Update Record)
class CardPersonal : public CardFlow {
public:
    using CardFlow::CardFlow;

    struct RecordInput {
        int rdId = -1;    // RD ID;-1 = 此檔案沒有 RD ID
        int recNo = 1;    // 沒有 RD ID 的檔案,更新時要指定的記錄編號
        Bytes data;       // 各欄位依序串接後的內容(不含 RD ID 與長度)
    };
    struct RecordData {   // 查看時讀回來的一筆
        int rdId = -1;
        Bytes data;
    };

    bool write(const personal::FileDef& f, const std::vector<RecordInput>& recs);
    bool update(const personal::FileDef& f, const std::vector<RecordInput>& recs);
    bool read(const personal::FileDef& f, std::vector<RecordData>& out, Bytes& raw);

private:
    bool selectFile(const personal::FileDef& f);
};