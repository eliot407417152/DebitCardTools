#pragma once

#include "FlowPageBase.h"

class QPushButton;
class QLineEdit;
class ReaderBar;

// ─────────────────────────────────────────────────────────────────────────────
// 頁面 3 — 洗卡
//   流程與指令內容來自 CardIssuer 專案（CardFlow / CardWasher）：
//     1~5   認證端末設備
//     6~8   建立交談基碼
//     9~11  更新製造商生產認證基碼為發卡端末認證基碼（00C3）
//     12~14 更新卡片發行亂碼基碼（00C1）
//     15~17 新增發卡單位主控基碼（00C0，不存在時自動 Create File）
//   9~17 的 Get Div Key 使用「多樣化資料（8 bytes）」，由本頁輸入。
//   讀卡機：ISAM 讀卡機 + 卡片讀卡機（需為兩台不同的讀卡機）
// ─────────────────────────────────────────────────────────────────────────────
class PageWashCard : public FlowPageBase
{
    Q_OBJECT

public:
    explicit PageWashCard(ReaderManager* manager, QWidget* parent = nullptr);

private slots:
    void onWashCard();

private:
    ReaderBar* m_isamBar = nullptr;
    ReaderBar* m_cardBar = nullptr;
    QLineEdit* m_divEdit = nullptr;   // 多樣化資料（8 bytes，HEX）
    QPushButton* m_washBtn = nullptr;
};
