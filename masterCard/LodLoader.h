#pragma once

#include "CardUtils.h"
#include <QString>
#include <QStringList>

// ─────────────────────────────────────────────────────────────────────────────
// LodLoader — 專屬 LOD 格式解析器（含自動修復功能）
//
// 支援：
//   - 欄位值被逗號切斷時自動合併
//   - 長度不足時補 FF，超過時截斷
//   - 載入後對照 IsamKeyData 自動補齊 c3Code / c1Code
//   - 傳入 warnings 可收集修復過程的說明訊息
// ─────────────────────────────────────────────────────────────────────────────
class LodLoader
{
public:
    static bool load(const QString& filePath,
        CustomerData& out,
        QStringList* warnings = nullptr);

private:
    static QString getField(const QStringList& headers,
        const QStringList& tokens,
        const QString& fieldName);

    static QString getNextField(const QStringList& headers,
        const QStringList& tokens,
        const QString& fieldName);
};
