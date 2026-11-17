#include "PageCardOperation.h"
#include "IsamKeyUpdater.h"
#include "IsamKeyChecker.h"
#include "IsamKeyData.h"
#include "MasterCardUpdater.h"
#include "LodLoader.h"
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QLabel>
#include <QComboBox>
#include <QTabWidget>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QScrollArea>
#include <QRadioButton>
#include <QFile>
#include <QListWidget>
#include <QStackedWidget>
#include <QSplitter>

// ─────────────────────────────────────────────────────────────────────────────
// ApduFields::buildApdu
// ─────────────────────────────────────────────────────────────────────────────
QString ApduFields::buildApdu() const
{
    auto get = [](QLineEdit* f) {
        return f ? f->text().trimmed().remove(' ').toUpper() : QString();
        };

    QString result;
    result += get(cla).leftJustified(2, '0').left(2);
    result += get(ins).leftJustified(2, '0').left(2);
    result += get(p1).leftJustified(2, '0').left(2);
    result += get(p2).leftJustified(2, '0').left(2);

    QString lcVal = get(lc);
    QString leVal = get(le);
    QString dataVal = get(dataIn);

    if (!lcVal.isEmpty()) result += lcVal.leftJustified(2, '0').left(2);
    if (!dataVal.isEmpty()) result += dataVal;
    if (!leVal.isEmpty()) result += leVal.leftJustified(2, '0').left(2);

    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// CreateFileFields::buildDataIn — 組合 16 bytes DataIn
// ─────────────────────────────────────────────────────────────────────────────
QString CreateFileFields::buildDataIn() const
{
    auto get = [](QLineEdit* f, int bytes) -> QString {
        QString v = f ? f->text().trimmed().remove(' ').toUpper() : QString();
        int needed = bytes * 2;
        // 補 0 至需要長度，超過截斷
        return v.leftJustified(needed, '0').left(needed);
        };

    return get(efid, 2)   // #1~#2
        + get(ac1, 2)   // #3~#4
        + get(ac2, 2)   // #5~#6
        + get(ac3, 2)   // #7~#8
        + get(sob, 2)   // #9~#10
        + get(tof, 1)   // #11
        + get(sor, 1)   // #12
        + get(rc, 1)   // #13
        + get(index, 1)   // #14
        + get(tok, 1)   // #15
        + get(rfu, 1);  // #16
}

// CreateFileFields::buildApdu — Lc 固定 10h (16 bytes)
QString CreateFileFields::buildApdu() const
{
    auto get = [](QLineEdit* f) {
        return f ? f->text().trimmed().remove(' ').toUpper() : QString();
        };

    QString data = buildDataIn();
    QString lc = QString("%1").arg(data.length() / 2, 2, 16, QChar('0')).toUpper();

    return get(cla).leftJustified(2, '0').left(2)
        + get(ins).leftJustified(2, '0').left(2)
        + get(p1).leftJustified(2, '0').left(2)
        + get(p2).leftJustified(2, '0').left(2)
        + lc
        + data;
}

// ─────────────────────────────────────────────────────────────────────────────
// VerifyPinFields — computedP2 / buildApdu
//   P2 = Pq 邏輯左移 3 位（例如輸入 02 → 10h）
// ─────────────────────────────────────────────────────────────────────────────
QString VerifyPinFields::computedP2() const
{
    QString raw = pq ? pq->text().trimmed().remove(' ').toUpper() : QString();
    bool ok = false;
    unsigned int val = raw.left(2).toUInt(&ok, 16);
    if (!ok) val = 0;

    unsigned int shifted = (val << 3) & 0xFF;   // 邏輯左移 3 位，取低 8 bits
    return QString("%1").arg(shifted, 2, 16, QChar('0')).toUpper();
}

QString VerifyPinFields::buildApdu() const
{
    auto get = [](QLineEdit* f) {
        return f ? f->text().trimmed().remove(' ').toUpper() : QString();
        };

    QString claV = get(cla).leftJustified(2, '0').left(2);
    QString insV = get(ins).leftJustified(2, '0').left(2);
    QString p1V = get(p1).leftJustified(2, '0').left(2);
    QString p2V = computedP2();

    // DATA：PIN code，固定 8 bytes（16 HEX），不足補 FF
    QString pinV = get(pin).leftJustified(16, 'F').left(16);
    QString lcV = QString("%1").arg(pinV.length() / 2, 2, 16, QChar('0')).toUpper(); // 固定 08

    if (p2Display) p2Display->setText(p2V);
    if (lc)        lc->setText(lcV);

    return claV + insV + p1V + p2V + lcV + pinV;
}

// ─────────────────────────────────────────────────────────────────────────────
// TerminalAuthFields（卡片指令 — Terminal Authentication）
//   P2 邏輯左移 3 位運算比照 Verify Pin，但 DATA 固定 16 bytes（32 HEX），Lc = 10h
//   注意：Kq 為「十進位」輸入（規格範圍 16~31），需先以十進位解析成數值，
//         再做邏輯左移 3 位，而不是把輸入字串當十六進位解析！
//         例如輸入 16（十進位）→ 左移 3 位 = 128（十進位）= 80h
// ─────────────────────────────────────────────────────────────────────────────
QString TerminalAuthFields::computedP2() const
{
    QString raw = pq ? pq->text().trimmed().remove(' ') : QString();
    bool ok = false;
    unsigned int val = raw.toUInt(&ok, 10);   // 十進位解析（非十六進位）
    if (!ok) val = 0;

    unsigned int shifted = (val << 3) & 0xFF;   // 邏輯左移 3 位，取低 8 bits
    return QString("%1").arg(shifted, 2, 16, QChar('0')).toUpper();
}

QString TerminalAuthFields::buildApdu() const
{
    auto get = [](QLineEdit* f) {
        return f ? f->text().trimmed().remove(' ').toUpper() : QString();
        };

    QString claV = get(cla).leftJustified(2, '0').left(2);
    QString insV = get(ins).leftJustified(2, '0').left(2);
    QString p1V = get(p1).leftJustified(2, '0').left(2);
    QString p2V = computedP2();

    // DATA：不自動補 FF，Lc 依實際輸入長度計算（HEX 字元數需為偶數）
    QString pinV = get(pin);
    QString lcV = QString("%1").arg(pinV.length() / 2, 2, 16, QChar('0')).toUpper();

    if (p2Display) p2Display->setText(p2V);
    if (lc)        lc->setText(lcV);

    return claV + insV + p1V + p2V + lcV + pinV;
}


QString ReadRecordFields::buildApdu() const
{
    auto get = [](QLineEdit* f) {
        return f ? f->text().trimmed().remove(' ').toUpper() : QString();
        };

    QString claV = get(cla).leftJustified(2, '0').left(2);
    QString insV = get(ins).leftJustified(2, '0').left(2);
    QString p1V = get(recN).leftJustified(2, '0').left(2);   // Record Id
    QString p2V = get(p2).leftJustified(2, '0').left(2);     // 04 或 05（唯讀）

    // Le：依欄位 maxLength 決定要補齊/截斷成幾個 HEX 字元
    //   Read Record     → 2 個 HEX 字元（預設 00）
    //   Read All Record → 6 個 HEX 字元（預設 00 00 00）
    int leLen = (le && le->maxLength() > 0 && le->maxLength() <= 6) ? le->maxLength() : 2;
    QString leV = get(le).leftJustified(leLen, '0').left(leLen);

    return claV + insV + p1V + p2V + leV;
}

// ─────────────────────────────────────────────────────────────────────────────
// IsamSimpleFields::buildApdu — CLA INS P1 P2 (+ Le 可選)，無 Lc、無 DataIn
// ─────────────────────────────────────────────────────────────────────────────
QString IsamSimpleFields::buildApdu() const
{
    auto get = [](QLineEdit* f) {
        return f ? f->text().trimmed().remove(' ').toUpper() : QString();
        };

    QString result;
    result += get(cla).leftJustified(2, '0').left(2);
    result += get(ins).leftJustified(2, '0').left(2);
    result += get(p1).leftJustified(2, '0').left(2);
    result += get(p2).leftJustified(2, '0').left(2);

    QString leVal = get(le);
    if (!leVal.isEmpty()) result += leVal.leftJustified(2, '0').left(2);

    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// WriteKeyFields — buildDataIn / buildApdu
//   DataIn = EKey + CCode + Limit，Lc 依 DataIn 長度自動計算
// ─────────────────────────────────────────────────────────────────────────────
QString WriteKeyFields::buildDataIn() const
{
    auto get = [](QLineEdit* f) {
        return f ? f->text().trimmed().remove(' ').toUpper() : QString();
        };
    return get(eKey) + get(ccode) + get(limit);
}

QString WriteKeyFields::buildApdu() const
{
    auto get = [](QLineEdit* f) {
        return f ? f->text().trimmed().remove(' ').toUpper() : QString();
        };

    QString data = buildDataIn();
    QString lc = QString("%1").arg(data.length() / 2, 2, 16, QChar('0')).toUpper();

    return get(cla).leftJustified(2, '0').left(2)
        + get(ins).leftJustified(2, '0').left(2)
        + get(p1).leftJustified(2, '0').left(2)
        + get(p2).leftJustified(2, '0').left(2)
        + lc
        + data;
}

// ─────────────────────────────────────────────────────────────────────────────
// StoreManufacturerKeyFields — buildDataIn / buildApdu
//   DataIn = KT + EKey + CCode + Limit，Lc 依 DataIn 長度自動計算
// ─────────────────────────────────────────────────────────────────────────────
QString StoreManufacturerKeyFields::buildDataIn() const
{
    auto get = [](QLineEdit* f) {
        return f ? f->text().trimmed().remove(' ').toUpper() : QString();
        };
    return get(kt) + get(eKey) + get(ccode) + get(limit);
}

QString StoreManufacturerKeyFields::buildApdu() const
{
    auto get = [](QLineEdit* f) {
        return f ? f->text().trimmed().remove(' ').toUpper() : QString();
        };

    QString data = buildDataIn();
    QString lc = QString("%1").arg(data.length() / 2, 2, 16, QChar('0')).toUpper();

    return get(cla).leftJustified(2, '0').left(2)
        + get(ins).leftJustified(2, '0').left(2)
        + get(p1).leftJustified(2, '0').left(2)
        + get(p2).leftJustified(2, '0').left(2)
        + lc
        + data;
}

// ─────────────────────────────────────────────────────────────────────────────
// SetSessionKeyIsamFields — buildDataIn / buildApdu
//   DataIn = Div-Data + R2，Lc = DL + 8（依 DataIn 長度自動計算），無 Le
// ─────────────────────────────────────────────────────────────────────────────
QString SetSessionKeyIsamFields::buildDataIn() const
{
    auto get = [](QLineEdit* f) {
        return f ? f->text().trimmed().remove(' ').toUpper() : QString();
        };
    QString r2V = get(r2).leftJustified(16, 'F').left(16);   // R2 固定 8 bytes
    return get(divData) + r2V;
}

QString SetSessionKeyIsamFields::buildApdu() const
{
    auto get = [](QLineEdit* f) {
        return f ? f->text().trimmed().remove(' ').toUpper() : QString();
        };

    QString data = buildDataIn();
    QString lc = QString("%1").arg(data.length() / 2, 2, 16, QChar('0')).toUpper();

    return get(cla).leftJustified(2, '0').left(2)
        + get(ins).leftJustified(2, '0').left(2)
        + get(p1).leftJustified(2, '0').left(2)
        + get(p2).leftJustified(2, '0').left(2)
        + lc
        + data;
}

// ─────────────────────────────────────────────────────────────────────────────
// DiversifyKeyFields — buildApdu
//   Data = Div-Data，Lc 依 Div-Data 長度自動計算，Le 固定 00
// ─────────────────────────────────────────────────────────────────────────────
QString DiversifyKeyFields::buildApdu() const
{
    auto get = [](QLineEdit* f) {
        return f ? f->text().trimmed().remove(' ').toUpper() : QString();
        };

    QString data = get(divData);
    QString lc = QString("%1").arg(data.length() / 2, 2, 16, QChar('0')).toUpper();
    QString leV = get(le).isEmpty() ? QString("00") : get(le).leftJustified(2, '0').left(2);

    return get(cla).leftJustified(2, '0').left(2)
        + get(ins).leftJustified(2, '0').left(2)
        + get(p1).leftJustified(2, '0').left(2)
        + get(p2).leftJustified(2, '0').left(2)
        + lc
        + data
        + leV;
}

// ─────────────────────────────────────────────────────────────────────────────
// SetSessionKeyCardFields — computedP2 / buildApdu
//   P2 = Kq 邏輯左移 3 位（比照 Verify Pin 作法）；Data = R1（8 bytes），Lc 固定 08，Le 固定 08
//   注意：Kq 為「十進位」輸入（規格範圍 16~31），需先以十進位解析成數值，
//         再做邏輯左移 3 位，而不是把輸入字串當十六進位解析！
// ─────────────────────────────────────────────────────────────────────────────
QString SetSessionKeyCardFields::computedP2() const
{
    QString raw = pq ? pq->text().trimmed().remove(' ') : QString();
    bool ok = false;
    unsigned int val = raw.toUInt(&ok, 10);   // 十進位解析（非十六進位）
    if (!ok) val = 0;

    unsigned int shifted = (val << 3) & 0xFF;   // 邏輯左移 3 位，取低 8 bits
    return QString("%1").arg(shifted, 2, 16, QChar('0')).toUpper();
}

QString SetSessionKeyCardFields::buildApdu() const
{
    auto get = [](QLineEdit* f) {
        return f ? f->text().trimmed().remove(' ').toUpper() : QString();
        };

    QString claV = get(cla).leftJustified(2, '0').left(2);
    QString insV = get(ins).leftJustified(2, '0').left(2);
    QString p1V = get(p1).leftJustified(2, '0').left(2);
    QString p2V = computedP2();

    // DATA：Random R1，固定 8 bytes（16 HEX），不足補 FF
    QString r1V = get(r1).leftJustified(16, 'F').left(16);
    QString lcV = QString("%1").arg(r1V.length() / 2, 2, 16, QChar('0')).toUpper();   // 固定 08
    QString leV = get(le).isEmpty() ? QString("08") : get(le).leftJustified(2, '0').left(2);

    if (p2Display) p2Display->setText(p2V);
    if (lc)        lc->setText(lcV);

    return claV + insV + p1V + p2V + lcV + r1V + leV;
}

// ─────────────────────────────────────────────────────────────────────────────
// UpdateRecordCipheredFields — buildApdu
//   DATA 為自行輸入，Lc 依 DATA 長度自動計算（08/10/18）
// ─────────────────────────────────────────────────────────────────────────────
QString UpdateRecordCipheredFields::buildApdu() const
{
    auto get = [](QLineEdit* f) {
        return f ? f->text().trimmed().remove(' ').toUpper() : QString();
        };

    QString dataV = get(data);
    QString lc = QString("%1").arg(dataV.length() / 2, 2, 16, QChar('0')).toUpper();

    return get(cla).leftJustified(2, '0').left(2)
        + get(ins).leftJustified(2, '0').left(2)
        + get(p1).leftJustified(2, '0').left(2)
        + get(p2).leftJustified(2, '0').left(2)
        + lc
        + dataV;
}

// ─────────────────────────────────────────────────────────────────────────────
// UpdateRecordFields::buildApdu — CLA INS P1(RecN) P2 Lc DATA
//   Lc 依 DATA 實際長度自動計算
// ─────────────────────────────────────────────────────────────────────────────
QString UpdateRecordFields::buildApdu() const
{
    auto get = [](QLineEdit* f) {
        return f ? f->text().trimmed().remove(' ').toUpper() : QString();
        };

    QString dataV = get(data);
    QString lc = QString("%1").arg(dataV.length() / 2, 2, 16, QChar('0')).toUpper();

    return get(cla).leftJustified(2, '0').left(2)
        + get(ins).leftJustified(2, '0').left(2)
        + get(recN).rightJustified(2, '0').right(2)
        + get(p2).leftJustified(2, '0').left(2)
        + lc
        + dataV;
}

// ─────────────────────────────────────────────────────────────────────────────
// RawApduFields::buildApdu — CLA INS P1 P2 [Lc] [Data] [Le]
//   完全依使用者輸入組合，不補 0、不自動算 Lc；留空的 Lc/Data/Le 不組入
// ─────────────────────────────────────────────────────────────────────────────
QString RawApduFields::buildApdu() const
{
    auto get = [](QLineEdit* f) {
        return f ? f->text().trimmed().remove(' ').toUpper() : QString();
        };

    return get(cla) + get(ins) + get(p1) + get(p2)
        + get(lc) + get(data) + get(le);
}

// ─────────────────────────────────────────────────────────────────────────────
// CardAuthFields — buildApdu
//   P2 為自行輸入的 Kq（不做左移運算），DATA = R1 + R3（各 8 bytes），Lc/Le 固定 10h
// ─────────────────────────────────────────────────────────────────────────────
QString CardAuthFields::computedP2() const
{
    QString raw = pq ? pq->text().trimmed().remove(' ') : QString();
    bool ok = false;
    unsigned int val = raw.toUInt(&ok, 10);   // 十進位解析（非十六進位），比照 Set Session Key 作法
    if (!ok) val = 0;

    unsigned int shifted = (val << 3) & 0xFF;   // 邏輯左移 3 位，取低 8 bits
    return QString("%1").arg(shifted, 2, 16, QChar('0')).toUpper();
}

QString CardAuthFields::buildApdu() const
{
    auto get = [](QLineEdit* f) {
        return f ? f->text().trimmed().remove(' ').toUpper() : QString();
        };

    QString r1V = get(r1).leftJustified(16, 'F').left(16);   // R1 固定 8 bytes
    QString r3V = get(r3).leftJustified(16, 'F').left(16);   // R3 固定 8 bytes
    QString dataV = r1V + r3V;
    QString lcV = QString("%1").arg(dataV.length() / 2, 2, 16, QChar('0')).toUpper();   // 固定 10h
    QString leV = get(le).isEmpty() ? QString("10") : get(le).leftJustified(2, '0').left(2);
    QString p2V = computedP2();

    if (lc) lc->setText(lcV);
    if (p2Display) p2Display->setText(p2V);

    return get(cla).leftJustified(2, '0').left(2)
        + get(ins).leftJustified(2, '0').left(2)
        + get(p1).leftJustified(2, '0').left(2)
        + p2V
        + lcV
        + dataV
        + leV;
}

// ─────────────────────────────────────────────────────────────────────────────
// WriteRecordTacFields — buildApdu
//   P2 為自行輸入的 Kq，DATA 為自行輸入（長度依 P2 而定），Lc 依 DATA 長度自動計算，Le 固定 08
// ─────────────────────────────────────────────────────────────────────────────
QString WriteRecordTacFields::buildApdu() const
{
    auto get = [](QLineEdit* f) {
        return f ? f->text().trimmed().remove(' ').toUpper() : QString();
        };

    QString dataV = get(data);
    QString lcV = QString("%1").arg(dataV.length() / 2, 2, 16, QChar('0')).toUpper();
    QString leV = get(le).isEmpty() ? QString("08") : get(le).leftJustified(2, '0').left(2);

    if (lcDisplay) lcDisplay->setText(lcV);

    return get(cla).leftJustified(2, '0').left(2)
        + get(ins).leftJustified(2, '0').left(2)
        + get(p1).leftJustified(2, '0').left(2)
        + get(p2).leftJustified(2, '0').left(2)
        + lcV
        + dataV
        + leV;
}

// ─────────────────────────────────────────────────────────────────────────────
// WriteRecordWSNumTacFields — buildApdu
//   CLA 固定 00 / INS 固定 E6，P1、P2、Lc、DATA 皆自行輸入，Le 固定 00
// ─────────────────────────────────────────────────────────────────────────────
QString WriteRecordWSNumTacFields::buildApdu() const
{
    auto get = [](QLineEdit* f) {
        return f ? f->text().trimmed().remove(' ').toUpper() : QString();
        };

    QString dataV = get(data);
    QString lcV = get(lc).isEmpty()
        ? QString("%1").arg(dataV.length() / 2, 2, 16, QChar('0')).toUpper()
        : get(lc).leftJustified(2, '0').left(2);
    QString leV = get(le).isEmpty() ? QString("00") : get(le).leftJustified(2, '0').left(2);

    return get(cla).leftJustified(2, '0').left(2)
        + get(ins).leftJustified(2, '0').left(2)
        + get(p1).leftJustified(2, '0').left(2)
        + get(p2).leftJustified(2, '0').left(2)
        + lcV
        + dataV
        + leV;
}