#pragma once

#include "FlowPageBase.h"

class QPushButton;
class QLabel;
class ReaderBar;
class CustomerDataForm;

// ─────────────────────────────────────────────────────────────────────────────
// 頁面 1 — 製作母卡
//   一次完成：ISAM 寫入 Exchange Key → 母卡認證 → 寫入母卡資料（C8/C9/0001/0002）
//   讀卡機：ISAM 讀卡機 + 母卡讀卡機（需為兩台不同的讀卡機）
// ─────────────────────────────────────────────────────────────────────────────
class PageMakeMaster : public FlowPageBase
{
    Q_OBJECT

public:
    explicit PageMakeMaster(ReaderManager* manager, QWidget* parent = nullptr);

private slots:
    void onLoadFile();
    void onClearFields();
    void onFieldsChanged();
    void onMakeMaster();

private:
    ReaderBar* m_isamBar = nullptr;
    ReaderBar* m_masterBar = nullptr;
    CustomerDataForm* m_form = nullptr;
    QPushButton* m_loadBtn = nullptr;
    QPushButton* m_clearBtn = nullptr;
    QPushButton* m_makeBtn = nullptr;
    QLabel* m_fileLabel = nullptr;
};
