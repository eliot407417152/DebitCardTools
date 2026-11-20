#pragma once

#include <QLineEdit>
#include <QPushButton>
#include <QString>

// ─────────────────────────────────────────────────────────────────────────────
// 各流程頁面共用的小型 UI 工具（樣式與原本 PageCardOperation 內的版本一致）
// ─────────────────────────────────────────────────────────────────────────────
namespace UiHelpers
{
    inline QLineEdit* makeEditField(QWidget* parent, bool readOnly = false)
    {
        auto* f = new QLineEdit(parent);
        f->setReadOnly(readOnly);
        f->setStyleSheet(
            "QLineEdit { background:#F9F9F9; color:#000; font-family:'Courier New'; "
            "font-size:11px; padding:4px; border:1px solid #CCC; border-radius:3px; }"
            "QLineEdit:focus { border:1px solid #0078D4; background:#FFF; }");
        return f;
    }

    inline QPushButton* makeActionButton(const QString& text, const QString& color, QWidget* parent)
    {
        auto* b = new QPushButton(text, parent);
        b->setStyleSheet(QString(
            "QPushButton { background:%1; color:white; padding:10px 20px; "
            "font-weight:bold; font-size:13px; border-radius:4px; }"
            "QPushButton:disabled { background:#CCC; color:#666; }").arg(color));
        return b;
    }
}
