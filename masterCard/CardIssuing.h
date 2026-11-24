#pragma once
#include <set>
#include <string>
#include <vector>
#include "CardFlow.h"

// 發卡流程
class CardIssuing : public CardFlow {
public:
    using CardFlow::CardFlow;

    // 「新增卡片其他基碼」裡每一個項目
    enum class Kind {
        DivKey,      // Create File -> Get Div Key -> Select -> Update Record Cipher (需要多樣化資料)
        Pin,         // Create File -> Select -> Write Record (需要 PIN 碼)  [B00C2]
        WorkingKey,  // Create File -> Get Working Key -> Select -> Update Record Cipher  [B00C5]
        CreateOnly   // 只有 Create File(個人化資料)
    };
    struct Entry {
        std::string name;       // 顯示名稱,如 "B00C4"、"0001"
        std::string createHex;  // Create File 的 DATA(16 bytes),前 2 bytes 即檔案 ID
        int kid;                // Get Div / Working Key 的 P1,不需要時為 -1
        Kind kind;
    };
    static const std::vector<Entry>& extraEntries();

    bool authTerminal();                      // 認證端末設備 (C3)  Step 4 P1=21, Step 5 P2=18
    bool authTerminalC0();                    // 發卡單位主控基碼 (C0)  Step 4 P1=23, Step 5 P2=08
    bool authTerminalC4();                    // 金融用端末機認證基碼 (C4)  Step 4 P1=24, Step 5 P2=20
    bool verifyPin(const Bytes& pin);         // 認證PIN碼 (C2)  Verify PIN: 00 20 00 10 08 [8 bytes]
    bool createSessionKey();                  // 步驟 6~8   建立交談基碼
    bool updateC3(const Bytes& div);          // 步驟 9~11
    bool updateC1(const Bytes& div);          // 步驟 12~14
    bool updateC0(const Bytes& div);          // 步驟 15~17
    // input:DivKey 傳多樣化資料(8 bytes);Pin 傳 PIN 碼;其餘忽略
    bool addExtra(size_t index, const Bytes& input);      // 新增:Create File + 寫入內容
    bool modifyExtra(size_t index, const Bytes& input);   // 修改:只做寫入內容(跟 C3/C1/C0 一樣,不 Create File)

    // List File:讀取卡片目前有哪些檔案
    bool refreshFileList();
    void clearFileList() { files_.clear(); loaded_ = false; }
    bool fileListLoaded() const { return loaded_; }
    bool hasFile(size_t index) const;   // extraEntries()[index] 的檔案是否已在卡片內

private:
    bool checkExtraInput(const Entry& e, const Bytes& input);
    bool writeExtra(const Entry& e, const Bytes& input);   // Get Key -> Select -> Update / Write Record

    std::set<uint16_t> files_;
    bool loaded_ = false;
};