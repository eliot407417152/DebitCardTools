#include "FlowPageBase.h"
#include "ReaderManager.h"
#include "ReaderBar.h"
#include <QVBoxLayout>
#include <QScrollArea>
#include <QMessageBox>

FlowPageBase::FlowPageBase(ReaderManager* manager, QWidget* parent)
    : QWidget(parent), m_manager(manager)
{
}

QVBoxLayout* FlowPageBase::createContentLayout()
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    outer->addWidget(scroll);

    auto* content = new QWidget();
    scroll->setWidget(content);
    return new QVBoxLayout(content);
}

bool FlowPageBase::acquireHandles(ReaderBar* isamBar, ReaderBar* otherBar,
    const QString& otherRole, SCARDHANDLE& hIsam, SCARDHANDLE& hOther)
{
    const QString isamName = isamBar ? isamBar->readerName() : QString();
    const QString otherName = otherBar ? otherBar->readerName() : QString();

    if (isamName.isEmpty() || otherName.isEmpty() || isamName == otherName) {
        QMessageBox::warning(this, "設定錯誤",
            QString("請選擇兩台不同的讀卡機（ISAM／%1）。\n"
                "若剛插上讀卡機，請先按「重新整理」。").arg(otherRole));
        return false;
    }

    hIsam = isamBar->handle();
    hOther = otherBar->handle();
    if (hIsam == 0 || hOther == 0) {
        QMessageBox::critical(this, "連線失敗",
            "無法連線所選讀卡機，請確認卡片已插好。\n"
            "（剛重插卡片的話，可按「重新整理」後再試一次）");
        return false;
    }
    return true;
}
