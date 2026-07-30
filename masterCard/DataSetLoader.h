#pragma once

#include <QString>
#include <QStringList>
#include <QList>

// ─────────────────────────────────────────────────────────────────────────────
// DataSetEntry — 單筆 Data Set 資料
//   檔案格式（分號分隔，# 開頭視為註解，可忽略空白行）：
//     ID;長度(HEX);資料HEX;標籤;
//   例：01;08;0102030405060708;Card Account;
//     → id="01", lengthBytes=0x08=8, dataHex=(16碼), label="Card Account"
// ─────────────────────────────────────────────────────────────────────────────
struct DataSetEntry
{
    QString id;
    int     lengthBytes = 0;
    QString dataHex;
    QString label;

    QString displayText() const;
};

// ─────────────────────────────────────────────────────────────────────────────
// DataSetLoader — 從外部 .txt 檔載入 Data Set 資料（格式同 KeySetLoader）
// ─────────────────────────────────────────────────────────────────────────────
class DataSetLoader
{
public:
    static bool load(const QString& filePath, QList<DataSetEntry>& out, QStringList* warnings = nullptr);
};
