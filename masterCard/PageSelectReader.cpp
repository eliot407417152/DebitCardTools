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

SCARDHANDLE PageSelectReader::isamHandle()   const { return m_isamHandle; }
SCARDHANDLE PageSelectReader::masterHandle() const { return m_masterHandle; }

// ─────────────────────────────────────────────────────────────────────────────
// UI
// ─────────────────────────────────────────────────────────────────────────────
void PageSelectReader::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    m_statusLabel = new QLabel("正在偵測讀卡機...", this);
    m_statusLabel->setStyleSheet("font-size: 14px; font-weight: bold;");
    mainLayout->addWidget(m_statusLabel);

    auto* grp = new QGroupBox("讀卡機功能指派");
    auto* form = new QFormLayout(grp);
    m_isamCombo = new QComboBox(this);
    m_cardCombo = new QComboBox(this);
    form->addRow("🔑 ISAM 讀卡機：", m_isamCombo);
    form->addRow("💳 母卡 讀卡機：", m_cardCombo);
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

    if (readers.size() < 2) {
        m_statusLabel->setText(QString("⚠ 偵測到 %1 台讀卡機（需要至少 2 台）").arg(readers.size()));
        m_statusLabel->setStyleSheet("color:#E65100; font-weight:bold;");
        m_confirmBtn->setEnabled(false);
        log("硬體不足：請插入至少兩台讀卡機。", true);
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
    QString cardReader = m_cardCombo->currentText();

    if (isamReader == cardReader) {
        QMessageBox::warning(this, "設定錯誤", "ISAM 與母卡不可指派同一台讀卡機！");
        return;
    }

    if (m_isamHandle) { SCardDisconnect(m_isamHandle, SCARD_LEAVE_CARD); m_isamHandle = 0; }
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
    log("🎉 雙通道就緒，切換操作頁面...");

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
