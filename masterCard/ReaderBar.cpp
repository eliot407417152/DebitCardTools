#include "ReaderBar.h"
#include "ReaderManager.h"
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

ReaderBar::ReaderBar(ReaderManager* manager, int preferredIndex, QWidget* parent)
    : QWidget(parent), m_manager(manager), m_preferredIndex(preferredIndex)
{
    m_combo = new QComboBox(this);
    m_combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_combo->setMinimumContentsLength(18);
    m_combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_connectBtn = new QPushButton("🔌 連線", this);
    m_disconnectBtn = new QPushButton("⏏ 斷線", this);
    m_refreshBtn = new QPushButton("🔄 重新整理", this);
    m_connectBtn->setToolTip("連線到目前選擇的讀卡機");
    m_disconnectBtn->setToolTip("中斷與目前選擇讀卡機的連線");
    m_refreshBtn->setToolTip("重新偵測讀卡機並重新連線（重插卡片後按這個就能重新讀取，不用重開程式）");

    const QString btnStyle =
        "QPushButton { padding:4px 10px; border-radius:3px; background:#ECEFF1; border:1px solid #B0BEC5; }"
        "QPushButton:hover { background:#CFD8DC; }"
        "QPushButton:disabled { color:#999; background:#F5F5F5; }";
    m_connectBtn->setStyleSheet(btnStyle);
    m_disconnectBtn->setStyleSheet(btnStyle);
    m_refreshBtn->setStyleSheet(btnStyle);

    m_statusLbl = new QLabel(this);

    auto* row = new QHBoxLayout();
    row->setContentsMargins(0, 0, 0, 0);
    row->addWidget(m_combo, 1);
    row->addWidget(m_connectBtn);
    row->addWidget(m_disconnectBtn);
    row->addWidget(m_refreshBtn);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(2);
    lay->addLayout(row);
    lay->addWidget(m_statusLbl);

    connect(m_connectBtn, &QPushButton::clicked, this, &ReaderBar::onConnectClicked);
    connect(m_disconnectBtn, &QPushButton::clicked, this, &ReaderBar::onDisconnectClicked);
    connect(m_refreshBtn, &QPushButton::clicked, this, &ReaderBar::onRefreshClicked);
    connect(m_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this](int) { updateStatus(); });
    connect(m_manager, &ReaderManager::readersChanged, this, &ReaderBar::onReadersChanged);
    connect(m_manager, &ReaderManager::connectionChanged, this, &ReaderBar::onConnectionChanged);

    onReadersChanged();
}

QString ReaderBar::readerName() const
{
    return m_combo->currentText();
}

SCARDHANDLE ReaderBar::handle() const
{
    return m_manager->handle(readerName());
}

void ReaderBar::onConnectClicked()
{
    m_manager->connectReader(readerName());
    updateStatus();
}

void ReaderBar::onDisconnectClicked()
{
    m_manager->disconnectReader(readerName());
    updateStatus();
}

void ReaderBar::onRefreshClicked()
{
    // 清單更新與重新連線都由 ReaderManager 處理，所有頁面的 ReaderBar 會同步更新
    m_manager->refresh();
}

// 讀卡機清單更新：盡量保留原本選的那一台，找不到才用預設位置
void ReaderBar::onReadersChanged()
{
    const QString previous = m_combo->currentText();
    const QStringList readers = m_manager->readers();

    m_combo->blockSignals(true);
    m_combo->clear();
    m_combo->addItems(readers);

    int idx = previous.isEmpty() ? -1 : m_combo->findText(previous);
    if (idx < 0 && !readers.isEmpty())
        idx = qMin(m_preferredIndex, readers.size() - 1);
    m_combo->setCurrentIndex(idx);
    m_combo->blockSignals(false);

    updateStatus();
}

void ReaderBar::onConnectionChanged(const QString&)
{
    updateStatus();
}

void ReaderBar::updateStatus()
{
    const QString name = readerName();
    const bool hasReader = !name.isEmpty();
    const bool connected = hasReader && m_manager->isConnected(name);

    m_connectBtn->setEnabled(hasReader && !connected);
    m_disconnectBtn->setEnabled(connected);

    if (!hasReader) {
        m_statusLbl->setText("⚠ 找不到讀卡機，請插入後按「重新整理」");
        m_statusLbl->setStyleSheet("color:#D32F2F; font-size:11px;");
    }
    else if (connected) {
        m_statusLbl->setText("● 已連線");
        m_statusLbl->setStyleSheet("color:#2E7D32; font-size:11px; font-weight:bold;");
    }
    else {
        m_statusLbl->setText("○ 未連線（執行操作時會自動連線）");
        m_statusLbl->setStyleSheet("color:#888; font-size:11px;");
    }
}
