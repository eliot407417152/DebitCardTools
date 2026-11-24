#pragma once

#include <QWidget>
#include <QString>
#include <winscard.h>
#include "CardUtils.h"   // LogFn
#include "Hex.h"         // Bytes / toHex / fromHex
#include "Logger.h"      // CardFlow 系列用的 Logger

class QVBoxLayout;
class ReaderManager;
class ReaderBar;
class QLineEdit;

// ─────────────────────────────────────────────────────────────────────────────
// FlowPageBase — 「製作母卡 / 母卡轉換 / 洗卡 / 發卡」各流程頁面的共同基底
//
//   每個流程各自一個頁面（各一組 .h/.cpp），之後要調整某個流程時，
//   只需要改對應的那個頁面，不會牽動到其他流程。
//   共同的部分（日誌、取得兩台讀卡機連線、可捲動的頁面版型）放在這裡。
//
//   日誌：頁面不直接持有日誌視窗，而是發出 logMessage 訊號，
//         由 PageCardOperation 統一顯示在右側「通訊日誌」。
// ─────────────────────────────────────────────────────────────────────────────
class FlowPageBase : public QWidget
{
    Q_OBJECT

public:
    explicit FlowPageBase(ReaderManager* manager, QWidget* parent = nullptr);

signals:
    void logMessage(const QString& msg, bool isError);

protected:
    ReaderManager* readerManager() const { return m_manager; }

    void log(const QString& msg, bool isError = false) { emit logMessage(msg, isError); }
    LogFn logFn() { return [this](const QString& m, bool e) { log(m, e); }; }

    // 建立「可捲動」的頁面內容版型，回傳內層 layout 讓子類別往裡面加元件
    QVBoxLayout* createContentLayout();

    // 取得 ISAM 與另一台（母卡／卡片）讀卡機的連線代碼。
    // 兩台必須不同；任何一台取不到會跳出提示並回傳 false。
    bool acquireHandles(ReaderBar* isamBar, ReaderBar* otherBar,
        const QString& otherRole, SCARDHANDLE& hIsam, SCARDHANDLE& hOther);

    // 把 CardFlow 系列的 Logger 接到頁面日誌：每一行轉成 logMessage，
    // 內容含 "!!!"（CardFlow::fail 的標記）的行以錯誤色顯示。
    void bindLogger(Logger& lg);

    // 讀取十六進位輸入欄並轉成 bytes。
    //   expectedBytes >= 0：長度必須剛好等於該值；-1：不限長度（但不可為空）
    // 格式或長度不對時跳出提示並回傳 false。
    bool readHexInput(QLineEdit* edit, const QString& label, int expectedBytes, Bytes& out);

private:
    ReaderManager* m_manager = nullptr;
};
