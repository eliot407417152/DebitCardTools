#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <winscard.h>

// ─────────────────────────────────────────────────────────────────────────────
// ReaderManager — 全程式共用的讀卡機管理（取代原本的「偵測讀卡機」頁面）
//
//   • 啟動時自動偵測讀卡機（不需要再停在一個「顯示可用讀卡機」的頁面）
//   • 連線 / 斷線 / 重新整理 都集中在這裡，各頁面的 ReaderBar 只是操作它的介面
//   • 連線代碼以「讀卡機名稱」快取；每次取用前會先確認連線還有效，
//     卡片被拔掉或重插導致連線失效時會自動重連，不必重開程式
//   • 重新整理 = 放掉所有連線 → 重建 PC/SC 環境 → 重新偵測讀卡機 →
//     把重新整理前原本已連線的讀卡機再連線一次
// ─────────────────────────────────────────────────────────────────────────────
class ReaderManager : public QObject
{
    Q_OBJECT

public:
    explicit ReaderManager(QObject* parent = nullptr);
    ~ReaderManager() override;

    SCARDCONTEXT context() const { return m_context; }
    QStringList  readers() const { return m_readers; }

    bool isConnected(const QString& readerName) const;

    // 明確連線（連線按鈕使用）。已連線且仍有效時直接回傳 true。
    bool connectReader(const QString& readerName);

    // 取得可用的連線代碼：已連線且有效就直接回傳；失效或尚未連線則自動（重新）連線。
    // 失敗回傳 0（原因會從 message() 送出）。
    SCARDHANDLE handle(const QString& readerName);

    void disconnectReader(const QString& readerName);
    void disconnectAll();

    // 重新整理：見類別說明。回傳是否偵測到至少 1 台讀卡機。
    bool refresh();

signals:
    void readersChanged();                              // 讀卡機清單有更新
    void connectionChanged(const QString& readerName);  // 某台讀卡機的連線狀態改變
    void message(const QString& msg, bool isError);     // 要顯示在日誌的訊息

private:
    bool establishContext();
    void releaseContext();
    void listReaders();
    bool isHandleAlive(SCARDHANDLE h) const;
    bool doConnect(const QString& readerName);
    static QString errorText(LONG code);

    SCARDCONTEXT m_context = 0;
    QStringList  m_readers;
    QMap<QString, SCARDHANDLE> m_handles;
};
