#include "PageCardOperation.h"
#include "IsamKeyUpdater.h"
#include "IsamKeyChecker.h"
#include "IsamKeyData.h"
#include "MasterCardUpdater.h"
#include "LodLoader.h"
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QLabel>
#include <QComboBox>
#include <QTabWidget>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QScrollArea>
#include <QRadioButton>
#include <QFile>

// ─────────────────────────────────────────────────────────────────────────────
// ApduFields::buildApdu
// ─────────────────────────────────────────────────────────────────────────────
QString ApduFields::buildApdu() const
{
    auto get = [](QLineEdit* f) {
        return f ? f->text().trimmed().remove(' ').toUpper() : QString();
        };

    QString result;
    result += get(cla).leftJustified(2, '0').left(2);
    result += get(ins).leftJustified(2, '0').left(2);
    result += get(p1).leftJustified(2, '0').left(2);
    result += get(p2).leftJustified(2, '0').left(2);

    QString lcVal = get(lc);
    QString leVal = get(le);
    QString dataVal = get(dataIn);

    if (!lcVal.isEmpty()) result += lcVal.leftJustified(2, '0').left(2);
    if (!dataVal.isEmpty()) result += dataVal;
    if (!leVal.isEmpty()) result += leVal.leftJustified(2, '0').left(2);

    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// CreateFileFields::buildDataIn — 組合 16 bytes DataIn
// ─────────────────────────────────────────────────────────────────────────────
QString CreateFileFields::buildDataIn() const
{
    auto get = [](QLineEdit* f, int bytes) -> QString {
        QString v = f ? f->text().trimmed().remove(' ').toUpper() : QString();
        int needed = bytes * 2;
        // 補 0 至需要長度，超過截斷
        return v.leftJustified(needed, '0').left(needed);
        };

    return get(efid, 2)   // #1~#2
        + get(ac1, 2)   // #3~#4
        + get(ac2, 2)   // #5~#6
        + get(ac3, 2)   // #7~#8
        + get(sob, 2)   // #9~#10
        + get(tof, 1)   // #11
        + get(sor, 1)   // #12
        + get(rc, 1)   // #13
        + get(index, 1)   // #14
        + get(tok, 1)   // #15
        + get(rfu, 1);  // #16
}

// CreateFileFields::buildApdu — Lc 固定 10h (16 bytes)
QString CreateFileFields::buildApdu() const
{
    auto get = [](QLineEdit* f) {
        return f ? f->text().trimmed().remove(' ').toUpper() : QString();
        };

    QString data = buildDataIn();
    QString lc = QString("%1").arg(data.length() / 2, 2, 16, QChar('0')).toUpper();

    return get(cla).leftJustified(2, '0').left(2)
        + get(ins).leftJustified(2, '0').left(2)
        + get(p1).leftJustified(2, '0').left(2)
        + get(p2).leftJustified(2, '0').left(2)
        + lc
        + data;
}

// ─────────────────────────────────────────────────────────────────────────────
// VerifyPinFields — computedP2 / buildApdu
//   P2 = Pq 邏輯左移 3 位（例如輸入 02 → 10h）
// ─────────────────────────────────────────────────────────────────────────────
QString VerifyPinFields::computedP2() const
{
    QString raw = pq ? pq->text().trimmed().remove(' ').toUpper() : QString();
    bool ok = false;
    unsigned int val = raw.left(2).toUInt(&ok, 16);
    if (!ok) val = 0;

    unsigned int shifted = (val << 3) & 0xFF;   // 邏輯左移 3 位，取低 8 bits
    return QString("%1").arg(shifted, 2, 16, QChar('0')).toUpper();
}

QString VerifyPinFields::buildApdu() const
{
    auto get = [](QLineEdit* f) {
        return f ? f->text().trimmed().remove(' ').toUpper() : QString();
        };

    QString claV = get(cla).leftJustified(2, '0').left(2);
    QString insV = get(ins).leftJustified(2, '0').left(2);
    QString p1V = get(p1).leftJustified(2, '0').left(2);
    QString p2V = computedP2();

    // DATA：PIN code，固定 8 bytes（16 HEX），不足補 FF
    QString pinV = get(pin).leftJustified(16, 'F').left(16);
    QString lcV = QString("%1").arg(pinV.length() / 2, 2, 16, QChar('0')).toUpper(); // 固定 08

    if (p2Display) p2Display->setText(p2V);
    if (lc)        lc->setText(lcV);

    return claV + insV + p1V + p2V + lcV + pinV;
}

// ─────────────────────────────────────────────────────────────────────────────
// TerminalAuthFields（卡片指令 — Terminal Authentication）
//   P2 邏輯左移 3 位運算比照 Verify Pin，但 DATA 固定 16 bytes（32 HEX），Lc = 10h
//   注意：Kq 為「十進位」輸入（規格範圍 16~31），需先以十進位解析成數值，
//         再做邏輯左移 3 位，而不是把輸入字串當十六進位解析！
//         例如輸入 16（十進位）→ 左移 3 位 = 128（十進位）= 80h
// ─────────────────────────────────────────────────────────────────────────────
QString TerminalAuthFields::computedP2() const
{
    QString raw = pq ? pq->text().trimmed().remove(' ') : QString();
    bool ok = false;
    unsigned int val = raw.toUInt(&ok, 10);   // 十進位解析（非十六進位）
    if (!ok) val = 0;

    unsigned int shifted = (val << 3) & 0xFF;   // 邏輯左移 3 位，取低 8 bits
    return QString("%1").arg(shifted, 2, 16, QChar('0')).toUpper();
}

QString TerminalAuthFields::buildApdu() const
{
    auto get = [](QLineEdit* f) {
        return f ? f->text().trimmed().remove(' ').toUpper() : QString();
        };

    QString claV = get(cla).leftJustified(2, '0').left(2);
    QString insV = get(ins).leftJustified(2, '0').left(2);
    QString p1V = get(p1).leftJustified(2, '0').left(2);
    QString p2V = computedP2();

    // DATA：不自動補 FF，Lc 依實際輸入長度計算（HEX 字元數需為偶數）
    QString pinV = get(pin);
    QString lcV = QString("%1").arg(pinV.length() / 2, 2, 16, QChar('0')).toUpper();

    if (p2Display) p2Display->setText(p2V);
    if (lc)        lc->setText(lcV);

    return claV + insV + p1V + p2V + lcV + pinV;
}


QString ReadRecordFields::buildApdu() const
{
    auto get = [](QLineEdit* f) {
        return f ? f->text().trimmed().remove(' ').toUpper() : QString();
        };

    QString claV = get(cla).leftJustified(2, '0').left(2);
    QString insV = get(ins).leftJustified(2, '0').left(2);
    QString p1V = get(recN).leftJustified(2, '0').left(2);   // Record Id
    QString p2V = get(p2).leftJustified(2, '0').left(2);     // 04 或 05（唯讀）

    // Le：依欄位 maxLength 決定要補齊/截斷成幾個 HEX 字元
    //   Read Record     → 2 個 HEX 字元（預設 00）
    //   Read All Record → 6 個 HEX 字元（預設 00 00 00）
    int leLen = (le && le->maxLength() > 0 && le->maxLength() <= 6) ? le->maxLength() : 2;
    QString leV = get(le).leftJustified(leLen, '0').left(leLen);

    return claV + insV + p1V + p2V + leV;
}

// ─────────────────────────────────────────────────────────────────────────────
// IsamSimpleFields::buildApdu — CLA INS P1 P2 (+ Le 可選)，無 Lc、無 DataIn
// ─────────────────────────────────────────────────────────────────────────────
QString IsamSimpleFields::buildApdu() const
{
    auto get = [](QLineEdit* f) {
        return f ? f->text().trimmed().remove(' ').toUpper() : QString();
        };

    QString result;
    result += get(cla).leftJustified(2, '0').left(2);
    result += get(ins).leftJustified(2, '0').left(2);
    result += get(p1).leftJustified(2, '0').left(2);
    result += get(p2).leftJustified(2, '0').left(2);

    QString leVal = get(le);
    if (!leVal.isEmpty()) result += leVal.leftJustified(2, '0').left(2);

    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// WriteKeyFields — buildDataIn / buildApdu
//   DataIn = EKey + CCode + Limit，Lc 依 DataIn 長度自動計算
// ─────────────────────────────────────────────────────────────────────────────
QString WriteKeyFields::buildDataIn() const
{
    auto get = [](QLineEdit* f) {
        return f ? f->text().trimmed().remove(' ').toUpper() : QString();
        };
    return get(eKey) + get(ccode) + get(limit);
}

QString WriteKeyFields::buildApdu() const
{
    auto get = [](QLineEdit* f) {
        return f ? f->text().trimmed().remove(' ').toUpper() : QString();
        };

    QString data = buildDataIn();
    QString lc = QString("%1").arg(data.length() / 2, 2, 16, QChar('0')).toUpper();

    return get(cla).leftJustified(2, '0').left(2)
        + get(ins).leftJustified(2, '0').left(2)
        + get(p1).leftJustified(2, '0').left(2)
        + get(p2).leftJustified(2, '0').left(2)
        + lc
        + data;
}

// ─────────────────────────────────────────────────────────────────────────────
// StoreManufacturerKeyFields — buildDataIn / buildApdu
//   DataIn = KT + EKey + CCode + Limit，Lc 依 DataIn 長度自動計算
// ─────────────────────────────────────────────────────────────────────────────
QString StoreManufacturerKeyFields::buildDataIn() const
{
    auto get = [](QLineEdit* f) {
        return f ? f->text().trimmed().remove(' ').toUpper() : QString();
        };
    return get(kt) + get(eKey) + get(ccode) + get(limit);
}

QString StoreManufacturerKeyFields::buildApdu() const
{
    auto get = [](QLineEdit* f) {
        return f ? f->text().trimmed().remove(' ').toUpper() : QString();
        };

    QString data = buildDataIn();
    QString lc = QString("%1").arg(data.length() / 2, 2, 16, QChar('0')).toUpper();

    return get(cla).leftJustified(2, '0').left(2)
        + get(ins).leftJustified(2, '0').left(2)
        + get(p1).leftJustified(2, '0').left(2)
        + get(p2).leftJustified(2, '0').left(2)
        + lc
        + data;
}

// ─────────────────────────────────────────────────────────────────────────────
// SetSessionKeyIsamFields — buildDataIn / buildApdu
//   DataIn = Div-Data + R2，Lc = DL + 8（依 DataIn 長度自動計算），無 Le
// ─────────────────────────────────────────────────────────────────────────────
QString SetSessionKeyIsamFields::buildDataIn() const
{
    auto get = [](QLineEdit* f) {
        return f ? f->text().trimmed().remove(' ').toUpper() : QString();
        };
    QString r2V = get(r2).leftJustified(16, 'F').left(16);   // R2 固定 8 bytes
    return get(divData) + r2V;
}

QString SetSessionKeyIsamFields::buildApdu() const
{
    auto get = [](QLineEdit* f) {
        return f ? f->text().trimmed().remove(' ').toUpper() : QString();
        };

    QString data = buildDataIn();
    QString lc = QString("%1").arg(data.length() / 2, 2, 16, QChar('0')).toUpper();

    return get(cla).leftJustified(2, '0').left(2)
        + get(ins).leftJustified(2, '0').left(2)
        + get(p1).leftJustified(2, '0').left(2)
        + get(p2).leftJustified(2, '0').left(2)
        + lc
        + data;
}

// ─────────────────────────────────────────────────────────────────────────────
// DiversifyKeyFields — buildApdu
//   Data = Div-Data，Lc 依 Div-Data 長度自動計算，Le 固定 00
// ─────────────────────────────────────────────────────────────────────────────
QString DiversifyKeyFields::buildApdu() const
{
    auto get = [](QLineEdit* f) {
        return f ? f->text().trimmed().remove(' ').toUpper() : QString();
        };

    QString data = get(divData);
    QString lc = QString("%1").arg(data.length() / 2, 2, 16, QChar('0')).toUpper();
    QString leV = get(le).isEmpty() ? QString("00") : get(le).leftJustified(2, '0').left(2);

    return get(cla).leftJustified(2, '0').left(2)
        + get(ins).leftJustified(2, '0').left(2)
        + get(p1).leftJustified(2, '0').left(2)
        + get(p2).leftJustified(2, '0').left(2)
        + lc
        + data
        + leV;
}

// ─────────────────────────────────────────────────────────────────────────────
// SetSessionKeyCardFields — computedP2 / buildApdu
//   P2 = Kq 邏輯左移 3 位（比照 Verify Pin 作法）；Data = R1（8 bytes），Lc 固定 08，Le 固定 08
//   注意：Kq 為「十進位」輸入（規格範圍 16~31），需先以十進位解析成數值，
//         再做邏輯左移 3 位，而不是把輸入字串當十六進位解析！
// ─────────────────────────────────────────────────────────────────────────────
QString SetSessionKeyCardFields::computedP2() const
{
    QString raw = pq ? pq->text().trimmed().remove(' ') : QString();
    bool ok = false;
    unsigned int val = raw.toUInt(&ok, 10);   // 十進位解析（非十六進位）
    if (!ok) val = 0;

    unsigned int shifted = (val << 3) & 0xFF;   // 邏輯左移 3 位，取低 8 bits
    return QString("%1").arg(shifted, 2, 16, QChar('0')).toUpper();
}

QString SetSessionKeyCardFields::buildApdu() const
{
    auto get = [](QLineEdit* f) {
        return f ? f->text().trimmed().remove(' ').toUpper() : QString();
        };

    QString claV = get(cla).leftJustified(2, '0').left(2);
    QString insV = get(ins).leftJustified(2, '0').left(2);
    QString p1V = get(p1).leftJustified(2, '0').left(2);
    QString p2V = computedP2();

    // DATA：Random R1，固定 8 bytes（16 HEX），不足補 FF
    QString r1V = get(r1).leftJustified(16, 'F').left(16);
    QString lcV = QString("%1").arg(r1V.length() / 2, 2, 16, QChar('0')).toUpper();   // 固定 08
    QString leV = get(le).isEmpty() ? QString("08") : get(le).leftJustified(2, '0').left(2);

    if (p2Display) p2Display->setText(p2V);
    if (lc)        lc->setText(lcV);

    return claV + insV + p1V + p2V + lcV + r1V + leV;
}

// ─────────────────────────────────────────────────────────────────────────────
// UpdateRecordCipheredFields — buildApdu
//   DATA 為自行輸入，Lc 依 DATA 長度自動計算（08/10/18）
// ─────────────────────────────────────────────────────────────────────────────
QString UpdateRecordCipheredFields::buildApdu() const
{
    auto get = [](QLineEdit* f) {
        return f ? f->text().trimmed().remove(' ').toUpper() : QString();
        };

    QString dataV = get(data);
    QString lc = QString("%1").arg(dataV.length() / 2, 2, 16, QChar('0')).toUpper();

    return get(cla).leftJustified(2, '0').left(2)
        + get(ins).leftJustified(2, '0').left(2)
        + get(p1).leftJustified(2, '0').left(2)
        + get(p2).leftJustified(2, '0').left(2)
        + lc
        + dataV;
}

// ─────────────────────────────────────────────────────────────────────────────
// 建構子
//【已依需求調整】不再由上一頁預先指定 ISAM/母卡 handle，
//   改為接收 context + 讀卡機名稱清單，實際連線改為各功能區塊使用到時才建立。
// ─────────────────────────────────────────────────────────────────────────────
PageCardOperation::PageCardOperation(SCARDCONTEXT context, const QStringList& readers, QWidget* parent)
    : QWidget(parent), m_context(context), m_readers(readers)
{
    m_canMakeMaster = (m_readers.size() >= 2);

    setupUI();

    log(QString("✔ 偵測到 %1 台讀卡機：%2").arg(m_readers.size()).arg(m_readers.join("、")));
    if (m_canMakeMaster)
        log("讀卡機數量足夠（≥2 台），「製卡發卡流程」製作母卡功能已啟用。");
    else
        log("讀卡機數量不足 2 台，「製卡發卡流程」製作母卡功能已停用。");
}

PageCardOperation::~PageCardOperation()
{
    for (auto it = m_handleCache.begin(); it != m_handleCache.end(); ++it) {
        if (it.value())
            SCardDisconnect(it.value(), SCARD_LEAVE_CARD);
    }
    m_handleCache.clear();
}

// ─────────────────────────────────────────────────────────────────────────────
// 讀卡機連線輔助：依名稱取得已快取的連線，若尚未連線則現場建立
// ─────────────────────────────────────────────────────────────────────────────
SCARDHANDLE PageCardOperation::connectReader(const QString& readerName) const
{
    if (readerName.isEmpty()) {
        const_cast<PageCardOperation*>(this)->log("Error: 尚未選擇讀卡機", true);
        return 0;
    }

    auto it = m_handleCache.find(readerName);
    if (it != m_handleCache.end() && it.value() != 0)
        return it.value();

    DWORD proto = 0;
    SCARDHANDLE h = 0;
    LONG ret = SCardConnectA(m_context,
        readerName.toLocal8Bit().constData(),
        SCARD_SHARE_SHARED,
        SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
        &h, &proto);

    if (ret != SCARD_S_SUCCESS) {
        const_cast<PageCardOperation*>(this)->log(
            QString("SCardConnect [%1] 失敗: 0x%2").arg(readerName).arg(ret, 0, 16), true);
        return 0;
    }

    m_handleCache.insert(readerName, h);
    const_cast<PageCardOperation*>(this)->log(QString("✔ 已連線讀卡機：%1").arg(readerName));
    return h;
}

// ─────────────────────────────────────────────────────────────────────────────
// 主 UI
// ─────────────────────────────────────────────────────────────────────────────
void PageCardOperation::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* btnRow = new QHBoxLayout();
    auto* backBtn = new QPushButton("⬅️ 返回", this);
    backBtn->setStyleSheet(
        "QPushButton { background:#757575; color:white; padding:8px 15px; "
        "font-weight:bold; border-radius:4px; }");
    connect(backBtn, &QPushButton::clicked, this, &PageCardOperation::backRequested);
    btnRow->addWidget(backBtn);

    QString modeText = QString("讀卡機：%1 台偵測到 %2")
        .arg(m_readers.size())
        .arg(m_canMakeMaster ? "（🔮 可製作母卡）" : "（🛠 僅一般操作，母卡功能需 ≥2 台讀卡機）");
    auto* modeLbl = new QLabel(modeText, this);
    modeLbl->setStyleSheet("font-weight:bold; color:#555; padding-left:12px;");
    btnRow->addWidget(modeLbl);

    btnRow->addStretch();
    mainLayout->addLayout(btnRow);

    auto* bodyRow = new QHBoxLayout();

    auto* tabWidget = new QTabWidget(this);
    tabWidget->setStyleSheet(
        "QTabBar::tab { padding:8px 16px; font-weight:bold; }"
        "QTabBar::tab:selected { background:#0078D4; color:white; }");

    auto* isamTab = new QWidget();  setupIsamTab(isamTab);
    auto* isamCmdTab = new QWidget();  setupIsamCmdTab(isamCmdTab);
    auto* lodTab = new QWidget();  setupLodTab(lodTab);
    auto* apduTab = new QWidget();  setupApduTab(apduTab);

    tabWidget->addTab(isamTab, "🔑 ISAM 金鑰");
    tabWidget->addTab(isamCmdTab, "🔧 ISAM 指令");
    tabWidget->addTab(apduTab, "📡 卡片指令");
    tabWidget->addTab(lodTab, "📇 製卡發卡流程");

    // 讀卡機數量不足 2 台：不提供製作母卡功能
    if (!m_canMakeMaster) {
        int lodTabIndex = tabWidget->indexOf(lodTab);
        tabWidget->setTabEnabled(lodTabIndex, false);
        tabWidget->setTabToolTip(lodTabIndex, "目前讀卡機數量不足 2 台，無法製作母卡，請插入第 2 台讀卡機後重新進入。");
        if (m_makeMasterBtn) m_makeMasterBtn->setEnabled(false);
        if (m_authBtn)       m_authBtn->setEnabled(false);
        if (m_convertBtn)    m_convertBtn->setEnabled(false);
        if (m_washCardBtn)  m_washCardBtn->setEnabled(false);
    }

    bodyRow->addWidget(tabWidget, 3);

    // 右側日誌
    auto* logGrp = new QGroupBox("通訊日誌");
    auto* logLay = new QVBoxLayout(logGrp);

    auto* logBtnRow = new QHBoxLayout();
    m_exportLogBtn = new QPushButton("💾 匯出日誌", logGrp);
    m_exportLogBtn->setStyleSheet(
        "QPushButton { background:#455A64; color:white; padding:6px 12px; "
        "font-weight:bold; border-radius:4px; }");
    logBtnRow->addStretch();
    logBtnRow->addWidget(m_exportLogBtn);
    logLay->addLayout(logBtnRow);

    m_logEdit = new QTextEdit(this);
    m_logEdit->setReadOnly(true);
    m_logEdit->setStyleSheet(
        "background:#121212; color:#00FF66; "
        "font-family:'Courier New',monospace; font-size:11px;");
    logLay->addWidget(m_logEdit);
    bodyRow->addWidget(logGrp, 2);

    connect(m_exportLogBtn, &QPushButton::clicked, this, &PageCardOperation::onExportLog);

    mainLayout->addLayout(bodyRow);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab 1 — ISAM 金鑰（金鑰本體隱藏，唯讀）
// ─────────────────────────────────────────────────────────────────────────────
void PageCardOperation::setupIsamTab(QWidget* tab)
{
    auto* layout = new QVBoxLayout(tab);

    // ── 讀卡機選擇（本頁指令要發送到哪一台讀卡機）──────────────────────────
    auto* readerGrp = new QGroupBox("讀卡機選擇");
    auto* readerForm = new QFormLayout(readerGrp);
    m_isamKeyReaderCombo = new QComboBox(tab);
    m_isamKeyReaderCombo->addItems(m_readers);
    readerForm->addRow("發送對象：", m_isamKeyReaderCombo);
    layout->addWidget(readerGrp);

    auto* selGrp = new QGroupBox("選擇 ISAM KEY");
    auto* selForm = new QFormLayout(selGrp);

    m_isamKeyCombo = new QComboBox(tab);
    m_isamKeyCombo->addItem("── 請選擇 ──", -1);
    for (const auto& e : IsamKeyData::entries())
        m_isamKeyCombo->addItem(e.displayName, e.id);
    selForm->addRow("銀行 / 機構：", m_isamKeyCombo);
    layout->addWidget(selGrp);

    // 只顯示 Code（隱藏金鑰本體），且全部唯讀
    auto* infoGrp = new QGroupBox("金鑰資訊（金鑰本體受保護，不顯示）");
    auto* infoForm = new QFormLayout(infoGrp);

    auto makeLbl = [&](QLabel*& lbl, const QString& row) {
        lbl = new QLabel("—", tab);
        lbl->setStyleSheet(
            "QLabel { background:#EEEEEE; color:#333; font-family:'Courier New'; "
            "padding:4px 8px; border:1px solid #CCC; border-radius:3px; }");
        infoForm->addRow(row, lbl);
        };
    makeLbl(m_isamC3CodeLbl, "C3 Code：");
    makeLbl(m_isamC1CodeLbl, "C1 Code：");
    layout->addWidget(infoGrp);

    // 查看 ISAM — 直接讀取卡片目前的 C3 / C1 對照碼（不需先選擇銀行）
    auto* viewGrp = new QGroupBox("目前卡片 ISAM 金鑰對照碼（讀取自 ISAM）");
    auto* viewForm = new QFormLayout(viewGrp);
    auto makeViewLbl = [&](QLabel*& lbl, const QString& row) {
        lbl = new QLabel("—", tab);
        lbl->setStyleSheet(
            "QLabel { background:#EEEEEE; color:#333; font-family:'Courier New'; "
            "padding:4px 8px; border:1px solid #CCC; border-radius:3px; }");
        viewForm->addRow(row, lbl);
        };
    makeViewLbl(m_isamViewC3Lbl, "C3 對照碼：");
    makeViewLbl(m_isamViewC1Lbl, "C1 對照碼：");
    layout->addWidget(viewGrp);

    m_viewIsamBtn = new QPushButton("🔎 查看 ISAM", tab);
    m_viewIsamBtn->setStyleSheet(
        "QPushButton { background:#37474F; color:white; padding:10px; "
        "font-weight:bold; font-size:13px; border-radius:4px; }");
    layout->addWidget(m_viewIsamBtn);

    m_updateIsamBtn = new QPushButton("⚡ 寫入 ISAM 金鑰", tab);
    m_updateIsamBtn->setStyleSheet(
        "QPushButton { background:#E65100; color:white; padding:10px; "
        "font-weight:bold; font-size:13px; border-radius:4px; }"
        "QPushButton:disabled { background:#CCC; color:#666; }");
    m_updateIsamBtn->setEnabled(false);
    layout->addWidget(m_updateIsamBtn);

    m_checkIsamKeyBtn = new QPushButton("🔍 檢查卡片 KEY", tab);
    m_checkIsamKeyBtn->setStyleSheet(
        "QPushButton { background:#00695C; color:white; padding:10px; "
        "font-weight:bold; font-size:13px; border-radius:4px; }"
        "QPushButton:disabled { background:#CCC; color:#666; }");
    m_checkIsamKeyBtn->setEnabled(false);
    layout->addWidget(m_checkIsamKeyBtn);
    layout->addStretch();

    connect(m_isamKeyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &PageCardOperation::onIsamKeySelected);
    connect(m_viewIsamBtn, &QPushButton::clicked, this, &PageCardOperation::onViewIsam);
    connect(m_updateIsamBtn, &QPushButton::clicked, this, &PageCardOperation::onUpdateIsam);
    connect(m_checkIsamKeyBtn, &QPushButton::clicked, this, &PageCardOperation::onCheckIsamKey);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab 1.5 — ISAM 指令（Select ISAM Object / Get Key Check Code / Generate Random No /
//            Get TerminalAuthCode / Get Encrypted Key / Write Key / Store Manufacturer Key）
//   本頁所有指令發送至使用者於上方選擇的讀卡機（m_isamCmdReaderCombo）。
// ─────────────────────────────────────────────────────────────────────────────
void PageCardOperation::setupIsamCmdTab(QWidget* tab)
{
    auto* scroll = new QScrollArea(tab);
    scroll->setWidgetResizable(true);
    auto* content = new QWidget();
    auto* layout = new QVBoxLayout(content);
    layout->setSpacing(12);

    // ── 讀卡機選擇（本頁指令要發送到哪一台讀卡機）──────────────────────────
    auto* readerGrp = new QGroupBox("讀卡機選擇", content);
    auto* readerForm = new QFormLayout(readerGrp);
    m_isamCmdReaderCombo = new QComboBox(content);
    m_isamCmdReaderCombo->addItems(m_readers);
    readerForm->addRow("發送對象：", m_isamCmdReaderCombo);
    layout->addWidget(readerGrp);

    auto* noteLbl = new QLabel("以下指令發送至上方選擇的讀卡機。", content);
    noteLbl->setStyleSheet("color:#555; font-size:11px; padding:4px;");
    layout->addWidget(noteLbl);

    // 共用小工具：在一列中加入一個「標籤 + HEX 欄位」直欄
    auto addCol = [this](QWidget* parent, QHBoxLayout* row, QLineEdit*& outField,
        const QString& lbl, const QString& def, int maxLen, bool ro) {
            auto* col = new QVBoxLayout();
            auto* l = new QLabel(lbl, parent);
            l->setAlignment(Qt::AlignCenter);
            l->setStyleSheet("font-weight:bold; font-size:11px;");
            outField = makeHexField(parent, maxLen);
            outField->setText(def);
            outField->setReadOnly(ro);
            if (ro) {
                outField->setStyleSheet(
                    "QLineEdit { background:#EEE; color:#555; font-family:'Courier New'; "
                    "font-weight:bold; font-size:12px; padding:4px; "
                    "border:1px dashed #AAA; border-radius:3px; }");
            }
            col->addWidget(l);
            col->addWidget(outField);
            row->addLayout(col);
        };

    // ── ① Select ISAM Object ──────────────────────────────────────────────
    auto* selGrp = new QGroupBox("① Select ISAM Object", content);
    auto* selLay = new QVBoxLayout(selGrp);
    auto* selRow = new QHBoxLayout();
    addCol(selGrp, selRow, m_isamSelectObjFields.cla, "CLA", "82", 2, true);
    addCol(selGrp, selRow, m_isamSelectObjFields.ins, "INS", "A4", 2, true);
    addCol(selGrp, selRow, m_isamSelectObjFields.p1, "P1", "00", 2, true);
    addCol(selGrp, selRow, m_isamSelectObjFields.p2, "P2", "00", 2, true);
    addCol(selGrp, selRow, m_isamSelectObjFields.lc, "Lc", "02", 2, true);
    addCol(selGrp, selRow, m_isamSelectObjFields.dataIn, "DataIn", "", 4, false);
    addCol(selGrp, selRow, m_isamSelectObjFields.le, "Le", "0E", 2, true);
    selRow->addStretch();
    selLay->addLayout(selRow);
    auto* selHint = new QLabel("DataIn：4 個 HEX 字元（2 bytes），請自行輸入。", selGrp);
    selHint->setStyleSheet("color:#555; font-size:11px; padding:2px;");
    selLay->addWidget(selHint);
    auto* selBtn = new QPushButton("▶ 執行 Select ISAM Object", selGrp);
    selBtn->setStyleSheet(
        "QPushButton { background:#00897B; color:white; padding:8px; "
        "font-weight:bold; border-radius:4px; }");
    selLay->addWidget(selBtn);
    layout->addWidget(selGrp);
    connect(selBtn, &QPushButton::clicked, this, &PageCardOperation::onIsamSelectObject);

    // ── ② Get Key Check Code ──────────────────────────────────────────────
    auto* kccGrp = new QGroupBox("② Get Key Check Code", content);
    auto* kccLay = new QVBoxLayout(kccGrp);
    auto* kccRow = new QHBoxLayout();
    addCol(kccGrp, kccRow, m_isamKeyCheckFields.cla, "CLA", "82", 2, true);
    addCol(kccGrp, kccRow, m_isamKeyCheckFields.ins, "INS", "07", 2, true);
    addCol(kccGrp, kccRow, m_isamKeyCheckFields.p1, "P1", "", 2, false);
    addCol(kccGrp, kccRow, m_isamKeyCheckFields.p2, "P2", "00", 2, true);
    addCol(kccGrp, kccRow, m_isamKeyCheckFields.le, "Le", "07", 2, false);
    kccRow->addStretch();
    kccLay->addLayout(kccRow);
    auto* kccHint = new QLabel("P1：自行輸入。Le 預設 07，亦可自行修改。", kccGrp);
    kccHint->setStyleSheet("color:#555; font-size:11px; padding:2px;");
    kccLay->addWidget(kccHint);
    auto* kccBtn = new QPushButton("▶ 執行 Get Key Check Code", kccGrp);
    kccBtn->setStyleSheet(
        "QPushButton { background:#6A1B9A; color:white; padding:8px; "
        "font-weight:bold; border-radius:4px; }");
    kccLay->addWidget(kccBtn);
    layout->addWidget(kccGrp);
    connect(kccBtn, &QPushButton::clicked, this, &PageCardOperation::onIsamGetKeyCheckCode);

    // ── ③ Generate Random No ──────────────────────────────────────────────
    auto* rndGrp = new QGroupBox("③ Generate Random No", content);
    auto* rndLay = new QVBoxLayout(rndGrp);
    auto* rndRow = new QHBoxLayout();
    addCol(rndGrp, rndRow, m_isamRandomFields.cla, "CLA", "8F", 2, true);
    addCol(rndGrp, rndRow, m_isamRandomFields.ins, "INS", "86", 2, true);
    addCol(rndGrp, rndRow, m_isamRandomFields.p1, "P1", "00", 2, true);
    addCol(rndGrp, rndRow, m_isamRandomFields.p2, "P2", "00", 2, true);
    addCol(rndGrp, rndRow, m_isamRandomFields.le, "Le", "08", 2, true);
    rndRow->addStretch();
    rndLay->addLayout(rndRow);
    auto* rndBtn = new QPushButton("▶ 執行 Generate Random No", rndGrp);
    rndBtn->setStyleSheet(
        "QPushButton { background:#1565C0; color:white; padding:8px; "
        "font-weight:bold; border-radius:4px; }");
    rndLay->addWidget(rndBtn);
    layout->addWidget(rndGrp);
    connect(rndBtn, &QPushButton::clicked, this, &PageCardOperation::onIsamGenerateRandom);

    // ── ④ Get TerminalAuthCode ────────────────────────────────────────────
    auto* tacGrp = new QGroupBox("④ Get TerminalAuthCode", content);
    auto* tacLay = new QVBoxLayout(tacGrp);
    auto* tacRow = new QHBoxLayout();
    addCol(tacGrp, tacRow, m_isamTermAuthFields.cla, "CLA", "82", 2, true);
    addCol(tacGrp, tacRow, m_isamTermAuthFields.ins, "INS", "08", 2, true);
    addCol(tacGrp, tacRow, m_isamTermAuthFields.p1, "P1", "", 2, false);
    addCol(tacGrp, tacRow, m_isamTermAuthFields.p2, "P2", "00", 2, true);
    addCol(tacGrp, tacRow, m_isamTermAuthFields.lc, "Lc", "10", 2, true);
    addCol(tacGrp, tacRow, m_isamTermAuthFields.le, "Le", "08", 2, true);
    tacRow->addStretch();
    tacLay->addLayout(tacRow);

    auto* tacDataLbl = new QLabel("DataIn — 16 bytes（Read Binary + Random Num）：", tacGrp);
    tacDataLbl->setStyleSheet("font-weight:bold; font-size:11px;");
    m_isamTermAuthFields.dataIn = makeEditField(tacGrp, false);
    m_isamTermAuthFields.dataIn->setPlaceholderText("輸入 32 個 HEX 字元（16 bytes）");
    tacLay->addWidget(tacDataLbl);
    tacLay->addWidget(m_isamTermAuthFields.dataIn);

    auto* tacHint = new QLabel("P1：自行輸入。", tacGrp);
    tacHint->setStyleSheet("color:#555; font-size:11px; padding:2px;");
    tacLay->addWidget(tacHint);
    auto* tacBtn = new QPushButton("▶ 執行 Get TerminalAuthCode", tacGrp);
    tacBtn->setStyleSheet(
        "QPushButton { background:#EF6C00; color:white; padding:8px; "
        "font-weight:bold; border-radius:4px; }");
    tacLay->addWidget(tacBtn);
    layout->addWidget(tacGrp);
    connect(tacBtn, &QPushButton::clicked, this, &PageCardOperation::onIsamGetTerminalAuthCode);

    // ── ⑤ Get Encrypted Key ───────────────────────────────────────────────
    auto* ekGrp = new QGroupBox("⑤ Get Encrypted Key", content);
    auto* ekLay = new QVBoxLayout(ekGrp);
    auto* ekRow = new QHBoxLayout();
    addCol(ekGrp, ekRow, m_isamEncKeyFields.cla, "CLA", "82", 2, true);
    addCol(ekGrp, ekRow, m_isamEncKeyFields.ins, "INS", "06", 2, true);
    addCol(ekGrp, ekRow, m_isamEncKeyFields.p1, "P1（ExKID）", "10", 2, false);
    addCol(ekGrp, ekRow, m_isamEncKeyFields.p2, "P2（KID）", "", 2, false);
    addCol(ekGrp, ekRow, m_isamEncKeyFields.le, "Le", "00", 2, true);
    ekRow->addStretch();
    ekLay->addLayout(ekRow);
    auto* ekBtn = new QPushButton("▶ 執行 Get Encrypted Key", ekGrp);
    ekBtn->setStyleSheet(
        "QPushButton { background:#00838F; color:white; padding:8px; "
        "font-weight:bold; border-radius:4px; }");
    ekLay->addWidget(ekBtn);
    layout->addWidget(ekGrp);
    connect(ekBtn, &QPushButton::clicked, this, &PageCardOperation::onIsamGetEncryptedKey);

    // ── ⑥ Write Key ───────────────────────────────────────────────────────
    auto* wkGrp = new QGroupBox("⑥ Write Key", content);
    auto* wkLay = new QVBoxLayout(wkGrp);
    auto* wkRow = new QHBoxLayout();
    addCol(wkGrp, wkRow, m_writeKeyFields.cla, "CLA", "82", 2, true);
    addCol(wkGrp, wkRow, m_writeKeyFields.ins, "INS", "03", 2, true);
    addCol(wkGrp, wkRow, m_writeKeyFields.p1, "P1（KID）", "", 2, false);
    addCol(wkGrp, wkRow, m_writeKeyFields.p2, "P2（Key Type）", "", 2, false);
    addCol(wkGrp, wkRow, m_writeKeyFields.lcDisplay, "Lc", "", 2, true);
    wkRow->addStretch();
    wkLay->addLayout(wkRow);

    auto* wkHint1 = new QLabel(
        "P2（Key Type）：9500 → 08，9502 → 02。Lc 依 DataIn 長度自動計算（0E/16/1E）。", wkGrp);
    wkHint1->setStyleSheet("color:#555; font-size:11px; padding:2px;");
    wkLay->addWidget(wkHint1);

    // EKey 來源：CD Key 對 Exchange Key 做 3DES 加密，自動算出 EKey
    auto* wkSrcGrp = new QGroupBox("EKey 來源（CD Key 3DES 加密 Exchange Key）", wkGrp);
    auto* wkSrcLay = new QVBoxLayout(wkSrcGrp);
    auto addWkSrcLine = [&](QLineEdit*& outField, const QString& lbl, const QString& placeholder) {
        auto* l = new QLabel(lbl, wkSrcGrp);
        l->setStyleSheet("font-weight:bold; font-size:11px;");
        outField = makeEditField(wkSrcGrp, false);
        if (!placeholder.isEmpty()) outField->setPlaceholderText(placeholder);
        wkSrcLay->addWidget(l);
        wkSrcLay->addWidget(outField);
        };
    addWkSrcLine(m_writeKeyFields.cdKey, "CD Key：", "輸入 CD Key HEX");
    addWkSrcLine(m_writeKeyFields.exchangeKey, "Exchange Key（明碼基碼）：", "輸入 Exchange Key HEX");
    wkLay->addWidget(wkSrcGrp);

    auto* wkDataGrp = new QGroupBox("DataIn = EKey + CCode + Limit", wkGrp);
    auto* wkDataLay = new QVBoxLayout(wkDataGrp);
    auto addWkLine = [&](QLineEdit*& outField, const QString& lbl,
        const QString& placeholder, const QString& def, bool ro) {
            auto* l = new QLabel(lbl, wkDataGrp);
            l->setStyleSheet("font-weight:bold; font-size:11px;");
            outField = makeEditField(wkDataGrp, ro);
            if (!placeholder.isEmpty()) outField->setPlaceholderText(placeholder);
            if (!def.isEmpty()) outField->setText(def);
            if (ro) {
                outField->setStyleSheet(
                    "QLineEdit { background:#EEE; color:#555; font-family:'Courier New'; "
                    "font-size:11px; padding:4px; border:1px dashed #AAA; border-radius:3px; }");
            }
            wkDataLay->addWidget(l);
            wkDataLay->addWidget(outField);
        };
    addWkLine(m_writeKeyFields.eKey, "EKey（16 或 24 bytes，由 CD Key + Exchange Key 自動算出）：",
        "自動計算，無需手動輸入", QString(), /*ro=*/true);
    addWkLine(m_writeKeyFields.ccode, "CCode（2 bytes / 4 個 HEX 字元）：",
        "輸入 CCode HEX", QString(), /*ro=*/false);
    addWkLine(m_writeKeyFields.limit, "Limit（4 bytes / 8 個 HEX 字元，預設 FFFFFFFF）：",
        QString(), "FFFFFFFF", /*ro=*/false);
    wkLay->addWidget(wkDataGrp);

    connect(m_writeKeyFields.cdKey, &QLineEdit::textChanged, this, &PageCardOperation::onUpdateWriteKeyEKey);
    connect(m_writeKeyFields.exchangeKey, &QLineEdit::textChanged, this, &PageCardOperation::onUpdateWriteKeyEKey);
    connect(m_writeKeyFields.ccode, &QLineEdit::textChanged, this, &PageCardOperation::onUpdateWriteKeyLc);
    connect(m_writeKeyFields.limit, &QLineEdit::textChanged, this, &PageCardOperation::onUpdateWriteKeyLc);

    auto* wkBtn = new QPushButton("▶ 執行 Write Key", wkGrp);
    wkBtn->setStyleSheet(
        "QPushButton { background:#C62828; color:white; padding:8px; "
        "font-weight:bold; border-radius:4px; }");
    wkLay->addWidget(wkBtn);
    layout->addWidget(wkGrp);
    connect(wkBtn, &QPushButton::clicked, this, &PageCardOperation::onIsamWriteKey);
    onUpdateWriteKeyEKey();   // 初始化 EKey / Lc 顯示

    // ── ⑦ Store Manufacturer Key ──────────────────────────────────────────
    auto* smkGrp = new QGroupBox("⑦ Store Manufacturer Key", content);
    auto* smkLay = new QVBoxLayout(smkGrp);
    auto* smkRow = new QHBoxLayout();
    addCol(smkGrp, smkRow, m_storeMfgKeyFields.cla, "CLA", "82", 2, true);
    addCol(smkGrp, smkRow, m_storeMfgKeyFields.ins, "INS", "09", 2, true);
    addCol(smkGrp, smkRow, m_storeMfgKeyFields.p1, "P1（ExKID）", "", 2, false);
    addCol(smkGrp, smkRow, m_storeMfgKeyFields.p2, "P2（KID）", "", 2, false);
    addCol(smkGrp, smkRow, m_storeMfgKeyFields.lcDisplay, "Lc", "", 2, true);
    smkRow->addStretch();
    smkLay->addLayout(smkRow);

    auto* smkHint1 = new QLabel("Lc 依 DataIn 長度自動計算（0F/17/1F）。", smkGrp);
    smkHint1->setStyleSheet("color:#555; font-size:11px; padding:2px;");
    smkLay->addWidget(smkHint1);

    auto* smkDataGrp = new QGroupBox("DataIn = KT + EKey + CCode + Limit", smkGrp);
    auto* smkDataLay = new QVBoxLayout(smkDataGrp);
    auto addSmkLine = [&](QLineEdit*& outField, const QString& lbl,
        const QString& placeholder, const QString& def) {
            auto* l = new QLabel(lbl, smkDataGrp);
            l->setStyleSheet("font-weight:bold; font-size:11px;");
            outField = makeEditField(smkDataGrp, false);
            if (!placeholder.isEmpty()) outField->setPlaceholderText(placeholder);
            if (!def.isEmpty()) outField->setText(def);
            smkDataLay->addWidget(l);
            smkDataLay->addWidget(outField);
        };
    addSmkLine(m_storeMfgKeyFields.kt, "KT（1 byte / 2 個 HEX 字元）：",
        "輸入 Key Type HEX", QString());
    addSmkLine(m_storeMfgKeyFields.eKey, "EKey（8 / 16 / 24 bytes）：",
        "輸入加密後金鑰 HEX", QString());
    addSmkLine(m_storeMfgKeyFields.ccode, "CCode（2 bytes / 4 個 HEX 字元）：",
        "輸入 CCode HEX", QString());
    addSmkLine(m_storeMfgKeyFields.limit, "Limit（4 bytes / 8 個 HEX 字元，預設 FFFFFFFF）：",
        QString(), "FFFFFFFF");
    smkLay->addWidget(smkDataGrp);

    connect(m_storeMfgKeyFields.kt, &QLineEdit::textChanged, this, &PageCardOperation::onUpdateStoreMfgKeyLc);
    connect(m_storeMfgKeyFields.eKey, &QLineEdit::textChanged, this, &PageCardOperation::onUpdateStoreMfgKeyLc);
    connect(m_storeMfgKeyFields.ccode, &QLineEdit::textChanged, this, &PageCardOperation::onUpdateStoreMfgKeyLc);
    connect(m_storeMfgKeyFields.limit, &QLineEdit::textChanged, this, &PageCardOperation::onUpdateStoreMfgKeyLc);

    auto* smkBtn = new QPushButton("▶ 執行 Store Manufacturer Key", smkGrp);
    smkBtn->setStyleSheet(
        "QPushButton { background:#5C6BC0; color:white; padding:8px; "
        "font-weight:bold; border-radius:4px; }");
    smkLay->addWidget(smkBtn);
    layout->addWidget(smkGrp);
    connect(smkBtn, &QPushButton::clicked, this, &PageCardOperation::onIsamStoreManufacturerKey);
    onUpdateStoreMfgKeyLc();   // 初始化 Lc 顯示

    // ── ⑧ Set Session Key ───────────────────────────────────────────────────
    auto* sskGrp = new QGroupBox("⑧ Set Session Key", content);
    auto* sskLay = new QVBoxLayout(sskGrp);
    auto* sskRow = new QHBoxLayout();
    addCol(sskGrp, sskRow, m_setSessionKeyIsamFields.cla, "CLA", "82", 2, true);
    addCol(sskGrp, sskRow, m_setSessionKeyIsamFields.ins, "INS", "0A", 2, true);
    addCol(sskGrp, sskRow, m_setSessionKeyIsamFields.p1, "P1（KID）", "", 2, false);
    addCol(sskGrp, sskRow, m_setSessionKeyIsamFields.p2, "P2", "00", 2, true);
    addCol(sskGrp, sskRow, m_setSessionKeyIsamFields.lcDisplay, "Lc（DL+8）", "", 2, true);
    addCol(sskGrp, sskRow, m_setSessionKeyIsamFields.le, "Le", "--", 2, true);
    sskRow->addStretch();
    sskLay->addLayout(sskRow);

    auto* sskDivLbl = new QLabel("Div-Data（多樣化資料，可變長度）：", sskGrp);
    sskDivLbl->setStyleSheet("font-weight:bold; font-size:11px;");
    m_setSessionKeyIsamFields.divData = makeEditField(sskGrp, false);
    m_setSessionKeyIsamFields.divData->setPlaceholderText("輸入多樣化資料 HEX");
    sskLay->addWidget(sskDivLbl);
    sskLay->addWidget(m_setSessionKeyIsamFields.divData);

    auto* sskR2Lbl = new QLabel("R2（卡片指令 Set Session Key 回應之 R2，8 bytes）：", sskGrp);
    sskR2Lbl->setStyleSheet("font-weight:bold; font-size:11px;");
    m_setSessionKeyIsamFields.r2 = makeEditField(sskGrp, false);
    m_setSessionKeyIsamFields.r2->setPlaceholderText("輸入卡片指令 Set Session Key 回應之 R2（16 個 HEX 字元 / 8 bytes）");
    sskLay->addWidget(sskR2Lbl);
    sskLay->addWidget(m_setSessionKeyIsamFields.r2);

    auto* sskDataLbl = new QLabel("DATA（= Div-Data + R2，自動組合）：", sskGrp);
    sskDataLbl->setStyleSheet("font-weight:bold; font-size:11px;");
    m_setSessionKeyIsamFields.dataDisplay = makeEditField(sskGrp, true);
    m_setSessionKeyIsamFields.dataDisplay->setStyleSheet(
        "QLineEdit { background:#EEE; color:#555; font-family:'Courier New'; "
        "font-size:11px; padding:4px; border:1px dashed #AAA; border-radius:3px; }");
    sskLay->addWidget(sskDataLbl);
    sskLay->addWidget(m_setSessionKeyIsamFields.dataDisplay);

    auto* sskHint = new QLabel("Lc = DL + 8（依 Div-Data + R2 總長度自動計算）。Le：--（無）。", sskGrp);
    sskHint->setStyleSheet("color:#555; font-size:11px; padding:2px;");
    sskLay->addWidget(sskHint);

    connect(m_setSessionKeyIsamFields.divData, &QLineEdit::textChanged, this, &PageCardOperation::onUpdateSetSessionKeyLc);
    connect(m_setSessionKeyIsamFields.r2, &QLineEdit::textChanged, this, &PageCardOperation::onUpdateSetSessionKeyLc);

    auto* sskBtn = new QPushButton("▶ 執行 Set Session Key", sskGrp);
    sskBtn->setStyleSheet(
        "QPushButton { background:#AD1457; color:white; padding:8px; "
        "font-weight:bold; border-radius:4px; }");
    sskLay->addWidget(sskBtn);
    layout->addWidget(sskGrp);
    connect(sskBtn, &QPushButton::clicked, this, &PageCardOperation::onIsamSetSessionKey);
    onUpdateSetSessionKeyLc();   // 初始化 Lc 顯示

    // ── ⑨ Get Diversify Key with Session Key ────────────────────────────────
    auto* dkGrp = new QGroupBox("⑨ Get Diversify Key with Session Key", content);
    auto* dkLay = new QVBoxLayout(dkGrp);
    auto* dkRow = new QHBoxLayout();
    addCol(dkGrp, dkRow, m_diversifyKeyFields.cla, "CLA", "82", 2, true);
    addCol(dkGrp, dkRow, m_diversifyKeyFields.ins, "INS", "0B", 2, true);
    addCol(dkGrp, dkRow, m_diversifyKeyFields.p1, "P1（KID）", "", 2, false);
    addCol(dkGrp, dkRow, m_diversifyKeyFields.p2, "P2", "00", 2, true);
    addCol(dkGrp, dkRow, m_diversifyKeyFields.lcDisplay, "Lc（DL）", "", 2, true);
    addCol(dkGrp, dkRow, m_diversifyKeyFields.le, "Le", "00", 2, true);
    dkRow->addStretch();
    dkLay->addLayout(dkRow);

    auto* dkDivLbl = new QLabel("Div-Data（多樣化資料，可變長度）：", dkGrp);
    dkDivLbl->setStyleSheet("font-weight:bold; font-size:11px;");
    m_diversifyKeyFields.divData = makeEditField(dkGrp, false);
    m_diversifyKeyFields.divData->setPlaceholderText("輸入多樣化資料 HEX");
    dkLay->addWidget(dkDivLbl);
    dkLay->addWidget(m_diversifyKeyFields.divData);

    auto* dkHint = new QLabel("P1（KID）：WK 或 SK，自行輸入。Lc = DL（依 Div-Data 長度自動計算）。", dkGrp);
    dkHint->setStyleSheet("color:#555; font-size:11px; padding:2px;");
    dkLay->addWidget(dkHint);

    connect(m_diversifyKeyFields.divData, &QLineEdit::textChanged, this, &PageCardOperation::onUpdateDiversifyKeyLc);

    auto* dkBtn = new QPushButton("▶ 執行 Get Diversify Key with Session Key", dkGrp);
    dkBtn->setStyleSheet(
        "QPushButton { background:#00838F; color:white; padding:8px; "
        "font-weight:bold; border-radius:4px; }");
    dkLay->addWidget(dkBtn);
    layout->addWidget(dkGrp);
    connect(dkBtn, &QPushButton::clicked, this, &PageCardOperation::onIsamGetDiversifyKey);
    onUpdateDiversifyKeyLc();   // 初始化 Lc 顯示

    // ── ⑩ Get Working Key with Session Key ──────────────────────────────────
    auto* wkeyGrp = new QGroupBox("⑩ Get Working Key with Session Key", content);
    auto* wkeyLay = new QVBoxLayout(wkeyGrp);
    auto* wkeyRow = new QHBoxLayout();
    addCol(wkeyGrp, wkeyRow, m_workingKeyFields.cla, "CLA", "82", 2, true);
    addCol(wkeyGrp, wkeyRow, m_workingKeyFields.ins, "INS", "0C", 2, true);
    addCol(wkeyGrp, wkeyRow, m_workingKeyFields.p1, "P1（KID）", "", 2, false);
    addCol(wkeyGrp, wkeyRow, m_workingKeyFields.p2, "P2", "00", 2, true);
    addCol(wkeyGrp, wkeyRow, m_workingKeyFields.le, "Le", "00", 2, true);
    wkeyRow->addStretch();
    wkeyLay->addLayout(wkeyRow);

    auto* wkeyHint = new QLabel("P1（KID）：必須為 WK，自行輸入。無 Lc / DataIn。", wkeyGrp);
    wkeyHint->setStyleSheet("color:#555; font-size:11px; padding:2px;");
    wkeyLay->addWidget(wkeyHint);

    auto* wkeyBtn = new QPushButton("▶ 執行 Get Working Key with Session Key", wkeyGrp);
    wkeyBtn->setStyleSheet(
        "QPushButton { background:#1565C0; color:white; padding:8px; "
        "font-weight:bold; border-radius:4px; }");
    wkeyLay->addWidget(wkeyBtn);
    layout->addWidget(wkeyGrp);
    connect(wkeyBtn, &QPushButton::clicked, this, &PageCardOperation::onIsamGetWorkingKey);

    layout->addStretch();
    scroll->setWidget(content);

    auto* tabLayout = new QVBoxLayout(tab);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->addWidget(scroll);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab 4 — 製卡發卡流程（原 LOD / 母卡，已調整至卡片指令頁籤之後）
// ─────────────────────────────────────────────────────────────────────────────
void PageCardOperation::setupLodTab(QWidget* tab)
{
    auto* layout = new QVBoxLayout(tab);

    // ── 製作母卡：需選擇兩台不同的讀卡機（ISAM / 母卡）──────────────────────
    auto* readerGrp = new QGroupBox("製作母卡 — 讀卡機選擇（需 2 台不同讀卡機）", tab);
    auto* readerForm = new QFormLayout(readerGrp);
    m_masterIsamReaderCombo = new QComboBox(tab);
    m_masterCardReaderCombo = new QComboBox(tab);
    m_masterIsamReaderCombo->addItems(m_readers);
    m_masterCardReaderCombo->addItems(m_readers);
    if (m_readers.size() > 1) m_masterCardReaderCombo->setCurrentIndex(1);
    readerForm->addRow("🔑 ISAM 讀卡機：", m_masterIsamReaderCombo);
    readerForm->addRow("💳 母卡 讀卡機：", m_masterCardReaderCombo);
    layout->addWidget(readerGrp);
    if (!m_canMakeMaster) {
        readerGrp->setEnabled(false);
        readerGrp->setToolTip("目前讀卡機數量不足 2 台，無法製作母卡。");
    }

    auto* btnRow = new QHBoxLayout();
    m_loadBtn = new QPushButton("📂 載入 LOD", tab);
    m_loadBtn->setStyleSheet("QPushButton { background:#4CAF50; color:white; padding:8px 16px; font-weight:bold; border-radius:4px; }");

    m_clearBtn = new QPushButton("🗑 清除", tab);
    m_clearBtn->setStyleSheet("QPushButton { background:#9E9E9E; color:white; padding:8px 12px; font-weight:bold; border-radius:4px; }");

    m_makeMasterBtn = new QPushButton("🔮 製作母卡", tab);
    m_makeMasterBtn->setStyleSheet(
        "QPushButton { background:#0288D1; color:white; padding:8px 16px; font-weight:bold; border-radius:4px; }"
        "QPushButton:disabled { background:#CCC; color:#666; }");
    m_makeMasterBtn->setEnabled(false);

    m_authBtn = new QPushButton("🔐 母卡認證", tab);
    m_authBtn->setStyleSheet(
        "QPushButton { background:#6A1B9A; color:white; padding:8px 16px; font-weight:bold; border-radius:4px; }"
        "QPushButton:disabled { background:#CCC; color:#666; }");

    m_convertBtn = new QPushButton("🔄 母卡轉換", tab);
    m_convertBtn->setStyleSheet(
        "QPushButton { background:#EF6C00; color:white; padding:8px 16px; font-weight:bold; border-radius:4px; }"
        "QPushButton:disabled { background:#CCC; color:#666; }");

    m_washCardBtn = new QPushButton("🧼 洗卡", tab);
    m_washCardBtn->setStyleSheet(
        "QPushButton { background:#00838F; color:white; padding:8px 16px; font-weight:bold; border-radius:4px; }"
        "QPushButton:disabled { background:#CCC; color:#666; }");

    m_fileLabel = new QLabel("尚未載入 — 或直接手動輸入", tab);
    m_fileLabel->setStyleSheet("color:#888; font-style:italic;");

    btnRow->addWidget(m_loadBtn);
    btnRow->addWidget(m_clearBtn);
    btnRow->addWidget(m_authBtn);
    btnRow->addWidget(m_makeMasterBtn);
    btnRow->addWidget(m_convertBtn);
    btnRow->addWidget(m_washCardBtn);
    btnRow->addWidget(m_fileLabel, 1);
    layout->addLayout(btnRow);

    auto* dataGrp = new QGroupBox("客戶資料欄位（可手動編輯）");
    auto* dataForm = new QFormLayout(dataGrp);
    dataForm->setLabelAlignment(Qt::AlignRight);

    // 調整欄位間距，讓排版更緊湊
    dataForm->setVerticalSpacing(10);

    auto mf = [&](QLineEdit*& f, const QString& lbl) {
        f = makeEditField(tab, false);
        // 設定最大高度，避免輸入框被垂直拉伸變形
        f->setMaximumHeight(28);
        dataForm->addRow(lbl, f);
        };
    mf(m_nameEdit, "客戶名稱：");
    mf(m_c3Edit, "C3 金鑰：");
    mf(m_c3CodeEdit, "C3 Code：");
    mf(m_c1Edit, "C1 金鑰：");
    mf(m_c1CodeEdit, "C1 Code：");
    mf(m_codeAEdit, "Code A：");
    mf(m_codeBEdit, "Code B：");
    mf(m_exchEdit, "Exchange Code：");
    mf(m_ccodeEdit, "CCode：");
    mf(m_pinAEdit, "Pin A：");
    mf(m_pinBEdit, "Pin B：");
    layout->addWidget(dataGrp);

    // 【關鍵修正】在最底部加上彈簧，把上面的元件往上推，避免元件被垂直平分撐開
    layout->addStretch();

    connect(m_loadBtn, &QPushButton::clicked, this, &PageCardOperation::onLoadFile);
    connect(m_clearBtn, &QPushButton::clicked, this, &PageCardOperation::onClearFields);
    connect(m_authBtn, &QPushButton::clicked, this, &PageCardOperation::onAuthenticate);
    connect(m_makeMasterBtn, &QPushButton::clicked, this, &PageCardOperation::onMakeMaster);
    connect(m_convertBtn, &QPushButton::clicked, this, &PageCardOperation::onConvertMaster);
    connect(m_washCardBtn, &QPushButton::clicked, this, &PageCardOperation::onWashCard);

    // 客戶資料欄位如果是手動填入（而非透過載入 LOD），只要 C3 / C1 都有值，
    // 也要能啟用「製作母卡」按鈕，不強制一定要先載入檔案。
    connect(m_c3Edit, &QLineEdit::textChanged, this, &PageCardOperation::onCustomerFieldChanged);
    connect(m_c1Edit, &QLineEdit::textChanged, this, &PageCardOperation::onCustomerFieldChanged);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab 3 — 卡片指令（Create / Select / Write / List / Verify Pin / Read Record / Read All Record）
// ─────────────────────────────────────────────────────────────────────────────
void PageCardOperation::setupApduTab(QWidget* tab)
{
    // Scroll area 讓版面不擁擠
    auto* scroll = new QScrollArea(tab);
    scroll->setWidgetResizable(true);
    auto* content = new QWidget();
    auto* layout = new QVBoxLayout(content);
    layout->setSpacing(12);

    // ── 讀卡機選擇（比照 ISAM 頁籤做法，自行選擇本頁指令要發送到哪一台讀卡機）──
    auto* targetGrp = new QGroupBox("目標讀卡機");
    auto* targetForm = new QFormLayout(targetGrp);
    auto* m_targetCombo = new QComboBox(content);
    m_targetCombo->addItems(m_readers);
    targetForm->addRow("發送對象：", m_targetCombo);
    layout->addWidget(targetGrp);

    // ── Create File ──
    setupCreateFileGroup(content, layout);

    // ── Select File ──
    auto* selectGrp = makeApduGroup(
        content,
        "② Select File",
        "00", "A4", "02", "00",
        /*hasDataIn=*/true,
        /*hasLe=*/false,
        m_selectFields);

    // Lc 預設 02
    if (m_selectFields.lc) m_selectFields.lc->setText("02");

    // Select EF (P1=02) / Select DF (P1=01) 切換
    auto* selModeRow = new QHBoxLayout();
    auto* selEfRadio = new QRadioButton("Select EF（P1=02）", content);
    auto* selDfRadio = new QRadioButton("Select DF（P1=01）", content);
    selEfRadio->setChecked(true);
    selModeRow->addWidget(selEfRadio);
    selModeRow->addWidget(selDfRadio);
    selModeRow->addStretch();

    auto* selectBtn = new QPushButton("▶ 執行 Select File", content);
    selectBtn->setStyleSheet(
        "QPushButton { background:#00897B; color:white; padding:8px; "
        "font-weight:bold; border-radius:4px; }");
    auto* selectLayout = qobject_cast<QVBoxLayout*>(selectGrp->layout());
    if (selectLayout) {
        selectLayout->addLayout(selModeRow);
        selectLayout->addWidget(selectBtn);
    }
    layout->addWidget(selectGrp);

    connect(selEfRadio, &QRadioButton::toggled, this, [this, selEfRadio]() {
        if (selEfRadio->isChecked() && m_selectFields.p1)
            m_selectFields.p1->setText("02");
        });
    connect(selDfRadio, &QRadioButton::toggled, this, [this, selDfRadio]() {
        if (selDfRadio->isChecked() && m_selectFields.p1)
            m_selectFields.p1->setText("01");
        });

    // ── List File ──
    auto* listGrp = new QGroupBox("③ List File");
    auto* listLay = new QVBoxLayout(listGrp);

    auto* listInfo = new QLabel(
        "固定指令：CLA=00  INS=AA  P1=00  P2=00  Le=000000h\n"
        "回傳：FIDList（每個 File = FID 2 bytes + ToF 1 byte）", listGrp);
    listInfo->setStyleSheet("color:#555; font-size:11px; padding:4px;");
    listLay->addWidget(listInfo);

    auto* listBtn = new QPushButton("▶ 執行 List File", listGrp);
    listBtn->setStyleSheet(
        "QPushButton { background:#6A1B9A; color:white; padding:8px; "
        "font-weight:bold; border-radius:4px; }");
    listLay->addWidget(listBtn);
    layout->addWidget(listGrp);

    // ── Read Record ──
    auto* readGrp = new QGroupBox("④ Read Record", content);
    auto* readLay = new QVBoxLayout(readGrp);

    auto* readRow = new QHBoxLayout();
    auto addReadCol = [&](QLineEdit*& outField, const QString& lbl,
        const QString& def, int maxLen, bool ro) {
            auto* col = new QVBoxLayout();
            auto* l = new QLabel(lbl, readGrp);
            l->setAlignment(Qt::AlignCenter);
            l->setStyleSheet("font-weight:bold; font-size:11px;");
            outField = makeHexField(readGrp, maxLen);
            outField->setText(def);
            outField->setReadOnly(ro);
            if (ro) {
                outField->setStyleSheet(
                    "QLineEdit { background:#EEE; color:#555; font-family:'Courier New'; "
                    "font-weight:bold; font-size:12px; padding:4px; "
                    "border:1px dashed #AAA; border-radius:3px; }");
            }
            col->addWidget(l);
            col->addWidget(outField);
            readRow->addLayout(col);
        };

    addReadCol(m_readRecordFields.cla, "CLA", "00", 2, false);
    addReadCol(m_readRecordFields.ins, "INS", "B2", 2, false);
    addReadCol(m_readRecordFields.recN, "P1（RecN）", "", 2, false);
    addReadCol(m_readRecordFields.p2, "P2", "04", 2, true);
    addReadCol(m_readRecordFields.le, "Le", "00", 2, false);
    readRow->addStretch();
    readLay->addLayout(readRow);

    auto* readHint = new QLabel("P1 = Record Id（自行輸入），Le 預設 00", readGrp);
    readHint->setStyleSheet("color:#555; font-size:11px; padding:2px;");
    readLay->addWidget(readHint);

    auto* readBtn = new QPushButton("▶ 執行 Read Record", readGrp);
    readBtn->setStyleSheet(
        "QPushButton { background:#1565C0; color:white; padding:8px; "
        "font-weight:bold; border-radius:4px; }");
    readLay->addWidget(readBtn);
    layout->addWidget(readGrp);

    connect(readBtn, &QPushButton::clicked, this, &PageCardOperation::onReadRecord);

    // ── Read All REcord ──
    auto* readAllGrp = new QGroupBox("⑤ Read All Record", content);
    auto* readAllLay = new QVBoxLayout(readAllGrp);

    auto* readAllRow = new QHBoxLayout();
    auto addReadAllCol = [&](QLineEdit*& outField, const QString& lbl,
        const QString& def, int maxLen, bool ro) {
            auto* col = new QVBoxLayout();
            auto* l = new QLabel(lbl, readAllGrp);
            l->setAlignment(Qt::AlignCenter);
            l->setStyleSheet("font-weight:bold; font-size:11px;");
            outField = makeHexField(readAllGrp, maxLen);
            outField->setText(def);
            outField->setReadOnly(ro);
            if (ro) {
                outField->setStyleSheet(
                    "QLineEdit { background:#EEE; color:#555; font-family:'Courier New'; "
                    "font-weight:bold; font-size:12px; padding:4px; "
                    "border:1px dashed #AAA; border-radius:3px; }");
            }
            col->addWidget(l);
            col->addWidget(outField);
            readAllRow->addLayout(col);
        };

    addReadAllCol(m_readAllRecordFields.cla, "CLA", "00", 2, false);
    addReadAllCol(m_readAllRecordFields.ins, "INS", "B2", 2, false);
    addReadAllCol(m_readAllRecordFields.recN, "P1（RecN）", "", 2, false);
    addReadAllCol(m_readAllRecordFields.p2, "P2", "05", 2, true);
    addReadAllCol(m_readAllRecordFields.le, "Le", "000000", 6, false);
    readAllRow->addStretch();
    readAllLay->addLayout(readAllRow);

    auto* readAllHint = new QLabel("P1 = Record Id（自行輸入），Le 預設 00 00 00", readAllGrp);
    readAllHint->setStyleSheet("color:#555; font-size:11px; padding:2px;");
    readAllLay->addWidget(readAllHint);

    auto* readAllBtn = new QPushButton("▶ 執行 Read All Record", readAllGrp);
    readAllBtn->setStyleSheet(
        "QPushButton { background:#00838F; color:white; padding:8px; "
        "font-weight:bold; border-radius:4px; }");
    readAllLay->addWidget(readAllBtn);
    layout->addWidget(readAllGrp);

    connect(readAllBtn, &QPushButton::clicked, this, &PageCardOperation::onReadAllRecord);

    // ── ⑥ Read Binary ────────────────────────────────────────────────────────
    auto* rbGrp = new QGroupBox("⑥ Read Binary", content);
    auto* rbLay = new QVBoxLayout(rbGrp);
    auto* rbRow = new QHBoxLayout();
    auto addRbCol = [&](QLineEdit*& outField, const QString& lbl,
        const QString& def, int maxLen, bool ro) {
            auto* col = new QVBoxLayout();
            auto* l = new QLabel(lbl, rbGrp);
            l->setAlignment(Qt::AlignCenter);
            l->setStyleSheet("font-weight:bold; font-size:11px;");
            outField = makeHexField(rbGrp, maxLen);
            outField->setText(def);
            outField->setReadOnly(ro);
            if (ro) {
                outField->setStyleSheet(
                    "QLineEdit { background:#EEE; color:#555; font-family:'Courier New'; "
                    "font-weight:bold; font-size:12px; padding:4px; "
                    "border:1px dashed #AAA; border-radius:3px; }");
            }
            col->addWidget(l);
            col->addWidget(outField);
            rbRow->addLayout(col);
        };

    addRbCol(m_readBinaryFields.cla, "CLA", "00", 2, true);
    addRbCol(m_readBinaryFields.ins, "INS", "B0", 2, true);
    addRbCol(m_readBinaryFields.p1, "P1", "00", 2, true);
    addRbCol(m_readBinaryFields.p2, "P2（起始位置）", "00", 2, false);
    addRbCol(m_readBinaryFields.le, "Le（欲讀取長度）", "10", 2, false);
    rbRow->addStretch();
    rbLay->addLayout(rbRow);

    auto* rbHint = new QLabel("P2：起始位置，預設 00（可自行修改）。Le：欲讀取的長度，預設 10（可自行修改）。", rbGrp);
    rbHint->setStyleSheet("color:#555; font-size:11px; padding:2px;");
    rbLay->addWidget(rbHint);

    auto* rbBtn = new QPushButton("▶ 執行 Read Binary", rbGrp);
    rbBtn->setStyleSheet(
        "QPushButton { background:#1565C0; color:white; padding:8px; "
        "font-weight:bold; border-radius:4px; }");
    rbLay->addWidget(rbBtn);
    layout->addWidget(rbGrp);
    connect(rbBtn, &QPushButton::clicked, this, &PageCardOperation::onReadBinary);

    // ── Write Record ──
    auto* writeGrp = makeApduGroup(
        content,
        "⑦ Write Record",
        "00", "D2", "00", "05",
        /*hasDataIn=*/true,
        /*hasLe=*/false,
        m_writeFields);

    auto* writeBtn = new QPushButton("▶ 執行 Write Record", content);
    writeBtn->setStyleSheet(
        "QPushButton { background:#EF6C00; color:white; padding:8px; "
        "font-weight:bold; border-radius:4px; }");
    auto* writeLayout = qobject_cast<QVBoxLayout*>(writeGrp->layout());
    if (writeLayout) writeLayout->addWidget(writeBtn);
    layout->addWidget(writeGrp);

    // ── ⑧ Update Record Ciphered (Write Record Ciphered) ────────────────────
    auto* urcGrp = new QGroupBox("⑧ Update Record Ciphered (Write Record Ciphered)", content);
    auto* urcLay = new QVBoxLayout(urcGrp);

    auto* urcRow = new QHBoxLayout();
    auto addUrcCol = [&](QLineEdit*& outField, const QString& lbl,
        const QString& def, int maxLen, bool ro) {
            auto* col = new QVBoxLayout();
            auto* l = new QLabel(lbl, urcGrp);
            l->setAlignment(Qt::AlignCenter);
            l->setStyleSheet("font-weight:bold; font-size:11px;");
            outField = makeHexField(urcGrp, maxLen);
            outField->setText(def);
            outField->setReadOnly(ro);
            if (ro) {
                outField->setStyleSheet(
                    "QLineEdit { background:#EEE; color:#555; font-family:'Courier New'; "
                    "font-weight:bold; font-size:12px; padding:4px; "
                    "border:1px dashed #AAA; border-radius:3px; }");
            }
            col->addWidget(l);
            col->addWidget(outField);
            urcRow->addLayout(col);
        };

    addUrcCol(m_updateRecordCipheredFields.cla, "CLA", "00", 2, true);
    addUrcCol(m_updateRecordCipheredFields.ins, "INS", "E4", 2, true);
    addUrcCol(m_updateRecordCipheredFields.p1, "P1", "01", 2, true);
    addUrcCol(m_updateRecordCipheredFields.p2, "P2", "04", 2, true);
    addUrcCol(m_updateRecordCipheredFields.lcDisplay, "Lc", "", 2, true);
    urcRow->addStretch();
    urcLay->addLayout(urcRow);

    auto* urcDataLbl = new QLabel("DATA（自行輸入）：", urcGrp);
    urcDataLbl->setStyleSheet("font-weight:bold; font-size:11px;");
    m_updateRecordCipheredFields.data = makeEditField(urcGrp, false);
    m_updateRecordCipheredFields.data->setPlaceholderText("輸入 DATA HEX（8/16/24 bytes）");
    urcLay->addWidget(urcDataLbl);
    urcLay->addWidget(m_updateRecordCipheredFields.data);

    auto* urcHint = new QLabel("Lc 依 DATA 長度自動計算（08/10/18）。", urcGrp);
    urcHint->setStyleSheet("color:#555; font-size:11px; padding:2px;");
    urcLay->addWidget(urcHint);

    connect(m_updateRecordCipheredFields.data, &QLineEdit::textChanged,
        this, &PageCardOperation::onUpdateRecordCipheredLc);

    auto* urcBtn = new QPushButton("▶ 執行 Update Record Ciphered", urcGrp);
    urcBtn->setStyleSheet(
        "QPushButton { background:#EF6C00; color:white; padding:8px; "
        "font-weight:bold; border-radius:4px; }");
    urcLay->addWidget(urcBtn);
    layout->addWidget(urcGrp);
    connect(urcBtn, &QPushButton::clicked, this, &PageCardOperation::onUpdateRecordCiphered);
    onUpdateRecordCipheredLc();   // 初始化 Lc 顯示

    // ── Verify Pin ──
    auto* verifyGrp = new QGroupBox("⑨ Verify Pin", content);
    auto* verifyLay = new QVBoxLayout(verifyGrp);

    auto* verifyRow = new QHBoxLayout();
    auto addVerifyCol = [&](QLineEdit*& outField, const QString& lbl,
        const QString& def, int maxLen, bool ro) {
            auto* col = new QVBoxLayout();
            auto* l = new QLabel(lbl, verifyGrp);
            l->setAlignment(Qt::AlignCenter);
            l->setStyleSheet("font-weight:bold; font-size:11px;");
            outField = makeHexField(verifyGrp, maxLen);
            outField->setText(def);
            outField->setReadOnly(ro);
            if (ro) {
                outField->setStyleSheet(
                    "QLineEdit { background:#EEE; color:#555; font-family:'Courier New'; "
                    "font-weight:bold; font-size:12px; padding:4px; "
                    "border:1px dashed #AAA; border-radius:3px; }");
            }
            col->addWidget(l);
            col->addWidget(outField);
            verifyRow->addLayout(col);
        };

    addVerifyCol(m_verifyPinFields.cla, "CLA", "00", 2, false);
    addVerifyCol(m_verifyPinFields.ins, "INS", "20", 2, false);
    addVerifyCol(m_verifyPinFields.p1, "P1", "00", 2, false);
    addVerifyCol(m_verifyPinFields.pq, "Pq（原始值）", "", 2, false);
    addVerifyCol(m_verifyPinFields.p2Display, "P2（Pq<<3）", "00", 2, true);
    addVerifyCol(m_verifyPinFields.lc, "Lc", "08", 2, true);
    verifyRow->addStretch();
    verifyLay->addLayout(verifyRow);

    auto* pinLabel = new QLabel("DATA — PIN code（8 bytes / 16 HEX）：", verifyGrp);
    pinLabel->setStyleSheet("font-weight:bold; font-size:11px;");
    m_verifyPinFields.pin = makeEditField(verifyGrp, false);
    m_verifyPinFields.pin->setPlaceholderText("輸入 PIN（HEX，不足自動補 FF）");
    verifyLay->addWidget(pinLabel);
    verifyLay->addWidget(m_verifyPinFields.pin);

    auto* verifyHint = new QLabel(
        "P2 由 Pq 邏輯左移 3 位自動運算（例如輸入 02 → P2=10）", verifyGrp);
    verifyHint->setStyleSheet("color:#555; font-size:11px; padding:2px;");
    verifyLay->addWidget(verifyHint);

    auto* verifyBtn = new QPushButton("▶ 執行 Verify Pin", verifyGrp);
    verifyBtn->setStyleSheet(
        "QPushButton { background:#C62828; color:white; padding:8px; "
        "font-weight:bold; border-radius:4px; }");
    verifyLay->addWidget(verifyBtn);
    layout->addWidget(verifyGrp);

    connect(m_verifyPinFields.pq, &QLineEdit::textChanged,
        this, &PageCardOperation::onUpdateVerifyPinP2);
    connect(verifyBtn, &QPushButton::clicked, this, &PageCardOperation::onVerifyPin);
    onUpdateVerifyPinP2();   // 初始化顯示

    // ── ⑩ Terminal Authentication ────────────────────────────────────────────
    auto* taGrp = new QGroupBox("⑩ Terminal Authentication", content);
    auto* taLay = new QVBoxLayout(taGrp);

    auto* taRow = new QHBoxLayout();
    auto addTaCol = [&](QLineEdit*& outField, const QString& lbl,
        const QString& def, int maxLen, bool ro) {
            auto* col = new QVBoxLayout();
            auto* l = new QLabel(lbl, taGrp);
            l->setAlignment(Qt::AlignCenter);
            l->setStyleSheet("font-weight:bold; font-size:11px;");
            outField = makeHexField(taGrp, maxLen);
            outField->setText(def);
            outField->setReadOnly(ro);
            if (ro) {
                outField->setStyleSheet(
                    "QLineEdit { background:#EEE; color:#555; font-family:'Courier New'; "
                    "font-weight:bold; font-size:12px; padding:4px; "
                    "border:1px dashed #AAA; border-radius:3px; }");
            }
            col->addWidget(l);
            col->addWidget(outField);
            taRow->addLayout(col);
        };

    addTaCol(m_terminalAuthFields.cla, "CLA", "00", 2, false);
    addTaCol(m_terminalAuthFields.ins, "INS", "82", 2, false);
    addTaCol(m_terminalAuthFields.p1, "P1", "00", 2, false);
    addTaCol(m_terminalAuthFields.pq, "Kq（原始值）", "", 2, false);
    addTaCol(m_terminalAuthFields.p2Display, "P2（Kq<<3）", "00", 2, true);
    addTaCol(m_terminalAuthFields.lc, "Lc", "08", 2, true);
    taRow->addStretch();
    taLay->addLayout(taRow);

    auto* taDataLbl = new QLabel("DATA — Encrypted Data（HEX，長度依實際輸入，不自動補 FF）：", taGrp);
    taDataLbl->setStyleSheet("font-weight:bold; font-size:11px;");
    m_terminalAuthFields.pin = makeEditField(taGrp, false);
    m_terminalAuthFields.pin->setPlaceholderText("輸入經亂碼後資料（HEX，請輸入完整長度，不會自動補 FF）");
    taLay->addWidget(taDataLbl);
    taLay->addWidget(m_terminalAuthFields.pin);

    auto* taHint = new QLabel(
        "P2 由 Kq 邏輯左移 3 位自動運算（例如輸入 01 → P2=08）", taGrp);
    taHint->setStyleSheet("color:#555; font-size:11px; padding:2px;");
    taLay->addWidget(taHint);

    connect(m_terminalAuthFields.pq, &QLineEdit::textChanged,
        this, &PageCardOperation::onUpdateTerminalAuthP2);

    auto* taBtn = new QPushButton("▶ 執行 Terminal Authentication", taGrp);
    taBtn->setStyleSheet(
        "QPushButton { background:#C62828; color:white; padding:8px; "
        "font-weight:bold; border-radius:4px; }");
    taLay->addWidget(taBtn);
    layout->addWidget(taGrp);
    connect(taBtn, &QPushButton::clicked, this, &PageCardOperation::onTerminalAuthentication);
    onUpdateTerminalAuthP2();   // 初始化顯示

    // ── ⑪ Set Session Key ───────────────────────────────────────────────────
    auto* sskCardGrp = new QGroupBox("⑪ Set Session Key", content);
    auto* sskCardLay = new QVBoxLayout(sskCardGrp);

    auto* sskCardRow = new QHBoxLayout();
    auto addSskCardCol = [&](QLineEdit*& outField, const QString& lbl,
        const QString& def, int maxLen, bool ro) {
            auto* col = new QVBoxLayout();
            auto* l = new QLabel(lbl, sskCardGrp);
            l->setAlignment(Qt::AlignCenter);
            l->setStyleSheet("font-weight:bold; font-size:11px;");
            outField = makeHexField(sskCardGrp, maxLen);
            outField->setText(def);
            outField->setReadOnly(ro);
            if (ro) {
                outField->setStyleSheet(
                    "QLineEdit { background:#EEE; color:#555; font-family:'Courier New'; "
                    "font-weight:bold; font-size:12px; padding:4px; "
                    "border:1px dashed #AAA; border-radius:3px; }");
            }
            col->addWidget(l);
            col->addWidget(outField);
            sskCardRow->addLayout(col);
        };

    addSskCardCol(m_setSessionKeyCardFields.cla, "CLA", "00", 2, false);
    addSskCardCol(m_setSessionKeyCardFields.ins, "INS", "84", 2, false);
    addSskCardCol(m_setSessionKeyCardFields.p1, "P1", "00", 2, false);
    addSskCardCol(m_setSessionKeyCardFields.pq, "Kq（原始值）", "", 2, false);
    addSskCardCol(m_setSessionKeyCardFields.p2Display, "P2（Kq<<3）", "00", 2, true);
    addSskCardCol(m_setSessionKeyCardFields.lc, "Lc", "08", 2, true);
    addSskCardCol(m_setSessionKeyCardFields.le, "Le", "08", 2, true);
    sskCardRow->addStretch();
    sskCardLay->addLayout(sskCardRow);

    auto* sskCardDataLbl = new QLabel("DATA — Random R1（8 bytes / 16 HEX）：", sskCardGrp);
    sskCardDataLbl->setStyleSheet("font-weight:bold; font-size:11px;");
    m_setSessionKeyCardFields.r1 = makeEditField(sskCardGrp, false);
    m_setSessionKeyCardFields.r1->setPlaceholderText("輸入 Terminal Random Number（HEX，不足自動補 FF）");
    sskCardLay->addWidget(sskCardDataLbl);
    sskCardLay->addWidget(m_setSessionKeyCardFields.r1);

    auto* sskCardHint = new QLabel(
        "P2 由 Kq 邏輯左移 3 位自動運算（例如輸入 01 → P2=08）", sskCardGrp);
    sskCardHint->setStyleSheet("color:#555; font-size:11px; padding:2px;");
    sskCardLay->addWidget(sskCardHint);

    connect(m_setSessionKeyCardFields.pq, &QLineEdit::textChanged,
        this, &PageCardOperation::onUpdateSessionKeyCardP2);

    auto* sskCardBtn = new QPushButton("▶ 執行 Set Session Key", sskCardGrp);
    sskCardBtn->setStyleSheet(
        "QPushButton { background:#6A1B9A; color:white; padding:8px; "
        "font-weight:bold; border-radius:4px; }");
    sskCardLay->addWidget(sskCardBtn);
    layout->addWidget(sskCardGrp);
    connect(sskCardBtn, &QPushButton::clicked, this, &PageCardOperation::onSetSessionKeyCard);
    onUpdateSessionKeyCardP2();   // 初始化顯示

    // ── ⑫ Generate Random Number ─────────────────────────────────────────────
    auto* grcGrp = new QGroupBox("⑫ Generate Random Number", content);
    auto* grcLay = new QVBoxLayout(grcGrp);
    auto* grcRow = new QHBoxLayout();
    auto addGrcCol = [&](QLineEdit*& outField, const QString& lbl,
        const QString& def, int maxLen, bool ro) {
            auto* col = new QVBoxLayout();
            auto* l = new QLabel(lbl, grcGrp);
            l->setAlignment(Qt::AlignCenter);
            l->setStyleSheet("font-weight:bold; font-size:11px;");
            outField = makeHexField(grcGrp, maxLen);
            outField->setText(def);
            outField->setReadOnly(ro);
            if (ro) {
                outField->setStyleSheet(
                    "QLineEdit { background:#EEE; color:#555; font-family:'Courier New'; "
                    "font-weight:bold; font-size:12px; padding:4px; "
                    "border:1px dashed #AAA; border-radius:3px; }");
            }
            col->addWidget(l);
            col->addWidget(outField);
            grcRow->addLayout(col);
        };

    addGrcCol(m_generateRandomCardFields.cla, "CLA", "00", 2, true);
    addGrcCol(m_generateRandomCardFields.ins, "INS", "86", 2, true);
    addGrcCol(m_generateRandomCardFields.p1, "P1", "00", 2, true);
    addGrcCol(m_generateRandomCardFields.p2, "P2", "00", 2, true);
    addGrcCol(m_generateRandomCardFields.le, "Le", "08", 2, true);
    grcRow->addStretch();
    grcLay->addLayout(grcRow);

    auto* grcBtn = new QPushButton("▶ 執行 Generate Random Number", grcGrp);
    grcBtn->setStyleSheet(
        "QPushButton { background:#1565C0; color:white; padding:8px; "
        "font-weight:bold; border-radius:4px; }");
    grcLay->addWidget(grcBtn);
    layout->addWidget(grcGrp);
    connect(grcBtn, &QPushButton::clicked, this, &PageCardOperation::onGenerateRandomCard);

    layout->addStretch();
    scroll->setWidget(content);

    auto* tabLayout = new QVBoxLayout(tab);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->addWidget(scroll);

    // ── 綁定事件 ──
    connect(selectBtn, &QPushButton::clicked, this, &PageCardOperation::onSelectFile);
    connect(writeBtn, &QPushButton::clicked, this, &PageCardOperation::onWriteRecord);
    connect(listBtn, &QPushButton::clicked, this, &PageCardOperation::onListFile);

    // 把 targetCombo 存起來供 slot 使用
    m_targetCombo->setObjectName("apduTargetCombo");
}

// ─────────────────────────────────────────────────────────────────────────────
// setupCreateFileGroup — Create File 含子欄位的詳細版本
//   CLA/INS/P1/P2/Lc 維持固定預設值；16 bytes DataIn 子欄位改為自行輸入
//   （原本的建議值僅以 placeholder 顯示，不再預先帶入）
// ─────────────────────────────────────────────────────────────────────────────
void PageCardOperation::setupCreateFileGroup(QWidget* parent, QVBoxLayout* layout)
{
    auto* grp = new QGroupBox("① Create File", parent);
    auto* grpLay = new QVBoxLayout(grp);

    // ── Header Row: CLA INS P1 P2 Lc(唯讀) ──────────────────────────────────
    auto* hdrRow = new QHBoxLayout();

    // 逐一建立 header 欄位（不用 lambda，避免 C2671）
    auto makeHdrCol = [&](QLineEdit*& outField, const QString& lbl,
        const QString& def, int maxLen, bool ro) {
            auto* col = new QVBoxLayout();
            auto* l = new QLabel(lbl, grp);
            l->setAlignment(Qt::AlignCenter);
            l->setStyleSheet("font-weight:bold; font-size:11px;");
            outField = new QLineEdit(grp);
            outField->setMaxLength(maxLen);
            outField->setFixedWidth(50);
            outField->setAlignment(Qt::AlignCenter);
            outField->setText(def);
            outField->setReadOnly(ro);
            outField->setStyleSheet(ro
                ? "QLineEdit { background:#EEE; color:#555; font-family:'Courier New'; "
                "font-weight:bold; font-size:12px; padding:4px; "
                "border:1px dashed #AAA; border-radius:3px; }"
                : "QLineEdit { background:#F9F9F9; color:#000; font-family:'Courier New'; "
                "font-weight:bold; font-size:12px; padding:4px; "
                "border:1px solid #CCC; border-radius:3px; }"
                "QLineEdit:focus { border:1px solid #0078D4; background:#FFF; }");
            col->addWidget(l);
            col->addWidget(outField);
            hdrRow->addLayout(col);
        };

    // Create File 預設固定：CLA:00 INS:E0 P1:00 P2:00 Lc:10（其他 DataIn 子欄位自行輸入）
    makeHdrCol(m_createFields.cla, "CLA", "00", 2, false);
    makeHdrCol(m_createFields.ins, "INS", "E0", 2, false);
    makeHdrCol(m_createFields.p1, "P1", "00", 2, false);
    makeHdrCol(m_createFields.p2, "P2", "00", 2, false);
    makeHdrCol(m_createFields.lcDisplay, "Lc", "10", 2, true);
    hdrRow->addStretch();
    grpLay->addLayout(hdrRow);

    // ── DataIn 子欄位（格線 4 欄）───────────────────────────────────────────
    auto* dataGrp = new QGroupBox("DataIn — 16 bytes（請自行輸入，以下僅為提示值）", grp);
    auto* dataGrid = new QGridLayout(dataGrp);
    dataGrid->setHorizontalSpacing(14);
    dataGrid->setVerticalSpacing(8);

    // 使用平行陣列取代巢狀 struct（避免 C2440 initializer_list 問題）
    QLineEdit** fieldPtrs[] = {
        &m_createFields.efid,
        &m_createFields.ac1,
        &m_createFields.ac2,
        &m_createFields.ac3,
        &m_createFields.sob,
        &m_createFields.tof,
        &m_createFields.sor,
        &m_createFields.rc,
        &m_createFields.index,
        &m_createFields.tok,
        &m_createFields.rfu,
    };
    const char* fieldLabels[] = {
        "#1~2  EFID/DFID", "#3~4  AC1", "#5~6  AC2", "#7~8  AC3",
        "#9~10 SoB", "#11  ToF", "#12  SoR", "#13  RC",
        "#14  Index", "#15  ToK", "#16  RFU"
    };
    const char* fieldHints[] = {
        "檔案識別碼 2B", "存取權限1 2B", "存取權限2 2B", "存取權限3 2B",
        "檔案空間  2B", "檔案類型  1B", "資料錄長度 1B", "錯誤限制  1B",
        "基碼認證  1B", "基碼類別  1B", "保留系統  1B"
    };
    // 提示值（僅作為 placeholder，不再自動帶入實際內容）
    const char* fieldDefs[] = {
        "00C0", "0040", "0500", "0400",
        "001C", "90",   "18",   "02",
        "01",   "02",   "00"
    };
    int fieldMaxLen[] = {
        4, 4, 4, 4,
        4, 2, 2, 2,
        2, 2, 2
    };
    const int fieldCount = 11;

    int gridRow = 0, gridCol = 0;
    for (int i = 0; i < fieldCount; ++i) {
        auto* cell = new QVBoxLayout();

        auto* lbl = new QLabel(QString::fromUtf8(fieldLabels[i]), dataGrp);
        lbl->setStyleSheet("font-weight:bold; font-size:10px; color:#333;");

        auto* hint = new QLabel(QString::fromUtf8(fieldHints[i]), dataGrp);
        hint->setStyleSheet("font-size:9px; color:#888;");

        QLineEdit* f = new QLineEdit(dataGrp);
        f->setMaxLength(fieldMaxLen[i]);
        f->setFixedWidth(fieldMaxLen[i] <= 2 ? 52 : 80);
        f->setAlignment(Qt::AlignCenter);
        // 不再預先帶入內容，改以 placeholder 顯示建議值，需使用者自行輸入
        f->setPlaceholderText(QString::fromLatin1(fieldDefs[i]));
        f->setStyleSheet(
            "QLineEdit { background:#F9F9F9; color:#000; font-family:'Courier New'; "
            "font-weight:bold; font-size:12px; padding:4px; "
            "border:1px solid #CCC; border-radius:3px; }"
            "QLineEdit:focus { border:1px solid #0078D4; background:#FFF; }");
        *(fieldPtrs[i]) = f;

        // 自動更新 Lc
        connect(f, &QLineEdit::textChanged, this, &PageCardOperation::onUpdateCreateLc);

        cell->addWidget(lbl);
        cell->addWidget(f);
        cell->addWidget(hint);
        dataGrid->addLayout(cell, gridRow, gridCol);
        if (++gridCol >= 4) { gridCol = 0; ++gridRow; }
    }
    grpLay->addWidget(dataGrp);

    auto* createBtn = new QPushButton("▶ 執行 Create File", grp);
    createBtn->setStyleSheet(
        "QPushButton { background:#5C6BC0; color:white; padding:8px; "
        "font-weight:bold; border-radius:4px; }");
    grpLay->addWidget(createBtn);
    connect(createBtn, &QPushButton::clicked, this, &PageCardOperation::onCreateFile);

    layout->addWidget(grp);
}

// ─────────────────────────────────────────────────────────────────────────────
// makeApduGroup — 建立一組 APDU 欄位 GroupBox
// ─────────────────────────────────────────────────────────────────────────────
QGroupBox* PageCardOperation::makeApduGroup(QWidget* parent,
    const QString& title,
    const QString& defaultCla,
    const QString& defaultIns,
    const QString& defaultP1,
    const QString& defaultP2,
    bool hasDataIn,
    bool hasLe,
    ApduFields& out)
{
    auto* grp = new QGroupBox(title, parent);
    auto* layout = new QVBoxLayout(grp);

    // 上排：CLA INS P1 P2 Lc Le（固定欄位）
    auto* fixRow = new QHBoxLayout();
    auto addHexField = [&](QLineEdit*& field, const QString& lbl,
        const QString& def, int maxLen) {
            auto* col = new QVBoxLayout();
            auto* label = new QLabel(lbl, grp);
            label->setAlignment(Qt::AlignCenter);
            label->setStyleSheet("font-weight:bold; font-size:11px;");
            field = makeHexField(grp, maxLen);
            field->setText(def);
            col->addWidget(label);
            col->addWidget(field);
            fixRow->addLayout(col);
        };

    addHexField(out.cla, "CLA", defaultCla, 2);
    addHexField(out.ins, "INS", defaultIns, 2);
    addHexField(out.p1, "P1", defaultP1, 2);
    addHexField(out.p2, "P2", defaultP2, 2);
    addHexField(out.lc, "Lc", "", 2);
    if (hasLe) {
        addHexField(out.le, "Le", "", 6);
    }
    else {
        out.le = nullptr;
    }
    layout->addLayout(fixRow);

    // 下排：DataIn（長欄位）
    if (hasDataIn) {
        auto* dataLabel = new QLabel("DataIn (HEX)：", grp);
        dataLabel->setStyleSheet("font-weight:bold; font-size:11px;");
        out.dataIn = makeEditField(grp, false);
        out.dataIn->setPlaceholderText("輸入 HEX 資料（不含空格）");
        layout->addWidget(dataLabel);
        layout->addWidget(out.dataIn);
    }
    else {
        out.dataIn = nullptr;
    }

    return grp;
}

// ─────────────────────────────────────────────────────────────────────────────
// 輔助：建立固定長度 HEX 欄位
// ─────────────────────────────────────────────────────────────────────────────
QLineEdit* PageCardOperation::makeHexField(QWidget* parent, int maxLen, const QString& placeholder)
{
    auto* f = new QLineEdit(parent);
    f->setMaxLength(maxLen > 0 ? maxLen : 999);
    f->setFixedWidth(maxLen <= 2 ? 40 : (maxLen <= 4 ? 60 : 120));
    f->setAlignment(Qt::AlignCenter);
    f->setPlaceholderText(placeholder.isEmpty()
        ? QString(maxLen, 'X').left(maxLen)
        : placeholder);
    f->setStyleSheet(
        "QLineEdit { background:#F9F9F9; color:#000; font-family:'Courier New'; "
        "font-weight:bold; font-size:12px; padding:4px; "
        "border:1px solid #CCC; border-radius:3px; }"
        "QLineEdit:focus { border:1px solid #0078D4; background:#FFF; }");
    return f;
}

QLineEdit* PageCardOperation::makeEditField(QWidget* parent, bool readOnly)
{
    auto* f = new QLineEdit(parent);
    f->setReadOnly(readOnly);
    f->setStyleSheet(
        "QLineEdit { background:#F9F9F9; color:#000; font-family:'Courier New'; "
        "font-size:11px; padding:4px; border:1px solid #CCC; border-radius:3px; }"
        "QLineEdit:focus { border:1px solid #0078D4; background:#FFF; }");
    return f;
}

// ─────────────────────────────────────────────────────────────────────────────
// 輔助：取「卡片指令」頁籤目前選擇的目標讀卡機 handle（惰性連線）
// ─────────────────────────────────────────────────────────────────────────────
SCARDHANDLE PageCardOperation::getTargetHandle() const
{
    auto* combo = findChild<QComboBox*>("apduTargetCombo");
    QString readerName = combo ? combo->currentText() : QString();
    return connectReader(readerName);
}

// ─────────────────────────────────────────────────────────────────────────────
// 輔助：送出 APDU 並 log
// ─────────────────────────────────────────────────────────────────────────────
bool PageCardOperation::sendApduFromFields(SCARDHANDLE hCard,
    const ApduFields& fields,
    const QString& label)
{
    QString apdu = fields.buildApdu();
    if (apdu.length() < 8) {
        log(QString("[%1] APDU 格式不完整（CLA INS P1 P2 至少需填寫）").arg(label), true);
        return false;
    }
    if (apdu.length() % 2 != 0) {
        log(QString("[%1] APDU HEX 長度奇數，請確認欄位填寫正確").arg(label), true);
        return false;
    }

    log(QString("====== %1 ======").arg(label));
    bool ok = false;
    QString resp = CardUtils::sendApdu(hCard, apdu, ok,
        [this](const QString& msg, bool err) { log(msg, err); });

    QString sw = resp.right(4);
    QString data = resp.left(qMax(0, resp.length() - 4));
    if (!ok) { log("[" + label + "] 通訊失敗", true); return false; }

    if (sw == "9000") {
        log(QString("✔ [%1] 成功 SW=9000").arg(label));
        if (!data.isEmpty())
            log("Data: " + data);
    }
    else {
        log(QString("⚠️ [%1] SW=%2（非 9000，請確認參數）").arg(label).arg(sw), true);
    }

    return (sw == "9000");
}

// ─────────────────────────────────────────────────────────────────────────────
// Slots — ISAM Tab
// ─────────────────────────────────────────────────────────────────────────────
void PageCardOperation::onIsamKeySelected(int /*index*/)
{
    int id = m_isamKeyCombo->currentData().toInt();
    if (id < 0) {
        m_isamC3CodeLbl->setText("—");
        m_isamC1CodeLbl->setText("—");
        m_updateIsamBtn->setEnabled(false);
        if (m_checkIsamKeyBtn) m_checkIsamKeyBtn->setEnabled(false);
        return;
    }
    for (const auto& e : IsamKeyData::entries()) {
        if (e.id == id) {
            // 只顯示 Code，不顯示金鑰本體
            m_isamC3CodeLbl->setText(e.c3Code);
            m_isamC1CodeLbl->setText(e.c1Code);
            m_updateIsamBtn->setEnabled(true);
            if (m_checkIsamKeyBtn) m_checkIsamKeyBtn->setEnabled(true);
            log(QString("已選擇 KEY：%1  (C3Code=%2, C1Code=%3)")
                .arg(e.displayName).arg(e.c3Code).arg(e.c1Code));
            break;
        }
    }
}

void PageCardOperation::onViewIsam()
{
    SCARDHANDLE hIsam = connectReader(m_isamKeyReaderCombo ? m_isamKeyReaderCombo->currentText() : QString());
    if (hIsam == 0) { log("Error: 無效的 ISAM 卡片控制代碼", true); return; }

    log("====== 查看 ISAM ======");

    auto logFn = [this](const QString& msg, bool err) { log(msg, err); };

    // 1. Select ISAM Object
    bool ok = false;
    QString resp = CardUtils::sendApdu(hIsam, "82A400000295020E", ok, logFn);
    QString sw = resp.right(4);
    if (!ok || sw != "9000") {
        log(QString("Error: [Select ISAM Object] 失敗 (SW=%1)").arg(sw), true);
        QMessageBox::critical(this, "卡片錯誤", "查看 ISAM 失敗，請查看日誌。");
        return;
    }

    // 2. 查詢 C3 對照碼
    QString c3Resp = CardUtils::sendApdu(hIsam, "8207050007", ok, logFn);
    QString c3Sw = c3Resp.right(4);
    if (!ok || c3Sw != "9000") {
        log(QString("Error: [查詢 C3 對照碼] 失敗 (SW=%1)").arg(c3Sw), true);
        QMessageBox::critical(this, "卡片錯誤", "查看 ISAM 失敗，請查看日誌。");
        return;
    }
    QString c3Check = c3Resp.left(4);

    // 3. 查詢 C1 對照碼
    QString c1Resp = CardUtils::sendApdu(hIsam, "8207060007", ok, logFn);
    QString c1Sw = c1Resp.right(4);
    if (!ok || c1Sw != "9000") {
        log(QString("Error: [查詢 C1 對照碼] 失敗 (SW=%1)").arg(c1Sw), true);
        QMessageBox::critical(this, "卡片錯誤", "查看 ISAM 失敗，請查看日誌。");
        return;
    }
    QString c1Check = c1Resp.left(4);

    if (m_isamViewC3Lbl) m_isamViewC3Lbl->setText(c3Check);
    if (m_isamViewC1Lbl) m_isamViewC1Lbl->setText(c1Check);

    log(QString("✔ 查看 ISAM 完成：C3 對照碼=%1  C1 對照碼=%2").arg(c3Check, c1Check));
    QMessageBox::information(this, "查看完成",
        QString("目前卡片 ISAM 金鑰對照碼：\nC3：%1\nC1：%2").arg(c3Check, c1Check));
}

void PageCardOperation::onUpdateIsam()
{
    int id = m_isamKeyCombo->currentData().toInt();
    if (id < 0) return;

    const IsamKeyEntry* entry = nullptr;
    for (const auto& e : IsamKeyData::entries())
        if (e.id == id) { entry = &e; break; }
    if (!entry) return;

    CustomerData d;
    d.c3 = entry->c3;
    d.c3Code = entry->c3Code;
    d.c1 = entry->c1;
    d.c1Code = entry->c1Code;

    SCARDHANDLE hIsam = connectReader(m_isamKeyReaderCombo ? m_isamKeyReaderCombo->currentText() : QString());
    if (hIsam == 0) { log("Error: 無效的 ISAM 卡片控制代碼", true); return; }

    log("====== 開始 ISAM 金鑰更新 ======");
    IsamKeyUpdater updater(hIsam);
    bool ok = updater.execute(d, [this](const QString& msg, bool err) { log(msg, err); });

    if (ok) QMessageBox::information(this, "完成", "✔ ISAM 金鑰更新成功！");
    else    QMessageBox::critical(this, "失敗", "ISAM 金鑰更新失敗，請查看日誌。");
}

void PageCardOperation::onCheckIsamKey()
{
    int id = m_isamKeyCombo->currentData().toInt();
    if (id < 0) return;

    const IsamKeyEntry* entry = nullptr;
    for (const auto& e : IsamKeyData::entries())
        if (e.id == id) { entry = &e; break; }
    if (!entry) return;

    CustomerData d;
    d.c3 = entry->c3;
    d.c3Code = entry->c3Code;
    d.c1 = entry->c1;
    d.c1Code = entry->c1Code;

    SCARDHANDLE hIsam = connectReader(m_isamKeyReaderCombo ? m_isamKeyReaderCombo->currentText() : QString());
    if (hIsam == 0) { log("Error: 無效的 ISAM 卡片控制代碼", true); return; }

    log("====== 開始檢查卡片 KEY ======");
    IsamKeyChecker checker(hIsam);
    // 目前未比對 List File，如需比對請將卡片預期的 List File 內容傳入第二個參數
    KeyCheckResult result = checker.execute(d, QString(),
        [this](const QString& msg, bool err) { log(msg, err); });

    if (result.success)
        QMessageBox::information(this, "檢查完成",
            QString("✔ 卡片 KEY 檢查通過！\nICID：%1").arg(result.icid));
    else
        QMessageBox::critical(this, "檢查失敗",
            QString("❌ 卡片 KEY 檢查未通過。\nICID：%1\n狀態碼：%2\n請查看日誌。")
            .arg(result.icid, result.lastStatus));
}

// ─────────────────────────────────────────────────────────────────────────────
// Slots — ISAM 指令 Tab（皆發送至使用者選擇的讀卡機）
// ─────────────────────────────────────────────────────────────────────────────
void PageCardOperation::onIsamSelectObject()
{
    SCARDHANDLE hIsam = connectReader(m_isamCmdReaderCombo ? m_isamCmdReaderCombo->currentText() : QString());
    if (hIsam == 0) { log("Error: 無效的 ISAM 卡片控制代碼", true); return; }
    sendApduFromFields(hIsam, m_isamSelectObjFields, "Select ISAM Object");
}

void PageCardOperation::onIsamGetKeyCheckCode()
{
    SCARDHANDLE hIsam = connectReader(m_isamCmdReaderCombo ? m_isamCmdReaderCombo->currentText() : QString());
    if (hIsam == 0) { log("Error: 無效的 ISAM 卡片控制代碼", true); return; }

    QString apdu = m_isamKeyCheckFields.buildApdu();
    if (apdu.length() < 8) {
        log("[Get Key Check Code] APDU 不完整，請確認欄位", true);
        return;
    }

    log("====== Get Key Check Code ======");
    bool ok = false;
    QString resp = CardUtils::sendApdu(hIsam, apdu, ok,
        [this](const QString& msg, bool err) { log(msg, err); });
    if (!ok) { log("[Get Key Check Code] 通訊失敗", true); return; }

    QString sw = resp.right(4);
    QString data = resp.left(qMax(0, resp.length() - 4));
    if (sw == "9000") {
        log("✔ Get Key Check Code 成功 SW=9000");
        log("Key Check Code: " + (data.isEmpty() ? QString("(空)") : data));
    }
    else {
        log("⚠️ Get Key Check Code SW=" + sw, true);
    }
}

void PageCardOperation::onIsamGenerateRandom()
{
    SCARDHANDLE hIsam = connectReader(m_isamCmdReaderCombo ? m_isamCmdReaderCombo->currentText() : QString());
    if (hIsam == 0) { log("Error: 無效的 ISAM 卡片控制代碼", true); return; }

    QString apdu = m_isamRandomFields.buildApdu();
    if (apdu.length() < 8) {
        log("[Generate Random No] APDU 不完整，請確認欄位", true);
        return;
    }

    log("====== Generate Random No ======");
    bool ok = false;
    QString resp = CardUtils::sendApdu(hIsam, apdu, ok,
        [this](const QString& msg, bool err) { log(msg, err); });
    if (!ok) { log("[Generate Random No] 通訊失敗", true); return; }

    QString sw = resp.right(4);
    QString data = resp.left(qMax(0, resp.length() - 4));
    if (sw == "9000") {
        log("✔ Generate Random No 成功 SW=9000");
        log("Random: " + (data.isEmpty() ? QString("(空)") : data));
    }
    else {
        log("⚠️ Generate Random No SW=" + sw, true);
    }
}

void PageCardOperation::onIsamGetTerminalAuthCode()
{
    SCARDHANDLE hIsam = connectReader(m_isamCmdReaderCombo ? m_isamCmdReaderCombo->currentText() : QString());
    if (hIsam == 0) { log("Error: 無效的 ISAM 卡片控制代碼", true); return; }
    sendApduFromFields(hIsam, m_isamTermAuthFields, "Get TerminalAuthCode");
}

void PageCardOperation::onIsamGetEncryptedKey()
{
    SCARDHANDLE hIsam = connectReader(m_isamCmdReaderCombo ? m_isamCmdReaderCombo->currentText() : QString());
    if (hIsam == 0) { log("Error: 無效的 ISAM 卡片控制代碼", true); return; }

    QString apdu = m_isamEncKeyFields.buildApdu();
    if (apdu.length() < 8) {
        log("[Get Encrypted Key] APDU 不完整，請確認欄位", true);
        return;
    }

    log("====== Get Encrypted Key ======");
    bool ok = false;
    QString resp = CardUtils::sendApdu(hIsam, apdu, ok,
        [this](const QString& msg, bool err) { log(msg, err); });
    if (!ok) { log("[Get Encrypted Key] 通訊失敗", true); return; }

    QString sw = resp.right(4);
    QString data = resp.left(qMax(0, resp.length() - 4));
    if (sw == "9000") {
        log("✔ Get Encrypted Key 成功 SW=9000");
        log("Encrypted Key: " + (data.isEmpty() ? QString("(空)") : data));
    }
    else {
        log("⚠️ Get Encrypted Key SW=" + sw, true);
    }
}

void PageCardOperation::onUpdateWriteKeyEKey()
{
    if (!m_writeKeyFields.eKey) return;

    QString cd = m_writeKeyFields.cdKey
        ? m_writeKeyFields.cdKey->text().trimmed().remove(' ').toUpper()
        : QString();
    QString exch = m_writeKeyFields.exchangeKey
        ? m_writeKeyFields.exchangeKey->text().trimmed().remove(' ').toUpper()
        : QString();

    QString eKeyVal;
    if (!cd.isEmpty() && !exch.isEmpty())
        eKeyVal = CardUtils::encrypt3DES(cd, exch);   // CD Key 3DES 加密 Exchange Key → EKey

    m_writeKeyFields.eKey->setText(eKeyVal);
    onUpdateWriteKeyLc();
}

void PageCardOperation::onUpdateWriteKeyLc()
{
    if (m_writeKeyFields.lcDisplay) {
        QString data = m_writeKeyFields.buildDataIn();
        m_writeKeyFields.lcDisplay->setText(
            QString("%1").arg(data.length() / 2, 2, 16, QChar('0')).toUpper());
    }
}

void PageCardOperation::onIsamWriteKey()
{
    SCARDHANDLE hIsam = connectReader(m_isamCmdReaderCombo ? m_isamCmdReaderCombo->currentText() : QString());
    if (hIsam == 0) { log("Error: 無效的 ISAM 卡片控制代碼", true); return; }

    onUpdateWriteKeyEKey();   // 確保 EKey / Lc 為最新值
    if (m_writeKeyFields.eKey && m_writeKeyFields.eKey->text().isEmpty()) {
        log("[Write Key] 請先輸入 CD Key 與 Exchange Key 以算出 EKey", true);
        return;
    }

    QString apdu = m_writeKeyFields.buildApdu();
    if (apdu.length() < 10) {
        log("[Write Key] APDU 不完整，請確認欄位", true);
        return;
    }

    log("====== Write Key ======");
    log("EKey: " + m_writeKeyFields.eKey->text());
    log("DataIn: " + m_writeKeyFields.buildDataIn());
    bool ok = false;
    QString resp = CardUtils::sendApdu(hIsam, apdu, ok,
        [this](const QString& msg, bool err) { log(msg, err); });
    if (!ok) { log("[Write Key] 通訊失敗", true); return; }

    QString sw = resp.right(4);
    if (sw == "9000")
        log("✔ Write Key 成功 SW=9000");
    else
        log("⚠️ Write Key SW=" + sw, true);
}

void PageCardOperation::onUpdateStoreMfgKeyLc()
{
    if (m_storeMfgKeyFields.lcDisplay) {
        QString data = m_storeMfgKeyFields.buildDataIn();
        m_storeMfgKeyFields.lcDisplay->setText(
            QString("%1").arg(data.length() / 2, 2, 16, QChar('0')).toUpper());
    }
}

void PageCardOperation::onIsamStoreManufacturerKey()
{
    SCARDHANDLE hIsam = connectReader(m_isamCmdReaderCombo ? m_isamCmdReaderCombo->currentText() : QString());
    if (hIsam == 0) { log("Error: 無效的 ISAM 卡片控制代碼", true); return; }

    QString apdu = m_storeMfgKeyFields.buildApdu();   // 呼叫後 Lc 顯示會同步更新
    onUpdateStoreMfgKeyLc();
    if (apdu.length() < 10) {
        log("[Store Manufacturer Key] APDU 不完整，請確認欄位", true);
        return;
    }

    log("====== Store Manufacturer Key ======");
    log("DataIn: " + m_storeMfgKeyFields.buildDataIn());
    bool ok = false;
    QString resp = CardUtils::sendApdu(hIsam, apdu, ok,
        [this](const QString& msg, bool err) { log(msg, err); });
    if (!ok) { log("[Store Manufacturer Key] 通訊失敗", true); return; }

    QString sw = resp.right(4);
    if (sw == "9000")
        log("✔ Store Manufacturer Key 成功 SW=9000");
    else
        log("⚠️ Store Manufacturer Key SW=" + sw, true);
}

void PageCardOperation::onUpdateSetSessionKeyLc()
{
    QString data = m_setSessionKeyIsamFields.buildDataIn();

    if (m_setSessionKeyIsamFields.lcDisplay) {
        m_setSessionKeyIsamFields.lcDisplay->setText(
            QString("%1").arg(data.length() / 2, 2, 16, QChar('0')).toUpper());
    }
    if (m_setSessionKeyIsamFields.dataDisplay) {
        m_setSessionKeyIsamFields.dataDisplay->setText(data);
    }
}

void PageCardOperation::onIsamSetSessionKey()
{
    SCARDHANDLE hIsam = connectReader(m_isamCmdReaderCombo ? m_isamCmdReaderCombo->currentText() : QString());
    if (hIsam == 0) { log("Error: 無效的 ISAM 卡片控制代碼", true); return; }

    QString apdu = m_setSessionKeyIsamFields.buildApdu();   // 呼叫後 Lc 顯示會同步更新
    onUpdateSetSessionKeyLc();
    if (apdu.length() < 10) {
        log("[Set Session Key] APDU 不完整，請確認欄位", true);
        return;
    }

    log("====== Set Session Key ======");
    bool ok = false;
    QString resp = CardUtils::sendApdu(hIsam, apdu, ok,
        [this](const QString& msg, bool err) { log(msg, err); });
    if (!ok) { log("[Set Session Key] 通訊失敗", true); return; }

    QString sw = resp.right(4);
    if (sw == "9000")
        log("✔ Set Session Key 成功 SW=9000");
    else
        log("⚠️ Set Session Key SW=" + sw, true);
}

void PageCardOperation::onUpdateDiversifyKeyLc()
{
    if (m_diversifyKeyFields.lcDisplay) {
        QString data = m_diversifyKeyFields.divData
            ? m_diversifyKeyFields.divData->text().trimmed().remove(' ').toUpper()
            : QString();
        m_diversifyKeyFields.lcDisplay->setText(
            QString("%1").arg(data.length() / 2, 2, 16, QChar('0')).toUpper());
    }
}

void PageCardOperation::onIsamGetDiversifyKey()
{
    SCARDHANDLE hIsam = connectReader(m_isamCmdReaderCombo ? m_isamCmdReaderCombo->currentText() : QString());
    if (hIsam == 0) { log("Error: 無效的 ISAM 卡片控制代碼", true); return; }

    QString apdu = m_diversifyKeyFields.buildApdu();
    onUpdateDiversifyKeyLc();
    if (apdu.length() < 10) {
        log("[Get Diversify Key with Session Key] APDU 不完整，請確認欄位", true);
        return;
    }

    log("====== Get Diversify Key with Session Key ======");
    bool ok = false;
    QString resp = CardUtils::sendApdu(hIsam, apdu, ok,
        [this](const QString& msg, bool err) { log(msg, err); });
    if (!ok) { log("[Get Diversify Key with Session Key] 通訊失敗", true); return; }

    QString sw = resp.right(4);
    QString data = resp.left(qMax(0, resp.length() - 4));
    if (sw == "9000") {
        log("✔ Get Diversify Key with Session Key 成功 SW=9000");
        log("Diversify Key: " + (data.isEmpty() ? QString("(空)") : data));
    }
    else {
        log("⚠️ Get Diversify Key with Session Key SW=" + sw, true);
    }
}

void PageCardOperation::onIsamGetWorkingKey()
{
    SCARDHANDLE hIsam = connectReader(m_isamCmdReaderCombo ? m_isamCmdReaderCombo->currentText() : QString());
    if (hIsam == 0) { log("Error: 無效的 ISAM 卡片控制代碼", true); return; }

    QString apdu = m_workingKeyFields.buildApdu();
    if (apdu.length() < 8) {
        log("[Get Working Key with Session Key] APDU 不完整，請確認欄位", true);
        return;
    }

    log("====== Get Working Key with Session Key ======");
    bool ok = false;
    QString resp = CardUtils::sendApdu(hIsam, apdu, ok,
        [this](const QString& msg, bool err) { log(msg, err); });
    if (!ok) { log("[Get Working Key with Session Key] 通訊失敗", true); return; }

    QString sw = resp.right(4);
    QString data = resp.left(qMax(0, resp.length() - 4));
    if (sw == "9000") {
        log("✔ Get Working Key with Session Key 成功 SW=9000");
        log("Working Key: " + (data.isEmpty() ? QString("(空)") : data));
    }
    else {
        log("⚠️ Get Working Key with Session Key SW=" + sw, true);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Slots — LOD Tab
// ─────────────────────────────────────────────────────────────────────────────
void PageCardOperation::onLoadFile()
{
    QString path = QFileDialog::getOpenFileName(
        this, "選擇客戶資料檔案", {},
        "LOD 檔案 (*.lod *.LOD);;所有檔案 (*.*)");
    if (path.isEmpty()) return;

    QStringList warnings;
    CustomerData tmp;
    if (!LodLoader::load(path, tmp, &warnings)) {
        QMessageBox::critical(this, "載入失敗", "無法解析 LOD 檔案（至少需包含 C3 與 C1）。");
        return;
    }

    m_data = tmp; m_loaded = true;
    m_fileLabel->setText("✔ " + path.section('/', -1));
    m_fileLabel->setStyleSheet("color:#2E7D32; font-weight:bold;");
    displayData(m_data);
    if (m_canMakeMaster)
        m_makeMasterBtn->setEnabled(true);
    log("✔ LOD 載入：" + m_data.name);
    for (const auto& w : warnings) log("⚠️ " + w, true);
}

// ─────────────────────────────────────────────────────────────────────────────
// 客戶資料欄位變更檢查：不論是載入 LOD 或手動輸入，只要 C3 / C1 都有值
// 即可啟用「製作母卡」按鈕。
// ─────────────────────────────────────────────────────────────────────────────
void PageCardOperation::onCustomerFieldChanged()
{
    if (!m_canMakeMaster || !m_makeMasterBtn) return;

    bool hasData = m_c3Edit && !m_c3Edit->text().trimmed().isEmpty()
        && m_c1Edit && !m_c1Edit->text().trimmed().isEmpty();

    m_makeMasterBtn->setEnabled(hasData);
}

void PageCardOperation::onAuthenticate()
{
    if (!m_canMakeMaster) {
        log("讀卡機數量不足 2 台，不提供母卡認證功能。", true);
        return;
    }

    QString isamReaderName = m_masterIsamReaderCombo ? m_masterIsamReaderCombo->currentText() : QString();
    QString cardReaderName = m_masterCardReaderCombo ? m_masterCardReaderCombo->currentText() : QString();
    if (isamReaderName.isEmpty() || cardReaderName.isEmpty() || isamReaderName == cardReaderName) {
        QMessageBox::warning(this, "設定錯誤", "請選擇兩台不同的讀卡機（ISAM／母卡）。");
        return;
    }

    SCARDHANDLE hIsam = connectReader(isamReaderName);
    SCARDHANDLE hMaster = connectReader(cardReaderName);
    if (hIsam == 0 || hMaster == 0) {
        QMessageBox::critical(this, "連線失敗", "無法連線所選讀卡機，請確認卡片已插好。");
        return;
    }

    log("====== 🔐 母卡認證 ======");
    MasterCardUpdater mc(hMaster, hIsam);
    bool ok = mc.authenticate([this](const QString& msg, bool err) { log(msg, err); });

    if (ok) {
        m_authenticated = true;
        m_authBtn->setStyleSheet(
            "QPushButton { background:#2E7D32; color:white; padding:8px 16px; "
            "font-weight:bold; border-radius:4px; }");
        m_authBtn->setText("✔ 已認證");
        QMessageBox::information(this, "認證成功", "🎉 母卡認證成功，可以進行寫入！");
    }
    else {
        m_authenticated = false;
        m_authBtn->setStyleSheet(
            "QPushButton { background:#6A1B9A; color:white; padding:8px 16px; "
            "font-weight:bold; border-radius:4px; }");
        m_authBtn->setText("🔐 母卡認證");
        QMessageBox::critical(this, "認證失敗", "母卡認證失敗，請查看日誌。");
    }
}

void PageCardOperation::onMakeMaster()
{
    if (!m_canMakeMaster) {
        log("讀卡機數量不足 2 台，不提供製作母卡功能。", true);
        return;
    }

    QString isamReaderName = m_masterIsamReaderCombo ? m_masterIsamReaderCombo->currentText() : QString();
    QString cardReaderName = m_masterCardReaderCombo ? m_masterCardReaderCombo->currentText() : QString();
    if (isamReaderName.isEmpty() || cardReaderName.isEmpty() || isamReaderName == cardReaderName) {
        QMessageBox::warning(this, "設定錯誤", "請選擇兩台不同的讀卡機（ISAM／母卡）。");
        return;
    }

    SCARDHANDLE hIsam = connectReader(isamReaderName);
    SCARDHANDLE hMaster = connectReader(cardReaderName);
    if (hIsam == 0 || hMaster == 0) {
        QMessageBox::critical(this, "連線失敗", "無法連線所選讀卡機，請確認卡片已插好。");
        return;
    }

    CustomerData d = collectDataFromFields();
    if (d.c3.isEmpty() || d.c1.isEmpty()) {
        QMessageBox::warning(this, "資料不足", "至少需要填入 C3 / C1 金鑰！");
        return;
    }

    // 若尚未認證，先自動執行認證
    if (!m_authenticated) {
        log("⚠️ 尚未認證，自動執行認證...");
        MasterCardUpdater mcAuth(hMaster, hIsam);
        if (!mcAuth.authenticate([this](const QString& msg, bool err) { log(msg, err); })) {
            QMessageBox::critical(this, "失敗", "母卡認證失敗，請先按「母卡認證」後再試。");
            return;
        }
        m_authenticated = true;
    }

    log("====== 開始製作母卡 ======");
    MasterCardUpdater mc(hMaster, hIsam);
    bool ok = mc.writeData(d, d.pinA, d.pinB, [this](const QString& m, bool e) { log(m, e); });

    // 寫入後重置認證狀態（每次寫入都需要重新認證）
    m_authenticated = false;
    m_authBtn->setText("🔐 母卡認證");
    m_authBtn->setStyleSheet(
        "QPushButton { background:#6A1B9A; color:white; padding:8px 16px; "
        "font-weight:bold; border-radius:4px; }");

    if (ok) QMessageBox::information(this, "完成", "🎉 母卡寫入完成！");
    else    QMessageBox::critical(this, "失敗", "❌ 寫入失敗，請查看日誌。");
}

void PageCardOperation::onConvertMaster()
{
    if (!m_canMakeMaster) {
        log("讀卡機數量不足 2 台，不提供母卡轉換功能。", true);
        return;
    }

    QString isamReaderName = m_masterIsamReaderCombo ? m_masterIsamReaderCombo->currentText() : QString();
    QString cardReaderName = m_masterCardReaderCombo ? m_masterCardReaderCombo->currentText() : QString();
    if (isamReaderName.isEmpty() || cardReaderName.isEmpty() || isamReaderName == cardReaderName) {
        QMessageBox::warning(this, "設定錯誤", "請選擇兩台不同的讀卡機（ISAM／母卡）。");
        return;
    }

    SCARDHANDLE hIsam = connectReader(isamReaderName);
    SCARDHANDLE hMaster = connectReader(cardReaderName);
    if (hIsam == 0 || hMaster == 0) {
        QMessageBox::critical(this, "連線失敗", "無法連線所選讀卡機，請確認卡片已插好。");
        return;
    }

    CustomerData d = collectDataFromFields();
    if (d.exchangeCode.isEmpty() || d.ccode.isEmpty()) {
        QMessageBox::warning(this, "資料不足", "請確認 Exchange Code 與 CCode 皆已填寫！");
        return;
    }
    if (d.pinA.isEmpty() || d.pinB.isEmpty()) {
        QMessageBox::warning(this, "資料不足", "請確認 Pin A / Pin B 皆已填寫！");
        return;
    }

    log("====== 開始母卡轉換 ======");
    MasterCardUpdater mc(hMaster, hIsam);
    bool ok = mc.convertToBankIsam(d, [this](const QString& m, bool e) { log(m, e); });

    if (ok) QMessageBox::information(this, "完成", "🎉 母卡轉換完成！");
    else    QMessageBox::critical(this, "失敗", "❌ 母卡轉換失敗，請查看日誌。");
}

void PageCardOperation::onWashCard()
{
    if (!m_canMakeMaster) {
        log("讀卡機數量不足 2 台，不提供洗卡功能。", true);
        return;
    }

    QString isamReaderName = m_masterIsamReaderCombo ? m_masterIsamReaderCombo->currentText() : QString();
    QString cardReaderName = m_masterCardReaderCombo ? m_masterCardReaderCombo->currentText() : QString();
    if (isamReaderName.isEmpty() || cardReaderName.isEmpty() || isamReaderName == cardReaderName) {
        QMessageBox::warning(this, "設定錯誤", "請選擇兩台不同的讀卡機（ISAM／卡片）。");
        return;
    }

    SCARDHANDLE hIsam = connectReader(isamReaderName);
    SCARDHANDLE hCard = connectReader(cardReaderName);
    if (hIsam == 0 || hCard == 0) {
        QMessageBox::critical(this, "連線失敗", "無法連線所選讀卡機，請確認卡片已插好。");
        return;
    }

    log("====== 開始洗卡 ======");
    CardWasher washer(hCard, hIsam);
    bool ok = washer.execute([this](const QString& m, bool e) { log(m, e); });

    if (ok) QMessageBox::information(this, "完成", "🎉 洗卡完成！");
    else    QMessageBox::critical(this, "失敗", "❌ 洗卡失敗，請查看日誌。");
}

void PageCardOperation::onClearFields()
{
    for (auto* f : { m_nameEdit, m_c3Edit, m_c3CodeEdit, m_c1Edit, m_c1CodeEdit,
                    m_codeAEdit, m_codeBEdit, m_exchEdit, m_ccodeEdit, m_pinAEdit, m_pinBEdit })
        f->clear();
    m_loaded = false;
    m_fileLabel->setText("尚未載入 — 或直接手動輸入");
    m_fileLabel->setStyleSheet("color:#888; font-style:italic;");
    m_makeMasterBtn->setEnabled(false);
    log("欄位已清除。");
}

// ─────────────────────────────────────────────────────────────────────────────
// Slots — 卡片指令 Tab
// ─────────────────────────────────────────────────────────────────────────────
void PageCardOperation::onCreateFile()
{
    QString apdu = m_createFields.buildApdu();
    if (apdu.length() < 8) {
        log("[Create File] APDU 不完整，請確認欄位", true);
        return;
    }

    // 更新 Lc 顯示
    QString dataIn = m_createFields.buildDataIn();
    if (m_createFields.lcDisplay)
        m_createFields.lcDisplay->setText(
            QString("%1").arg(dataIn.length() / 2, 2, 16, QChar('0')).toUpper());

    log("====== Create File ======");
    log("DataIn: " + dataIn);
    bool ok = false;
    QString resp = CardUtils::sendApdu(getTargetHandle(), apdu, ok,
        [this](const QString& msg, bool err) { log(msg, err); });
    if (!ok) return;
    QString sw = resp.right(4);
    if (sw == "9000")
        log("✔ Create File 成功 SW=9000");
    else
        log("⚠️ Create File SW=" + sw, true);
}

void PageCardOperation::onUpdateCreateLc()
{
    if (m_createFields.lcDisplay) {
        QString di = m_createFields.buildDataIn();
        m_createFields.lcDisplay->setText(
            QString("%1").arg(di.length() / 2, 2, 16, QChar('0')).toUpper());
    }
}

void PageCardOperation::onSelectFile()
{
    sendApduFromFields(getTargetHandle(), m_selectFields, "Select File");
}

void PageCardOperation::onWriteRecord()
{
    sendApduFromFields(getTargetHandle(), m_writeFields, "Write Record");
}

void PageCardOperation::onListFile()
{
    log("====== List File ======");
    bool ok = false;
    SCARDHANDLE hCard = getTargetHandle();
    QString resp = CardUtils::sendApdu(hCard, "00AA000000", ok,
        [this](const QString& msg, bool err) { log(msg, err); });

    if (!ok) { log("List File 通訊失敗", true); return; }

    QString sw = resp.right(4);
    QString data = resp.left(resp.length() - 4);

    if (sw != "9000") {
        log(QString("⚠️ List File 回傳 SW=%1").arg(sw), true);
        return;
    }

    // ToF 對應表
    auto tofDesc = [](const QString& tofHex) -> QString {
        QString t = tofHex.toUpper();
        if (t == "01") return "DF";
        if (t == "00") return "DEF";
        if (t == "90") return "AEF";
        if (t == "B0") return "NEF";
        if (t == "A0") return "TEF";
        if (t == "C0") return "SEF";
        if (t == "D0") return "TSEF";
        if (t == "E0") return "BEF";
        return "Unknown(" + tofHex + ")";
        };

    // 解析 FIDList：每 3 bytes = FID(2bytes) + ToF(1byte) = 6 HEX chars
    log("✔ List File 成功，解析結果：");
    if (data.isEmpty()) {
        log("  （無檔案）");
        return;
    }

    int fileCount = 0;
    for (int i = 0; i + 6 <= data.length(); i += 6) {
        QString fid = data.mid(i, 4).toUpper();
        QString tof = data.mid(i + 4, 2).toUpper();
        log(QString("  [%1] %2 [%3]")
            .arg(++fileCount)
            .arg(fid)
            .arg(tofDesc(tof)));
    }
    log(QString("  共 %1 個檔案").arg(fileCount));
}

void PageCardOperation::onUpdateVerifyPinP2()
{
    if (m_verifyPinFields.p2Display)
        m_verifyPinFields.p2Display->setText(m_verifyPinFields.computedP2());
}

void PageCardOperation::onVerifyPin()
{
    QString apdu = m_verifyPinFields.buildApdu();   // 呼叫後 p2Display / lc 會同步更新
    if (apdu.length() < 8) {
        log("[Verify Pin] APDU 不完整，請確認欄位", true);
        return;
    }

    log("====== Verify Pin ======");
    log(QString("Pq 邏輯左移 3 位 → P2 = %1")
        .arg(m_verifyPinFields.p2Display ? m_verifyPinFields.p2Display->text() : QString()));

    bool ok = false;
    QString resp = CardUtils::sendApdu(getTargetHandle(), apdu, ok,
        [this](const QString& msg, bool err) { log(msg, err); });
    if (!ok) { log("[Verify Pin] 通訊失敗", true); return; }

    QString sw = resp.right(4);
    if (sw == "9000") {
        log("✔ Verify Pin 成功 SW=9000");
    }
    else if (sw.left(2) == "63") {
        log(QString("⚠️ Verify Pin 未通過（SW=%1，63Cx 代表剩餘可嘗試次數）").arg(sw), true);
    }
    else {
        log("⚠️ Verify Pin SW=" + sw, true);
    }
}

void PageCardOperation::onReadRecord()
{
    QString apdu = m_readRecordFields.buildApdu();
    if (apdu.length() < 10) {
        log("[Read Record] APDU 不完整，請確認欄位", true);
        return;
    }

    log("====== Read Record ======");
    bool ok = false;
    QString resp = CardUtils::sendApdu(getTargetHandle(), apdu, ok,
        [this](const QString& msg, bool err) { log(msg, err); });
    if (!ok) { log("[Read Record] 通訊失敗", true); return; }

    QString sw = resp.right(4);
    QString data = resp.left(resp.length() - 4);
    if (sw == "9000") {
        log("✔ Read Record 成功 SW=9000");
        log("Data: " + (data.isEmpty() ? QString("(空)") : data));
    }
    else {
        log("⚠️ Read Record SW=" + sw, true);
    }
}

void PageCardOperation::onReadAllRecord()
{
    QString apdu = m_readAllRecordFields.buildApdu();
    if (apdu.length() < 10) {
        log("[Read All Record] APDU 不完整，請確認欄位", true);
        return;
    }

    log("====== Read All Record ======");
    bool ok = false;
    QString resp = CardUtils::sendApdu(getTargetHandle(), apdu, ok,
        [this](const QString& msg, bool err) { log(msg, err); });
    if (!ok) { log("[Read All Record] 通訊失敗", true); return; }

    QString sw = resp.right(4);
    QString data = resp.left(resp.length() - 4);
    if (sw == "9000") {
        log("✔ Read All Record 成功 SW=9000");
        log("Data: " + (data.isEmpty() ? QString("(空)") : data));
    }
    else {
        log("⚠️ Read All Record SW=" + sw, true);
    }
}

void PageCardOperation::onUpdateTerminalAuthP2()
{
    if (m_terminalAuthFields.p2Display)
        m_terminalAuthFields.p2Display->setText(m_terminalAuthFields.computedP2());
}

void PageCardOperation::onTerminalAuthentication()
{
    QString apdu = m_terminalAuthFields.buildApdu();   // 呼叫後 p2Display / lc 會同步更新
    if (apdu.length() < 8) {
        log("[Terminal Authentication] APDU 不完整，請確認欄位", true);
        return;
    }

    log("====== Terminal Authentication ======");
    log(QString("Kq 邏輯左移 3 位 → P2 = %1")
        .arg(m_terminalAuthFields.p2Display ? m_terminalAuthFields.p2Display->text() : QString()));

    bool ok = false;
    QString resp = CardUtils::sendApdu(getTargetHandle(), apdu, ok,
        [this](const QString& msg, bool err) { log(msg, err); });
    if (!ok) { log("[Terminal Authentication] 通訊失敗", true); return; }

    QString sw = resp.right(4);
    if (sw == "9000")
        log("✔ Terminal Authentication 成功 SW=9000");
    else
        log("⚠️ Terminal Authentication SW=" + sw, true);
}

void PageCardOperation::onUpdateSessionKeyCardP2()
{
    if (m_setSessionKeyCardFields.p2Display)
        m_setSessionKeyCardFields.p2Display->setText(m_setSessionKeyCardFields.computedP2());
}

void PageCardOperation::onSetSessionKeyCard()
{
    QString apdu = m_setSessionKeyCardFields.buildApdu();   // 呼叫後 p2Display / lc 會同步更新
    if (apdu.length() < 8) {
        log("[Set Session Key] APDU 不完整，請確認欄位", true);
        return;
    }

    log("====== Set Session Key（卡片）======");
    log(QString("Kq 邏輯左移 3 位 → P2 = %1")
        .arg(m_setSessionKeyCardFields.p2Display ? m_setSessionKeyCardFields.p2Display->text() : QString()));

    bool ok = false;
    QString resp = CardUtils::sendApdu(getTargetHandle(), apdu, ok,
        [this](const QString& msg, bool err) { log(msg, err); });
    if (!ok) { log("[Set Session Key] 通訊失敗", true); return; }

    QString sw = resp.right(4);
    QString data = resp.left(qMax(0, resp.length() - 4));
    if (sw == "9000") {
        log("✔ Set Session Key（卡片）成功 SW=9000");
        log("R2（回應資料，可填入 ISAM 指令 Set Session Key 的 R2 欄位）：" + (data.isEmpty() ? QString("(空)") : data));
    }
    else {
        log("⚠️ Set Session Key SW=" + sw, true);
    }
}

void PageCardOperation::onUpdateRecordCipheredLc()
{
    if (!m_updateRecordCipheredFields.lcDisplay) return;

    QString dataV = m_updateRecordCipheredFields.data
        ? m_updateRecordCipheredFields.data->text().trimmed().remove(' ').toUpper()
        : QString();

    m_updateRecordCipheredFields.lcDisplay->setText(
        QString("%1").arg(dataV.length() / 2, 2, 16, QChar('0')).toUpper());
}

void PageCardOperation::onUpdateRecordCiphered()
{
    onUpdateRecordCipheredLc();   // 確保 Lc 為最新值

    if (m_updateRecordCipheredFields.data &&
        m_updateRecordCipheredFields.data->text().trimmed().isEmpty()) {
        log("[Update Record Ciphered] 請先輸入 DATA", true);
        return;
    }

    QString apdu = m_updateRecordCipheredFields.buildApdu();
    if (apdu.length() < 10) {
        log("[Update Record Ciphered] APDU 不完整，請確認欄位", true);
        return;
    }

    log("====== Update Record Ciphered ======");

    bool ok = false;
    QString resp = CardUtils::sendApdu(getTargetHandle(), apdu, ok,
        [this](const QString& msg, bool err) { log(msg, err); });
    if (!ok) { log("[Update Record Ciphered] 通訊失敗", true); return; }

    QString sw = resp.right(4);
    if (sw == "9000")
        log("✔ Update Record Ciphered 成功 SW=9000");
    else
        log("⚠️ Update Record Ciphered SW=" + sw, true);
}

void PageCardOperation::onReadBinary()
{
    QString apdu = m_readBinaryFields.buildApdu();
    if (apdu.length() < 8) {
        log("[Read Binary] APDU 不完整，請確認欄位", true);
        return;
    }

    log("====== Read Binary ======");
    bool ok = false;
    QString resp = CardUtils::sendApdu(getTargetHandle(), apdu, ok,
        [this](const QString& msg, bool err) { log(msg, err); });
    if (!ok) { log("[Read Binary] 通訊失敗", true); return; }

    QString sw = resp.right(4);
    QString data = resp.left(qMax(0, resp.length() - 4));
    if (sw == "9000") {
        log("✔ Read Binary 成功 SW=9000");
        log("Data: " + (data.isEmpty() ? QString("(空)") : data));
    }
    else {
        log("⚠️ Read Binary SW=" + sw, true);
    }
}

void PageCardOperation::onGenerateRandomCard()
{
    QString apdu = m_generateRandomCardFields.buildApdu();
    if (apdu.length() < 8) {
        log("[Generate Random Number] APDU 不完整，請確認欄位", true);
        return;
    }

    log("====== Generate Random Number ======");
    bool ok = false;
    QString resp = CardUtils::sendApdu(getTargetHandle(), apdu, ok,
        [this](const QString& msg, bool err) { log(msg, err); });
    if (!ok) { log("[Generate Random Number] 通訊失敗", true); return; }

    QString sw = resp.right(4);
    QString data = resp.left(qMax(0, resp.length() - 4));
    if (sw == "9000") {
        log("✔ Generate Random Number 成功 SW=9000");
        log("Random: " + (data.isEmpty() ? QString("(空)") : data));
    }
    else {
        log("⚠️ Generate Random Number SW=" + sw, true);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 輔助
// ─────────────────────────────────────────────────────────────────────────────
void PageCardOperation::displayData(const CustomerData& d)
{
    m_nameEdit->setText(d.name);
    m_c3Edit->setText(d.c3);
    m_c3CodeEdit->setText(d.c3Code);
    m_c1Edit->setText(d.c1);
    m_c1CodeEdit->setText(d.c1Code);
    m_codeAEdit->setText(d.codeA);
    m_codeBEdit->setText(d.codeB);
    m_exchEdit->setText(d.exchangeCode);
    m_ccodeEdit->setText(d.ccode);
    m_pinAEdit->setText(d.pinA);
    m_pinBEdit->setText(d.pinB);
}

CustomerData PageCardOperation::collectDataFromFields() const
{
    CustomerData d;
    d.name = m_nameEdit->text().trimmed();
    d.c3 = m_c3Edit->text().trimmed().toUpper();
    d.c3Code = m_c3CodeEdit->text().trimmed().toUpper();
    d.c1 = m_c1Edit->text().trimmed().toUpper();
    d.c1Code = m_c1CodeEdit->text().trimmed().toUpper();
    d.codeA = m_codeAEdit->text().trimmed().toUpper();
    d.codeB = m_codeBEdit->text().trimmed().toUpper();
    d.exchangeCode = m_exchEdit->text().trimmed().toUpper();
    d.ccode = m_ccodeEdit->text().trimmed().toUpper();
    d.pinA = m_pinAEdit->text().trimmed().toUpper();
    d.pinB = m_pinBEdit->text().trimmed().toUpper();
    return d;
}

void PageCardOperation::log(const QString& msg, bool isError)
{
    QString color = isError ? "#FF3333" : "#00FF66";
    m_logEdit->append(QString("<font color=\"%1\">[%2] %3</font>")
        .arg(color, QDateTime::currentDateTime().toString("hh:mm:ss"), msg));
}

void PageCardOperation::onExportLog()
{
    if (!m_logEdit || m_logEdit->toPlainText().trimmed().isEmpty()) {
        QMessageBox::information(this, "匯出日誌", "目前沒有日誌內容可以匯出。");
        return;
    }

    QString defaultName = QString("card_log_%1.txt")
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));

    QString path = QFileDialog::getSaveFileName(this, "匯出日誌",
        defaultName, "文字檔 (*.txt);;所有檔案 (*.*)");
    if (path.isEmpty()) return;   // 使用者取消

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "匯出失敗", "無法寫入檔案，請確認路徑或權限。");
        return;
    }
    file.write(m_logEdit->toPlainText().toUtf8());
    file.close();

    log("✔ 日誌已匯出：" + path);
    QMessageBox::information(this, "匯出完成", "日誌已成功匯出至：\n" + path);
}
