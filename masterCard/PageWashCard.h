#pragma once

#include "FlowPageBase.h"

class QPushButton;
class ReaderBar;

// ─────────────────────────────────────────────────────────────────────────────
// 頁面 3 — 洗卡
//   對「卡片」與銀行 ISAM 建立 Session Key，並處理 00C3 / 00C1 / 00C0 三個金鑰檔案
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
    QPushButton* m_washBtn = nullptr;
};
