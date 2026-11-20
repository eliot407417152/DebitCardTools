#include "PageCardOperation.h"
#include "IsamKeyUpdater.h"
#include "IsamKeyChecker.h"
#include "IsamKeyData.h"
#include "MasterCardUpdater.h"
#include "ReaderManager.h"
#include "ReaderBar.h"
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
// Tab 1 — ISAM 金鑰（金鑰本體隱藏，唯讀）
// ─────────────────────────────────────────────────────────────────────────────
void PageCardOperation::setupIsamTab(QWidget* tab)
{
    auto* layout = new QVBoxLayout(tab);

    // ── 讀卡機選擇（本頁指令要發送到哪一台讀卡機）──────────────────────────
    auto* readerGrp = new QGroupBox("讀卡機選擇");
    auto* readerForm = new QFormLayout(readerGrp);
    auto* readerBar = new ReaderBar(m_readerMgr, 0, tab);
    m_isamKeyReaderCombo = readerBar->combo();
    readerForm->addRow("發送對象：", readerBar);
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
