#include "PageIssueCard.h"
#include "ReaderBar.h"
#include "UiHelpers.h"
#include "CardIssuer.h"
#include <QGroupBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QMessageBox>

PageIssueCard::PageIssueCard(ReaderManager* manager, QWidget* parent)
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

    // ── 發卡：00C2 Update Record Ciphered 自行輸入資料 ──────────────────────
    auto* c2Grp = new QGroupBox("發卡 — 00C2 Update Record Ciphered（自行輸入資料，HEX）", this);
    auto* c2Lay = new QHBoxLayout(c2Grp);
    m_c2DataEdit = UiHelpers::makeEditField(c2Grp, false);
    m_c2DataEdit->setPlaceholderText("請輸入 00C2 Update Record Ciphered 的 DataIn（HEX）");
    m_c2DataEdit->setMinimumWidth(360);
    c2Lay->addWidget(m_c2DataEdit);
    layout->addWidget(c2Grp);

    m_issueBtn = UiHelpers::makeActionButton("💳 發卡", "#2E7D32", this);
    layout->addWidget(m_issueBtn);
    layout->addStretch();

    connect(m_issueBtn, &QPushButton::clicked, this, &PageIssueCard::onIssueCard);
}

void PageIssueCard::onIssueCard()
{
    QString manualC2Data = m_c2DataEdit->text().trimmed().toUpper();
    if (manualC2Data.isEmpty()) {
        QMessageBox::warning(this, "資料不足", "請先輸入 00C2 Update Record Ciphered 的自行輸入資料（HEX）！");
        return;
    }

    SCARDHANDLE hIsam = 0, hCard = 0;
    if (!acquireHandles(m_isamBar, m_cardBar, "卡片", hIsam, hCard))
        return;

    log("====== 開始發卡 ======");
    CardIssuer issuer(hCard, hIsam);
    bool ok = issuer.execute(manualC2Data, logFn());

    if (ok) QMessageBox::information(this, "完成", "🎉 發卡完成！");
    else    QMessageBox::critical(this, "失敗", "❌ 發卡失敗，請查看日誌。");
}
