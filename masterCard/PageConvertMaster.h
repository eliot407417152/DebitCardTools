#pragma once

#include "FlowPageBase.h"

class QPushButton;
class QLabel;
class ReaderBar;
class CustomerDataForm;

// ─────────────────────────────────────────────────────────────────────────────
// 頁面 2 — 母卡轉換
//   把製作好的母卡與銀行 ISAM 做轉換（Write Key、Verify PIN、0001/0002 金鑰區塊轉換）
//   讀卡機：ISAM 讀卡機 + 母卡讀卡機（需為兩台不同的讀卡機）
//   需要的資料：Exchange Code、CCode、Pin A、Pin B（可載入 LOD 或手動輸入）
// ─────────────────────────────────────────────────────────────────────────────
class PageConvertMaster : public FlowPageBase
{
    Q_OBJECT

public:
    explicit PageConvertMaster(ReaderManager* manager, QWidget* parent = nullptr);

private slots:
    void onLoadFile();
    void onClearFields();
    void onConvert();

private:
    ReaderBar* m_isamBar = nullptr;
    ReaderBar* m_masterBar = nullptr;
    CustomerDataForm* m_form = nullptr;
    QPushButton* m_loadBtn = nullptr;
    QPushButton* m_clearBtn = nullptr;
    QPushButton* m_convertBtn = nullptr;
    QLabel* m_fileLabel = nullptr;
};
