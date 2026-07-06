#pragma once

#include <QWidget>
#include <QString>
#include <QStringList>
#include <winscard.h>

class QPushButton;
class QLabel;
class QTextEdit;

// ─────────────────────────────────────────────────────────────────────────────
// PageSelectReader
//
//【已依需求調整】
//   本頁不再提供「指派讀卡機用途」的手動選擇（不再有 ISAM／母卡 讀卡機下拉選單，
//   也不再有「母卡製作／一般操作」模式選擇）。
//   本頁僅單純「偵測」目前插著的讀卡機，只要偵測到至少 1 台讀卡機，
//   即可直接按「進入」前往操作頁面。
//
//   實際要用哪一台讀卡機做 ISAM 操作、哪一台做母卡操作，
//   改到「操作頁面 (PageCardOperation)」的各功能區塊中，
//   以下拉選單個別選擇（做法比照原本「卡片指令」頁的「發送對象」下拉選單）。
//   若目前偵測到的讀卡機少於 2 台，操作頁面會自動停用「製作母卡」相關功能。
// ─────────────────────────────────────────────────────────────────────────────
class PageSelectReader : public QWidget
{
    Q_OBJECT

public:
    explicit PageSelectReader(QWidget* parent = nullptr);
    ~PageSelectReader();

    // 提供給下一頁 (PageCardOperation) 使用：連線環境與目前偵測到的讀卡機名稱清單
    SCARDCONTEXT context() const;
    QStringList  readers() const;

signals:
    void readerSelected();

private slots:
    void onRefresh();
    void onConfirm();

private:
    void setupUI();
    void log(const QString& msg, bool isError = false);

    SCARDCONTEXT m_context = 0;
    QStringList  m_readers;

    QLabel* m_statusLabel = nullptr;
    QPushButton* m_refreshBtn = nullptr;
    QPushButton* m_confirmBtn = nullptr;
    QTextEdit* m_logEdit = nullptr;
};
