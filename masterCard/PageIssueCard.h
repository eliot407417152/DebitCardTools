#pragma once

#include "FlowPageBase.h"
#include "CardReader.h"
#include "PersonalData.h"

class QPushButton;
class QLineEdit;
class QComboBox;
class ReaderBar;

// ─────────────────────────────────────────────────────────────────────────────
// 頁面 4 — 發卡
//   流程與指令內容來自 CardIssuer 專案（CardFlow / CardIssuing / CardPersonal）。
//
//   「發卡」一鍵流程（建議先洗卡）：
//     認證端末設備(C3) → 建立交談基碼 → 更新 C3 / C1 / C0（多樣化資料）
//     → 依序新增發卡基碼 B00C2(PIN) / B00C4 / B00C5 / B00C6 / B00D6 / B00D7 / B00C8
//     → 建立個人化資料檔案 0001 / 0002 / 1001 / 1002 / 1003 / 1080 / 1090 / 1102 / 1103 / 007F
//
//   「個人化資料」：寫入 / 查看 / 更新，欄位依 PersonalData 的規格逐欄輸入並檢查長度；
//   寫入 / 更新前會依檔案的存取條件自動完成所需認證（C3 / C0 / C4 / C2）。
//
//   讀卡機：ISAM 讀卡機 + 卡片讀卡機（需為兩台不同的讀卡機）
// ─────────────────────────────────────────────────────────────────────────────
class PageIssueCard : public FlowPageBase
{
    Q_OBJECT

public:
    explicit PageIssueCard(ReaderManager* manager, QWidget* parent = nullptr);

private slots:
    void onIssueCard();
    void onPersonalWrite()  { runPersonal(PersonalOp::Write); }
    void onPersonalView()   { runPersonal(PersonalOp::View); }
    void onPersonalUpdate() { runPersonal(PersonalOp::Update); }

private:
    enum class PersonalOp { Write, View, Update };

    // 一次操作用到的 Logger 與兩台讀卡機（借用 ReaderManager 的連線）
    struct Session {
        Logger lg;
        CardReader isam;
        CardReader icc;
    };
    bool openSession(Session& s);

    void runPersonal(PersonalOp op);

    // 依存取條件完成所需認證；回傳 false 代表無法滿足（已跳出提示或寫入日誌）
    bool ensureAuth(Session& s, const personal::AccessRule& rule, const QString& what);

    ReaderBar* m_isamBar = nullptr;
    ReaderBar* m_cardBar = nullptr;
    QLineEdit* m_divEdit = nullptr;    // 多樣化資料（8 bytes，HEX）
    QLineEdit* m_pinEdit = nullptr;    // B00C2 的 PIN 碼 / 認證 PIN 碼 (C2)（HEX）
    QPushButton* m_issueBtn = nullptr;
    QComboBox* m_fileCombo = nullptr;  // 個人化資料檔案
    QPushButton* m_writeBtn = nullptr;
    QPushButton* m_viewBtn = nullptr;
    QPushButton* m_updateBtn = nullptr;
};
