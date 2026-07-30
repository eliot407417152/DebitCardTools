#include "KeySetLoader.h"
#include <QFile>
#include <QTextStream>

QString KeySetEntry::displayText() const
{
    return QString("%1 - %2（KID=%3, %4 bytes）").arg(id, label, kid).arg(lengthBytes);
}

bool KeySetLoader::load(const QString& filePath, QList<KeySetEntry>& out, QStringList* warnings)
{
    out.clear();

    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream in(&f);
    in.setAutoDetectUnicode(true);

    int lineNo = 0;
    while (!in.atEnd()) {
        QString line = in.readLine();
        ++lineNo;

        QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith('#'))
            continue;

        // 格式：ID;長度(HEX);金鑰HEX;標籤;KID;（結尾分號可有可無）
        QStringList parts = trimmed.split(';', Qt::KeepEmptyParts);
        if (parts.size() < 5) {
            if (warnings)
                warnings->append(QString("第 %1 行欄位數不足（需要 5 欄），已略過：%2").arg(lineNo).arg(trimmed));
            continue;
        }

        KeySetEntry entry;
        entry.id = parts.at(0).trimmed();
        QString lenHex = parts.at(1).trimmed();
        entry.keyHex = parts.at(2).trimmed().toUpper();
        entry.label = parts.at(3).trimmed();
        entry.kid = parts.at(4).trimmed();

        bool lenOk = false;
        entry.lengthBytes = lenHex.toInt(&lenOk, 16);
        if (!lenOk) {
            if (warnings)
                warnings->append(QString("第 %1 行長度欄位無法解析為 HEX（%2），已略過：%3")
                    .arg(lineNo).arg(lenHex, trimmed));
            continue;
        }

        if (entry.keyHex.length() != entry.lengthBytes * 2 && warnings) {
            warnings->append(QString("第 %1 行金鑰實際長度（%2 bytes）與宣告長度（%3 bytes）不符：%4")
                .arg(lineNo).arg(entry.keyHex.length() / 2).arg(entry.lengthBytes).arg(trimmed));
        }

        if (entry.id.isEmpty() || entry.keyHex.isEmpty()) {
            if (warnings)
                warnings->append(QString("第 %1 行 ID 或金鑰為空，已略過：%2").arg(lineNo).arg(trimmed));
            continue;
        }

        out.append(entry);
    }

    return true;
}
