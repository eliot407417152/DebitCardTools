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

    auto* noteLbl = new QLabel("以下指令發送至上方選擇的讀卡機。左側點選指令名稱即可切換。", content);
    noteLbl->setStyleSheet("color:#555; font-size:11px; padding:4px;");
    layout->addWidget(noteLbl);

    // ── 左側指令清單 + 右側指令內容（取代原本一路往下排列，方便快速找到指令）──
    auto* isamCmdList = new QListWidget(content);
    isamCmdList->setFixedWidth(230);
    isamCmdList->setStyleSheet(
        "QListWidget { font-size:12px; }"
        "QListWidget::item { padding:8px 6px; }"
        "QListWidget::item:selected { background:#1565C0; color:white; }");
    auto* isamCmdStack = new QStackedWidget(content);

    auto* cmdSplitRow = new QHBoxLayout();
    cmdSplitRow->addWidget(isamCmdList);
    cmdSplitRow->addWidget(isamCmdStack, 1);
    layout->addLayout(cmdSplitRow, 1);

    connect(isamCmdList, &QListWidget::currentRowChanged, isamCmdStack, &QStackedWidget::setCurrentIndex);

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

    // 小工具：把指令群組加入清單與堆疊（取代原本的 layout->addWidget(xxxGrp)）
    // 【排版修正】直接把 grp 丟進 QStackedWidget 會被拉伸撐滿整個頁面高度，
    // 導致欄位/按鈕之間的間距被拉得很開。這裡改成包一層容器，
    // 讓 grp 保持原本自然大小、靠上對齊，剩餘空間留白在下方即可。
    auto addCmdPage = [&](QGroupBox* grp, const QString& title) {
        auto* page = new QWidget();
        auto* pageLay = new QVBoxLayout(page);
        pageLay->setContentsMargins(4, 4, 4, 4);
        pageLay->addWidget(grp);
        pageLay->addStretch();
        isamCmdList->addItem(title);
        isamCmdStack->addWidget(page);
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
    addCmdPage(selGrp, "① Select ISAM Object");
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
    addCmdPage(kccGrp, "② Get Key Check Code");
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
    addCmdPage(rndGrp, "③ Generate Random No");
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
    addCmdPage(tacGrp, "④ Get TerminalAuthCode");
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
    addCmdPage(ekGrp, "⑤ Get Encrypted Key");
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
    addCmdPage(wkGrp, "⑥ Write Key");
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
    addCmdPage(smkGrp, "⑦ Store Manufacturer Key");
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
    addCmdPage(sskGrp, "⑧ Set Session Key");
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
    addCmdPage(dkGrp, "⑨ Get Diversify Key with Session Key");
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
    addCmdPage(wkeyGrp, "⑩ Get Working Key with Session Key");
    connect(wkeyBtn, &QPushButton::clicked, this, &PageCardOperation::onIsamGetWorkingKey);

    isamCmdList->setCurrentRow(0);
    scroll->setWidget(content);

    auto* tabLayout = new QVBoxLayout(tab);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->addWidget(scroll);
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
