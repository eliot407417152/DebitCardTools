#include "PageSelectReader.h"
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDateTime>
#include <QDebug>
#include <cstring>

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
    // 本頁不再持有任何卡片連線 (SCARDHANDLE)，只需釋放環境 (context)。
    // 實際的讀卡機連線改由 PageCardOperation 依需求各自建立/釋放。
    if (m_context)
        SCardReleaseContext(m_context);
}

SCARDCONTEXT PageSelectReader::context() const { return m_context; }
QStringList  PageSelectReader::readers() const { return m_readers; }

// ─────────────────────────────────────────────────────────────────────────────
// UI —— 僅剩「狀態顯示」＋「重新偵測」＋「進入」＋ 日誌
// ─────────────────────────────────────────────────────────────────────────────
void PageSelectReader::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    m_statusLabel = new QLabel("正在偵測讀卡機...", this);
    m_statusLabel->setStyleSheet("font-size: 14px; font-weight: bold;");
    mainLayout->addWidget(m_statusLabel);

    auto* hintLbl = new QLabel(
        "說明：本頁僅偵測目前插入的讀卡機，偵測到至少 1 台即可進入。\n"
        "ISAM／母卡 個別要用哪一台讀卡機，請於下一頁各功能區塊自行選擇；\n"
        "若讀卡機少於 2 台，下一頁的「製作母卡」功能將自動停用。", this);
    hintLbl->setStyleSheet("color:#666; font-size:12px;");
    hintLbl->setWordWrap(true);
    mainLayout->addWidget(hintLbl);

    auto* btnRow = new QHBoxLayout();
    m_refreshBtn = new QPushButton("🔄 重新偵測", this);
    m_confirmBtn = new QPushButton("➡️ 進入", this);
    m_confirmBtn->setStyleSheet(
        "QPushButton { background:#0078D4; color:white; font-weight:bold; padding:6px 12px; }"
        "QPushButton:disabled { background:#CCC; color:#666; }");
    m_confirmBtn->setEnabled(false);
    btnRow->addWidget(m_refreshBtn);
    btnRow->addWidget(m_confirmBtn);
    mainLayout->addLayout(btnRow);

    auto* logGrp = new QGroupBox("偵測日誌");
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
// 偵測讀卡機（僅偵測，不進行任何 SCardConnect）
// ─────────────────────────────────────────────────────────────────────────────
void PageSelectReader::onRefresh()
{
    m_readers.clear();
    log("開始偵測讀卡機...");

    DWORD dwLen = SCARD_AUTOALLOCATE;
    LPSTR mszReaders = nullptr;
    LONG ret = SCardListReadersA(m_context, nullptr, reinterpret_cast<LPSTR>(&mszReaders), &dwLen);

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

    for (LPSTR p = mszReaders; *p != '\0'; p += strlen(p) + 1)
        m_readers.append(QString::fromLocal8Bit(p));
    SCardFreeMemory(m_context, mszReaders);

    if (m_readers.isEmpty()) {
        m_statusLabel->setText("⚠ 找不到任何讀卡機！");
        m_statusLabel->setStyleSheet("color:#D32F2F; font-weight:bold;");
        m_confirmBtn->setEnabled(false);
        log("未偵測到讀卡機，請插入硬體後重試。", true);
        return;
    }

    m_statusLabel->setText(QString("✔ 偵測到 %1 台讀卡機：%2")
        .arg(m_readers.size()).arg(m_readers.join("、")));
    m_statusLabel->setStyleSheet("color:#388E3C; font-weight:bold;");
    m_confirmBtn->setEnabled(true);

    if (m_readers.size() >= 2)
        log(QString("找到 %1 台讀卡機，可進行一般操作，亦可製作母卡。").arg(m_readers.size()));
    else
        log("找到 1 台讀卡機，可進行一般操作（製作母卡需至少 2 台讀卡機，目前不足）。");
}

// ─────────────────────────────────────────────────────────────────────────────
// 進入操作頁面：不再於此連線卡片，直接把 context + 讀卡機清單交給下一頁
// ─────────────────────────────────────────────────────────────────────────────
void PageSelectReader::onConfirm()
{
    if (m_readers.isEmpty()) {
        log("尚未偵測到讀卡機，無法進入。", true);
        return;
    }
    log("🎉 讀卡機就緒，進入操作頁面...");
    emit readerSelected();
}

void PageSelectReader::log(const QString& msg, bool isError)
{
    QString color = isError ? "#FF5555" : "#00FF00";
    m_logEdit->append(QString("<font color=\"%1\">[%2] %3</font>")
        .arg(color, QDateTime::currentDateTime().toString("hh:mm:ss"), msg));
}
