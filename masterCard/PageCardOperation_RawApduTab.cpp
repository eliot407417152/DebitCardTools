#include "PageCardOperation.h"
#include "ReaderManager.h"
#include "ReaderBar.h"
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QScrollArea>

// ─────────────────────────────────────────────────────────────────────────────
// Tab — Raw APDU（自訂指令）
//   原本是「卡片指令」頁籤裡的 ⑰，現在獨立成一個分類（頁籤）。
//   CLA INS P1 P2 Lc Data Le 全部自行輸入，用於任意指令測試。
//   本頁自己有一個讀卡機選擇列（ReaderBar），不再依賴「卡片指令」頁籤的選擇。
// ─────────────────────────────────────────────────────────────────────────────
void PageCardOperation::setupRawApduTab(QWidget* tab)
{
    auto* scroll = new QScrollArea(tab);
    scroll->setWidgetResizable(true);
    auto* content = new QWidget();
    auto* layout = new QVBoxLayout(content);
    layout->setSpacing(12);

    // ── 讀卡機選擇 ──
    auto* targetGrp = new QGroupBox("讀卡機選擇");
    auto* targetForm = new QFormLayout(targetGrp);
    m_rawApduReaderBar = new ReaderBar(m_readerMgr, 0, content);
    targetForm->addRow("發送對象：", m_rawApduReaderBar);
    layout->addWidget(targetGrp);

    // ── Raw APDU ──
    auto* rawGrp = new QGroupBox("Raw APDU（自訂指令）", content);
    auto* rawLay = new QVBoxLayout(rawGrp);

    auto* rawRow = new QHBoxLayout();
    auto addRawCol = [&](QLineEdit*& outField, const QString& lbl, int maxLen, int width) {
        auto* col = new QVBoxLayout();
        auto* l = new QLabel(lbl, rawGrp);
        l->setAlignment(Qt::AlignCenter);
        l->setStyleSheet("font-weight:bold; font-size:11px;");
        outField = makeHexField(rawGrp, maxLen);
        outField->setFixedWidth(width);
        col->addWidget(l);
        col->addWidget(outField);
        rawRow->addLayout(col);
        };

    addRawCol(m_rawApduFields.cla, "CLA", 2, 60);
    addRawCol(m_rawApduFields.ins, "INS", 2, 60);
    addRawCol(m_rawApduFields.p1, "P1", 2, 60);
    addRawCol(m_rawApduFields.p2, "P2", 2, 60);
    addRawCol(m_rawApduFields.lc, "Lc", 2, 60);
    addRawCol(m_rawApduFields.le, "Le（00 或 000000）", 6, 120);
    rawRow->addStretch();
    rawLay->addLayout(rawRow);

    auto* rawDataLbl = new QLabel("DATA（HEX，自行輸入）：", rawGrp);
    rawDataLbl->setStyleSheet("font-weight:bold; font-size:11px;");
    m_rawApduFields.data = makeEditField(rawGrp, false);
    m_rawApduFields.data->setPlaceholderText("輸入 DATA HEX（可留空）");
    rawLay->addWidget(rawDataLbl);
    rawLay->addWidget(m_rawApduFields.data);

    auto* rawPreviewTitle = new QLabel("完整 APDU：", rawGrp);
    rawPreviewTitle->setStyleSheet("font-weight:bold; font-size:11px;");
    m_rawApduFields.preview = new QLabel("(尚未輸入)", rawGrp);
    m_rawApduFields.preview->setWordWrap(true);
    m_rawApduFields.preview->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_rawApduFields.preview->setStyleSheet(
        "QLabel { background:#F5F5F5; color:#222; font-family:'Courier New'; "
        "font-size:12px; padding:6px; border:1px solid #CCC; border-radius:3px; }");
    rawLay->addWidget(rawPreviewTitle);
    rawLay->addWidget(m_rawApduFields.preview);

    auto* rawHint = new QLabel(
        "所有欄位皆由自行輸入，程式不會自動計算 Lc 或補值；Lc / DATA / Le 留空則不組入 APDU。\n"
        "CLA / INS / P1 / P2 為必填（各 2 個 HEX 字元）。若 Lc 與 DATA 實際長度不符，會於日誌提醒但仍會送出。",
        rawGrp);
    rawHint->setStyleSheet("color:#555; font-size:11px; padding:2px;");
    rawLay->addWidget(rawHint);

    for (QLineEdit* f : { m_rawApduFields.cla, m_rawApduFields.ins, m_rawApduFields.p1,
                          m_rawApduFields.p2, m_rawApduFields.lc, m_rawApduFields.data,
                          m_rawApduFields.le }) {
        connect(f, &QLineEdit::textChanged, this, &PageCardOperation::onRawApduPreview);
    }

    auto* rawBtn = new QPushButton("▶ 送出 Raw APDU", rawGrp);
    rawBtn->setStyleSheet(
        "QPushButton { background:#455A64; color:white; padding:8px; "
        "font-weight:bold; border-radius:4px; }");
    rawLay->addWidget(rawBtn);
    connect(rawBtn, &QPushButton::clicked, this, &PageCardOperation::onRawApdu);

    layout->addWidget(rawGrp);
    layout->addStretch();
    onRawApduPreview();   // 初始化預覽

    scroll->setWidget(content);
    auto* tabLayout = new QVBoxLayout(tab);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->addWidget(scroll);
}

// ─────────────────────────────────────────────────────────────────────────────
// Slots — Raw APDU 頁籤
// ─────────────────────────────────────────────────────────────────────────────
void PageCardOperation::onRawApduPreview()
{
    if (!m_rawApduFields.preview) return;

    QString apdu = m_rawApduFields.buildApdu();
    if (apdu.isEmpty()) {
        m_rawApduFields.preview->setText("(尚未輸入)");
        return;
    }

    // 每 2 個 HEX 字元加一個空格，方便閱讀
    QString spaced;
    for (int i = 0; i < apdu.length(); i += 2) {
        if (i > 0) spaced += ' ';
        spaced += apdu.mid(i, 2);
    }
    m_rawApduFields.preview->setText(spaced);
}

void PageCardOperation::onRawApdu()
{
    auto get = [](QLineEdit* f) {
        return f ? f->text().trimmed().remove(' ').toUpper() : QString();
        };

    // ── 欄位檢查（只擋會讓 APDU 無法組成的錯誤；Lc 與 DATA 不符僅提醒）──
    struct Req { QLineEdit* f; const char* name; };
    for (const Req& r : { Req{ m_rawApduFields.cla, "CLA" }, Req{ m_rawApduFields.ins, "INS" },
                          Req{ m_rawApduFields.p1, "P1" },   Req{ m_rawApduFields.p2, "P2" } }) {
        if (get(r.f).length() != 2) {
            log(QString("[Raw APDU] %1 須為 2 個 HEX 字元").arg(r.name), true);
            return;
        }
    }

    const QString lcV = get(m_rawApduFields.lc);
    const QString dataV = get(m_rawApduFields.data);
    const QString leV = get(m_rawApduFields.le);

    struct Opt { const QString* v; const char* name; };
    for (const Opt& o : { Opt{ &lcV, "Lc" }, Opt{ &dataV, "DATA" }, Opt{ &leV, "Le" } }) {
        if (o.v->length() % 2 != 0) {
            log(QString("[Raw APDU] %1 長度須為偶數個 HEX 字元").arg(o.name), true);
            return;
        }
    }

    if (!lcV.isEmpty() && !dataV.isEmpty()) {
        bool okLc = false;
        int lc = lcV.toInt(&okLc, 16);
        if (okLc && lc != dataV.length() / 2) {
            log(QString("⚠️ [Raw APDU] Lc=%1h（%2）與 DATA 實際長度 %3 bytes 不符，仍照輸入送出")
                .arg(lcV).arg(lc).arg(dataV.length() / 2));
        }
    }
    else if (lcV.isEmpty() != dataV.isEmpty()) {
        log("⚠️ [Raw APDU] Lc 與 DATA 其中一個為空，仍照輸入送出");
    }

    QString apdu = m_rawApduFields.buildApdu();

    log("====== Raw APDU ======");

    bool ok = false;
    QString resp = CardUtils::sendApdu(m_rawApduReaderBar ? m_rawApduReaderBar->handle() : 0, apdu, ok,
        [this](const QString& msg, bool err) { log(msg, err); });
    if (!ok) { log("[Raw APDU] 通訊失敗", true); return; }

    if (resp.length() < 4) {
        log("[Raw APDU] 回應長度不足：" + resp, true);
        return;
    }

    QString sw = resp.right(4);
    QString data = resp.left(resp.length() - 4);
    log("Data: " + (data.isEmpty() ? QString("(空)") : data));
    if (sw == "9000")
        log("✔ Raw APDU 完成 SW=9000");
    else
        log("⚠️ Raw APDU SW=" + sw, true);
}