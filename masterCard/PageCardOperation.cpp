#include "PageCardOperation.h"
#include "IsamKeyUpdater.h"
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
// ReadBinaryFields::buildApdu（Read Binary / Read All Binary 共用）
// ─────────────────────────────────────────────────────────────────────────────
QString ReadBinaryFields::buildApdu() const
{
    auto get = [](QLineEdit* f) {
        return f ? f->text().trimmed().remove(' ').toUpper() : QString();
        };

    QString claV = get(cla).leftJustified(2, '0').left(2);
    QString insV = get(ins).leftJustified(2, '0').left(2);
    QString p1V = get(recN).leftJustified(2, '0').left(2);   // Record Id
    QString p2V = get(p2).leftJustified(2, '0').left(2);     // 04 或 05（唯讀）

    // Le：依欄位 maxLength 決定要補齊/截斷成幾個 HEX 字元
    //   Read Binary     → 2 個 HEX 字元（預設 00）
    //   Read All Binary → 6 個 HEX 字元（預設 00 00 00）
    int leLen = (le && le->maxLength() > 0 && le->maxLength() <= 6) ? le->maxLength() : 2;
    QString leV = get(le).leftJustified(leLen, '0').left(leLen);

    return claV + insV + p1V + p2V + leV;
}

// ─────────────────────────────────────────────────────────────────────────────
// 建構子
// ─────────────────────────────────────────────────────────────────────────────
PageCardOperation::PageCardOperation(SCARDHANDLE isamHandle, SCARDHANDLE masterHandle,
    OperationMode mode, QWidget* parent)
    : QWidget(parent), m_isamHandle(isamHandle), m_masterHandle(masterHandle), m_mode(mode)
{
    setupUI();
    if (m_isamHandle == 0)
        log("提示: 未分配 ISAM 連線控制代碼 (Handle=0)", true);
    else
        log("✔ ISAM 通訊端已對接。");

    if (m_mode == OperationMode::GeneralOperation)
        log("目前為「一般操作」模式：僅單一讀卡機通道，不提供母卡製作功能。");
    else
        log("目前為「母卡製作」模式：雙讀卡機通道已就緒。");
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

    QString modeText = (m_mode == OperationMode::MasterCardProduction)
        ? "🔮 模式：母卡製作"
        : "🛠 模式：一般操作（無母卡製作功能）";
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
    auto* lodTab = new QWidget();  setupLodTab(lodTab);
    auto* apduTab = new QWidget();  setupApduTab(apduTab);

    tabWidget->addTab(isamTab, "🔑 ISAM 金鑰");
    tabWidget->addTab(lodTab, "💳 LOD / 母卡");
    tabWidget->addTab(apduTab, "📡 卡片指令");

    // 一般操作模式：不提供製作母卡功能
    if (m_mode == OperationMode::GeneralOperation) {
        tabWidget->setTabEnabled(1, false);
        tabWidget->setTabToolTip(1, "一般操作模式不提供母卡製作功能，請切換至母卡製作模式使用。");
        if (m_makeMasterBtn) m_makeMasterBtn->setEnabled(false);
        if (m_authBtn)       m_authBtn->setEnabled(false);
    }

    bodyRow->addWidget(tabWidget, 3);

    // 右側日誌
    auto* logGrp = new QGroupBox("通訊日誌");
    auto* logLay = new QVBoxLayout(logGrp);
    m_logEdit = new QTextEdit(this);
    m_logEdit->setReadOnly(true);
    m_logEdit->setStyleSheet(
        "background:#121212; color:#00FF66; "
        "font-family:'Courier New',monospace; font-size:11px;");
    logLay->addWidget(m_logEdit);
    bodyRow->addWidget(logGrp, 2);

    mainLayout->addLayout(bodyRow);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab 1 — ISAM 金鑰（金鑰本體隱藏，唯讀）
// ─────────────────────────────────────────────────────────────────────────────
void PageCardOperation::setupIsamTab(QWidget* tab)
{
    auto* layout = new QVBoxLayout(tab);

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

    m_updateIsamBtn = new QPushButton("⚡ 寫入 ISAM 金鑰", tab);
    m_updateIsamBtn->setStyleSheet(
        "QPushButton { background:#E65100; color:white; padding:10px; "
        "font-weight:bold; font-size:13px; border-radius:4px; }"
        "QPushButton:disabled { background:#CCC; color:#666; }");
    m_updateIsamBtn->setEnabled(false);
    layout->addWidget(m_updateIsamBtn);
    layout->addStretch();

    connect(m_isamKeyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &PageCardOperation::onIsamKeySelected);
    connect(m_updateIsamBtn, &QPushButton::clicked, this, &PageCardOperation::onUpdateIsam);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab 2 — LOD / 母卡
// ─────────────────────────────────────────────────────────────────────────────
void PageCardOperation::setupLodTab(QWidget* tab)
{
    auto* layout = new QVBoxLayout(tab);

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

    m_fileLabel = new QLabel("尚未載入 — 或直接手動輸入", tab);
    m_fileLabel->setStyleSheet("color:#888; font-style:italic;");

    btnRow->addWidget(m_loadBtn);
    btnRow->addWidget(m_clearBtn);
    btnRow->addWidget(m_authBtn);
    btnRow->addWidget(m_makeMasterBtn);
    btnRow->addWidget(m_fileLabel, 1);
    layout->addLayout(btnRow);

    auto* dataGrp = new QGroupBox("客戶資料欄位（可手動編輯）");
    auto* dataForm = new QFormLayout(dataGrp);
    dataForm->setLabelAlignment(Qt::AlignRight);

    auto mf = [&](QLineEdit*& f, const QString& lbl) {
        f = makeEditField(tab, false);
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
    mf(m_pinAEdit, "Pin A：");
    mf(m_pinBEdit, "Pin B：");
    layout->addWidget(dataGrp);

    connect(m_loadBtn, &QPushButton::clicked, this, &PageCardOperation::onLoadFile);
    connect(m_clearBtn, &QPushButton::clicked, this, &PageCardOperation::onClearFields);
    connect(m_authBtn, &QPushButton::clicked, this, &PageCardOperation::onAuthenticate);
    connect(m_makeMasterBtn, &QPushButton::clicked, this, &PageCardOperation::onMakeMaster);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab 3 — 卡片指令（Create / Select / Write / List / Verify Pin / Read Binary / Read All Binary）
// ─────────────────────────────────────────────────────────────────────────────
void PageCardOperation::setupApduTab(QWidget* tab)
{
    // Scroll area 讓版面不擁擠
    auto* scroll = new QScrollArea(tab);
    scroll->setWidgetResizable(true);
    auto* content = new QWidget();
    auto* layout = new QVBoxLayout(content);
    layout->setSpacing(12);

    // ── 讀卡機選擇（選母卡或 ISAM）──
    auto* targetGrp = new QGroupBox("目標讀卡機");
    auto* targetForm = new QFormLayout(targetGrp);
    auto* m_targetCombo = new QComboBox(content);
    if (m_mode == OperationMode::MasterCardProduction) {
        m_targetCombo->addItem("母卡 (Master Card)", 0);
        m_targetCombo->addItem("ISAM 卡", 1);
    }
    else {
        // 一般操作模式：僅有單一讀卡機通道（掛在 ISAM handle 上）
        m_targetCombo->addItem("讀卡機（單一通道）", 1);
    }
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

    // ── Write Record ──
    auto* writeGrp = makeApduGroup(
        content,
        "③ Write Record",
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

    // ── List File ──
    auto* listGrp = new QGroupBox("④ List File");
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

    // ── Verify Pin ──
    auto* verifyGrp = new QGroupBox("⑤ Verify Pin", content);
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

    // ── Read Binary ──
    auto* readGrp = new QGroupBox("⑥ Read Binary", content);
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

    addReadCol(m_readBinaryFields.cla, "CLA", "00", 2, false);
    addReadCol(m_readBinaryFields.ins, "INS", "B2", 2, false);
    addReadCol(m_readBinaryFields.recN, "P1（RecN）", "", 2, false);
    addReadCol(m_readBinaryFields.p2, "P2", "04", 2, true);
    addReadCol(m_readBinaryFields.le, "Le", "00", 2, false);
    readRow->addStretch();
    readLay->addLayout(readRow);

    auto* readHint = new QLabel("P1 = Record Id（自行輸入），Le 預設 00", readGrp);
    readHint->setStyleSheet("color:#555; font-size:11px; padding:2px;");
    readLay->addWidget(readHint);

    auto* readBtn = new QPushButton("▶ 執行 Read Binary", readGrp);
    readBtn->setStyleSheet(
        "QPushButton { background:#1565C0; color:white; padding:8px; "
        "font-weight:bold; border-radius:4px; }");
    readLay->addWidget(readBtn);
    layout->addWidget(readGrp);

    connect(readBtn, &QPushButton::clicked, this, &PageCardOperation::onReadBinary);

    // ── Read All Binary ──
    auto* readAllGrp = new QGroupBox("⑦ Read All Binary", content);
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

    addReadAllCol(m_readAllBinaryFields.cla, "CLA", "00", 2, false);
    addReadAllCol(m_readAllBinaryFields.ins, "INS", "B2", 2, false);
    addReadAllCol(m_readAllBinaryFields.recN, "P1（RecN）", "", 2, false);
    addReadAllCol(m_readAllBinaryFields.p2, "P2", "05", 2, true);
    addReadAllCol(m_readAllBinaryFields.le, "Le", "000000", 6, false);
    readAllRow->addStretch();
    readAllLay->addLayout(readAllRow);

    auto* readAllHint = new QLabel("P1 = Record Id（自行輸入），Le 預設 00 00 00", readAllGrp);
    readAllHint->setStyleSheet("color:#555; font-size:11px; padding:2px;");
    readAllLay->addWidget(readAllHint);

    auto* readAllBtn = new QPushButton("▶ 執行 Read All Binary", readAllGrp);
    readAllBtn->setStyleSheet(
        "QPushButton { background:#00838F; color:white; padding:8px; "
        "font-weight:bold; border-radius:4px; }");
    readAllLay->addWidget(readAllBtn);
    layout->addWidget(readAllGrp);

    connect(readAllBtn, &QPushButton::clicked, this, &PageCardOperation::onReadAllBinary);

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
// 輔助：取目標 handle
// ─────────────────────────────────────────────────────────────────────────────
SCARDHANDLE PageCardOperation::getTargetHandle() const
{
    // 找 apduTargetCombo
    auto* combo = findChild<QComboBox*>("apduTargetCombo");
    if (combo && combo->currentData().toInt() == 1)
        return m_isamHandle;
    return m_masterHandle;
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
    if (!ok) { log("[" + label + "] 通訊失敗", true); return false; }

    if (sw == "9000")
        log(QString("✔ [%1] 成功 SW=9000").arg(label));
    else
        log(QString("⚠️ [%1] SW=%2（非 9000，請確認參數）").arg(label).arg(sw), true);

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
        return;
    }
    for (const auto& e : IsamKeyData::entries()) {
        if (e.id == id) {
            // 只顯示 Code，不顯示金鑰本體
            m_isamC3CodeLbl->setText(e.c3Code);
            m_isamC1CodeLbl->setText(e.c1Code);
            m_updateIsamBtn->setEnabled(true);
            log(QString("已選擇 KEY：%1  (C3Code=%2, C1Code=%3)")
                .arg(e.displayName).arg(e.c3Code).arg(e.c1Code));
            break;
        }
    }
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

    log("====== 開始 ISAM 金鑰更新 ======");
    IsamKeyUpdater updater(m_isamHandle);
    bool ok = updater.execute(d, [this](const QString& msg, bool err) { log(msg, err); });

    if (ok) QMessageBox::information(this, "完成", "✔ ISAM 金鑰更新成功！");
    else    QMessageBox::critical(this, "失敗", "ISAM 金鑰更新失敗，請查看日誌。");
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
    if (m_mode == OperationMode::MasterCardProduction)
        m_makeMasterBtn->setEnabled(true);
    log("✔ LOD 載入：" + m_data.name);
    for (const auto& w : warnings) log("⚠️ " + w, true);
}

void PageCardOperation::onAuthenticate()
{
    if (m_mode == OperationMode::GeneralOperation) {
        log("一般操作模式不提供母卡認證功能。", true);
        return;
    }

    log("====== 🔐 母卡認證 ======");
    MasterCardUpdater mc(m_masterHandle, m_isamHandle);
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
    if (m_mode == OperationMode::GeneralOperation) {
        log("一般操作模式不提供製作母卡功能。", true);
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
        MasterCardUpdater mcAuth(m_masterHandle, m_isamHandle);
        if (!mcAuth.authenticate([this](const QString& msg, bool err) { log(msg, err); })) {
            QMessageBox::critical(this, "失敗", "母卡認證失敗，請先按「母卡認證」後再試。");
            return;
        }
        m_authenticated = true;
    }

    log("====== 開始製作母卡 ======");
    MasterCardUpdater mc(m_masterHandle, m_isamHandle);
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

void PageCardOperation::onClearFields()
{
    for (auto* f : { m_nameEdit, m_c3Edit, m_c3CodeEdit, m_c1Edit, m_c1CodeEdit,
                    m_codeAEdit, m_codeBEdit, m_exchEdit, m_pinAEdit, m_pinBEdit })
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

void PageCardOperation::onReadBinary()
{
    QString apdu = m_readBinaryFields.buildApdu();
    if (apdu.length() < 10) {
        log("[Read Binary] APDU 不完整，請確認欄位", true);
        return;
    }

    log("====== Read Binary ======");
    bool ok = false;
    QString resp = CardUtils::sendApdu(getTargetHandle(), apdu, ok,
        [this](const QString& msg, bool err) { log(msg, err); });
    if (!ok) { log("[Read Binary] 通訊失敗", true); return; }

    QString sw = resp.right(4);
    QString data = resp.left(resp.length() - 4);
    if (sw == "9000") {
        log("✔ Read Binary 成功 SW=9000");
        log("Data: " + (data.isEmpty() ? QString("(空)") : data));
    }
    else {
        log("⚠️ Read Binary SW=" + sw, true);
    }
}

void PageCardOperation::onReadAllBinary()
{
    QString apdu = m_readAllBinaryFields.buildApdu();
    if (apdu.length() < 10) {
        log("[Read All Binary] APDU 不完整，請確認欄位", true);
        return;
    }

    log("====== Read All Binary ======");
    bool ok = false;
    QString resp = CardUtils::sendApdu(getTargetHandle(), apdu, ok,
        [this](const QString& msg, bool err) { log(msg, err); });
    if (!ok) { log("[Read All Binary] 通訊失敗", true); return; }

    QString sw = resp.right(4);
    QString data = resp.left(resp.length() - 4);
    if (sw == "9000") {
        log("✔ Read All Binary 成功 SW=9000");
        log("Data: " + (data.isEmpty() ? QString("(空)") : data));
    }
    else {
        log("⚠️ Read All Binary SW=" + sw, true);
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
