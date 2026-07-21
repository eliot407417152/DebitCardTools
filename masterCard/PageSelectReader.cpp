#include "PageSelectReader.h"
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
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
// UI —— 簡化版：狀態文字 + 讀卡機清單 + 重新偵測／進入
// ─────────────────────────────────────────────────────────────────────────────
void PageSelectReader::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    m_statusLabel = new QLabel("正在偵測讀卡機...", this);
    m_statusLabel->setStyleSheet("font-size: 14px; font-weight: bold;");
    mainLayout->addWidget(m_statusLabel);

    m_readerListWidget = new QListWidget(this);
    m_readerListWidget->setStyleSheet(
        "QListWidget { font-size:13px; padding:4px; }"
        "QListWidget::item { padding:6px; }");
    mainLayout->addWidget(m_readerListWidget, 1);

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

    connect(m_refreshBtn, &QPushButton::clicked, this, &PageSelectReader::onRefresh);
    connect(m_confirmBtn, &QPushButton::clicked, this, &PageSelectReader::onConfirm);
}

// ─────────────────────────────────────────────────────────────────────────────
// 偵測讀卡機（僅偵測，不進行任何 SCardConnect）
// ─────────────────────────────────────────────────────────────────────────────
void PageSelectReader::onRefresh()
{
    m_readers.clear();
    m_readerListWidget->clear();
    setStatus("正在偵測讀卡機...");

    DWORD dwLen = SCARD_AUTOALLOCATE;
    LPSTR mszReaders = nullptr;
    LONG ret = SCardListReadersA(m_context, nullptr, reinterpret_cast<LPSTR>(&mszReaders), &dwLen);

    if (ret == SCARD_E_NO_READERS_AVAILABLE) {
        setStatus("⚠ 找不到任何讀卡機！", true);
        m_confirmBtn->setEnabled(false);
        return;
    }
    if (ret != SCARD_S_SUCCESS) {
        setStatus(QString("⚠ 偵測失敗 (0x%1)").arg(ret, 0, 16), true);
        m_confirmBtn->setEnabled(false);
        return;
    }

    for (LPSTR p = mszReaders; *p != '\0'; p += strlen(p) + 1)
        m_readers.append(QString::fromLocal8Bit(p));
    SCardFreeMemory(m_context, mszReaders);

    if (m_readers.isEmpty()) {
        setStatus("⚠ 找不到任何讀卡機！", true);
        m_confirmBtn->setEnabled(false);
        return;
    }

    for (const QString& name : m_readers)
        m_readerListWidget->addItem("🔌 " + name);

    setStatus(QString("✔ 偵測到 %1 台讀卡機").arg(m_readers.size()));
    m_confirmBtn->setEnabled(true);
}

// ─────────────────────────────────────────────────────────────────────────────
// 進入操作頁面：不再於此連線卡片，直接把 context + 讀卡機清單交給下一頁
// ─────────────────────────────────────────────────────────────────────────────
void PageSelectReader::onConfirm()
{
    if (m_readers.isEmpty()) {
        setStatus("⚠ 尚未偵測到讀卡機，無法進入。", true);
        return;
    }
    emit readerSelected();
}

void PageSelectReader::setStatus(const QString& msg, bool isError)
{
    m_statusLabel->setText(msg);
    m_statusLabel->setStyleSheet(isError
        ? "font-size:14px; font-weight:bold; color:#D32F2F;"
        : "font-size:14px; font-weight:bold; color:#388E3C;");
}
