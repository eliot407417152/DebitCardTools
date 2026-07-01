#include "LodLoader.h"
#include "IsamKeyData.h"
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QRegularExpression>

// 各欄位預期的 HEX 字元長度 (0 = 不限制)
static const QMap<QString, int> FIELD_TARGET_LEN = {
    {"c3",            48},
    {"c1",            48},
    {"exchange code", 48},
    {"a code",        48},
    {"b code",        48},
    {"ccode",          4},
    {"pina",          16},
    {"pinb",          16},
};

// ─────────────────────────────────────────────────────────────────────────────
// 輔助：確保字串是合法 HEX，補齊或截斷至 targetLen
// ─────────────────────────────────────────────────────────────────────────────
static QString normalizeHex(const QString& raw, int targetLen)
{
    // 只保留 HEX 字元
    QString hex;
    for (QChar c : raw) {
        if ((c >= '0' && c <= '9') ||
            (c >= 'A' && c <= 'F') ||
            (c >= 'a' && c <= 'f'))
            hex += c.toUpper();
    }
    if (targetLen <= 0) return hex;

    // 長度不夠：補 FF
    if (hex.length() < targetLen)
        hex = hex.leftJustified(targetLen, 'F');
    // 長度超過：截斷
    if (hex.length() > targetLen)
        hex = hex.left(targetLen);

    return hex;
}

// ─────────────────────────────────────────────────────────────────────────────
// 主要解析入口
// ─────────────────────────────────────────────────────────────────────────────
bool LodLoader::load(const QString& filePath, CustomerData& out, QStringList* warnings)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream in(&f);
    in.setAutoDetectUnicode(true);

    QString headerLine, dataLine;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        headerLine = line;
        break;
    }
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        dataLine = line;
        break;
    }

    if (headerLine.isEmpty() || dataLine.isEmpty())
        return false;

    while (dataLine.endsWith(',')) dataLine.chop(1);

    QStringList headers = headerLine.split(',', Qt::KeepEmptyParts);
    QStringList allTokens = dataLine.split(',', Qt::KeepEmptyParts);

    for (auto& h : headers)   h = h.trimmed();
    for (auto& t : allTokens) t = t.trimmed().toUpper();

    int total = allTokens.size();
    if (total < 4) return false;

    // ── pinA/pinB 固定從末端取 ───────────────────────────────────────────────
    out.pinA = normalizeHex(allTokens.at(total - 2), 16);
    out.pinB = normalizeHex(allTokens.at(total - 1), 16);

    // ── 前段：header 有幾欄對應幾個 token，不足補空，多的捨棄 ───────────────
    int hCount = headers.size();
    // 但 allTokens 可能因為值被逗號切斷而比 header 多
    // 策略：逐個 header 欄位處理，遇到短值就嘗試合併下一個 token
    QStringList tokens;
    int ti = 0;
    for (int hi = 0; hi < hCount; ++hi) {
        if (ti >= allTokens.size()) { tokens.append(QString()); continue; }

        QString hname = headers.at(hi).toLower();
        int target = FIELD_TARGET_LEN.value(hname, 0);

        QString val = allTokens.at(ti);
        ti++;

        if (target > 0) {
            // 如果值長度不足且下一個 token 看起來是延續（非 4 碼的 ccode）
            while (val.length() < target && ti < allTokens.size()) {
                QString next = allTokens.at(ti);
                // 下一個是 4 碼的 ccode 就停止合併
                if (next.length() == 4) break;
                // 下一個是空的就停止（代表真的沒有值）
                if (next.isEmpty()) break;
                val += next;
                ti++;
            }
            val = normalizeHex(val, target);
            if (warnings && val.length() != target) {
                warnings->append(QString("欄位 [%1] 長度異常，已自動補齊至 %2 字元").arg(hname).arg(target));
            }
        }
        tokens.append(val);
    }

    // 補齊
    while (tokens.size() < hCount) tokens.append(QString());

    // ── 填入各欄位 ────────────────────────────────────────────────────────────
    out.name = QFileInfo(filePath).completeBaseName();
    out.c3 = getField(headers, tokens, "c3");
    out.c3Code = getField(headers, tokens, "ccode");           // 第一個 CCODE = c3code
    out.c1 = getField(headers, tokens, "c1");
    out.c1Code = getNextField(headers, tokens, "c1");          // c1 後面的 CCODE
    out.exchangeCode = getField(headers, tokens, "exchange code");
    out.codeA = getField(headers, tokens, "a code");
    out.codeB = getField(headers, tokens, "b code");

    // ── 嘗試用 c3 從 Key 清單對齊正確的 c3Code/c1Code ──────────────────────
    if (const IsamKeyEntry* entry = IsamKeyData::findByC3(out.c3)) {
        if (out.c3Code.isEmpty() || out.c3Code == "FFFF")
            out.c3Code = entry->c3Code;
        if (out.c1.isEmpty())
            out.c1 = entry->c1;
        if (out.c1Code.isEmpty() || out.c1Code == "FFFF")
            out.c1Code = entry->c1Code;
    }

    // isValid 只要 c3/c1 有值就算過，其他欄位允許空
    return !out.c3.isEmpty() && !out.c1.isEmpty();
}

QString LodLoader::getField(const QStringList& headers,
    const QStringList& tokens,
    const QString& fieldName)
{
    QString name = fieldName.trimmed().toLower();
    for (int i = 0; i < headers.size(); ++i)
        if (headers.at(i).toLower() == name && i < tokens.size())
            return tokens.at(i);
    return {};
}

QString LodLoader::getNextField(const QStringList& headers,
    const QStringList& tokens,
    const QString& fieldName)
{
    QString name = fieldName.trimmed().toLower();
    for (int i = 0; i < headers.size() - 1; ++i)
        if (headers.at(i).toLower() == name && i + 1 < tokens.size())
            return tokens.at(i + 1);
    return {};
}
