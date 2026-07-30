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
#include <QTextCursor>
#include <QTextBlockFormat>

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

    // Key Set / Data Set 管理視窗現在改由選單觸發開啟；提前建立好視窗（僅建立，不顯示）
    // 並嘗試自動載入程式目錄下的 KeySet.txt / DataSet.txt，這樣「卡片指令」頁籤上的
    // 唯讀狀態顯示，一開啟操作頁面就會是最新的，不用等使用者先手動開一次管理視窗。
    buildKeySetDataSetDialog();
    autoLoadKeySetAndDataSet();

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
    m_logEdit->setStyleSheet(
        QString("background:#121212; color:#00FF66; "
            "font-family:'Courier New',monospace; font-size:%1px;")
        .arg(m_logFontSize));
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
        .arg(color, QDateTime::currentDateTime().toString("hh:mm:ss"), msg.toHtmlEscaped()));

    // 讓剛新增的這一行也套用目前設定的行距，不用等下次調整設定才生效
    QTextCursor cursor = m_logEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    QTextBlockFormat fmt = cursor.blockFormat();
    fmt.setLineHeight(m_logLineSpacingPercent, QTextBlockFormat::ProportionalHeight);
    cursor.setBlockFormat(fmt);
}

// 設定選單「日誌顯示設定」使用：套用字體大小(px)與行距(百分比)，
// 並立即套用到目前已存在的每一行日誌，讓使用者馬上看到效果
void PageCardOperation::applyLogAppearance(int fontSizePx, int lineSpacingPercent)
{
    m_logFontSize = fontSizePx;
    m_logLineSpacingPercent = lineSpacingPercent;

    if (!m_logEdit)
        return;

    m_logEdit->setStyleSheet(
        QString("background:#121212; color:#00FF66; "
            "font-family:'Courier New',monospace; font-size:%1px;")
        .arg(m_logFontSize));

    QTextCursor cursor(m_logEdit->document());
    cursor.beginEditBlock();
    cursor.movePosition(QTextCursor::Start);
    QTextBlockFormat fmt;
    fmt.setLineHeight(m_logLineSpacingPercent, QTextBlockFormat::ProportionalHeight);
    do {
        cursor.setBlockFormat(fmt);
    } while (cursor.movePosition(QTextCursor::NextBlock));
    cursor.endEditBlock();
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