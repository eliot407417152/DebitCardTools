#pragma once

#include <QString>
#include <QStringList>
#include <QList>

// ─────────────────────────────────────────────────────────────────────────────
// KeySetEntry — 單筆 Key Set 資料
//   檔案格式（分號分隔，# 開頭視為註解，可忽略空白行）：
//     ID;長度(HEX);金鑰HEX;標籤;KID;
//   例：01;18;3F00C0C0111111113F00C0C0222222223F00C0C033333333;C0;01;
//     → id="01", lengthBytes=0x18=24, keyHex=(48碼), label="C0", kid="01"
// ─────────────────────────────────────────────────────────────────────────────
struct KeySetEntry
{
    QString id;          // 編號（例："01"）
    int     lengthBytes = 0; // 金鑰長度（bytes，由 HEX 長度欄位換算）
    QString keyHex;       // 金鑰本體（HEX 字串）
    QString label;        // 標籤／對應檔案名稱（例："C0"）
    QString kid;          // Key ID（例："01"）

    QString displayText() const;
};

// ─────────────────────────────────────────────────────────────────────────────
// KeySetLoader — 從外部 .txt 檔載入 Key Set 資料（類似 .ini 的簡易分號分隔格式）
// ─────────────────────────────────────────────────────────────────────────────
class KeySetLoader
{
public:
    // 讀取檔案，成功回傳 true 並把解析結果放進 out（原有內容會先清空）
    // warnings 可為 nullptr；若提供，格式異常的行會記錄警告訊息但不中斷解析
    static bool load(const QString& filePath, QList<KeySetEntry>& out, QStringList* warnings = nullptr);
};
