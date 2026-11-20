#include "CustomerDataForm.h"
#include "UiHelpers.h"
#include <QLineEdit>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QRegularExpression>
#include <QRegularExpressionValidator>

CustomerDataForm::CustomerDataForm(Fields shown, QWidget* parent)
    : QGroupBox("客戶資料欄位（可手動編輯）", parent)
{
    auto* form = new QFormLayout(this);
    form->setLabelAlignment(Qt::AlignRight);
    form->setVerticalSpacing(10);

    // 一般欄位：所有欄位一律建立，沒有被選到顯示的就隱藏（資料仍保留，載入 LOD 時不會遺失）
    auto mf = [&](QLineEdit*& f, Field flag, const QString& lbl) {
        f = UiHelpers::makeEditField(this, false);
        f->setMaximumHeight(28);
        if (shown.testFlag(flag)) form->addRow(lbl, f);
        else f->hide();
        };

    // 檢查碼欄位：自動計算、唯讀
    auto mkCheckRow = [&](QLineEdit*& f, Field flag, const QString& lbl) {
        f = UiHelpers::makeEditField(this, true);
        f->setMaximumHeight(28);
        f->setFixedWidth(80);
        f->setPlaceholderText("自動計算");
        if (shown.testFlag(flag)) form->addRow(lbl, f);
        else f->hide();
        };

    // 三段式欄位：K1 / K2 / K3，每段 16 碼（8 bytes）
    auto mkSegRow = [&](QLineEdit* (&segs)[3], Field flag, const QString& lbl) {
        auto* row = new QWidget(this);
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(6);
        for (int i = 0; i < 3; ++i) {
            segs[i] = UiHelpers::makeEditField(row, false);
            segs[i]->setMaximumHeight(28);
            segs[i]->setFixedWidth(150);
            segs[i]->setMaxLength(16);
            segs[i]->setPlaceholderText(QString("K%1（16 碼）").arg(i + 1));
            segs[i]->setValidator(new QRegularExpressionValidator(
                QRegularExpression("[0-9A-Fa-f]*"), segs[i]));
            h->addWidget(segs[i]);
        }
        h->addStretch();
        if (shown.testFlag(flag)) {
            form->addRow(lbl, row);
        }
        else {
            row->hide();   // 子欄位仍存在、仍可被 setData() 寫入
        }
        };

    mf(m_nameEdit, Name, "客戶名稱：");
    mf(m_c3Edit, C3, "C3 金鑰：");
    mf(m_c3CodeEdit, C3Code, "C3 Code：");
    mf(m_c1Edit, C1, "C1 金鑰：");
    mf(m_c1CodeEdit, C1Code, "C1 Code：");

    mkSegRow(m_codeAEdit, CodeA, "Code A：");
    mkCheckRow(m_codeACheckEdit, CodeA, "Code A 檢查碼：");
    mkSegRow(m_codeBEdit, CodeB, "Code B：");
    mkCheckRow(m_codeBCheckEdit, CodeB, "Code B 檢查碼：");
    mkSegRow(m_exchEdit, Exchange, "Exchange Code：");
    mf(m_ccodeEdit, Exchange, "CCode（Exchange 檢查碼）：");
    mf(m_pinAEdit, PinA, "Pin A：");
    mf(m_pinBEdit, PinB, "Pin B：");

    // 任何欄位被修改都通知外面（例如判斷「製作母卡」按鈕可不可以按）
    for (QLineEdit* f : { m_nameEdit, m_c3Edit, m_c3CodeEdit, m_c1Edit, m_c1CodeEdit,
                          m_ccodeEdit, m_pinAEdit, m_pinBEdit })
        connect(f, &QLineEdit::textChanged, this, &CustomerDataForm::changed);

    // Code A / B / Exchange Code 的三段欄位：任一段改變就重算檢查碼
    for (int i = 0; i < 3; ++i) {
        connect(m_codeAEdit[i], &QLineEdit::textChanged, this, &CustomerDataForm::onCodeABChanged);
        connect(m_codeBEdit[i], &QLineEdit::textChanged, this, &CustomerDataForm::onCodeABChanged);
        connect(m_exchEdit[i], &QLineEdit::textChanged, this, &CustomerDataForm::onExchangeChanged);
        connect(m_exchEdit[i], &QLineEdit::textChanged, this, &CustomerDataForm::changed);
    }
}

bool CustomerDataForm::hasC3AndC1() const
{
    return !m_c3Edit->text().trimmed().isEmpty()
        && !m_c1Edit->text().trimmed().isEmpty();
}

// ── 三段式欄位（K1 / K2 / K3，各 16 碼）：合併 / 拆分 ───────────────────────
QString CustomerDataForm::joinSegs(QLineEdit* const* segs) const
{
    QString s;
    for (int i = 0; i < 3; ++i)
        s += segs[i]->text().trimmed().toUpper();
    return s;
}

void CustomerDataForm::setSegs(QLineEdit* const* segs, const QString& value)
{
    const QString v = value.trimmed().toUpper();
    for (int i = 0; i < 3; ++i)
        segs[i]->setText(v.mid(i * 16, 16));
}

// Code A / Code B 任一段改變：
//   1. 重算 Code A / Code B 檢查碼（3DES-ECB，Data=0000000000000000，取前 4 碼）
//   2. Code A、Code B 都是完整 48 碼時：Exchange Code = Code A XOR Code B
//   3. 重算 CCode（Exchange Code 的檢查碼）
void CustomerDataForm::onCodeABChanged()
{
    if (m_updatingKeyFields) return;

    const QString a = joinSegs(m_codeAEdit);
    const QString b = joinSegs(m_codeBEdit);

    m_codeACheckEdit->setText(CardUtils::calcKeyCheckCode(a));
    m_codeBCheckEdit->setText(CardUtils::calcKeyCheckCode(b));

    if (a.length() == 48 && b.length() == 48) {
        const QString x = CardUtils::xorHex(a, b);
        if (!x.isEmpty()) {
            m_updatingKeyFields = true;
            setSegs(m_exchEdit, x);
            m_updatingKeyFields = false;
        }
    }

    m_ccodeEdit->setText(CardUtils::calcKeyCheckCode(joinSegs(m_exchEdit)));
}

// Exchange Code 任一段改變（手動編輯）：只重算 CCode
void CustomerDataForm::onExchangeChanged()
{
    if (m_updatingKeyFields) return;
    m_ccodeEdit->setText(CardUtils::calcKeyCheckCode(joinSegs(m_exchEdit)));
}

CustomerData CustomerDataForm::data() const
{
    CustomerData d;
    d.name = m_nameEdit->text().trimmed();
    d.c3 = m_c3Edit->text().trimmed().toUpper();
    d.c3Code = m_c3CodeEdit->text().trimmed().toUpper();
    d.c1 = m_c1Edit->text().trimmed().toUpper();
    d.c1Code = m_c1CodeEdit->text().trimmed().toUpper();
    d.codeA = joinSegs(m_codeAEdit);
    d.codeB = joinSegs(m_codeBEdit);
    d.exchangeCode = joinSegs(m_exchEdit);
    d.ccode = m_ccodeEdit->text().trimmed().toUpper();
    d.pinA = m_pinAEdit->text().trimmed().toUpper();
    d.pinB = m_pinBEdit->text().trimmed().toUpper();
    return d;
}

void CustomerDataForm::setData(const CustomerData& d)
{
    m_nameEdit->setText(d.name);
    m_c3Edit->setText(d.c3);
    m_c3CodeEdit->setText(d.c3Code);
    m_c1Edit->setText(d.c1);
    m_c1CodeEdit->setText(d.c1Code);

    // Code A / Code B / Exchange Code：拆成三段（每段 16 碼）顯示
    m_updatingKeyFields = true;
    setSegs(m_codeAEdit, d.codeA);
    setSegs(m_codeBEdit, d.codeB);
    setSegs(m_exchEdit, d.exchangeCode);
    m_updatingKeyFields = false;
    m_ccodeEdit->setText(d.ccode);

    // Exchange Code 應等於 Code A XOR Code B：若 LOD 內的值不同，提醒一下（最後以 XOR 結果為準）
    if (d.codeA.length() == 48 && d.codeB.length() == 48
        && !d.exchangeCode.isEmpty()
        && d.exchangeCode.toUpper() != CardUtils::xorHex(d.codeA, d.codeB)) {
        emit warning("LOD 內的 Exchange Code 與「Code A XOR Code B」不一致，已改用 XOR 結果。");
    }

    // 重算 Code A / Code B 檢查碼，並套用 Exchange Code = A XOR B、重算 CCode
    onCodeABChanged();

    m_pinAEdit->setText(d.pinA);
    m_pinBEdit->setText(d.pinB);
}

void CustomerDataForm::clear()
{
    for (QLineEdit* f : { m_nameEdit, m_c3Edit, m_c3CodeEdit, m_c1Edit, m_c1CodeEdit,
                          m_ccodeEdit, m_pinAEdit, m_pinBEdit })
        f->clear();
    for (int i = 0; i < 3; ++i) {
        m_codeAEdit[i]->clear();
        m_codeBEdit[i]->clear();
        m_exchEdit[i]->clear();
    }
    m_codeACheckEdit->clear();
    m_codeBCheckEdit->clear();
}
