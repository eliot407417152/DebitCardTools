#include "PageWashCard.h"
#include "ReaderBar.h"
#include "UiHelpers.h"
#include "CardReader.h"
#include "CardWasher.h"
#include <QGroupBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QMessageBox>
#include <QRegularExpression>
#include <QRegularExpressionValidator>

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

    // ── 多樣化資料 ──────────────────────────────────────────────────────────
    auto* divGrp = new QGroupBox("洗卡 — 多樣化資料", this);
    auto* divForm = new QFormLayout(divGrp);
    m_divEdit = UiHelpers::makeEditField(divGrp, false);
    m_divEdit->setPlaceholderText("8 bytes（16 個十六進位字元），例如 0011223344556677");
    m_divEdit->setValidator(new QRegularExpressionValidator(QRegularExpression("[0-9A-Fa-f ]*"), m_divEdit));
    m_divEdit->setClearButtonEnabled(true);
    m_divEdit->setMinimumWidth(360);
    divForm->addRow("多樣化資料：", m_divEdit);
    layout->addWidget(divGrp);

    auto* noteLbl = new QLabel(
        "洗卡流程：認證端末設備 → 建立交談基碼 → 更新 00C3 → 更新 00C1 → 新增 00C0。\n"
        "00C3 / 00C1 / 00C0 的 Get Div Key 都使用上方的多樣化資料。", this);
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
    Bytes div;
    if (!readHexInput(m_divEdit, "多樣化資料", 8, div))
        return;

    SCARDHANDLE hIsam = 0, hCard = 0;
    if (!acquireHandles(m_isamBar, m_cardBar, "卡片", hIsam, hCard))
        return;

    log("====== 開始洗卡 ======");

    Logger lg;
    bindLogger(lg);
    CardReader isam, icc;
    isam.setLog(&lg, "[ISAM]");
    icc.setLog(&lg, "[CARD]");
    isam.attach(hIsam);   // 借用 ReaderManager 的連線，不在此斷線
    icc.attach(hCard);

    CardWasher washer(isam, icc, lg);
    const bool ok = washer.authTerminal()
        && washer.createSessionKey()
        && washer.updateIssuerAuthKey(div)
        && washer.updateCardRandomKey(div)
        && washer.addMasterKey(div);

    if (ok) {
        log("🎉 ====== ✔ 洗卡完成 ======");
        QMessageBox::information(this, "完成", "🎉 洗卡完成！");
    }
    else {
        QMessageBox::critical(this, "失敗",
            "❌ 洗卡失敗：" + QString::fromStdString(washer.lastError()) + "\n\n詳細內容請查看日誌。");
    }
}
