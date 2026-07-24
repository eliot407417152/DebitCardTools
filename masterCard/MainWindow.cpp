#include "MainWindow.h"
#include "PageSelectReader.h"
#include "PageCardOperation.h"
#include <QTimer>
#include <QIcon>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QSettings>
#include <QCoreApplication>
#include <QDialog>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QGuiApplication>
#include <QScreen>
#include <QMessageBox>
#include <algorithm>

namespace {
    // 尚未有使用者自訂設定、也還沒套用螢幕限制前的預設理想大小
    constexpr int kSelectPageDefaultW = 550;
    constexpr int kSelectPageDefaultH = 480;
    constexpr int kOperationPageDefaultW = 1050;
    constexpr int kOperationPageDefaultH = 600;

    // 日誌顯示設定的預設值與可調整範圍
    constexpr int kLogFontSizeDefault = 11;   // px
    constexpr int kLogFontSizeMin = 8;
    constexpr int kLogFontSizeMax = 40;
    constexpr int kLogLineSpacingDefault = 100;  // % (100 = 預設單行行高)
    constexpr int kLogLineSpacingMin = 50;
    constexpr int kLogLineSpacingMax = 300;
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowIcon(QIcon(":/masterCard/atm-card.ico"));

    buildMenuBar();

    m_stack = new QStackedWidget(this);
    setCentralWidget(m_stack);

    m_selectReaderPage = new PageSelectReader(this);
    m_stack->addWidget(m_selectReaderPage);

    connect(m_selectReaderPage, &PageSelectReader::readerSelected,
        this, &MainWindow::onReaderSelected);

    setWindowTitle("智慧卡產製系統 - 請選擇讀卡機");
    applyWindowSizeForCurrentPage();
}

void MainWindow::buildMenuBar()
{
    QMenu* settingMenu = menuBar()->addMenu(QStringLiteral("設定(&S)"));
    QAction* windowSizeAct = settingMenu->addAction(QStringLiteral("視窗大小設定(&W)..."));
    connect(windowSizeAct, &QAction::triggered, this, &MainWindow::onEditWindowSize);

    QAction* logAppearanceAct = settingMenu->addAction(QStringLiteral("日誌顯示設定(&L)..."));
    connect(logAppearanceAct, &QAction::triggered, this, &MainWindow::onEditLogAppearance);

    QMenu* programMenu = menuBar()->addMenu(QStringLiteral("程式(&P)"));
    QAction* backAct = programMenu->addAction(QStringLiteral("返回偵測頁面(&B)"));
    connect(backAct, &QAction::triggered, this, &MainWindow::onBackToSelectReader);

    programMenu->addSeparator();
    QAction* quitAct = programMenu->addAction(QStringLiteral("結束程式(&X)"));
    connect(quitAct, &QAction::triggered, this, &QWidget::close);
}

void MainWindow::onReaderSelected()
{
    m_opPage = new PageCardOperation(
        m_selectReaderPage->context(),
        m_selectReaderPage->readers(),
        this);

    m_stack->addWidget(m_opPage);
    m_stack->setCurrentWidget(m_opPage);

    setWindowTitle("智慧卡產製系統 - 操作工作區");
    applyWindowSizeForCurrentPage();
    applyLogSettingsToOperationPage();
}

// 返回「選擇讀卡機」頁面：現在只透過選單「程式→返回偵測頁面」觸發
// (頁面內原本的「⬅️ 返回」按鈕已移除)
void MainWindow::onBackToSelectReader()
{
    if (!m_opPage)
        return; // 已經在偵測頁面，不用做任何事

    m_stack->setCurrentIndex(0);
    m_stack->removeWidget(m_opPage);
    m_opPage->deleteLater();
    m_opPage = nullptr;

    setWindowTitle("智慧卡產製系統 - 請選擇讀卡機");

    QTimer::singleShot(0, this, [this]() {
        applyWindowSizeForCurrentPage();
        });
}

// 依目前顯示的頁面，套用「使用者先前存過的大小」或「預設大小」，並夾在目前螢幕可用範圍內置中顯示，
// 避免不同電腦解析度不同時視窗一開就超出畫面、還要手動拉動調整
void MainWindow::applyWindowSizeForCurrentPage()
{
    const bool onOperationPage = (m_opPage != nullptr);
    const QString key = onOperationPage ? QStringLiteral("Window/OperationPage")
        : QStringLiteral("Window/SelectPage");

    QSettings settings(QCoreApplication::applicationDirPath() + "/config.ini", QSettings::IniFormat);
    int w = settings.value(key + "Width", 0).toInt();
    int h = settings.value(key + "Height", 0).toInt();

    if (w <= 0 || h <= 0) {
        w = onOperationPage ? kOperationPageDefaultW : kSelectPageDefaultW;
        h = onOperationPage ? kOperationPageDefaultH : kSelectPageDefaultH;
    }

    applyClampedResize(w, h);
}

void MainWindow::applyClampedResize(int w, int h)
{
    const QRect avail = QGuiApplication::primaryScreen()
        ? QGuiApplication::primaryScreen()->availableGeometry()
        : QRect(0, 0, 1280, 800);

    // 不論是先前存的還是預設值，都夾在目前螢幕可用範圍內，避免視窗開啟時超出畫面
    w = std::min(w, avail.width());
    h = std::min(h, avail.height());

    resize(w, h);
    move(avail.center() - QPoint(w / 2, h / 2));
}

// 視窗大小設定：套用到「目前顯示的頁面」，設定完成後存入 config.ini，
// 下次切到同一種頁面(選擇讀卡機 / 操作工作區)會直接套用，不用每次換電腦都手動拉動調整
void MainWindow::onEditWindowSize()
{
    const bool onOperationPage = (m_opPage != nullptr);

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("視窗大小設定"));

    auto* hintLbl = new QLabel(onOperationPage
        ? QStringLiteral("目前套用對象：操作工作區頁面")
        : QStringLiteral("目前套用對象：選擇讀卡機頁面"), &dlg);
    hintLbl->setStyleSheet(QStringLiteral("color:#666;"));

    auto* widthSpin = new QSpinBox(&dlg);
    widthSpin->setRange(500, 6000);
    widthSpin->setSuffix(QStringLiteral(" px"));
    widthSpin->setValue(width());

    auto* heightSpin = new QSpinBox(&dlg);
    heightSpin->setRange(400, 4000);
    heightSpin->setSuffix(QStringLiteral(" px"));
    heightSpin->setValue(height());

    auto* formLayout = new QFormLayout();
    formLayout->addRow(QStringLiteral("視窗寬度："), widthSpin);
    formLayout->addRow(QStringLiteral("視窗高度："), heightSpin);

    auto* fitScreenBtn = new QPushButton(QStringLiteral("套用目前螢幕建議大小"), &dlg);
    connect(fitScreenBtn, &QPushButton::clicked, &dlg, [widthSpin, heightSpin]() {
        const QRect avail = QGuiApplication::primaryScreen()
            ? QGuiApplication::primaryScreen()->availableGeometry()
            : QRect(0, 0, 1280, 800);
        widthSpin->setValue(std::max(500, static_cast<int>(avail.width() * 0.85)));
        heightSpin->setValue(std::max(400, static_cast<int>(avail.height() * 0.85)));
        });

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(buttonBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    auto* mainLayout = new QVBoxLayout(&dlg);
    mainLayout->addWidget(hintLbl);
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(fitScreenBtn);
    mainLayout->addWidget(buttonBox);

    if (dlg.exec() != QDialog::Accepted)
        return;

    const int w = widthSpin->value();
    const int h = heightSpin->value();

    applyClampedResize(w, h);

    const QString key = onOperationPage ? QStringLiteral("Window/OperationPage")
        : QStringLiteral("Window/SelectPage");
    QSettings settings(QCoreApplication::applicationDirPath() + "/config.ini", QSettings::IniFormat);
    settings.setValue(key + "Width", w);
    settings.setValue(key + "Height", h);

    QMessageBox::information(this, QStringLiteral("完成"),
        QStringLiteral("視窗大小已儲存：%1 x %2\n下次切到%3會直接套用這個大小。")
        .arg(w).arg(h).arg(onOperationPage ? QStringLiteral("操作工作區頁面") : QStringLiteral("選擇讀卡機頁面")));
}

// 日誌顯示設定：調整「通訊日誌」區塊的字體大小與行距，設定完成後存入 config.ini，
// 下次開啟操作工作區頁面會直接套用；若目前已在操作工作區頁面，則立即套用讓使用者馬上看到效果
void MainWindow::onEditLogAppearance()
{
    QSettings settings(QCoreApplication::applicationDirPath() + "/config.ini", QSettings::IniFormat);
    const int curFontSize = settings.value(QStringLiteral("Log/FontSize"), kLogFontSizeDefault).toInt();
    const int curLineSpacing = settings.value(QStringLiteral("Log/LineSpacingPercent"), kLogLineSpacingDefault).toInt();

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("日誌顯示設定"));

    auto* hintLbl = new QLabel(QStringLiteral("套用對象：操作工作區頁面右側的「通訊日誌」"), &dlg);
    hintLbl->setStyleSheet(QStringLiteral("color:#666;"));

    auto* fontSizeSpin = new QSpinBox(&dlg);
    fontSizeSpin->setRange(kLogFontSizeMin, kLogFontSizeMax);
    fontSizeSpin->setSuffix(QStringLiteral(" px"));
    fontSizeSpin->setValue(curFontSize);

    auto* lineSpacingSpin = new QSpinBox(&dlg);
    lineSpacingSpin->setRange(kLogLineSpacingMin, kLogLineSpacingMax);
    lineSpacingSpin->setSingleStep(10);
    lineSpacingSpin->setSuffix(QStringLiteral(" %"));
    lineSpacingSpin->setValue(curLineSpacing);

    auto* formLayout = new QFormLayout();
    formLayout->addRow(QStringLiteral("日誌字體大小："), fontSizeSpin);
    formLayout->addRow(QStringLiteral("每行間距："), lineSpacingSpin);

    auto* resetBtn = new QPushButton(QStringLiteral("還原預設值"), &dlg);
    connect(resetBtn, &QPushButton::clicked, &dlg, [fontSizeSpin, lineSpacingSpin]() {
        fontSizeSpin->setValue(kLogFontSizeDefault);
        lineSpacingSpin->setValue(kLogLineSpacingDefault);
        });

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(buttonBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    auto* mainLayout = new QVBoxLayout(&dlg);
    mainLayout->addWidget(hintLbl);
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(resetBtn);
    mainLayout->addWidget(buttonBox);

    if (dlg.exec() != QDialog::Accepted)
        return;

    const int fontSize = fontSizeSpin->value();
    const int lineSpacing = lineSpacingSpin->value();

    settings.setValue(QStringLiteral("Log/FontSize"), fontSize);
    settings.setValue(QStringLiteral("Log/LineSpacingPercent"), lineSpacing);

    if (m_opPage)
        m_opPage->applyLogAppearance(fontSize, lineSpacing);

    QMessageBox::information(this, QStringLiteral("完成"),
        QStringLiteral("日誌顯示設定已儲存：字體 %1px、行距 %2%\n下次進入操作工作區頁面會直接套用。")
        .arg(fontSize).arg(lineSpacing));
}

void MainWindow::applyLogSettingsToOperationPage()
{
    if (!m_opPage)
        return;

    QSettings settings(QCoreApplication::applicationDirPath() + "/config.ini", QSettings::IniFormat);
    const int fontSize = settings.value(QStringLiteral("Log/FontSize"), kLogFontSizeDefault).toInt();
    const int lineSpacing = settings.value(QStringLiteral("Log/LineSpacingPercent"), kLogLineSpacingDefault).toInt();

    m_opPage->applyLogAppearance(fontSize, lineSpacing);
}
