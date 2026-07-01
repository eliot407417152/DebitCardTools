#pragma once

#include "CardUtils.h"
#include <QWidget>
#include <winscard.h>

class QComboBox;
class QPushButton;
class QLabel;
class QTextEdit;

// ─────────────────────────────────────────────────────────────────────────────
// PageSelectReader — 第一頁：偵測讀卡機 & 選擇 ISAM / 母卡 用途
// ─────────────────────────────────────────────────────────────────────────────
class PageSelectReader : public QWidget
{
    Q_OBJECT

public:
    explicit PageSelectReader(QWidget* parent = nullptr);
    ~PageSelectReader() override;

    SCARDHANDLE isamHandle()   const;
    SCARDHANDLE masterHandle() const;

signals:
    void readerSelected();   // 雙卡確認連線後發出，通知主視窗切換頁面

private slots:
    void onRefresh();
    void onConfirm();

private:
    void setupUI();
    bool connectCard(const QString& readerName, SCARDHANDLE& outHandle);
    void log(const QString& msg, bool isError = false);

    SCARDCONTEXT m_context = 0;
    SCARDHANDLE  m_isamHandle = 0;
    SCARDHANDLE  m_masterHandle = 0;

    QComboBox* m_isamCombo = nullptr;
    QComboBox* m_cardCombo = nullptr;
    QPushButton* m_refreshBtn = nullptr;
    QPushButton* m_confirmBtn = nullptr;
    QLabel* m_statusLabel = nullptr;
    QTextEdit* m_logEdit = nullptr;
};
