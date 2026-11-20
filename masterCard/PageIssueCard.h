#pragma once

#include "FlowPageBase.h"

class QPushButton;
class QLineEdit;
class ReaderBar;

// ─────────────────────────────────────────────────────────────────────────────
// 頁面 4 — 發卡
//   包含洗卡的全部步驟，並延伸建立 00C2/00C4/00C5/00C6/00D6/00D7/00C8 應用檔案
//   與個人化資料檔案（0001/0002/1001/1002 等）的實際資料寫入
//   讀卡機：ISAM 讀卡機 + 卡片讀卡機（需為兩台不同的讀卡機）
// ─────────────────────────────────────────────────────────────────────────────
class PageIssueCard : public FlowPageBase
{
    Q_OBJECT

public:
    explicit PageIssueCard(ReaderManager* manager, QWidget* parent = nullptr);

private slots:
    void onIssueCard();

private:
    ReaderBar* m_isamBar = nullptr;
    ReaderBar* m_cardBar = nullptr;
    QLineEdit* m_c2DataEdit = nullptr;   // 00C2 Update Record Ciphered 自行輸入的 DataIn（HEX）
    QPushButton* m_issueBtn = nullptr;
};
