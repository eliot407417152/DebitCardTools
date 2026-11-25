#include "MainWindow.h"
#include "PageCardOperation.h"
#include "ThemeManager.h"
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
#include <QColorDialog>
#include <QApplication>
#include <QStyle>
#include <QStyleFactory>
#include <QPalette>
#include <QHBoxLayout>
#include <QActionGroup>
#include <algorithm>

namespace {
    // 尚未有使用者自訂設定、也還沒套用螢幕限制前的預設理想大小（頁籤變多，預設寬度略為加大）
    constexpr int kOperationPageDefaultW = 1280;
    constexpr int kOperationPageDefaultH = 720;

    // 日誌顯示設定的預設值與可調整範圍
    constexpr int kLogFontSizeDefault = 11;   // px
    constexpr int kLogFontSizeMin = 8;
    constexpr int kLogFontSizeMax = 40;
    constexpr int kLogLineSpacingDefault = 100;  // % (100 = 預設單行行高)
    constexpr int kLogLineSpacingMin = 50;
    constexpr int kLogLineSpacingMax = 300;

    bool sameColors(const LogColors& a, const LogColors& b)
    {
        return a.background.compare(b.background, Qt::CaseInsensitive) == 0
            && a.text.compare(b.text, Qt::CaseInsensitive) == 0
            && a.error.compare(b.error, Qt::CaseInsensitive) == 0;
    }

    // 讀 config.ini 裡的日誌顏色。
    //   沒存過，或存的就是「亮色 / 深色預設」→ 跟著目前的外觀模式（亮色介面用亮色日誌、深色介面用深色日誌）；
    //   使用者自訂過的顏色則維持不變。
    LogColors loadLogColors(QSettings& st)
    {
        const LogColors themeDef = ThemeManager::instance().isDark()
            ? LogColors::darkPreset() : LogColors::lightPreset();
        LogColors c;
        c.background = st.value(QStringLiteral("Log/BgColor"), themeDef.background).toString();
        c.text = st.value(QStringLiteral("Log/TextColor"), themeDef.text).toString();
        c.error = st.value(QStringLiteral("Log/ErrorColor"), themeDef.error).toString();
        if (sameColors(c, LogColors::lightPreset()) || sameColors(c, LogColors::darkPreset()))
            return themeDef;
        return c;
    }
    void saveLogColors(QSettings& st, const LogColors& c)
    {
        st.setValue(QStringLiteral("Log/BgColor"), c.background);
        st.setValue(QStringLiteral("Log/TextColor"), c.text);
        st.setValue(QStringLiteral("Log/ErrorColor"), c.error);
    }
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowIcon(QIcon(":/masterCard/atm-card.ico"));

    // 主題要在建立任何元件之前先裝好（調色盤 + stylesheet 攔截），之後建立的元件才會直接是正確的顏色
    ThemeManager::instance().install();
    ThemeManager::instance().setMode(ThemeManager::loadMode());

    buildMenuBar();

    // 啟動後直接進入操作工作區（不再先停在「顯示可用讀卡機」的頁面）
    m_opPage = new PageCardOperation(this);
    setCentralWidget(m_opPage);

    setWindowTitle("智慧卡產製系統");
    applyWindowSize();
    applyLogSettingsToOperationPage();
}

void MainWindow::buildMenuBar()
{
    QMenu* settingMenu = menuBar()->addMenu(QStringLiteral("設定(&S)"));
    QAction* windowSizeAct = settingMenu->addAction(QStringLiteral("視窗大小設定(&W)..."));
    connect(windowSizeAct, &QAction::triggered, this, &MainWindow::onEditWindowSize);

    QAction* logAppearanceAct = settingMenu->addAction(QStringLiteral("日誌顯示設定(&L)..."));
    connect(logAppearanceAct, &QAction::triggered, this, &MainWindow::onEditLogAppearance);

    // 外觀：亮色 / 深色 / 跟隨系統（切換時有淡入淡出動畫）
    QMenu* themeMenu = settingMenu->addMenu(QStringLiteral("外觀(&T)"));
    auto* themeGroup = new QActionGroup(this);
    themeGroup->setExclusive(true);
    struct ThemeItem { QString text; ThemeMode mode; };
    const ThemeItem items[] = {
        { QStringLiteral("☀ 亮色"), ThemeMode::Light },
        { QStringLiteral("🌙 深色"), ThemeMode::Dark },
        { QStringLiteral("🖥 跟隨系統"), ThemeMode::System },
    };
    for (const ThemeItem& it : items) {
        QAction* act = themeMenu->addAction(it.text);
        act->setCheckable(true);
        act->setChecked(ThemeManager::instance().mode() == it.mode);
        themeGroup->addAction(act);
        const ThemeMode mode = it.mode;
        connect(act, &QAction::triggered, this, [this, mode]() {
            ThemeManager::saveMode(mode);
            ThemeManager::instance().setMode(mode, this);
            });
    }
    // 外觀實際切換完成後（包含「跟隨系統」時作業系統自己切換），日誌的預設顏色也跟著換
    ThemeManager::instance().setOnChanged([this](bool) { applyLogSettingsToOperationPage(); });

    settingMenu->addSeparator();
    QAction* keyDataSetAct = settingMenu->addAction(QStringLiteral("Key Set / Data Set 管理(&K)..."));
    connect(keyDataSetAct, &QAction::triggered, this, &MainWindow::onOpenKeySetDataSetManager);

    QMenu* programMenu = menuBar()->addMenu(QStringLiteral("程式(&P)"));
    QAction* quitAct = programMenu->addAction(QStringLiteral("結束程式(&X)"));
    connect(quitAct, &QAction::triggered, this, &QWidget::close);
}

// 套用「使用者先前存過的大小」或「預設大小」，並夾在目前螢幕可用範圍內置中顯示，
// 避免不同電腦解析度不同時視窗一開就超出畫面、還要手動拉動調整
void MainWindow::applyWindowSize()
{
    QSettings settings(QCoreApplication::applicationDirPath() + "/config.ini", QSettings::IniFormat);
    int w = settings.value("Window/OperationPageWidth", 0).toInt();
    int h = settings.value("Window/OperationPageHeight", 0).toInt();

    if (w <= 0 || h <= 0) {
        w = kOperationPageDefaultW;
        h = kOperationPageDefaultH;
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
// 下次開啟程式會直接套用，不用每次換電腦都手動拉動調整
void MainWindow::onEditWindowSize()
{
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("視窗大小設定"));

    auto* hintLbl = new QLabel(QStringLiteral("套用對象：主視窗"), &dlg);
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

    QSettings settings(QCoreApplication::applicationDirPath() + "/config.ini", QSettings::IniFormat);
    settings.setValue("Window/OperationPageWidth", w);
    settings.setValue("Window/OperationPageHeight", h);

    QMessageBox::information(this, QStringLiteral("完成"),
        QStringLiteral("視窗大小已儲存：%1 x %2\n下次開啟程式會直接套用這個大小。")
        .arg(w).arg(h));
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

    auto* hintLbl = new QLabel(QStringLiteral("套用對象：右側的「通訊日誌」"), &dlg);
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

    // 顏色：點色塊開啟調色盤；也可直接套用「亮色 / 深色」預設
    LogColors colors = loadLogColors(settings);
    auto* bgBtn = new QPushButton(&dlg);
    auto* textBtn = new QPushButton(&dlg);
    auto* errBtn = new QPushButton(&dlg);
    auto refreshSwatches = [&colors, bgBtn, textBtn, errBtn]() {
        auto paint = [](QPushButton* b, const QString& hex) {
            b->setText(hex.toUpper());
            ThemeManager::excludeFromTheme(b);
            // 文字用對比色，避免跟色塊同色看不到
            const QColor c(hex);
            const QString fg = c.lightness() > 128 ? "#000000" : "#FFFFFF";
            b->setStyleSheet(QString("QPushButton { background:%1; color:%2; padding:6px 14px; "
                "border:1px solid #888; border-radius:3px; font-family:'Courier New'; }").arg(hex, fg));
            };
        paint(bgBtn, colors.background);
        paint(textBtn, colors.text);
        paint(errBtn, colors.error);
        };
    refreshSwatches();

    auto pick = [&dlg, &colors, refreshSwatches](QString& target, const QString& title) {
        const QColor c = QColorDialog::getColor(QColor(target), &dlg, title);
        if (c.isValid()) {
            target = c.name(QColor::HexRgb);
            refreshSwatches();
        }
        };
    connect(bgBtn, &QPushButton::clicked, &dlg, [&, pick]() { pick(colors.background, QStringLiteral("選擇日誌背景色")); });
    connect(textBtn, &QPushButton::clicked, &dlg, [&, pick]() { pick(colors.text, QStringLiteral("選擇日誌文字顏色")); });
    connect(errBtn, &QPushButton::clicked, &dlg, [&, pick]() { pick(colors.error, QStringLiteral("選擇日誌錯誤文字顏色")); });

    auto* lightPresetBtn = new QPushButton(QStringLiteral("☀ 亮色（預設）"), &dlg);
    auto* darkPresetBtn = new QPushButton(QStringLiteral("🌙 深色（黑底綠字）"), &dlg);
    connect(lightPresetBtn, &QPushButton::clicked, &dlg, [&colors, refreshSwatches]() {
        colors = LogColors::lightPreset(); refreshSwatches(); });
    connect(darkPresetBtn, &QPushButton::clicked, &dlg, [&colors, refreshSwatches]() {
        colors = LogColors::darkPreset(); refreshSwatches(); });
    auto* presetRow = new QHBoxLayout();
    presetRow->addWidget(lightPresetBtn);
    presetRow->addWidget(darkPresetBtn);

    auto* formLayout = new QFormLayout();
    formLayout->addRow(QStringLiteral("日誌字體大小："), fontSizeSpin);
    formLayout->addRow(QStringLiteral("每行間距："), lineSpacingSpin);
    formLayout->addRow(QStringLiteral("背景顏色："), bgBtn);
    formLayout->addRow(QStringLiteral("一般文字顏色："), textBtn);
    formLayout->addRow(QStringLiteral("錯誤文字顏色："), errBtn);
    formLayout->addRow(QStringLiteral("快速套用："), presetRow);

    auto* resetBtn = new QPushButton(QStringLiteral("還原預設值"), &dlg);
    connect(resetBtn, &QPushButton::clicked, &dlg, [=, &colors]() {
        fontSizeSpin->setValue(kLogFontSizeDefault);
        lineSpacingSpin->setValue(kLogLineSpacingDefault);
        colors = LogColors::lightPreset();
        refreshSwatches();
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
    saveLogColors(settings, colors);

    if (m_opPage)
        m_opPage->applyLogAppearance(fontSize, lineSpacing, colors);

    QMessageBox::information(this, QStringLiteral("完成"),
        QStringLiteral("日誌顯示設定已儲存：字體 %1px、行距 %2%、顏色已更新\n下次開啟程式會直接套用。")
        .arg(fontSize).arg(lineSpacing));
}

// Key Set / Data Set 管理視窗實際內容（載入按鈕、下拉選單…）屬於操作工作區 (PageCardOperation) 的資料
void MainWindow::onOpenKeySetDataSetManager()
{
    if (m_opPage)
        m_opPage->openKeySetDataSetManager();
}

void MainWindow::applyLogSettingsToOperationPage()
{
    if (!m_opPage)
        return;

    QSettings settings(QCoreApplication::applicationDirPath() + "/config.ini", QSettings::IniFormat);
    const int fontSize = settings.value(QStringLiteral("Log/FontSize"), kLogFontSizeDefault).toInt();
    const int lineSpacing = settings.value(QStringLiteral("Log/LineSpacingPercent"), kLogLineSpacingDefault).toInt();

    m_opPage->applyLogAppearance(fontSize, lineSpacing, loadLogColors(settings));
}