#include "PageWashCard.h"
#include "ReaderBar.h"
#include "UiHelpers.h"
#include "CardWasher.h"
#include <QGroupBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>

PageWashCard::PageWashCard(ReaderManager* manager, QWidget* parent)
    : FlowPageBase(manager, parent)
{
    auto* layout = createContentLayout();

    // ── 讀卡機選擇：ISAM / 卡片（兩台不同）──────────────────────────────────
    auto* readerGrp = new QGroupBox("讀卡機選擇", this);
    auto* readerForm = new QFormLayout(readerGrp);
    m_isamBar = new ReaderBar(manager, 0, readerGrp);
    m_cardBar = new ReaderBar(manager, 1, readerGrp);
    readerForm->addRow("🔑 ISAM 讀卡機：", m_isamBar);
    readerForm->addRow("💳 卡片 讀卡機：", m_cardBar);
    layout->addWidget(readerGrp);

    auto* noteLbl = new QLabel(
        "洗卡流程：對「卡片」與 ISAM 建立 Session Key，並處理 00C3 / 00C1 / 00C0 三個金鑰檔案。", this);
    noteLbl->setWordWrap(true);
    noteLbl->setStyleSheet("color:#555; font-size:11px; padding:4px;");
    layout->addWidget(noteLbl);

    m_washBtn = UiHelpers::makeActionButton("🧼 洗卡", "#00838F", this);
    layout->addWidget(m_washBtn);
    layout->addStretch();

    connect(m_washBtn, &QPushButton::clicked, this, &PageWashCard::onWashCard);
}

void PageWashCard::onWashCard()
{
    SCARDHANDLE hIsam = 0, hCard = 0;
    if (!acquireHandles(m_isamBar, m_cardBar, "卡片", hIsam, hCard))
        return;

    log("====== 開始洗卡 ======");
    CardWasher washer(hCard, hIsam);
    bool ok = washer.execute(logFn());

    if (ok) QMessageBox::information(this, "完成", "🎉 洗卡完成！");
    else    QMessageBox::critical(this, "失敗", "❌ 洗卡失敗，請查看日誌。");
}
