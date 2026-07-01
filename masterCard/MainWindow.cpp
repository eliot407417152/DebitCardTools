#include "MainWindow.h"
#include "PageSelectReader.h"
#include "PageCardOperation.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    m_stack = new QStackedWidget(this);
    setCentralWidget(m_stack);

    m_selectReaderPage = new PageSelectReader(this);
    m_stack->addWidget(m_selectReaderPage);

    connect(m_selectReaderPage, &PageSelectReader::readerSelected,
        this, &MainWindow::onReaderSelected);

    setWindowTitle("智慧卡產製系統 - 請選擇讀卡機");
    resize(550, 480);
}

void MainWindow::onReaderSelected()
{
    auto* opPage = new PageCardOperation(
        m_selectReaderPage->isamHandle(),
        m_selectReaderPage->masterHandle(),
        this);

    connect(opPage, &PageCardOperation::backRequested, [this, opPage]() {
        m_stack->setCurrentIndex(0);
        setWindowTitle("智慧卡產製系統 - 請選擇讀卡機");
        resize(550, 480);
        m_stack->removeWidget(opPage);
        opPage->deleteLater();
        });

    m_stack->addWidget(opPage);
    m_stack->setCurrentWidget(opPage);

    setWindowTitle("智慧卡產製系統 - 操作工作區");
    resize(1050, 600);
}
