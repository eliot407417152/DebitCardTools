#include "PageCardOperation.h"
#include "IsamKeyUpdater.h"
#include "IsamKeyChecker.h"
#include "IsamKeyData.h"
#include "ReaderManager.h"
#include "ReaderBar.h"
#include "PageMakeMaster.h"
#include "PageConvertMaster.h"
#include "PageWashCard.h"
#include "PageIssueCard.h"
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
#include <QTextCursor>
#include <QTextBlockFormat>
#include <initializer_list>

// ─────────────────────────────────────────────────────────────────────────────
// 建構子
//   不再由上一頁傳入讀卡機：程式一啟動就由 ReaderManager 自行偵測，
//   各頁面用 ReaderBar 選擇讀卡機，並可隨時「連線 / 斷線 / 重新整理」。
// ─────────────────────────────────────────────────────────────────────────────
PageCardOperation::PageCardOperation(QWidget* parent)
    : QWidget(parent)
{
    m_readerMgr = new ReaderManager(this);

    setupUI();

    // 日誌視窗建好之後，才接上讀卡機管理的訊息（連線 / 斷線 / 重新整理的結果都會顯示在日誌）
    connect(m_readerMgr, &ReaderManager::message, this, &PageCardOperation::log);

    // Key Set / Data Set 管理視窗現在改由選單觸發開啟；提前建立好視窗（僅建立，不顯示）
    // 並嘗試自動載入程式目錄下的 KeySet.txt / DataSet.txt，這樣「卡片指令」頁籤上的
    // 唯讀狀態顯示，一開啟操作頁面就會是最新的，不用等使用者先手動開一次管理視窗。
    buildKeySetDataSetDialog();
    autoLoadKeySetAndDataSet();

    const QStringList readers = m_readerMgr->readers();
    if (readers.isEmpty())
        log("⚠ 目前偵測不到任何讀卡機，請插入讀卡機後按任一頁面上的「🔄 重新整理」。", true);
    else
        log(QString("✔ 偵測到 %1 台讀卡機：%2").arg(readers.size()).arg(readers.join("、")));
}

PageCardOperation::~PageCardOperation()
{
    // 所有讀卡機連線由 ReaderManager（本物件的子物件）在解構時一併釋放
}

// ─────────────────────────────────────────────────────────────────────────────
// 讀卡機連線輔助：委派給 ReaderManager
//   已連線且仍有效 → 直接使用；卡片被拔掉 / 重插導致失效，或尚未連線 → 自動（重新）連線
// ─────────────────────────────────────────────────────────────────────────────
SCARDHANDLE PageCardOperation::connectReader(const QString& readerName) const
{
    return m_readerMgr->handle(readerName);
}

// ─────────────────────────────────────────────────────────────────────────────
// 主 UI
// ─────────────────────────────────────────────────────────────────────────────
void PageCardOperation::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* bodyRow = new QHBoxLayout();

    auto* tabWidget = new QTabWidget(this);
    tabWidget->setStyleSheet(
        "QTabBar::tab { padding:8px 10px; font-weight:bold; }"
        "QTabBar::tab:selected { background:#0078D4; color:white; }");

    auto* isamTab = new QWidget();  setupIsamTab(isamTab);
    auto* isamCmdTab = new QWidget();  setupIsamCmdTab(isamCmdTab);
    auto* apduTab = new QWidget();  setupApduTab(apduTab);
    auto* rawApduTab = new QWidget();  setupRawApduTab(rawApduTab);
    auto* cryptoTab = new QWidget();  setupCryptoTab(cryptoTab);

    // 製卡 / 發卡流程：每個流程各自獨立一個頁面（之後調整流程只改對應的頁面即可）
    auto* makeMasterPage = new PageMakeMaster(m_readerMgr);
    auto* convertMasterPage = new PageConvertMaster(m_readerMgr);
    auto* washCardPage = new PageWashCard(m_readerMgr);
    auto* issueCardPage = new PageIssueCard(m_readerMgr);
    for (FlowPageBase* page : std::initializer_list<FlowPageBase*>{
            makeMasterPage, convertMasterPage, washCardPage, issueCardPage })
            connect(page, &FlowPageBase::logMessage, this, &PageCardOperation::log);

    tabWidget->addTab(isamTab, "🔑 ISAM 金鑰");
    tabWidget->addTab(isamCmdTab, "🔧 ISAM 指令");
    tabWidget->addTab(apduTab, "📡 卡片指令");
    tabWidget->addTab(rawApduTab, "🧪 Raw APDU");
    tabWidget->addTab(cryptoTab, "🔐 加密工具");
    tabWidget->addTab(makeMasterPage, "🔮 製作母卡");
    tabWidget->addTab(convertMasterPage, "🔄 母卡轉換");
    tabWidget->addTab(washCardPage, "🧼 洗卡");
    tabWidget->addTab(issueCardPage, "💳 發卡");

    bodyRow->addWidget(tabWidget, 3);

    // 右側日誌
    auto* logGrp = new QGroupBox("通訊日誌");
    auto* logLay = new QVBoxLayout(logGrp);

    auto* logBtnRow = new QHBoxLayout();
    m_clearLogBtn = new QPushButton("🧹 清除日誌", logGrp);
    m_clearLogBtn->setStyleSheet(
        "QPushButton { background:#795548; color:white; padding:6px 12px; "
        "font-weight:bold; border-radius:4px; }");
    m_exportLogBtn = new QPushButton("💾 匯出日誌", logGrp);
    m_exportLogBtn->setStyleSheet(
        "QPushButton { background:#455A64; color:white; padding:6px 12px; "
        "font-weight:bold; border-radius:4px; }");
    logBtnRow->addStretch();
    logBtnRow->addWidget(m_clearLogBtn);
    logBtnRow->addWidget(m_exportLogBtn);
    logLay->addLayout(logBtnRow);

    m_logEdit = new QTextEdit(this);
    m_logEdit->setReadOnly(true);
    m_logEdit->setStyleSheet(logStyleSheet());
    logLay->addWidget(m_logEdit);
    bodyRow->addWidget(logGrp, 2);

    connect(m_exportLogBtn, &QPushButton::clicked, this, &PageCardOperation::onExportLog);
    connect(m_clearLogBtn, &QPushButton::clicked, this, &PageCardOperation::onClearLog);

    mainLayout->addLayout(bodyRow);
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

QString PageCardOperation::logStyleSheet() const
{
    return QString("background:%1; color:%2; border:1px solid #CCC; "
        "font-family:'Courier New',monospace; font-size:%3px;")
        .arg(m_logColors.background, m_logColors.text).arg(m_logFontSize);
}

// 把一筆日誌顯示到畫面上（套用目前的顏色與行距）
void PageCardOperation::appendLogEntry(const LogEntry& e)
{
    const QString color = e.isError ? m_logColors.error : m_logColors.text;
    m_logEdit->append(QString("<font color=\"%1\">%2</font>")
        .arg(color, e.text.toHtmlEscaped()));

    QTextCursor cursor = m_logEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    QTextBlockFormat fmt = cursor.blockFormat();
    fmt.setLineHeight(m_logLineSpacingPercent, QTextBlockFormat::ProportionalHeight);
    cursor.setBlockFormat(fmt);
}

void PageCardOperation::log(const QString& msg, bool isError)
{
    LogEntry e{ QString("[%1] %2").arg(QDateTime::currentDateTime().toString("hh:mm:ss"), msg), isError };
    m_logEntries.append(e);
    appendLogEntry(e);
}

// 設定選單「日誌顯示設定」使用：套用字體大小(px)、行距(百分比)與顏色，
// 並把目前已存在的每一行日誌依新設定重新顯示，讓使用者馬上看到效果
void PageCardOperation::applyLogAppearance(int fontSizePx, int lineSpacingPercent, const LogColors& colors)
{
    m_logFontSize = fontSizePx;
    m_logLineSpacingPercent = lineSpacingPercent;
    m_logColors = colors;

    if (!m_logEdit)
        return;

    m_logEdit->setStyleSheet(logStyleSheet());

    m_logEdit->clear();
    for (const LogEntry& e : m_logEntries)
        appendLogEntry(e);
}

void PageCardOperation::onClearLog()
{
    if (!m_logEdit || m_logEdit->toPlainText().trimmed().isEmpty()) {
        return; // 本來就沒有內容，不用特別提示
    }

    const auto ret = QMessageBox::question(this, "清除日誌",
        "確定要清除目前的通訊日誌嗎？此動作無法復原。",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes)
        return;

    m_logEdit->clear();
    m_logEntries.clear();
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