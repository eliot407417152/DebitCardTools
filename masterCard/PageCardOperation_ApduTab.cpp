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
#include <QListWidget>
#include <QStackedWidget>
#include <QSplitter>
#include <QCoreApplication>
#include <QDir>
#include <QDialog>

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
    auto* targetGrp = new QGroupBox("讀卡機選擇");
    auto* targetForm = new QFormLayout(targetGrp);
    auto* m_targetCombo = new QComboBox(content);
    m_targetCombo->addItems(m_readers);
    targetForm->addRow("發送對象：", m_targetCombo);
    layout->addWidget(targetGrp);

    auto* apduNoteLbl = new QLabel("以下指令發送至上方選擇的讀卡機。左側點選指令名稱即可切換。", content);
    apduNoteLbl->setStyleSheet("color:#555; font-size:11px; padding:4px;");
    layout->addWidget(apduNoteLbl);

    // ── 左側指令清單 + 右側指令內容（取代原本一路往下排列，方便快速找到指令）──
    auto* apduCmdList = new QListWidget(content);
    apduCmdList->setFixedWidth(230);
    apduCmdList->setStyleSheet(
        "QListWidget { font-size:12px; }"
        "QListWidget::item { padding:8px 6px; }"
        "QListWidget::item:selected { background:#1565C0; color:white; }");
    auto* apduCmdStack = new QStackedWidget(content);

    auto* apduSplitRow = new QHBoxLayout();
    apduSplitRow->addWidget(apduCmdList);
    apduSplitRow->addWidget(apduCmdStack, 1);
    layout->addLayout(apduSplitRow, 1);

    connect(apduCmdList, &QListWidget::currentRowChanged, apduCmdStack, &QStackedWidget::setCurrentIndex);

    // 小工具：把指令群組加入清單與堆疊（取代原本的 layout->addWidget(xxxGrp)）
    // 【排版修正】同 ISAM 指令頁：包一層容器讓 grp 靠上對齊、維持自然大小。
    auto addApduCmdPage = [&](QGroupBox* grp, const QString& title) {
        auto* page = new QWidget();
        auto* pageLay = new QVBoxLayout(page);
        pageLay->setContentsMargins(4, 4, 4, 4);
        pageLay->addWidget(grp);
        pageLay->addStretch();
        apduCmdList->addItem(title);
        apduCmdStack->addWidget(page);
        };

    // ── Create File ──
    auto* createGrp = setupCreateFileGroup(content, layout);
    addApduCmdPage(createGrp, "① Create File");

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
    auto* selPfRadio = new QRadioButton("Select PF（P1=03）", content);
    selEfRadio->setChecked(true);
    selModeRow->addWidget(selEfRadio);
    selModeRow->addWidget(selDfRadio);
    selModeRow->addWidget(selPfRadio);
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
    addApduCmdPage(selectGrp, "② Select File");

    connect(selEfRadio, &QRadioButton::toggled, this, [this, selEfRadio]() {
        if (selEfRadio->isChecked()) {
            if (m_selectFields.p1) m_selectFields.p1->setText("02");
            if (m_selectFields.lc) m_selectFields.lc->setText("02");
        }
        });

    connect(selDfRadio, &QRadioButton::toggled, this, [this, selDfRadio]() {
        if (selDfRadio->isChecked()) {
            if (m_selectFields.p1) m_selectFields.p1->setText("01");
            if (m_selectFields.lc) m_selectFields.lc->setText("02");
        }
        });

    connect(selPfRadio, &QRadioButton::toggled, this, [this, selPfRadio]() {
        if (selPfRadio->isChecked()) {
            if (m_selectFields.p1) m_selectFields.p1->setText("03");
            if (m_selectFields.lc) m_selectFields.lc->clear();
            if (m_selectFields.dataIn) m_selectFields.dataIn->clear();
        }
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
    addApduCmdPage(listGrp, "③ List File");

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
    addApduCmdPage(readGrp, "④ Read Record");

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

    auto* readAllHint = new QLabel("P1 = Record Id（自行輸入），Le 預設 00 00 00\n"
        "回應資料格式：[Block編號 1B][長度 1B][該筆資料]，重複至結尾，會自動切開顯示每個 block", readAllGrp);
    readAllHint->setStyleSheet("color:#555; font-size:11px; padding:2px;");
    readAllLay->addWidget(readAllHint);

    auto* readAllBtn = new QPushButton("▶ 執行 Read All Record", readAllGrp);
    readAllBtn->setStyleSheet(
        "QPushButton { background:#00838F; color:white; padding:8px; "
        "font-weight:bold; border-radius:4px; }");
    readAllLay->addWidget(readAllBtn);
    addApduCmdPage(readAllGrp, "⑤ Read All Record");

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
    addApduCmdPage(rbGrp, "⑥ Read Binary");
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
    addApduCmdPage(writeGrp, "⑦ Write Record");

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
    addApduCmdPage(urcGrp, "⑧ Update Record Ciphered");
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
    addApduCmdPage(verifyGrp, "⑨ Verify Pin");

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
    addApduCmdPage(taGrp, "⑩ Terminal Authentication");
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
    addApduCmdPage(sskCardGrp, "⑪ Set Session Key");
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
    addApduCmdPage(grcGrp, "⑫ Generate Random Number");
    connect(grcBtn, &QPushButton::clicked, this, &PageCardOperation::onGenerateRandomCard);

    // ── ⑬ Card Authentication ────────────────────────────────────────────────
    auto* caGrp = new QGroupBox("⑬ Card Authentication", content);
    auto* caLay = new QVBoxLayout(caGrp);
    auto* caRow = new QHBoxLayout();
    auto addCaCol = [&](QLineEdit*& outField, const QString& lbl,
        const QString& def, int maxLen, bool ro) {
            auto* col = new QVBoxLayout();
            auto* l = new QLabel(lbl, caGrp);
            l->setAlignment(Qt::AlignCenter);
            l->setStyleSheet("font-weight:bold; font-size:11px;");
            outField = makeHexField(caGrp, maxLen);
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
            caRow->addLayout(col);
        };

    addCaCol(m_cardAuthFields.cla, "CLA", "00", 2, true);
    addCaCol(m_cardAuthFields.ins, "INS", "88", 2, true);
    addCaCol(m_cardAuthFields.p1, "P1", "00", 2, true);
    addCaCol(m_cardAuthFields.pq, "Kq（原始值）", "", 2, false);
    addCaCol(m_cardAuthFields.p2Display, "P2（Kq<<3）", "00", 2, true);
    addCaCol(m_cardAuthFields.lc, "Lc", "10", 2, true);
    addCaCol(m_cardAuthFields.le, "Le", "10", 2, true);
    caRow->addStretch();
    caLay->addLayout(caRow);

    auto* caR1Lbl = new QLabel("R1（8 bytes，送入 DATA）：", caGrp);
    caR1Lbl->setStyleSheet("font-weight:bold; font-size:11px;");
    m_cardAuthFields.r1 = makeEditField(caGrp, false);
    m_cardAuthFields.r1->setPlaceholderText("輸入 16 個 HEX 字元（8 bytes）");
    caLay->addWidget(caR1Lbl);
    caLay->addWidget(m_cardAuthFields.r1);

    auto* caR3Lbl = new QLabel("R3（8 bytes，送入 DATA）：", caGrp);
    caR3Lbl->setStyleSheet("font-weight:bold; font-size:11px;");
    m_cardAuthFields.r3 = makeEditField(caGrp, false);
    m_cardAuthFields.r3->setPlaceholderText("輸入 16 個 HEX 字元（8 bytes）");
    caLay->addWidget(caR3Lbl);
    caLay->addWidget(m_cardAuthFields.r3);

    // ── Key Set 狀態（唯讀顯示，實際載入 / 選擇改到選單「設定 → Key Set / Data Set 管理...」）──
    auto* caKeyStatusGrp = new QGroupBox("使用中的 Key Set（Card Authentication 用的 K）", caGrp);
    auto* caKeyStatusLay = new QVBoxLayout(caKeyStatusGrp);

    auto* caKeyStatusHint = new QLabel(
        "Key Set / Data Set 的載入與選擇，請至上方選單「設定 → Key Set / Data Set 管理...」開啟管理視窗操作，"
        "這裡僅顯示目前套用的結果，供執行 Card Authentication 前確認。", caKeyStatusGrp);
    caKeyStatusHint->setWordWrap(true);
    caKeyStatusHint->setStyleSheet("color:#555; font-size:11px; padding:2px;");
    caKeyStatusLay->addWidget(caKeyStatusHint);

    auto* caSetKeyLbl = new QLabel("Set Key（目前選取項目的金鑰本體）：", caKeyStatusGrp);
    caSetKeyLbl->setStyleSheet("font-weight:bold; font-size:11px;");
    m_caKeySetStatusLbl = new QLabel("—（尚未選擇，請至「設定 → Key Set / Data Set 管理...」選取）", caKeyStatusGrp);
    m_caKeySetStatusLbl->setWordWrap(true);
    m_caKeySetStatusLbl->setStyleSheet(
        "QLabel { background:#EEEEEE; color:#333; font-family:'Courier New'; "
        "padding:4px 8px; border:1px solid #CCC; border-radius:3px; }");
    caKeyStatusLay->addWidget(caSetKeyLbl);
    caKeyStatusLay->addWidget(m_caKeySetStatusLbl);

    caLay->addWidget(caKeyStatusGrp);

    auto* caVerifyHint = new QLabel(
        "執行後會：① 送出 Card Authentication（DATA=R1+R3），回應 16 bytes 拆成 "
        "[MAC 前 8 bytes] + [R2 後 8 bytes]；② 使用上方 Key Set 選取的 K；"
        "③ 計算 DES(DES(DES(K,R1),R2),R3) 並與 MAC 比對，"
        "結果會顯示 O（正確）或 X（錯誤）。", caGrp);
    caVerifyHint->setWordWrap(true);
    caVerifyHint->setStyleSheet("color:#555; font-size:11px; padding:2px;");
    caLay->addWidget(caVerifyHint);

    auto* caHint = new QLabel("P2 由 Kq 邏輯左移 3 位自動運算（比照 Set Session Key 作法）。DATA = R1 + R3，Lc/Le 固定 10h。", caGrp);
    caHint->setStyleSheet("color:#555; font-size:11px; padding:2px;");
    caLay->addWidget(caHint);

    connect(m_cardAuthFields.pq, &QLineEdit::textChanged,
        this, &PageCardOperation::onUpdateCardAuthP2);

    auto* caBtn = new QPushButton("▶ 執行 Card Authentication", caGrp);
    caBtn->setStyleSheet(
        "QPushButton { background:#C62828; color:white; padding:8px; "
        "font-weight:bold; border-radius:4px; }");
    caLay->addWidget(caBtn);
    addApduCmdPage(caGrp, "⑬ Card Authentication");
    connect(caBtn, &QPushButton::clicked, this, &PageCardOperation::onCardAuthentication);
    onUpdateCardAuthP2();   // 初始化顯示

    // ── ⑭ Write Record With TAC ──────────────────────────────────────────────
    auto* wrtGrp = new QGroupBox("⑭ Write Record With TAC", content);
    auto* wrtLay = new QVBoxLayout(wrtGrp);
    auto* wrtRow = new QHBoxLayout();
    auto addWrtCol = [&](QLineEdit*& outField, const QString& lbl,
        const QString& def, int maxLen, bool ro) {
            auto* col = new QVBoxLayout();
            auto* l = new QLabel(lbl, wrtGrp);
            l->setAlignment(Qt::AlignCenter);
            l->setStyleSheet("font-weight:bold; font-size:11px;");
            outField = makeHexField(wrtGrp, maxLen);
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
            wrtRow->addLayout(col);
        };

    addWrtCol(m_writeRecordTacFields.cla, "CLA", "00", 2, true);
    addWrtCol(m_writeRecordTacFields.ins, "INS", "E6", 2, true);
    addWrtCol(m_writeRecordTacFields.p1, "P1", "00", 2, true);
    addWrtCol(m_writeRecordTacFields.p2, "P2（Kq，自行輸入）", "", 2, false);
    addWrtCol(m_writeRecordTacFields.lcDisplay, "Lc（自動）", "", 2, true);
    addWrtCol(m_writeRecordTacFields.le, "Le", "08", 2, true);
    wrtRow->addStretch();
    wrtLay->addLayout(wrtRow);

    auto* wrtDataLbl = new QLabel("DATA（自行輸入，長度依 P2 而定）：", wrtGrp);
    wrtDataLbl->setStyleSheet("font-weight:bold; font-size:11px;");
    m_writeRecordTacFields.data = makeEditField(wrtGrp, false);
    m_writeRecordTacFields.data->setPlaceholderText("例如 P2=10 時，輸入 16 個 HEX 字元（8 bytes）");
    wrtLay->addWidget(wrtDataLbl);
    wrtLay->addWidget(m_writeRecordTacFields.data);

    auto* wrtHint = new QLabel("Lc 依 DATA 實際長度自動計算，不需自行輸入。", wrtGrp);
    wrtHint->setStyleSheet("color:#555; font-size:11px; padding:2px;");
    wrtLay->addWidget(wrtHint);

    connect(m_writeRecordTacFields.data, &QLineEdit::textChanged,
        this, &PageCardOperation::onWriteRecordWithTacLcUpdate);

    auto* wrtBtn = new QPushButton("▶ 執行 Write Record With TAC", wrtGrp);
    wrtBtn->setStyleSheet(
        "QPushButton { background:#EF6C00; color:white; padding:8px; "
        "font-weight:bold; border-radius:4px; }");
    wrtLay->addWidget(wrtBtn);
    addApduCmdPage(wrtGrp, "⑭ Write Record With TAC");
    connect(wrtBtn, &QPushButton::clicked, this, &PageCardOperation::onWriteRecordWithTac);
    onWriteRecordWithTacLcUpdate();   // 初始化 Lc 顯示

    // ── ⑮ Write Record With S/N TAC ──────────────────────────────────────────
    auto* wrtSnGrp = new QGroupBox("⑮ Write Record With S/N TAC", content);
    auto* wrtSnLay = new QVBoxLayout(wrtSnGrp);
    auto* wrtSnRow = new QHBoxLayout();
    auto addWrtSnCol = [&](QLineEdit*& outField, const QString& lbl,
        const QString& def, int maxLen, bool ro) {
            auto* col = new QVBoxLayout();
            auto* l = new QLabel(lbl, wrtSnGrp);
            l->setAlignment(Qt::AlignCenter);
            l->setStyleSheet("font-weight:bold; font-size:11px;");
            outField = makeHexField(wrtSnGrp, maxLen);
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
            wrtSnRow->addLayout(col);
        };

    addWrtSnCol(m_writeRecordWSNumTacFields.cla, "CLA", "00", 2, true);
    addWrtSnCol(m_writeRecordWSNumTacFields.ins, "INS", "E2", 2, true);
    addWrtSnCol(m_writeRecordWSNumTacFields.p1, "P1（自行輸入）", "", 2, false);
    addWrtSnCol(m_writeRecordWSNumTacFields.p2, "P2（自行輸入）", "", 2, false);
    addWrtSnCol(m_writeRecordWSNumTacFields.lc, "Lc（自行輸入）", "", 2, false);
    addWrtSnCol(m_writeRecordWSNumTacFields.le, "Le", "00", 2, true);
    wrtSnRow->addStretch();
    wrtSnLay->addLayout(wrtSnRow);

    auto* wrtSnDataLbl = new QLabel("DATA（自行輸入）：", wrtSnGrp);
    wrtSnDataLbl->setStyleSheet("font-weight:bold; font-size:11px;");
    m_writeRecordWSNumTacFields.data = makeEditField(wrtSnGrp, false);
    m_writeRecordWSNumTacFields.data->setPlaceholderText("輸入 DATA 的 HEX 字元");
    wrtSnLay->addWidget(wrtSnDataLbl);
    wrtSnLay->addWidget(m_writeRecordWSNumTacFields.data);

    // 依 Lc（HEX，代表 bytes 數）動態限制 DATA 最多可輸入的 HEX 字元數
    // 例：Lc=1C（28 bytes）→ DATA 最多輸入 56 個 HEX 字元；Lc=08（8 bytes）→ 最多 16 個 HEX 字元
    auto updateWrtSnDataMaxLen = [this]() {
        if (!m_writeRecordWSNumTacFields.lc || !m_writeRecordWSNumTacFields.data) return;
        QString lcText = m_writeRecordWSNumTacFields.lc->text().trimmed().remove(' ').toUpper();
        bool ok = false;
        int lcByteCount = lcText.toInt(&ok, 16);
        if (!ok || lcText.length() != 2) {
            // Lc 尚未輸入完整（2 個 HEX 字元），暫不限制 DATA 長度
            m_writeRecordWSNumTacFields.data->setMaxLength(32767);
            m_writeRecordWSNumTacFields.data->setPlaceholderText("請先輸入 Lc，決定 DATA 可輸入的長度");
            return;
        }
        int maxHexChars = lcByteCount * 2;
        m_writeRecordWSNumTacFields.data->setMaxLength(maxHexChars);
        m_writeRecordWSNumTacFields.data->setPlaceholderText(
            QString("依 Lc=%1，DATA 需輸入 %2 bytes（%3 個 HEX 字元）")
            .arg(lcText).arg(lcByteCount).arg(maxHexChars));
        };
    connect(m_writeRecordWSNumTacFields.lc, &QLineEdit::textChanged, this, updateWrtSnDataMaxLen);
    updateWrtSnDataMaxLen();   // 初始化（Lc 尚未輸入，先不限制）

    auto* wrtSnHint = new QLabel(
        "固定：CLA=00  INS=E6  Le=00\nP1 / P2 / Lc / DATA 皆自行輸入。", wrtSnGrp);
    wrtSnHint->setStyleSheet("color:#555; font-size:11px; padding:2px;");
    wrtSnLay->addWidget(wrtSnHint);

    auto* wrtSnBtn = new QPushButton("▶ 執行 Write Record With S/N TAC", wrtSnGrp);
    wrtSnBtn->setStyleSheet(
        "QPushButton { background:#EF6C00; color:white; padding:8px; "
        "font-weight:bold; border-radius:4px; }");
    wrtSnLay->addWidget(wrtSnBtn);
    addApduCmdPage(wrtSnGrp, "⑮ Write Record With S/N TAC");
    connect(wrtSnBtn, &QPushButton::clicked, this, &PageCardOperation::onWriteRecordWSNumTac);

    apduCmdList->setCurrentRow(0);
    // Key Set / Data Set 的建立與自動載入，改到建構子裡（buildKeySetDataSetDialog() +
    // autoLoadKeySetAndDataSet()），因為相關控制項現在位於獨立管理視窗，而不是這個頁籤裡。
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
QGroupBox* PageCardOperation::setupCreateFileGroup(QWidget* parent, QVBoxLayout* layout)
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

    return grp;
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
    if (sw != "9000") {
        log("⚠️ Read All Record SW=" + sw, true);
        return;
    }

    log("✔ Read All Record 成功 SW=9000");

    if (data.isEmpty()) {
        log("Data: (空)");
        return;
    }

    // 回應資料格式：[Block 編號 1 byte][長度 1 byte][該筆資料 長度 bytes]，重複至結尾
    // 例如 "011311223344..." → 01=第1筆，13(hex)=19(dec) bytes 長度，後面19 bytes是該筆資料
    int pos = 0;
    int blockCount = 0;
    while (pos < data.length()) {
        // 至少要有 Block 編號 + 長度 這 2 bytes（4 個 HEX 字元）才夠解析
        if (pos + 4 > data.length()) {
            log(QString("⚠️ 剩餘資料不足以組成一個 block（剩餘 %1 bytes）：%2")
                .arg((data.length() - pos) / 2).arg(data.mid(pos)), true);
            break;
        }

        QString blockNoHex = data.mid(pos, 2);
        QString lenHex = data.mid(pos + 2, 2);

        bool lenOk = false;
        int lenBytes = lenHex.toInt(&lenOk, 16);
        if (!lenOk) {
            log(QString("⚠️ 無法解析長度欄位 (%1)，剩餘資料：%2").arg(lenHex, data.mid(pos)), true);
            break;
        }

        int dataStart = pos + 4;
        int dataHexLen = lenBytes * 2;

        if (dataStart + dataHexLen > data.length()) {
            log(QString("⚠️ Block %1 宣告長度 %2 bytes，但剩餘資料不足（只剩 %3 bytes）：%4")
                .arg(blockNoHex).arg(lenBytes).arg((data.length() - dataStart) / 2)
                .arg(data.mid(dataStart)), true);
            break;
        }

        QString blockData = data.mid(dataStart, dataHexLen);
        ++blockCount;
        log(QString("Block %1（編號 %2，長度 %3 bytes）：%4")
            .arg(blockCount).arg(blockNoHex).arg(lenBytes).arg(blockData));

        pos = dataStart + dataHexLen;
    }

    if (blockCount == 0)
        log("Data: " + data);
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

void PageCardOperation::onUpdateCardAuthP2()
{
    if (m_cardAuthFields.p2Display)
        m_cardAuthFields.p2Display->setText(m_cardAuthFields.computedP2());
}

void PageCardOperation::onCardAuthentication()
{
    QString apdu = m_cardAuthFields.buildApdu();   // 呼叫後 P2/Lc 顯示會同步更新
    if (apdu.length() < 8) {
        log("[Card Authentication] APDU 不完整，請確認欄位", true);
        return;
    }

    log("====== Card Authentication ======");
    log(QString("Kq 邏輯左移 3 位 → P2 = %1")
        .arg(m_cardAuthFields.p2Display ? m_cardAuthFields.p2Display->text() : QString()));

    bool ok = false;
    QString resp = CardUtils::sendApdu(getTargetHandle(), apdu, ok,
        [this](const QString& msg, bool err) { log(msg, err); });
    if (!ok) { log("[Card Authentication] 通訊失敗", true); return; }

    QString sw = resp.right(4);
    QString data = resp.left(qMax(0, resp.length() - 4));
    if (sw != "9000") {
        log("⚠️ Card Authentication SW=" + sw, true);
        return;
    }

    log("✔ Card Authentication 成功 SW=9000");
    log("Data: " + (data.isEmpty() ? QString("(空)") : data));

    // 回應（16 bytes）拆成 [MAC 前 8 bytes] + [R2 後 8 bytes]
    QString mac = data.left(16);
    QString r2FromResp = data.mid(16, 16);
    if (mac.length() < 16 || r2FromResp.length() < 16) {
        log("⚠️ 回應長度不足 16 bytes，無法拆出 MAC / R2，略過驗證。", true);
        return;
    }
    log("回應拆解 → MAC（前8 bytes）：" + mac + "　R2（後8 bytes）：" + r2FromResp);

    // 取得 Key：由「Key Set」下拉選單選取（需先載入 Key Set 檔案）
    int keySetIdx = m_keySetCombo ? m_keySetCombo->currentIndex() : -1;
    if (keySetIdx < 0 || keySetIdx >= m_keySetEntries.size()) {
        log("Error: 尚未載入 Key Set 或尚未選擇要使用的項目，略過驗證。", true);
        return;
    }
    const KeySetEntry& keySetEntry = m_keySetEntries.at(keySetIdx);
    QString key = keySetEntry.keyHex;
    log(QString("✔ 使用 Key Set：%1（KID=%2）Key=%3")
        .arg(keySetEntry.label, keySetEntry.kid, key));

    // 計算 DES(DES(DES(Key,R1),R2),R3)
    QString r1 = m_cardAuthFields.r1 ? m_cardAuthFields.r1->text().trimmed().remove(' ').toUpper() : QString();
    QString r3 = m_cardAuthFields.r3 ? m_cardAuthFields.r3->text().trimmed().remove(' ').toUpper() : QString();

    // K 不強制裁切成 8 bytes——K 可能是 8/16/24 bytes（單/雙/三長度金鑰），
    // 交給 tripleDesHex 依實際長度判斷要用單一 DES 還是 3DES（對應規格書的 E(DES/3DES)(K,R1)）
    QString keyV = key;
    QString r1V = r1.leftJustified(16, 'F').left(16);
    QString r2V = r2FromResp.leftJustified(16, 'F').left(16);
    QString r3V = r3.leftJustified(16, 'F').left(16);

    log(QString("驗證用值 → Key：%1（%2 bytes）　R1：%3　R2：%4　R3：%5")
        .arg(keyV).arg(keyV.length() / 2).arg(r1V, r2V, r3V));

    // 第一步 E(DES/3DES)(K, R1)：依 K 長度自動判斷單一 DES 或 3DES
    QString step1 = CardUtils::tripleDesHex(keyV, r1V);
    // 第二、三步固定為單一 DES：E(DES)(上一步, R2) → E(DES)(上一步, R3)
    QString step2 = CardUtils::desSingleBlockHex(step1, r2V, /*encrypt=*/true);  // S = E(DES)(step1, R2)
    QString step3 = CardUtils::desSingleBlockHex(step2, r3V, /*encrypt=*/true);  // R = E(DES)(S, R3)

    log(QString("驗證計算 DES(DES(DES(Key,R1),R2),R3)　R1=%1　R3=%2　結果 = %3")
        .arg(r1V, r3V, step3));

    if (!step3.isEmpty() && step3.compare(mac, Qt::CaseInsensitive) == 0)
        log("驗證結果：O（正確，與 MAC 一致）");
    else
        log(QString("驗證結果：X（錯誤！計算值 %1 與 MAC %2 不一致）").arg(step3, mac), true);
}

// ─────────────────────────────────────────────────────────────────────────────
// Key Set / Data Set 管理視窗
//   原本這些控制項（載入按鈕、下拉選單、依 ID 查詢…）是直接畫在「卡片指令」頁籤的
//   Card Authentication 區塊裡；現在改成獨立視窗，由主視窗選單「設定 → Key Set /
//   Data Set 管理...」開啟，卡片指令頁籤只保留一個唯讀狀態顯示（m_caKeySetStatusLbl）。
//   視窗只會建立一次（lazy singleton），重複開啟只是把同一個視窗喚起到最上層，
//   已載入的資料與目前選取狀態都會保留。
// ─────────────────────────────────────────────────────────────────────────────
void PageCardOperation::buildKeySetDataSetDialog()
{
    if (m_keySetDataSetDialog)
        return; // 已建立過，不重複建立

    auto* dlg = new QDialog(this);
    dlg->setWindowTitle("Key Set / Data Set 管理");
    dlg->resize(520, 620);
    // 使用非強制 Modal（Qt::Window）即可：允許使用者開著這個視窗、切換回主視窗操作，
    // 不用每次都先關閉管理視窗才能繼續操作卡片指令。
    dlg->setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    auto* dlgLay = new QVBoxLayout(dlg);

    // ── Key Set ──────────────────────────────────────────────────────────
    auto* keySetGrp = new QGroupBox("Key Set（Card Authentication 用的 K）", dlg);
    auto* keySetLay = new QVBoxLayout(keySetGrp);

    auto* keySetBtnRow = new QHBoxLayout();
    m_loadKeySetBtn = new QPushButton("📂 重新載入 Key Set 檔案", keySetGrp);
    m_loadKeySetBtn->setStyleSheet(
        "QPushButton { background:#455A64; color:white; padding:6px 12px; font-weight:bold; border-radius:4px; }");
    m_keySetFileLbl = new QLabel("尚未載入", keySetGrp);
    m_keySetFileLbl->setStyleSheet("color:#888; font-style:italic;");
    keySetBtnRow->addWidget(m_loadKeySetBtn);
    keySetBtnRow->addWidget(m_keySetFileLbl, 1);
    keySetLay->addLayout(keySetBtnRow);

    m_keySetCombo = new QComboBox(keySetGrp);
    m_keySetCombo->addItem("── 尚未載入 Key Set 檔案 ──");
    keySetLay->addWidget(m_keySetCombo);

    auto* keySetIdRow = new QHBoxLayout();
    auto* keySetIdLbl = new QLabel("依 ID 查詢：", keySetGrp);
    keySetIdLbl->setStyleSheet("font-weight:bold; font-size:11px;");
    m_keySetIdEdit = makeEditField(keySetGrp, false);
    m_keySetIdEdit->setPlaceholderText("輸入 ID，例如 05");
    m_keySetApplyBtn = new QPushButton("套用", keySetGrp);
    keySetIdRow->addWidget(keySetIdLbl);
    keySetIdRow->addWidget(m_keySetIdEdit);
    keySetIdRow->addWidget(m_keySetApplyBtn);
    keySetLay->addLayout(keySetIdRow);

    auto* setKeyLbl = new QLabel("Set Key（目前選取項目的金鑰本體）：", keySetGrp);
    setKeyLbl->setStyleSheet("font-weight:bold; font-size:11px;");
    m_keySetValueLbl = new QLabel("—", keySetGrp);
    m_keySetValueLbl->setWordWrap(true);
    m_keySetValueLbl->setStyleSheet(
        "QLabel { background:#EEEEEE; color:#333; font-family:'Courier New'; "
        "padding:4px 8px; border:1px solid #CCC; border-radius:3px; }");
    keySetLay->addWidget(setKeyLbl);
    keySetLay->addWidget(m_keySetValueLbl);

    auto* keySetHint = new QLabel(
        "Key Set 檔案（KeySet.txt）預設放在程式所在目錄，啟動時會自動嘗試載入；"
        "也可以用上方按鈕重新指定檔案。輸入 ID 按「套用」即可直接切換到對應項目，"
        "其金鑰本體就是公式裡的 K（卡片認證基碼），不再向 ISAM 查詢或自行輸入。"
        "選取的項目也會同步顯示在「卡片指令 → Card Authentication」頁面上。", keySetGrp);
    keySetHint->setWordWrap(true);
    keySetHint->setStyleSheet("color:#555; font-size:11px; padding:2px;");
    keySetLay->addWidget(keySetHint);

    dlgLay->addWidget(keySetGrp);

    connect(m_loadKeySetBtn, &QPushButton::clicked, this, &PageCardOperation::onLoadKeySet);
    connect(m_keySetApplyBtn, &QPushButton::clicked, this, &PageCardOperation::onKeySetIdApply);
    connect(m_keySetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &PageCardOperation::onKeySetSelectionChanged);

    // ── Data Set（通用測試資料查詢，格式同 Key Set，格式見 DataSetLoader.h）──
    auto* dataSetGrp = new QGroupBox("Data Set（測試資料檢視，供參考用）", dlg);
    auto* dataSetLay = new QVBoxLayout(dataSetGrp);

    auto* dataSetBtnRow = new QHBoxLayout();
    m_loadDataSetBtn = new QPushButton("📂 載入 Data Set 檔案", dataSetGrp);
    m_loadDataSetBtn->setStyleSheet(
        "QPushButton { background:#455A64; color:white; padding:6px 12px; font-weight:bold; border-radius:4px; }");
    m_dataSetFileLbl = new QLabel("尚未載入", dataSetGrp);
    m_dataSetFileLbl->setStyleSheet("color:#888; font-style:italic;");
    dataSetBtnRow->addWidget(m_loadDataSetBtn);
    dataSetBtnRow->addWidget(m_dataSetFileLbl, 1);
    dataSetLay->addLayout(dataSetBtnRow);

    m_dataSetCombo = new QComboBox(dataSetGrp);
    m_dataSetCombo->addItem("── 請先載入 Data Set 檔案 ──");
    dataSetLay->addWidget(m_dataSetCombo);

    m_dataSetValueLbl = new QLabel("—", dataSetGrp);
    m_dataSetValueLbl->setWordWrap(true);
    m_dataSetValueLbl->setStyleSheet(
        "QLabel { background:#EEEEEE; color:#333; font-family:'Courier New'; "
        "padding:4px 8px; border:1px solid #CCC; border-radius:3px; }");
    dataSetLay->addWidget(m_dataSetValueLbl);

    dlgLay->addWidget(dataSetGrp);
    dlgLay->addStretch();

    connect(m_loadDataSetBtn, &QPushButton::clicked, this, &PageCardOperation::onLoadDataSet);
    connect(m_dataSetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &PageCardOperation::onDataSetSelectionChanged);

    auto* closeBtn = new QPushButton("關閉", dlg);
    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::close);
    dlgLay->addWidget(closeBtn);

    m_keySetDataSetDialog = dlg;

    // 視窗剛建立好、控制項都已存在，把先前（若有）已載入的資料同步顯示上去
    populateKeySetCombo();
    populateDataSetCombo();
}

// 選單「設定 → Key Set / Data Set 管理...」觸發：第一次呼叫時才真正建立視窗，
// 之後每次呼叫只是把既有視窗顯示、帶到最上層。
void PageCardOperation::openKeySetDataSetManager()
{
    buildKeySetDataSetDialog();
    if (!m_keySetDataSetDialog)
        return;

    m_keySetDataSetDialog->show();
    m_keySetDataSetDialog->raise();
    m_keySetDataSetDialog->activateWindow();
}

// ─────────────────────────────────────────────────────────────────────────────
// Key Set / Data Set：自動載入 + 手動載入 + 下拉選單同步 + 依 ID 查詢
// ─────────────────────────────────────────────────────────────────────────────
void PageCardOperation::autoLoadKeySetAndDataSet()
{
    QString appDir = QCoreApplication::applicationDirPath();

    QString keySetPath = appDir + "/KeySet.txt";
    if (QFile::exists(keySetPath)) {
        QStringList warnings;
        if (KeySetLoader::load(keySetPath, m_keySetEntries, &warnings)) {
            if (m_keySetFileLbl) {
                m_keySetFileLbl->setText("✔ 已自動載入：" + keySetPath);
                m_keySetFileLbl->setStyleSheet("color:#2E7D32; font-weight:bold;");
            }
            populateKeySetCombo();
            log("✔ 自動載入 Key Set：" + keySetPath);
            for (const auto& w : warnings) log("⚠️ Key Set：" + w, true);
        }
    }

    QString dataSetPath = appDir + "/DataSet.txt";
    if (QFile::exists(dataSetPath)) {
        QStringList warnings;
        if (DataSetLoader::load(dataSetPath, m_dataSetEntries, &warnings)) {
            if (m_dataSetFileLbl) {
                m_dataSetFileLbl->setText("✔ 已自動載入：" + dataSetPath);
                m_dataSetFileLbl->setStyleSheet("color:#2E7D32; font-weight:bold;");
            }
            populateDataSetCombo();
            log("✔ 自動載入 Data Set：" + dataSetPath);
            for (const auto& w : warnings) log("⚠️ Data Set：" + w, true);
        }
    }
}

void PageCardOperation::populateKeySetCombo()
{
    if (!m_keySetCombo) return;
    m_keySetCombo->blockSignals(true);
    m_keySetCombo->clear();
    if (m_keySetEntries.isEmpty()) {
        m_keySetCombo->addItem("── 尚未載入 Key Set 檔案 ──");
    }
    else {
        for (const auto& e : m_keySetEntries)
            m_keySetCombo->addItem(e.displayText());
    }
    m_keySetCombo->blockSignals(false);
    onKeySetSelectionChanged(m_keySetCombo->currentIndex());
}

void PageCardOperation::populateDataSetCombo()
{
    if (!m_dataSetCombo) return;
    m_dataSetCombo->blockSignals(true);
    m_dataSetCombo->clear();
    if (m_dataSetEntries.isEmpty()) {
        m_dataSetCombo->addItem("── 尚未載入 Data Set 檔案 ──");
    }
    else {
        for (const auto& e : m_dataSetEntries)
            m_dataSetCombo->addItem(e.displayText());
    }
    m_dataSetCombo->blockSignals(false);
    onDataSetSelectionChanged(m_dataSetCombo->currentIndex());
}

void PageCardOperation::onLoadKeySet()
{
    QString path = QFileDialog::getOpenFileName(this, "載入 Key Set 檔案",
        QCoreApplication::applicationDirPath(), "文字檔 (*.txt);;所有檔案 (*.*)");
    if (path.isEmpty()) return;

    QStringList warnings;
    if (!KeySetLoader::load(path, m_keySetEntries, &warnings)) {
        QMessageBox::critical(this, "載入失敗", "無法開啟檔案，請確認路徑或格式。");
        return;
    }

    if (m_keySetFileLbl) {
        m_keySetFileLbl->setText("✔ " + path);
        m_keySetFileLbl->setStyleSheet("color:#2E7D32; font-weight:bold;");
    }
    populateKeySetCombo();

    log("✔ Key Set 載入完成：" + path + QString("（共 %1 筆）").arg(m_keySetEntries.size()));
    for (const auto& w : warnings) log("⚠️ Key Set：" + w, true);
}

void PageCardOperation::onKeySetIdApply()
{
    QString id = m_keySetIdEdit ? m_keySetIdEdit->text().trimmed() : QString();
    if (id.isEmpty()) {
        log("請先輸入要查詢的 ID。", true);
        return;
    }

    for (int i = 0; i < m_keySetEntries.size(); ++i) {
        if (m_keySetEntries.at(i).id.compare(id, Qt::CaseInsensitive) == 0) {
            m_keySetCombo->setCurrentIndex(i);   // 觸發 onKeySetSelectionChanged 更新顯示
            log(QString("✔ 依 ID=%1 套用 Key Set：%2").arg(id, m_keySetEntries.at(i).displayText()));
            return;
        }
    }
    log(QString("找不到 ID=%1 對應的 Key Set 項目，請確認已載入正確檔案。").arg(id), true);
}

void PageCardOperation::onKeySetSelectionChanged(int index)
{
    if (index < 0 || index >= m_keySetEntries.size()) {
        if (m_keySetValueLbl) m_keySetValueLbl->setText("—");
        if (m_caKeySetStatusLbl)
            m_caKeySetStatusLbl->setText("—（尚未選擇，請至「設定 → Key Set / Data Set 管理...」選取）");
        return;
    }
    const KeySetEntry& e = m_keySetEntries.at(index);
    if (m_keySetValueLbl) m_keySetValueLbl->setText(e.keyHex);
    if (m_keySetIdEdit) m_keySetIdEdit->setText(e.id);

    // 同步更新「卡片指令」頁 Card Authentication 區塊的唯讀狀態顯示，
    // 讓使用者不用開啟管理視窗也能確認目前套用的 Key Set 內容
    if (m_caKeySetStatusLbl)
        m_caKeySetStatusLbl->setText(
            QString("%1（KID=%2）Key=%3").arg(e.label, e.kid, e.keyHex));
}

void PageCardOperation::onLoadDataSet()
{
    QString path = QFileDialog::getOpenFileName(this, "載入 Data Set 檔案",
        QCoreApplication::applicationDirPath(), "文字檔 (*.txt);;所有檔案 (*.*)");
    if (path.isEmpty()) return;

    QStringList warnings;
    if (!DataSetLoader::load(path, m_dataSetEntries, &warnings)) {
        QMessageBox::critical(this, "載入失敗", "無法開啟檔案，請確認路徑或格式。");
        return;
    }

    if (m_dataSetFileLbl) {
        m_dataSetFileLbl->setText("✔ " + path);
        m_dataSetFileLbl->setStyleSheet("color:#2E7D32; font-weight:bold;");
    }
    populateDataSetCombo();

    log("✔ Data Set 載入完成：" + path + QString("（共 %1 筆）").arg(m_dataSetEntries.size()));
    for (const auto& w : warnings) log("⚠️ Data Set：" + w, true);
}

void PageCardOperation::onDataSetSelectionChanged(int index)
{
    if (!m_dataSetValueLbl) return;
    if (index < 0 || index >= m_dataSetEntries.size()) {
        m_dataSetValueLbl->setText("—");
        return;
    }
    m_dataSetValueLbl->setText(m_dataSetEntries.at(index).dataHex);
}

void PageCardOperation::onWriteRecordWithTacLcUpdate()
{
    if (!m_writeRecordTacFields.lcDisplay) return;

    QString dataV = m_writeRecordTacFields.data
        ? m_writeRecordTacFields.data->text().trimmed().remove(' ').toUpper()
        : QString();

    m_writeRecordTacFields.lcDisplay->setText(
        QString("%1").arg(dataV.length() / 2, 2, 16, QChar('0')).toUpper());
}

void PageCardOperation::onWriteRecordWithTac()
{
    onWriteRecordWithTacLcUpdate();   // 確保 Lc 為最新值

    if (m_writeRecordTacFields.data &&
        m_writeRecordTacFields.data->text().trimmed().isEmpty()) {
        log("[Write Record With TAC] 請先輸入 DATA", true);
        return;
    }

    QString apdu = m_writeRecordTacFields.buildApdu();
    if (apdu.length() < 10) {
        log("[Write Record With TAC] APDU 不完整，請確認欄位", true);
        return;
    }

    log("====== Write Record With TAC ======");
    bool ok = false;
    QString resp = CardUtils::sendApdu(getTargetHandle(), apdu, ok,
        [this](const QString& msg, bool err) { log(msg, err); });
    if (!ok) { log("[Write Record With TAC] 通訊失敗", true); return; }

    QString sw = resp.right(4);
    QString data = resp.left(qMax(0, resp.length() - 4));
    if (sw == "9000") {
        log("✔ Write Record With TAC 成功 SW=9000");
        log("Data: " + (data.isEmpty() ? QString("(空)") : data));
    }
    else {
        log("⚠️ Write Record With TAC SW=" + sw, true);
    }
}

void PageCardOperation::onWriteRecordWSNumTac()
{
    if (m_writeRecordWSNumTacFields.data &&
        m_writeRecordWSNumTacFields.data->text().trimmed().isEmpty()) {
        log("[Write Record With S/N TAC] 請先輸入 DATA", true);
        return;
    }

    // 檢查 DATA 長度是否與 Lc 宣告的 bytes 數一致
    QString lcText = m_writeRecordWSNumTacFields.lc
        ? m_writeRecordWSNumTacFields.lc->text().trimmed().remove(' ').toUpper() : QString();
    QString dataText = m_writeRecordWSNumTacFields.data
        ? m_writeRecordWSNumTacFields.data->text().trimmed().remove(' ').toUpper() : QString();
    bool lcOk = false;
    int lcByteCount = lcText.toInt(&lcOk, 16);
    if (!lcOk || lcText.length() != 2) {
        log("[Write Record With S/N TAC] 請輸入正確的 Lc（2 個 HEX 字元）", true);
        return;
    }
    if (dataText.length() != lcByteCount * 2) {
        log(QString("[Write Record With S/N TAC] DATA 長度不符：Lc=%1 需要 %2 bytes（%3 個 HEX 字元），"
            "目前輸入 %4 個 HEX 字元")
            .arg(lcText).arg(lcByteCount).arg(lcByteCount * 2).arg(dataText.length()), true);
        return;
    }

    QString apdu = m_writeRecordWSNumTacFields.buildApdu();
    if (apdu.length() < 10) {
        log("[Write Record With S/N TAC] APDU 不完整，請確認欄位", true);
        return;
    }

    log("====== Write Record With S/N TAC ======");
    bool ok = false;
    QString resp = CardUtils::sendApdu(getTargetHandle(), apdu, ok,
        [this](const QString& msg, bool err) { log(msg, err); });
    if (!ok) { log("[Write Record With S/N TAC] 通訊失敗", true); return; }

    QString sw = resp.right(4);
    QString data = resp.left(qMax(0, resp.length() - 4));
    if (sw == "9000") {
        log("✔ Write Record With S/N TAC 成功 SW=9000");
        log("Data: " + (data.isEmpty() ? QString("(空)") : data));
    }
    else {
        log("⚠️ Write Record With S/N TAC SW=" + sw, true);
    }
}