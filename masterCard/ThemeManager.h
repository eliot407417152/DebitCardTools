#pragma once

#include <QObject>
#include <QString>
#include <QPointer>
#include <functional>

class QWidget;
class QLabel;

// ─────────────────────────────────────────────────────────────────────────────
// 外觀模式：亮色 / 深色 / 跟隨系統
// ─────────────────────────────────────────────────────────────────────────────
enum class ThemeMode { Light, Dark, System };

// ─────────────────────────────────────────────────────────────────────────────
// ThemeManager — 全程式統一的亮 / 深色切換
//
//   為什麼原本切換會「卡住、不完全」：
//     1. 程式裡有一百多處 setStyleSheet() 把 #F9F9F9 / #EEE / #555 / #CCC 這類顏色寫死，
//        這些寫死的顏色不會跟著調色盤（QPalette）改變，所以切到深色後欄位、提示文字仍是亮色版。
//     2. 切回「系統預設」時又換回原生樣式（windowsvista 等），原生樣式本來就不支援深色調色盤，
//        而且在已有大量 stylesheet 的元件上重新換樣式，容易殘留舊樣式。
//
//   做法：
//     • 永遠使用 Fusion 樣式（只設定一次），亮 / 深色各一組完整調色盤。
//     • 對每個元件的 stylesheet 做「顏色對應」：原始（亮色）stylesheet 另外存在元件屬性裡，
//       切到深色就把中性色（底色、文字、邊框）換成深色版，切回亮色就用原始的還原；
//       之後程式任何地方再呼叫 setStyleSheet()，也會自動被攔截並套上目前主題，不用逐一修改。
//       強調色按鈕（藍 / 綠 / 紅…，白字）維持原樣。
//     • 切換時先把畫面截圖蓋在最上層，換完主題再淡出，達到像網頁一樣的漸變效果。
//     • Windows 標題列也一併切換深 / 淺色。
//
//   不想被轉換的元件（例如使用者自訂顏色的日誌、色塊預覽按鈕）：
//     ThemeManager::excludeFromTheme(widget);
// ─────────────────────────────────────────────────────────────────────────────
class ThemeManager : public QObject
{
public:
    static ThemeManager& instance();

    // 安裝全域事件過濾器（只需呼叫一次，須在建立任何視窗元件之前）
    void install();

    ThemeMode mode() const { return m_mode; }
    bool isDark() const { return m_dark; }

    // 切換外觀。animateOver 不為空且已顯示時，會以淡入淡出動畫過渡。
    void setMode(ThemeMode mode, QWidget* animateOver = nullptr);

    // 主題實際切換完成後的回呼（參數：目前是否為深色）
    void setOnChanged(std::function<void(bool)> cb) { m_onChanged = std::move(cb); }

    // 讀 / 寫 config.ini 的 UI/Theme（相容舊版 UI/ForceLight）
    static ThemeMode loadMode();
    static void saveMode(ThemeMode mode);

    // 讓某個元件的 stylesheet 不被主題轉換（顏色由程式自行決定）
    static void excludeFromTheme(QWidget* w);

    // 將「亮色版」stylesheet 轉成深色版（亮色時原樣回傳）
    static QString mapQss(const QString& qss, bool dark);

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    ThemeManager() = default;

    bool systemIsDark() const;
    void applyNow(bool dark);
    void restyleAllWidgets();
    void adoptWidget(QWidget* w);
    void fadeTransition(QWidget* over, const std::function<void()>& change);

    ThemeMode m_mode = ThemeMode::Light;
    bool m_dark = false;
    bool m_installed = false;
    bool m_changing = false;
    std::function<void(bool)> m_onChanged;
    QPointer<QLabel> m_overlay;
};