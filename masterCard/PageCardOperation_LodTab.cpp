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
// Tab 4 — 製卡發卡流程（原 LOD / 母卡，已調整至卡片指令頁籤之後）
// ─────────────────────────────────────────────────────────────────────────────
void PageCardOperation::setupLodTab(QWidget* tab)
{
    auto* layout = new QVBoxLayout(tab);

    // ── 製作母卡：需選擇兩台不同的讀卡機（ISAM / 母卡）──────────────────────
    auto* readerGrp = new QGroupBox("讀卡機選擇", tab);
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

    m_issueCardBtn = new QPushButton("💳 發卡", tab);
    m_issueCardBtn->setStyleSheet(
        "QPushButton { background:#2E7D32; color:white; padding:8px 16px; font-weight:bold; border-radius:4px; }"
        "QPushButton:disabled { background:#CCC; color:#666; }");

    m_fileLabel = new QLabel("尚未載入 — 或直接手動輸入", tab);
    m_fileLabel->setStyleSheet("color:#888; font-style:italic;");

    btnRow->addWidget(m_loadBtn);
    btnRow->addWidget(m_clearBtn);
    btnRow->addWidget(m_authBtn);
    btnRow->addWidget(m_makeMasterBtn);
    btnRow->addWidget(m_convertBtn);
    btnRow->addWidget(m_washCardBtn);
    btnRow->addWidget(m_issueCardBtn);
    btnRow->addWidget(m_fileLabel, 1);
    layout->addLayout(btnRow);

    // ── 發卡：00C2 Update Record Ciphered 自行輸入資料 ──────────────────────
    auto* issueGrp = new QGroupBox("發卡 — 00C2 Update Record Ciphered（自行輸入資料，HEX）", tab);
    auto* issueLay = new QHBoxLayout(issueGrp);
    m_issueC2DataEdit = makeHexField(tab, 0, "請輸入 00C2 Update Record Ciphered 的 DataIn（HEX）");
    m_issueC2DataEdit->setFixedWidth(360);
    issueLay->addWidget(m_issueC2DataEdit);
    layout->addWidget(issueGrp);

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
    connect(m_issueCardBtn, &QPushButton::clicked, this, &PageCardOperation::onIssueCard);

    // 客戶資料欄位如果是手動填入（而非透過載入 LOD），只要 C3 / C1 都有值，
    // 也要能啟用「製作母卡」按鈕，不強制一定要先載入檔案。
    connect(m_c3Edit, &QLineEdit::textChanged, this, &PageCardOperation::onCustomerFieldChanged);
    connect(m_c1Edit, &QLineEdit::textChanged, this, &PageCardOperation::onCustomerFieldChanged);
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

void PageCardOperation::onIssueCard()
{
    if (!m_canMakeMaster) {
        log("讀卡機數量不足 2 台，不提供發卡功能。", true);
        return;
    }

    QString isamReaderName = m_masterIsamReaderCombo ? m_masterIsamReaderCombo->currentText() : QString();
    QString cardReaderName = m_masterCardReaderCombo ? m_masterCardReaderCombo->currentText() : QString();
    if (isamReaderName.isEmpty() || cardReaderName.isEmpty() || isamReaderName == cardReaderName) {
        QMessageBox::warning(this, "設定錯誤", "請選擇兩台不同的讀卡機（ISAM／卡片）。");
        return;
    }

    QString manualC2Data = m_issueC2DataEdit ? m_issueC2DataEdit->text().trimmed().toUpper() : QString();
    if (manualC2Data.isEmpty()) {
        QMessageBox::warning(this, "資料不足", "請先輸入 00C2 Update Record Ciphered 的自行輸入資料（HEX）！");
        return;
    }

    SCARDHANDLE hIsam = connectReader(isamReaderName);
    SCARDHANDLE hCard = connectReader(cardReaderName);
    if (hIsam == 0 || hCard == 0) {
        QMessageBox::critical(this, "連線失敗", "無法連線所選讀卡機，請確認卡片已插好。");
        return;
    }

    log("====== 開始發卡 ======");
    CardIssuer issuer(hCard, hIsam);
    bool ok = issuer.execute(manualC2Data, [this](const QString& m, bool e) { log(m, e); });

    if (ok) QMessageBox::information(this, "完成", "🎉 發卡完成！");
    else    QMessageBox::critical(this, "失敗", "❌ 發卡失敗，請查看日誌。");
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
