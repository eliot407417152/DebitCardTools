#pragma once

#include <QMainWindow>
#include <QStackedWidget>

class PageSelectReader;

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

private:
    QStackedWidget* m_stack = nullptr;
    PageSelectReader* m_selectReaderPage = nullptr;
};
