#include "PageSelectReader.h"
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QDateTime>
#include <QRadioButton>
#include <QButtonGroup>
#include <QDebug>

PageSelectReader::PageSelectReader(QWidget* parent)
    : QWidget(parent)
{
    LONG ret = SCardEstablishContext(SCARD_SCOPE_SYSTEM, nullptr, nullptr, &m_context);
    if (ret != SCARD_S_SUCCESS)
        qWarning("SCardEstablishContext 失敗: 0x%lx", ret);

    setupUI();
    onRefresh();
}

PageSelectReader::~PageSelectReader()
{
    if (m_isamHandle)   SCardDisconnect(m_isamHandle, SCARD_LEAVE_CARD);
    if (m_masterHandle) SCardDisconnect(m_masterHandle, SCARD_LEAVE_CARD);
    if (m_context)      SCardReleaseContext(m_context);
}

SCARDHANDLE   PageSelectReader::isamHandle()   const { return m_isamHandle; }
SCARDHANDLE   PageSelectReader::masterHandle() const { return m_masterHandle; }
OperationMode PageSelectReader::mode()         const { return m_mode; }

// ─────────────────────────────────────────────────────────────────────────────
// UI
// ─────────────────────────────────────────────────────────────────────────────
void PageSelectReader::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    // ── 操作模式選擇 ──────────────────────────────────────────────────────
    auto* modeGrp = new QGroupBox("操作模式");
    auto* modeLay = new QHBoxLayout(modeGrp);
    m_modeMasterRadio = new QRadioButton("🔮 母卡製作（需要 2 台讀卡機）", this);
    m_modeGeneralRadio = new QRadioButton("🛠 一般操作", this);
    m_modeMasterRadio->setChecked(true);

    m_modeGroup = new QButtonGroup(this);
    m_modeGroup->addButton(m_modeMasterRadio);
    m_modeGroup->addButton(m_modeGeneralRadio);

    modeLay->addWidget(m_modeMasterRadio);
    modeLay->addWidget(m_modeGeneralRadio);
    mainLayout->addWidget(modeGrp);

    m_statusLabel = new QLabel("正在偵測讀卡機...", this);
    m_statusLabel->setStyleSheet("font-size: 14px; font-weight: bold;");
    mainLayout->addWidget(m_statusLabel);

    auto* grp = new QGroupBox("讀卡機功能指派");
    m_readerForm = new QFormLayout(grp);
    m_isamCombo = new QComboBox(this);
    m_cardCombo = new QComboBox(this);
    m_readerForm->addRow("🔑 ISAM 讀卡機：", m_isamCombo);
    m_readerForm->addRow("💳 母卡 讀卡機：", m_cardCombo);
    m_isamRowLabel = qobject_cast<QLabel*>(m_readerForm->labelForField(m_isamCombo));
    m_cardRowLabel = qobject_cast<QLabel*>(m_readerForm->labelForField(m_cardCombo));
    mainLayout->addWidget(grp);

    auto* btnRow = new QHBoxLayout();
    m_refreshBtn = new QPushButton("🔄 重新偵測", this);
    m_confirmBtn = new QPushButton("➡️ 確認並連線", this);
    m_confirmBtn->setStyleSheet(
        "QPushButton { background:#0078D4; color:white; font-weight:bold; padding:6px 12px; }"
        "QPushButton:disabled { background:#CCC; color:#666; }");
    btnRow->addWidget(m_refreshBtn);
    btnRow->addWidget(m_confirmBtn);
    mainLayout->addLayout(btnRow);

    auto* logGrp = new QGroupBox("連線狀態日誌");
    auto* logLayout = new QVBoxLayout(logGrp);
    m_logEdit = new QTextEdit(this);
    m_logEdit->setReadOnly(true);
    m_logEdit->setStyleSheet("background:#1E1E1E; color:#D4D4D4; font-family:monospace;");
    logLayout->addWidget(m_logEdit);
    mainLayout->addWidget(logGrp);

    connect(m_refreshBtn, &QPushButton::clicked, this, &PageSelectReader::onRefresh);
    connect(m_confirmBtn, &QPushButton::clicked, this, &PageSelectReader::onConfirm);
    connect(m_modeMasterRadio, &QRadioButton::toggled, this, &PageSelectReader::onModeChanged);

    applyModeUI();
}

// ─────────────────────────────────────────────────────────────────────────────
// 模式切換
// ─────────────────────────────────────────────────────────────────────────────
void PageSelectReader::onModeChanged()
{
    m_mode = m_modeMasterRadio->isChecked()
        ? OperationMode::MasterCardProduction
        : OperationMode::GeneralOperation;

    applyModeUI();

    log(m_mode == OperationMode::MasterCardProduction
        ? "已切換為「母卡製作」模式（需要 2 台讀卡機）。"
        : "已切換為「一般操作」模式（僅需 1 台讀卡機，不含製作母卡功能）。");

    onRefresh();
}

void PageSelectReader::applyModeUI()
{
    bool isMaster = (m_mode == OperationMode::MasterCardProduction);

    if (m_cardCombo)    m_cardCombo->setVisible(isMaster);
    if (m_cardRowLabel) m_cardRowLabel->setVisible(isMaster);

    if (m_isamRowLabel)
        m_isamRowLabel->setText(isMaster ? "🔑 ISAM 讀卡機：" : "🔑 讀卡機：");
}

// ─────────────────────────────────────────────────────────────────────────────
// 偵測讀卡機
// ─────────────────────────────────────────────────────────────────────────────
void PageSelectReader::onRefresh()
{
    m_isamCombo->clear();
    m_cardCombo->clear();
    log("開始偵測讀卡機...");

    DWORD  dwLen = SCARD_AUTOALLOCATE;
    LPSTR  mszReaders = nullptr;
    LONG   ret = SCardListReadersA(m_context, nullptr, reinterpret_cast<LPSTR>(&mszReaders), &dwLen);

    if (ret == SCARD_E_NO_READERS_AVAILABLE) {
        m_statusLabel->setText("⚠ 找不到任何讀卡機！");
        m_statusLabel->setStyleSheet("color:#D32F2F; font-weight:bold;");
        m_confirmBtn->setEnabled(false);
        log("未偵測到讀卡機，請插入硬體後重試。", true);
        return;
    }
    if (ret != SCARD_S_SUCCESS) {
        log(QString("SCardListReaders 失敗: 0x%1").arg(ret, 0, 16), true);
        m_confirmBtn->setEnabled(false);
        return;
    }

    QStringList readers;
    for (LPSTR p = mszReaders; *p != '\0'; p += strlen(p) + 1)
        readers.append(QString::fromLocal8Bit(p));
    SCardFreeMemory(m_context, mszReaders);

    m_isamCombo->addItems(readers);
    m_cardCombo->addItems(readers);
    if (readers.size() > 1) m_cardCombo->setCurrentIndex(1);

    const int required = (m_mode == OperationMode::MasterCardProduction) ? 2 : 1;

    if (readers.size() < required) {
        m_statusLabel->setText(QString("⚠ 偵測到 %1 台讀卡機（此模式需要至少 %2 台）")
            .arg(readers.size()).arg(required));
        m_statusLabel->setStyleSheet("color:#E65100; font-weight:bold;");
        m_confirmBtn->setEnabled(false);
        log(QString("硬體不足：請插入至少 %1 台讀卡機。").arg(required), true);
    }
    else {
        m_statusLabel->setText(QString("✔ 偵測到 %1 台讀卡機").arg(readers.size()));
        m_statusLabel->setStyleSheet("color:#388E3C; font-weight:bold;");
        m_confirmBtn->setEnabled(true);
        log(QString("找到 %1 台讀卡機，請指派用途後按確認。").arg(readers.size()));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// 確認連線
// ─────────────────────────────────────────────────────────────────────────────
void PageSelectReader::onConfirm()
{
    QString isamReader = m_isamCombo->currentText();

    if (m_mode == OperationMode::MasterCardProduction) {
        QString cardReader = m_cardCombo->currentText();

        if (isamReader == cardReader) {
            QMessageBox::warning(this, "設定錯誤", "ISAM 與母卡不可指派同一台讀卡機！");
            return;
        }

        if (m_isamHandle) { SCardDisconnect(m_isamHandle, SCARD_LEAVE_CARD);   m_isamHandle = 0; }
        if (m_masterHandle) { SCardDisconnect(m_masterHandle, SCARD_LEAVE_CARD); m_masterHandle = 0; }

        log("測試 ISAM 卡片連線...");
        if (!connectCard(isamReader, m_isamHandle)) {
            QMessageBox::critical(this, "連線失敗", "無法連線 ISAM 讀卡機，請確認卡片已插好。");
            return;
        }
        log("✔ ISAM 連線成功。");

        log("測試母卡連線...");
        if (!connectCard(cardReader, m_masterHandle)) {
            SCardDisconnect(m_isamHandle, SCARD_LEAVE_CARD); m_isamHandle = 0;
            QMessageBox::critical(this, "連線失敗", "無法連線母卡讀卡機，請確認卡片已插好。");
            return;
        }
        log("✔ 母卡連線成功。");
        log("🎉 雙通道就緒（母卡製作模式），切換操作頁面...");
    }
    else {
        // 一般操作：僅需一台讀卡機，掛在 ISAM 通道；母卡通道維持 0
        if (m_isamHandle) { SCardDisconnect(m_isamHandle, SCARD_LEAVE_CARD);   m_isamHandle = 0; }
        if (m_masterHandle) { SCardDisconnect(m_masterHandle, SCARD_LEAVE_CARD); m_masterHandle = 0; }

        log("測試讀卡機連線...");
        if (!connectCard(isamReader, m_isamHandle)) {
            QMessageBox::critical(this, "連線失敗", "無法連線讀卡機，請確認卡片已插好。");
            return;
        }
        log("✔ 讀卡機連線成功（一般操作模式，僅單一通道）。");
        log("🎉 就緒，切換操作頁面...");
    }

    emit readerSelected();
}

bool PageSelectReader::connectCard(const QString& readerName, SCARDHANDLE& outHandle)
{
    DWORD proto = 0;
    LONG  ret = SCardConnectA(m_context,
        readerName.toLocal8Bit().constData(),
        SCARD_SHARE_SHARED,
        SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
        &outHandle, &proto);
    if (ret != SCARD_S_SUCCESS) {
        log(QString("SCardConnect 失敗: 0x%1").arg(ret, 0, 16), true);
        return false;
    }
    return true;
}

void PageSelectReader::log(const QString& msg, bool isError)
{
    QString color = isError ? "#FF5555" : "#00FF00";
    m_logEdit->append(QString("<font color=\"%1\">[%2] %3</font>")
        .arg(color, QDateTime::currentDateTime().toString("hh:mm:ss"), msg));
}
