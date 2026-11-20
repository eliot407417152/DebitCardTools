#pragma once

#include <QMainWindow>

class PageCardOperation;

// ─────────────────────────────────────────────────────────────────────────────
// MainWindow — 主視窗。程式啟動後直接顯示操作工作區（PageCardOperation），
//              讀卡機的偵測 / 連線 / 斷線 / 重新整理改由各頁面上的 ReaderBar 處理，
//              不再有獨立的「偵測讀卡機」頁面。
// ─────────────────────────────────────────────────────────────────────────────
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onEditWindowSize();       // 設定選單：視窗大小設定
    void onEditLogAppearance();    // 設定選單：日誌字體大小/行距設定
    void onOpenKeySetDataSetManager(); // 設定選單：Key Set / Data Set 管理

private:
    void buildMenuBar();
    // 套用先前存過的視窗大小或預設大小，並夾在目前螢幕可用範圍內置中顯示
    void applyWindowSize();
    void applyClampedResize(int w, int h);
    // 從 config.ini 讀出使用者存過的日誌字體大小/行距，套用到操作頁面
    void applyLogSettingsToOperationPage();

    PageCardOperation* m_opPage = nullptr;
};