#pragma once
#include <QString>
#include <QList>

// ─────────────────────────────────────────────────────────────────────────────
// IsamKeyEntry — 單筆 ISAM KEY 資料
// ─────────────────────────────────────────────────────────────────────────────
struct IsamKeyEntry {
    int     id;
    QString displayName;   // 顯示名稱（整理後）
    QString c3;
    QString c3Code;
    QString c1;
    QString c1Code;
};

// ─────────────────────────────────────────────────────────────────────────────
// IsamKeyData — 內建 Key 清單（從 Key.csv 整理，去除重複 C3/C1 組合）
// 依 C3 唯一性合併，同組名稱用 " / " 串接
// ─────────────────────────────────────────────────────────────────────────────
class IsamKeyData
{
public:
    // 回傳所有唯一 key 清單（已去重）
    static const QList<IsamKeyEntry>& entries();

    // 依 C3 值查找對應的 entry（用於 LOD 載入後自動對齊）
    static const IsamKeyEntry* findByC3(const QString& c3);
};
