#include "PageCardOperation.h"
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QRadioButton>
#include <QButtonGroup>
#include <QScrollArea>
#include <QListWidget>
#include <QStackedWidget>

// ─────────────────────────────────────────────────────────────────────────────
// Tab — 加密工具（DES / 3DES，ECB / CBC）
//   比照「卡片指令」頁籤的呈現方式：左側清單選擇 DES / 3DES，右側顯示對應頁面
// ─────────────────────────────────────────────────────────────────────────────
void PageCardOperation::setupCryptoTab(QWidget* tab)
{
    auto* scroll = new QScrollArea(tab);
    scroll->setWidgetResizable(true);
    auto* content = new QWidget();
    auto* layout = new QVBoxLayout(content);
    layout->setSpacing(12);

    auto* noteLbl = new QLabel(
        "左側選擇 DES 或 3DES，右側帶入 IV（CBC 用）、Key、資料後按「加密」或「解密」。\n"
        "所有欄位皆為 HEX 字串；資料長度須為 8 bytes（16 個 HEX 字元）的整數倍。", content);
    noteLbl->setStyleSheet("color:#555; font-size:11px; padding:4px;");
    layout->addWidget(noteLbl);

    // ── 左側清單 + 右側堆疊（比照卡片指令頁籤做法）────────────────────────
    auto* cryptoList = new QListWidget(content);
    cryptoList->setFixedWidth(160);
    cryptoList->setStyleSheet(
        "QListWidget { font-size:12px; }"
        "QListWidget::item { padding:8px 6px; }"
        "QListWidget::item:selected { background:#1565C0; color:white; }");
    auto* cryptoStack = new QStackedWidget(content);

    auto* splitRow = new QHBoxLayout();
    splitRow->addWidget(cryptoList);
    splitRow->addWidget(cryptoStack, 1);
    layout->addLayout(splitRow, 1);

    connect(cryptoList, &QListWidget::currentRowChanged, cryptoStack, &QStackedWidget::setCurrentIndex);

    // 小工具：把一個群組包成頁面，加入清單與堆疊
    auto addCryptoPage = [&](QGroupBox* grp, const QString& title) {
        auto* page = new QWidget();
        auto* pageLay = new QVBoxLayout(page);
        pageLay->setContentsMargins(4, 4, 4, 4);
        pageLay->addWidget(grp);
        pageLay->addStretch();
        cryptoList->addItem(title);
        cryptoStack->addWidget(page);
        };

    // ── DES 頁面 ──────────────────────────────────────────────────────────
    auto* desGrp = new QGroupBox("DES", content);
    auto* desLay = new QVBoxLayout(desGrp);

    auto* desModeRow = new QHBoxLayout();
    desModeRow->addWidget(new QLabel("模式：", desGrp));
    m_cryptoFields.desModeEcb = new QRadioButton("ECB", desGrp);
    m_cryptoFields.desModeCbc = new QRadioButton("CBC", desGrp);
    m_cryptoFields.desModeEcb->setChecked(true);
    auto* desModeGroup = new QButtonGroup(desGrp);
    desModeGroup->addButton(m_cryptoFields.desModeEcb);
    desModeGroup->addButton(m_cryptoFields.desModeCbc);
    desModeRow->addWidget(m_cryptoFields.desModeEcb);
    desModeRow->addWidget(m_cryptoFields.desModeCbc);
    desModeRow->addStretch();
    desLay->addLayout(desModeRow);

    auto* desKeyForm = new QFormLayout();
    m_cryptoFields.desIv = makeEditField(desGrp, false);
    m_cryptoFields.desIv->setMaxLength(16);
    m_cryptoFields.desIv->setPlaceholderText("0000000000000000");
    m_cryptoFields.desIv->setEnabled(false);   // 預設 ECB，IV 不可輸入
    desKeyForm->addRow("IV（CBC 用）：", m_cryptoFields.desIv);

    m_cryptoFields.desKey = makeEditField(desGrp, false);
    m_cryptoFields.desKey->setMaxLength(16);
    m_cryptoFields.desKey->setPlaceholderText("8 bytes / 16 個 HEX 字元");
    desKeyForm->addRow("Key：", m_cryptoFields.desKey);
    desLay->addLayout(desKeyForm);

    auto* desDataInLbl = new QLabel("輸入資料（HEX，加密時為明文／解密時為密文）：", desGrp);
    desDataInLbl->setStyleSheet("font-weight:bold; font-size:11px;");
    m_cryptoFields.desDataIn = makeEditField(desGrp, false);
    m_cryptoFields.desDataIn->setPlaceholderText("輸入 HEX 字元，長度需為 16 的倍數（每 8 bytes 一個區塊）");
    desLay->addWidget(desDataInLbl);
    desLay->addWidget(m_cryptoFields.desDataIn);

    auto* desDataOutLbl = new QLabel("計算結果：", desGrp);
    desDataOutLbl->setStyleSheet("font-weight:bold; font-size:11px;");
    m_cryptoFields.desDataOut = makeEditField(desGrp, true);
    desLay->addWidget(desDataOutLbl);
    desLay->addWidget(m_cryptoFields.desDataOut);

    auto* desBtnRow = new QHBoxLayout();
    auto* desEncBtn = new QPushButton("🔒 加密", desGrp);
    desEncBtn->setStyleSheet(
        "QPushButton { background:#2E7D32; color:white; padding:8px; "
        "font-weight:bold; border-radius:4px; }");
    auto* desDecBtn = new QPushButton("🔓 解密", desGrp);
    desDecBtn->setStyleSheet(
        "QPushButton { background:#C62828; color:white; padding:8px; "
        "font-weight:bold; border-radius:4px; }");
    desBtnRow->addWidget(desEncBtn);
    desBtnRow->addWidget(desDecBtn);
    desLay->addLayout(desBtnRow);

    addCryptoPage(desGrp, "DES");
    connect(desEncBtn, &QPushButton::clicked, this, &PageCardOperation::onCryptoDesEncrypt);
    connect(desDecBtn, &QPushButton::clicked, this, &PageCardOperation::onCryptoDesDecrypt);
    connect(m_cryptoFields.desModeCbc, &QRadioButton::toggled, this,
        [this](bool checked) { if (m_cryptoFields.desIv) m_cryptoFields.desIv->setEnabled(checked); });

    // ── 3DES 頁面 ─────────────────────────────────────────────────────────
    auto* tdesGrp = new QGroupBox("3DES", content);
    auto* tdesLay = new QVBoxLayout(tdesGrp);

    auto* tdesModeRow = new QHBoxLayout();
    tdesModeRow->addWidget(new QLabel("模式：", tdesGrp));
    m_cryptoFields.tdesModeEcb = new QRadioButton("ECB", tdesGrp);
    m_cryptoFields.tdesModeCbc = new QRadioButton("CBC", tdesGrp);
    m_cryptoFields.tdesModeEcb->setChecked(true);
    auto* tdesModeGroup = new QButtonGroup(tdesGrp);
    tdesModeGroup->addButton(m_cryptoFields.tdesModeEcb);
    tdesModeGroup->addButton(m_cryptoFields.tdesModeCbc);
    tdesModeRow->addWidget(m_cryptoFields.tdesModeEcb);
    tdesModeRow->addWidget(m_cryptoFields.tdesModeCbc);
    tdesModeRow->addStretch();
    tdesLay->addLayout(tdesModeRow);

    auto* tdesKeyForm = new QFormLayout();
    m_cryptoFields.tdesIv = makeEditField(tdesGrp, false);
    m_cryptoFields.tdesIv->setMaxLength(16);
    m_cryptoFields.tdesIv->setPlaceholderText("0000000000000000");
    m_cryptoFields.tdesIv->setEnabled(false);   // 預設 ECB，IV 不可輸入
    tdesKeyForm->addRow("IV（CBC 用）：", m_cryptoFields.tdesIv);

    m_cryptoFields.tdesK1 = makeEditField(tdesGrp, false);
    m_cryptoFields.tdesK1->setMaxLength(16);
    m_cryptoFields.tdesK1->setPlaceholderText("8 bytes / 16 個 HEX 字元");
    tdesKeyForm->addRow("K1：", m_cryptoFields.tdesK1);

    m_cryptoFields.tdesK2 = makeEditField(tdesGrp, false);
    m_cryptoFields.tdesK2->setMaxLength(16);
    m_cryptoFields.tdesK2->setPlaceholderText("8 bytes / 16 個 HEX 字元");
    tdesKeyForm->addRow("K2：", m_cryptoFields.tdesK2);

    m_cryptoFields.tdesK3 = makeEditField(tdesGrp, false);
    m_cryptoFields.tdesK3->setMaxLength(16);
    m_cryptoFields.tdesK3->setPlaceholderText("8 bytes / 16 個 HEX 字元；留空＝雙長度 3DES（K1,K2,K1）");
    tdesKeyForm->addRow("K3：", m_cryptoFields.tdesK3);
    tdesLay->addLayout(tdesKeyForm);

    auto* tdesDataInLbl = new QLabel("輸入資料（HEX，加密時為明文／解密時為密文）：", tdesGrp);
    tdesDataInLbl->setStyleSheet("font-weight:bold; font-size:11px;");
    m_cryptoFields.tdesDataIn = makeEditField(tdesGrp, false);
    m_cryptoFields.tdesDataIn->setPlaceholderText("輸入 HEX 字元，長度需為 16 的倍數（每 8 bytes 一個區塊）");
    tdesLay->addWidget(tdesDataInLbl);
    tdesLay->addWidget(m_cryptoFields.tdesDataIn);

    auto* tdesDataOutLbl = new QLabel("計算結果：", tdesGrp);
    tdesDataOutLbl->setStyleSheet("font-weight:bold; font-size:11px;");
    m_cryptoFields.tdesDataOut = makeEditField(tdesGrp, true);
    tdesLay->addWidget(tdesDataOutLbl);
    tdesLay->addWidget(m_cryptoFields.tdesDataOut);

    auto* tdesBtnRow = new QHBoxLayout();
    auto* tdesEncBtn = new QPushButton("🔒 加密", tdesGrp);
    tdesEncBtn->setStyleSheet(
        "QPushButton { background:#2E7D32; color:white; padding:8px; "
        "font-weight:bold; border-radius:4px; }");
    auto* tdesDecBtn = new QPushButton("🔓 解密", tdesGrp);
    tdesDecBtn->setStyleSheet(
        "QPushButton { background:#C62828; color:white; padding:8px; "
        "font-weight:bold; border-radius:4px; }");
    tdesBtnRow->addWidget(tdesEncBtn);
    tdesBtnRow->addWidget(tdesDecBtn);
    tdesLay->addLayout(tdesBtnRow);

    addCryptoPage(tdesGrp, "3DES");
    connect(tdesEncBtn, &QPushButton::clicked, this, &PageCardOperation::onCryptoTdesEncrypt);
    connect(tdesDecBtn, &QPushButton::clicked, this, &PageCardOperation::onCryptoTdesDecrypt);
    connect(m_cryptoFields.tdesModeCbc, &QRadioButton::toggled, this,
        [this](bool checked) { if (m_cryptoFields.tdesIv) m_cryptoFields.tdesIv->setEnabled(checked); });

    cryptoList->setCurrentRow(0);

    scroll->setWidget(content);
    auto* tabLayout = new QVBoxLayout(tab);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->addWidget(scroll);
}

// ─────────────────────────────────────────────────────────────────────────────
// 加密工具 — 共用計算邏輯
// ─────────────────────────────────────────────────────────────────────────────
void PageCardOperation::cryptoCalculate(bool isDes, bool encrypt)
{
    QLineEdit* ivField = isDes ? m_cryptoFields.desIv : m_cryptoFields.tdesIv;
    QLineEdit* dataInField = isDes ? m_cryptoFields.desDataIn : m_cryptoFields.tdesDataIn;
    QLineEdit* dataOutField = isDes ? m_cryptoFields.desDataOut : m_cryptoFields.tdesDataOut;
    bool isCbc = isDes
        ? (m_cryptoFields.desModeCbc && m_cryptoFields.desModeCbc->isChecked())
        : (m_cryptoFields.tdesModeCbc && m_cryptoFields.tdesModeCbc->isChecked());

    QString iv = ivField ? ivField->text().trimmed().remove(' ').toUpper() : QString();
    QString data = dataInField ? dataInField->text().trimmed().remove(' ').toUpper() : QString();

    QString key;
    if (isDes) {
        key = m_cryptoFields.desKey ? m_cryptoFields.desKey->text().trimmed().remove(' ').toUpper() : QString();
        if (key.length() != 16) {
            log("[加密工具] DES 的 Key 長度須為 16 個 HEX 字元（8 bytes）", true);
            return;
        }
    }
    else {
        QString k1 = m_cryptoFields.tdesK1 ? m_cryptoFields.tdesK1->text().trimmed().remove(' ').toUpper() : QString();
        QString k2 = m_cryptoFields.tdesK2 ? m_cryptoFields.tdesK2->text().trimmed().remove(' ').toUpper() : QString();
        QString k3 = m_cryptoFields.tdesK3 ? m_cryptoFields.tdesK3->text().trimmed().remove(' ').toUpper() : QString();
        if (k1.length() != 16 || k2.length() != 16) {
            log("[加密工具] 3DES 的 K1 / K2 長度須各為 16 個 HEX 字元（8 bytes）", true);
            return;
        }
        if (!k3.isEmpty() && k3.length() != 16) {
            log("[加密工具] 3DES 的 K3 長度須為 16 個 HEX 字元（8 bytes），或留空使用雙長度 3DES", true);
            return;
        }
        key = k3.isEmpty() ? (k1 + k2) : (k1 + k2 + k3);
    }

    if (data.isEmpty()) {
        log("[加密工具] 請輸入資料", true);
        return;
    }
    if (data.length() % 16 != 0) {
        log("[加密工具] 資料長度須為 16 個 HEX 字元（8 bytes）的倍數", true);
        return;
    }
    if (isCbc && !iv.isEmpty() && iv.length() != 16) {
        log("[加密工具] IV 長度須為 16 個 HEX 字元（8 bytes），留空則預設為全 0", true);
        return;
    }

    QString result = isCbc
        ? CardUtils::desCbc(key, iv, data, encrypt)
        : CardUtils::desEcb(key, data, encrypt);

    if (result.isEmpty()) {
        log("[加密工具] 計算失敗，請確認 Key / 資料長度是否正確", true);
        return;
    }

    if (dataOutField) dataOutField->setText(result);

    QString algLbl = isDes ? "DES" : "3DES";
    QString modeLbl = isCbc ? "CBC" : "ECB";
    QString dirLbl = encrypt ? "加密" : "解密";
    log(QString("====== 加密工具：%1 / %2 / %3 ======").arg(algLbl, modeLbl, dirLbl));
    log("Key : " + key);
    if (isCbc) log("IV  : " + (iv.isEmpty() ? QString("0000000000000000（預設全 0）") : iv));
    log("輸入: " + data);
    log("結果: " + result);
}

void PageCardOperation::onCryptoDesEncrypt() { cryptoCalculate(/*isDes=*/true, /*encrypt=*/true); }
void PageCardOperation::onCryptoDesDecrypt() { cryptoCalculate(/*isDes=*/true, /*encrypt=*/false); }
void PageCardOperation::onCryptoTdesEncrypt() { cryptoCalculate(/*isDes=*/false, /*encrypt=*/true); }
void PageCardOperation::onCryptoTdesDecrypt() { cryptoCalculate(/*isDes=*/false, /*encrypt=*/false); }
