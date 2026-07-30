#pragma once

#include <QMainWindow>
#include <QStackedWidget>

class PageSelectReader;
class PageCardOperation;

// ─────────────────────────────────────────────────────────────────────────────
// MainWindow — 管理頁面堆疊切換（PageSelectReader ↔ PageCardOperation）
// ─────────────────────────────────────────────────────────────────────────────
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onReaderSelected();
    void onBackToSelectReader();   // 返回「選擇讀卡機」頁面(選單「程式→返回偵測頁面」與頁面內的返回按鈕共用)
    void onEditWindowSize();       // 設定選單：視窗大小設定
    void onEditLogAppearance();    // 設定選單：日誌字體大小/行距設定
    void onOpenKeySetDataSetManager(); // 設定選單：Key Set / Data Set 管理

private:
    void buildMenuBar();
    // 依目前顯示的頁面(選擇讀卡機 / 操作工作區)，套用先前存過的大小或預設大小，
    // 並夾在目前螢幕可用範圍內置中顯示
    void applyWindowSizeForCurrentPage();
    void applyClampedResize(int w, int h);
    // 從 config.ini 讀出使用者存過的日誌字體大小/行距，套用到目前的操作頁面(若已存在)
    void applyLogSettingsToOperationPage();

    QStackedWidget* m_stack = nullptr;
    PageSelectReader* m_selectReaderPage = nullptr;
    PageCardOperation* m_opPage = nullptr;   // 目前顯示中的操作頁面(尚未進入操作頁面時為 nullptr)
};