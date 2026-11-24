#include "PageIssueCard.h"
#include "ReaderBar.h"
#include "UiHelpers.h"
#include "CardIssuing.h"
#include "CardPersonal.h"
#include "PersonalDialogs.h"
#include <QGroupBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QMessageBox>
#include <QRegularExpression>
#include <QRegularExpressionValidator>

namespace {
QLineEdit* makeHexEdit(QWidget* parent, const QString& placeholder)
{
    auto* e = UiHelpers::makeEditField(parent, false);
    e->setPlaceholderText(placeholder);
    e->setValidator(new QRegularExpressionValidator(QRegularExpression("[0-9A-Fa-f ]*"), e));
    e->setClearButtonEnabled(true);
    e->setMinimumWidth(360);
    return e;
}
} // namespace

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

    // ── 發卡輸入 ────────────────────────────────────────────────────────────
    auto* inGrp = new QGroupBox("發卡 — 輸入資料", this);
    auto* inForm = new QFormLayout(inGrp);
    m_divEdit = makeHexEdit(inGrp, "8 bytes（16 個十六進位字元），例如 0011223344556677");
    m_pinEdit = makeHexEdit(inGrp, "例如 FFFFFFFFFFFFFFFF（B00C2 寫入 1~250 bytes；認證 PIN 碼 C2 須為 8 bytes）");
    inForm->addRow("多樣化資料：", m_divEdit);
    inForm->addRow("PIN 碼（B00C2 / C2）：", m_pinEdit);
    layout->addWidget(inGrp);

    auto* noteLbl = new QLabel(
        "發卡流程：認證端末設備 (C3) → 建立交談基碼 → 更新 C3 / C1 / C0 → 新增 B00C2、B00C4、B00C5、"
        "B00C6、B00D6、B00D7、B00C8 → 建立個人化資料檔案。\n"
        "個人化資料的內容請在下方「個人化資料」區塊逐欄寫入。", this);
    noteLbl->setWordWrap(true);
    noteLbl->setStyleSheet("color:#555; font-size:11px; padding:4px;");
    layout->addWidget(noteLbl);

    m_issueBtn = UiHelpers::makeActionButton("💳 發卡", "#2E7D32", this);
    layout->addWidget(m_issueBtn);

    // ── 個人化資料 ──────────────────────────────────────────────────────────
    auto* perGrp = new QGroupBox("個人化資料（寫入 / 查看 / 更新）", this);
    auto* perLay = new QHBoxLayout(perGrp);
    m_fileCombo = new QComboBox(perGrp);
    for (const auto& f : personal::allFiles()) {
        QString text = QString::fromStdString(f.name);
        if (!f.title.empty()) text += "  " + QString::fromStdString(f.title);
        m_fileCombo->addItem(text, QString::fromStdString(f.name));
    }
    m_writeBtn  = UiHelpers::makeActionButton("✍ 寫入", "#1565C0", perGrp);
    m_viewBtn   = UiHelpers::makeActionButton("👁 查看", "#6A1B9A", perGrp);
    m_updateBtn = UiHelpers::makeActionButton("🔄 更新", "#EF6C00", perGrp);
    perLay->addWidget(new QLabel("檔案：", perGrp));
    perLay->addWidget(m_fileCombo, 1);
    perLay->addWidget(m_writeBtn);
    perLay->addWidget(m_viewBtn);
    perLay->addWidget(m_updateBtn);
    layout->addWidget(perGrp);

    auto* perNote = new QLabel(
        "讀取不需認證；寫入 / 更新前會依該檔案的存取條件自動完成認證（C3 / C0 / C4 / C2）。\n"
        "需要 C2 認證的檔案（例如 0002 更新）會使用上方的 PIN 碼（8 bytes）。"
        "1080 / 1090 / 1102 / 1103 只能讀取。", this);
    perNote->setWordWrap(true);
    perNote->setStyleSheet("color:#555; font-size:11px; padding:4px;");
    layout->addWidget(perNote);
    layout->addStretch();

    connect(m_issueBtn,  &QPushButton::clicked, this, &PageIssueCard::onIssueCard);
    connect(m_writeBtn,  &QPushButton::clicked, this, &PageIssueCard::onPersonalWrite);
    connect(m_viewBtn,   &QPushButton::clicked, this, &PageIssueCard::onPersonalView);
    connect(m_updateBtn, &QPushButton::clicked, this, &PageIssueCard::onPersonalUpdate);
}

bool PageIssueCard::openSession(Session& s)
{
    SCARDHANDLE hIsam = 0, hCard = 0;
    if (!acquireHandles(m_isamBar, m_cardBar, "卡片", hIsam, hCard))
        return false;

    bindLogger(s.lg);
    s.isam.setLog(&s.lg, "[ISAM]");
    s.icc.setLog(&s.lg, "[CARD]");
    s.isam.attach(hIsam);   // 借用 ReaderManager 的連線，不在此斷線
    s.icc.attach(hCard);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 發卡（一鍵）
// ─────────────────────────────────────────────────────────────────────────────
void PageIssueCard::onIssueCard()
{
    Bytes div, pin;
    if (!readHexInput(m_divEdit, "多樣化資料", 8, div)) return;
    if (!readHexInput(m_pinEdit, "PIN 碼", -1, pin))    return;
    if (pin.size() > 250) {
        QMessageBox::warning(this, "資料長度錯誤", "「PIN 碼」長度必須是 1~250 bytes。");
        return;
    }

    Session s;
    if (!openSession(s)) return;

    log("====== 開始發卡 ======");
    CardIssuing issuing(s.isam, s.icc, s.lg);

    bool ok = issuing.authTerminal()           // C3 認證
        && issuing.createSessionKey()
        && issuing.updateC3(div)
        && issuing.updateC1(div)
        && issuing.updateC0(div);

    // 發卡基碼 + 個人化資料檔案：依 CardIssuing::extraEntries() 的順序逐項新增
    const auto& entries = CardIssuing::extraEntries();
    for (size_t i = 0; ok && i < entries.size(); ++i) {
        const auto& e = entries[i];
        Bytes input;                                          // WorkingKey / CreateOnly 不需要輸入
        if (e.kind == CardIssuing::Kind::DivKey) input = div;
        if (e.kind == CardIssuing::Kind::Pin)    input = pin;
        ok = issuing.addExtra(i, input);
    }

    if (ok) {
        log("🎉 ====== ✔ 發卡完成 ======");
        QMessageBox::information(this, "完成",
            "🎉 發卡完成！\n接著可在下方「個人化資料」寫入持卡人資料。");
    }
    else {
        QMessageBox::critical(this, "失敗",
            "❌ 發卡失敗：" + QString::fromStdString(issuing.lastError()) + "\n\n詳細內容請查看日誌。");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 依存取條件完成認證
//   anyOf 的每一組是「全部都要完成」，任一組滿足即可。
//   C3 / C0 / C4 不需輸入；C2 需要 8 bytes 的 PIN 碼（來自 PIN 欄位）。
//   優先挑不需要 C2 的組合，避免不必要的 PIN 錯誤計次。
// ─────────────────────────────────────────────────────────────────────────────
bool PageIssueCard::ensureAuth(Session& s, const personal::AccessRule& rule, const QString& what)
{
    using personal::Auth;

    if (rule.never) {
        QMessageBox::warning(this, "不允許", what + "：此檔案不允許這個動作。");
        return false;
    }
    if (rule.anyOf.empty()) return true;   // FREE

    Bytes pin8;
    const bool havePin = fromHex(m_pinEdit->text().toStdString(), pin8) && pin8.size() == 8;

    const std::vector<Auth>* chosen = nullptr;
    for (int pass = 0; pass < 2 && !chosen; ++pass) {         // pass 0：不含 C2；pass 1：含 C2
        for (const auto& group : rule.anyOf) {
            bool usesC2 = false;
            for (Auth a : group) usesC2 = usesC2 || a == Auth::C2;
            if (usesC2 != (pass == 1)) continue;
            if (usesC2 && !havePin) continue;
            chosen = &group;
            break;
        }
    }
    if (!chosen) {
        QMessageBox::warning(this, "需要 PIN 碼",
            what + "：需要認證 " + QString::fromStdString(personal::describe(rule)) +
            "。\n請先在上方輸入 8 bytes 的 PIN 碼。");
        return false;
    }

    CardIssuing issuing(s.isam, s.icc, s.lg);
    for (Auth a : *chosen) {
        bool ok = false;
        switch (a) {
        case Auth::C3: ok = issuing.authTerminal();       break;
        case Auth::C0: ok = issuing.authTerminalC0();     break;
        case Auth::C4: ok = issuing.authTerminalC4();     break;
        case Auth::C2: ok = issuing.verifyPin(pin8);      break;
        }
        if (!ok) {
            QMessageBox::critical(this, "認證失敗",
                what + "：認證 " + QString::fromStdString(personal::authName(a)) + " 失敗：" +
                QString::fromStdString(issuing.lastError()));
            return false;
        }
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 個人化資料：寫入 / 查看 / 更新
// ─────────────────────────────────────────────────────────────────────────────
void PageIssueCard::runPersonal(PersonalOp op)
{
    const QString fileName = m_fileCombo->currentData().toString();
    const personal::FileDef* def = personal::findFile(fileName.toStdString());
    if (!def) return;

    static const char* opNames[] = { "寫入", "查看", "更新" };
    const QString title = QString("%1個人化資料 %2").arg(QString::fromUtf8(opNames[static_cast<int>(op)]), fileName);

    // 先把輸入收齊（取消就不碰卡片）
    std::vector<CardPersonal::RecordInput> inputs;
    if (op != PersonalOp::View) {
        RecordDialog dlg(*def, op == PersonalOp::Write ? RecordDialog::Mode::Write : RecordDialog::Mode::Update, this);
        dlg.setWindowTitle(title);
        if (dlg.exec() != QDialog::Accepted) return;
        inputs = dlg.result();
    }

    Session s;
    if (!openSession(s)) return;

    log("##### " + title);
    if (op == PersonalOp::Write  && !ensureAuth(s, def->write,  title)) return;
    if (op == PersonalOp::Update && !ensureAuth(s, def->update, title)) return;

    CardPersonal personalFlow(s.isam, s.icc, s.lg);
    bool ok = false;
    std::vector<CardPersonal::RecordData> recs;
    Bytes raw;
    switch (op) {
    case PersonalOp::Write:  ok = personalFlow.write(*def, inputs);       break;
    case PersonalOp::Update: ok = personalFlow.update(*def, inputs);      break;
    case PersonalOp::View:   ok = personalFlow.read(*def, recs, raw);     break;
    }

    if (!ok) {
        log("##### " + title + " 失敗", true);
        QMessageBox::critical(this, "執行失敗", QString::fromStdString(personalFlow.lastError()));
        return;
    }
    log("##### " + title + " 完成");
    if (op == PersonalOp::View) showRecords(this, *def, recs);
    else QMessageBox::information(this, "執行成功", title + " 執行成功");
}
