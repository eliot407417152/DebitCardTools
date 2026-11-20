#pragma once

#include <QWidget>
#include <QString>
#include <winscard.h>

class QComboBox;
class QPushButton;
class QLabel;
class ReaderManager;

// ─────────────────────────────────────────────────────────────────────────────
// ReaderBar — 「選擇讀卡機」共用元件
//
//   [ 讀卡機下拉選單 ▼ ] [連線] [斷線] [🔄 重新整理]
//   ● 已連線 / ○ 未連線
//
//   凡是需要讓使用者選讀卡機的地方（ISAM 金鑰、ISAM 指令、卡片指令、
//   製作母卡、母卡轉換、洗卡、發卡…）都放一個這個元件即可，
//   連線 / 斷線 / 重新整理的邏輯統一在 ReaderManager。
//   重插卡片後按「重新整理」（或「斷線」→「連線」）就能重新讀取，不必重開程式。
// ─────────────────────────────────────────────────────────────────────────────
class ReaderBar : public QWidget
{
    Q_OBJECT

public:
    // preferredIndex：沒有先前選擇時，預設選第幾台（例如母卡讀卡機預設選第 2 台）
    explicit ReaderBar(ReaderManager* manager, int preferredIndex = 0, QWidget* parent = nullptr);

    QComboBox* combo() const { return m_combo; }
    QString    readerName() const;

    // 取得目前選擇讀卡機的連線代碼（尚未連線或連線已失效時會自動連線）；失敗回傳 0
    SCARDHANDLE handle() const;

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onRefreshClicked();
    void onReadersChanged();
    void onConnectionChanged(const QString& readerName);
    void updateStatus();

private:
    ReaderManager* m_manager = nullptr;
    int            m_preferredIndex = 0;

    QComboBox* m_combo = nullptr;
    QPushButton* m_connectBtn = nullptr;
    QPushButton* m_disconnectBtn = nullptr;
    QPushButton* m_refreshBtn = nullptr;
    QLabel* m_statusLbl = nullptr;
};
