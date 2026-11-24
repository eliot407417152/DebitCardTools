#pragma once
#include <QApplication>
#include <QFont>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QRect>
#include <QScreen>
#include <QSize>
#include <algorithm>
#include <cmath>

// 依螢幕大小調整介面比例。
// Qt 已經處理系統的 DPI 縮放(座標是「邏輯像素」),這裡處理的是:
// 螢幕可用空間大小不同時,字型和所有以字型為單位的間距、按鈕寬度一起放大或縮小。
namespace uiscale {

    inline QRect avail() { return QGuiApplication::primaryScreen()->availableGeometry(); }

    // 程式啟動時呼叫一次。以可用高度 1080 為基準:1080 → 1.0,1440 → 約 1.33,筆電 768 → 0.9
    inline void applyToApp() {
        static bool done = false;
        if (done) return;
        done = true;

        const double scale = std::clamp(avail().height() / 1080.0, 0.9, 1.5);
        QFont f = QApplication::font();
        if (f.pointSizeF() > 0) {
            f.setPointSizeF(f.pointSizeF() * scale);
            QApplication::setFont(f);
        }
    }

    // 以「一行文字的高度」為單位換算成像素,字型變大時,間距與按鈕寬度也跟著變大
    inline int u(double units) {
        return static_cast<int>(std::lround(units * QFontMetrics(QApplication::font()).height()));
    }

    // 限制大小不超過螢幕可用範圍的一定比例
    inline QSize fit(const QSize& s, double frac = 0.95) {
        const QRect a = avail();
        return s.boundedTo(QSize(static_cast<int>(a.width() * frac), static_cast<int>(a.height() * frac)));
    }

} // namespace uiscale