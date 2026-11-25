#include "ThemeManager.h"

#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QEventLoop>
#include <QGraphicsOpacityEffect>
#include <QGuiApplication>
#include <QHash>
#include <QLabel>
#include <QPalette>
#include <QPropertyAnimation>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSettings>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>
#include <QVersionNumber>
#include <QWidget>

#if defined(Q_OS_WIN) && QT_VERSION < QT_VERSION_CHECK(6, 8, 0)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#define THEME_USE_DWM 1
#endif

namespace {

    const char* kPropOrig = "_themeQssOrig";       // 原始（亮色版）stylesheet
    const char* kPropApplied = "_themeQssApplied";    // 我們實際套用上去的 stylesheet
    const char* kPropExclude = "noTheme";             // 不參與主題轉換

    // ── 調色盤 ──────────────────────────────────────────────────────────────
    QPalette makeLightPalette()
    {
        QPalette p(QColor("#F0F0F0"), QColor("#FFFFFF"));
        p.setColor(QPalette::Window, QColor("#FFFFFF"));
        p.setColor(QPalette::WindowText, QColor("#000000"));
        p.setColor(QPalette::Base, QColor("#FFFFFF"));
        p.setColor(QPalette::AlternateBase, QColor("#F5F5F5"));
        p.setColor(QPalette::Text, QColor("#000000"));
        p.setColor(QPalette::Button, QColor("#F0F0F0"));
        p.setColor(QPalette::ButtonText, QColor("#000000"));
        p.setColor(QPalette::BrightText, QColor("#FFFFFF"));
        p.setColor(QPalette::ToolTipBase, QColor("#FFFFDC"));
        p.setColor(QPalette::ToolTipText, QColor("#202020"));
        p.setColor(QPalette::PlaceholderText, QColor("#8A8A8A"));
        p.setColor(QPalette::Highlight, QColor("#0078D4"));
        p.setColor(QPalette::HighlightedText, QColor("#FFFFFF"));
        p.setColor(QPalette::Link, QColor("#0066CC"));
        p.setColor(QPalette::Disabled, QPalette::WindowText, QColor("#9A9A9A"));
        p.setColor(QPalette::Disabled, QPalette::Text, QColor("#9A9A9A"));
        p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#9A9A9A"));
        return p;
    }

    QPalette makeDarkPalette()
    {
        QPalette p(QColor("#35373C"), QColor("#2B2D31"));   // Button / Window，其餘立體陰影色自動推算
        p.setColor(QPalette::Window, QColor("#2B2D31"));
        p.setColor(QPalette::WindowText, QColor("#E6E6E6"));
        p.setColor(QPalette::Base, QColor("#1F2023"));
        p.setColor(QPalette::AlternateBase, QColor("#26282B"));
        p.setColor(QPalette::Text, QColor("#E6E6E6"));
        p.setColor(QPalette::Button, QColor("#35373C"));
        p.setColor(QPalette::ButtonText, QColor("#E6E6E6"));
        p.setColor(QPalette::BrightText, QColor("#FF6B6B"));
        p.setColor(QPalette::ToolTipBase, QColor("#3A3C41"));
        p.setColor(QPalette::ToolTipText, QColor("#EEEEEE"));
        p.setColor(QPalette::PlaceholderText, QColor("#8A8D93"));
        p.setColor(QPalette::Highlight, QColor("#0078D4"));
        p.setColor(QPalette::HighlightedText, QColor("#FFFFFF"));
        p.setColor(QPalette::Link, QColor("#4DA3FF"));
        p.setColor(QPalette::Light, QColor("#4A4D52"));
        p.setColor(QPalette::Midlight, QColor("#3F4247"));
        p.setColor(QPalette::Mid, QColor("#2E3034"));
        p.setColor(QPalette::Dark, QColor("#18191B"));
        p.setColor(QPalette::Shadow, QColor("#000000"));
        p.setColor(QPalette::Disabled, QPalette::WindowText, QColor("#7C7F85"));
        p.setColor(QPalette::Disabled, QPalette::Text, QColor("#7C7F85"));
        p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#7C7F85"));
        p.setColor(QPalette::Disabled, QPalette::Highlight, QColor("#4A4D52"));
        return p;
    }

    // ── stylesheet 顏色對應（亮 → 深）──────────────────────────────────────
    enum class Kind { Background, Text, Border };

    const QHash<QString, QString>& table(Kind k)
    {
        // 底色：中性灰 / 白 → 深灰。強調色（按鈕的藍綠紅…）不在表內，維持原色。
        static const QHash<QString, QString> bg = {
            {"#FFF", "#1F2023"}, {"#FFFFFF", "#1F2023"},
            {"#F9F9F9", "#2B2D31"}, {"#F5F5F5", "#2B2D31"},
            {"#F0F0F0", "#35373C"}, {"#EEE", "#35373C"}, {"#EEEEEE", "#35373C"},
            {"#ECEFF1", "#2F3338"}, {"#CFD8DC", "#454B52"}, {"#B0BEC5", "#555C64"},
            {"#CCC", "#4A4D52"},   // 例如停用按鈕的底色
        };
        // 文字：深灰 → 淺灰；暗淡的強調色（深綠 / 深紅…）→ 較亮的同色系，深色背景上才看得清楚
        static const QHash<QString, QString> text = {
            {"#000", "#E6E6E6"}, {"#000000", "#E6E6E6"}, {"#202020", "#EEEEEE"},
            {"#222", "#EDEDED"}, {"#333", "#DDDDDD"}, {"#555", "#B8B8B8"},
            {"#666", "#AAAAAA"}, {"#888", "#9A9A9A"}, {"#999", "#9A9A9A"},
            {"#9A9A9A", "#8A8A8A"}, {"#9E9E9E", "#8E8E8E"},
            {"#2E7D32", "#81C784"}, {"#388E3C", "#81C784"}, {"#00695C", "#4DB6AC"},
            {"#C62828", "#EF5350"}, {"#D32F2F", "#EF5350"}, {"#E65100", "#FFB74D"},
            {"#EF6C00", "#FFB74D"}, {"#AD1457", "#F48FB1"}, {"#6A1B9A", "#CE93D8"},
            {"#1565C0", "#64B5F6"}, {"#0288D1", "#4FC3F7"}, {"#0078D4", "#4DA3FF"},
            {"#00838F", "#4DD0E1"}, {"#37474F", "#CFD8DC"}, {"#455A64", "#B0BEC5"},
        };
        static const QHash<QString, QString> border = {
            {"#CCC", "#55585E"}, {"#AAA", "#6A6D73"}, {"#9E9E9E", "#6A6D73"},
            {"#888", "#6A6D73"}, {"#EEE", "#3A3C41"}, {"#EEEEEE", "#3A3C41"},
        };
        switch (k) {
        case Kind::Background: return bg;
        case Kind::Text:       return text;
        default:               return border;
        }
    }

#ifdef THEME_USE_DWM
    // Qt 6.8 以前，Windows 標題列要自己切換深 / 淺色
    void setTitleBarDark(QWidget* w, bool dark)
    {
        if (!w || !w->isWindow() || !w->isVisible())
            return;
        HWND hwnd = reinterpret_cast<HWND>(w->winId());
        BOOL v = dark ? TRUE : FALSE;
        DwmSetWindowAttribute(hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &v, sizeof(v));
        DwmSetWindowAttribute(hwnd, 19 /*舊版 Windows 10 的編號*/, &v, sizeof(v));
        // 強制標題列重繪，否則要等到視窗失焦 / 取得焦點才會變色
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
#endif

    // QScrollArea 的內容區預設是「透明」或沿用建立當下的底色；外層有 stylesheet 時，
    // 切換主題後常常不會重新取用新的調色盤，結果就是中間一大塊仍是亮色。
    // 這裡明確指定：視窗與內容區都用調色盤的 Window 底色並自動填滿，之後主題怎麼換都會跟著走。
    void fixScrollArea(QScrollArea* sa)
    {
        if (QWidget* vp = sa->viewport()) {
            vp->setBackgroundRole(QPalette::Window);
            vp->setAutoFillBackground(true);
        }
        if (QWidget* content = sa->widget()) {
            content->setBackgroundRole(QPalette::Window);
            content->setAutoFillBackground(true);
        }
    }

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
ThemeManager& ThemeManager::instance()
{
    static ThemeManager inst;
    return inst;
}

void ThemeManager::install()
{
    if (m_installed)
        return;
    m_installed = true;
    qApp->installEventFilter(this);

    // 一律使用 Fusion（只設定一次）：原生樣式不支援完整的深色調色盤
    if (qApp->style()->objectName().compare(QLatin1String("fusion"), Qt::CaseInsensitive) != 0)
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    // 「跟隨系統」模式：Windows 設定裡切換深 / 淺色時，程式即時跟著變
    QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
        [this](Qt::ColorScheme) {
            if (m_mode == ThemeMode::System && !m_changing)
                setMode(ThemeMode::System, QApplication::activeWindow());
        });
#endif
}

// ── 設定檔 ────────────────────────────────────────────────────────────────
ThemeMode ThemeManager::loadMode()
{
    QSettings st(QCoreApplication::applicationDirPath() + "/config.ini", QSettings::IniFormat);
    const QString v = st.value(QStringLiteral("UI/Theme")).toString().toLower();
    if (v == "dark")   return ThemeMode::Dark;
    if (v == "system") return ThemeMode::System;
    if (v == "light")  return ThemeMode::Light;
    // 相容舊版「強制亮色介面」選項：勾選 → 亮色；取消勾選（原本是系統預設）→ 跟隨系統
    if (st.contains(QStringLiteral("UI/ForceLight")))
        return st.value(QStringLiteral("UI/ForceLight"), true).toBool() ? ThemeMode::Light : ThemeMode::System;
    return ThemeMode::Light;
}

void ThemeManager::saveMode(ThemeMode mode)
{
    QSettings st(QCoreApplication::applicationDirPath() + "/config.ini", QSettings::IniFormat);
    st.setValue(QStringLiteral("UI/Theme"),
        mode == ThemeMode::Dark ? "dark" : mode == ThemeMode::System ? "system" : "light");
}

void ThemeManager::excludeFromTheme(QWidget* w)
{
    if (w)
        w->setProperty(kPropExclude, true);
}

// ── 系統是否為深色 ─────────────────────────────────────────────────────────
bool ThemeManager::systemIsDark() const
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    // 先取消強制設定，才讀得到作業系統真正的設定
    QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Unknown);
#endif
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#elif defined(Q_OS_WIN)
    QSettings reg(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"),
        QSettings::NativeFormat);
    return reg.value(QStringLiteral("AppsUseLightTheme"), 1).toInt() == 0;
#else
    return QApplication::palette().color(QPalette::Window).lightness() < 128;
#endif
}

// ── stylesheet 顏色對應 ───────────────────────────────────────────────────
QString ThemeManager::mapQss(const QString& qss, bool dark)
{
    if (!dark || qss.isEmpty())
        return qss;

    // 只處理 background / color / border 這幾種屬性裡的色碼
    static const QRegularExpression declRe(
        QStringLiteral(R"((?<![-\w])(background-color|background|color|border-color|border(?:-top|-bottom|-left|-right)?)\s*:\s*([^;{}]*))"));
    static const QRegularExpression hexRe(QStringLiteral(R"(#[0-9A-Fa-f]{3,6}\b)"));

    QString out;
    out.reserve(qss.size());
    int last = 0;

    auto it = declRe.globalMatch(qss);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        const QString prop = m.captured(1);
        const Kind kind = prop.startsWith(QLatin1String("background")) ? Kind::Background
            : (prop == QLatin1String("color")) ? Kind::Text : Kind::Border;
        const QHash<QString, QString>& map = table(kind);

        const QString val = m.captured(2);
        QString newVal;
        int vlast = 0;
        auto hit = hexRe.globalMatch(val);
        while (hit.hasNext()) {
            const QRegularExpressionMatch hm = hit.next();
            newVal += val.mid(vlast, hm.capturedStart() - vlast);
            const QString rep = map.value(hm.captured(0).toUpper());
            newVal += rep.isEmpty() ? hm.captured(0) : rep;
            vlast = hm.capturedEnd();
        }
        newVal += val.mid(vlast);

        out += qss.mid(last, m.capturedStart(2) - last) + newVal;
        last = m.capturedStart(2) + m.capturedLength(2);
    }
    out += qss.mid(last);
    return out;
}

// ── 攔截所有元件的 setStyleSheet / 顯示事件 ───────────────────────────────
bool ThemeManager::eventFilter(QObject* obj, QEvent* ev)
{
    if (!obj->isWidgetType())
        return false;

    switch (ev->type()) {
    case QEvent::StyleChange:
        adoptWidget(static_cast<QWidget*>(obj));
        break;
    case QEvent::Show: {
        auto* w = static_cast<QWidget*>(obj);
        if (auto* sa = qobject_cast<QScrollArea*>(w))
            fixScrollArea(sa);
        adoptWidget(w);
#ifdef THEME_USE_DWM
        if (w->isWindow())
            setTitleBarDark(w, m_dark);
#endif
        break;
    }
    default:
        break;
    }
    return false;   // 只是旁觀，不吃掉事件
}

// 一個元件的 stylesheet 剛被程式設定（或剛顯示）：記下原始版本，並套上目前主題的版本
void ThemeManager::adoptWidget(QWidget* w)
{
    if (w->property(kPropExclude).toBool())
        return;

    const QString cur = w->styleSheet();
    if (cur.isEmpty()) {
        if (w->property(kPropOrig).isValid()) {
            w->setProperty(kPropOrig, QVariant());
            w->setProperty(kPropApplied, QVariant());
        }
        return;
    }
    if (cur == w->property(kPropApplied).toString())
        return;   // 這是我們自己套上去的版本

    const QString mapped = mapQss(cur, m_dark);
    w->setProperty(kPropOrig, cur);
    w->setProperty(kPropApplied, mapped);
    if (mapped != cur)
        w->setStyleSheet(mapped);   // 會再觸發一次 StyleChange，但因為等於 applied 會直接返回
}

// 主題切換時：把所有現有元件的 stylesheet 依新主題重新轉換
void ThemeManager::restyleAllWidgets()
{
    const QWidgetList all = QApplication::allWidgets();

    // 第一步：所有元件強制重新套用樣式。
    //   外層有 stylesheet 的元件，其底色 / 文字色是「建立或上次套樣式當下」從調色盤算好存起來的，
    //   只換 QApplication 的調色盤並不會更新它們，必須 unpolish → polish 才會重新取用新調色盤。
    for (QWidget* w : all) {
        if (auto* sa = qobject_cast<QScrollArea*>(w))
            fixScrollArea(sa);
        QStyle* st = w->style();
        st->unpolish(w);
        st->polish(w);
        w->update();
    }

    // 第二步：stylesheet 裡寫死的顏色依主題轉換
    for (QWidget* w : all) {
        if (w->property(kPropExclude).toBool())
            continue;

        QString orig;
        const QString cur = w->styleSheet();
        if (cur.isEmpty())
            continue;
        if (w->property(kPropApplied).isValid() && cur == w->property(kPropApplied).toString())
            orig = w->property(kPropOrig).toString();
        else
            orig = cur;   // 尚未被接管過（例如在 install() 之前就設定的）

        const QString mapped = mapQss(orig, m_dark);
        w->setProperty(kPropOrig, orig);
        w->setProperty(kPropApplied, mapped);
        if (mapped != cur)
            w->setStyleSheet(mapped);
    }
}

// ── 切換主題（沒有動畫的核心）──────────────────────────────────────────────
void ThemeManager::applyNow(bool dark)
{
    m_dark = dark;
    m_changing = true;

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    // 讓 Windows 標題列、原生對話框一起跟著變
    QGuiApplication::styleHints()->setColorScheme(
        dark ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
#endif
    QApplication::setPalette(dark ? makeDarkPalette() : makeLightPalette());
    restyleAllWidgets();

#ifdef THEME_USE_DWM
    for (QWidget* w : QApplication::topLevelWidgets())
        setTitleBarDark(w, dark);
#endif

    m_changing = false;
    if (m_onChanged)
        m_onChanged(dark);
}

// ── 淡入淡出：先截圖蓋住，換完主題再讓截圖慢慢透明 ───────────────────────
void ThemeManager::fadeTransition(QWidget* over, const std::function<void()>& change)
{
    if (m_overlay) {            // 上一次動畫還沒結束就又切換：直接移除舊的覆蓋層
        m_overlay->deleteLater();
        m_overlay = nullptr;
    }

    auto* overlay = new QLabel(over);
    overlay->setPixmap(over->grab());
    overlay->setScaledContents(true);
    overlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    overlay->setGeometry(over->rect());
    overlay->show();
    overlay->raise();
    m_overlay = overlay;

    over->setUpdatesEnabled(false);
    change();
    over->setUpdatesEnabled(true);
    over->repaint();
    overlay->raise();
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);   // 先讓新主題畫出來，動畫才不會被吃掉前幾格

    auto* effect = new QGraphicsOpacityEffect(overlay);
    effect->setOpacity(1.0);
    overlay->setGraphicsEffect(effect);

    auto* anim = new QPropertyAnimation(effect, "opacity", overlay);
    anim->setDuration(260);
    anim->setStartValue(1.0);
    anim->setEndValue(0.0);
    anim->setEasingCurve(QEasingCurve::InOutCubic);
    QObject::connect(anim, &QPropertyAnimation::finished, overlay, &QObject::deleteLater);
    anim->start();
}

void ThemeManager::setMode(ThemeMode mode, QWidget* animateOver)
{
    install();   // 保險：沒呼叫過 install() 也能運作

    m_mode = mode;
    const bool dark = (mode == ThemeMode::Dark)
        || (mode == ThemeMode::System && systemIsDark());

    const bool needChange = (dark != m_dark) || !qApp->property("_themeInitialized").toBool();
    qApp->setProperty("_themeInitialized", true);
    if (!needChange)
        return;

    if (animateOver && animateOver->isVisible() && !animateOver->size().isEmpty())
        fadeTransition(animateOver, [this, dark]() { applyNow(dark); });
    else
        applyNow(dark);
}