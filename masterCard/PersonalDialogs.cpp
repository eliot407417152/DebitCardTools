#include "PersonalDialogs.h"
#include "UiScale.h"
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScrollArea>
#include <QSpinBox>
#include <QStyle>
#include <QVBoxLayout>

static QString lenText(const personal::Field& f) {
    return f.minLen == f.maxLen ? QString("%1 bytes").arg(f.minLen)
        : QString("%1~%2 bytes").arg(f.minLen).arg(f.maxLen);
}

// ======================= 輸入視窗 =======================
RecordDialog::RecordDialog(const personal::FileDef& def, Mode mode, QWidget* parent) : QDialog(parent), mode_(mode) {
    auto* root = new QVBoxLayout(this);

    QString hint = QString("EF %1").arg(QString::fromStdString(def.name));
    if (!def.title.empty()) hint += "  " + QString::fromStdString(def.title);
    hint += "\n兩個十六進位字元為一個 byte";
    hint += (mode == Mode::Update) ? ";勾選要更新的項目,只會送出有勾選的"
        : ";取消勾選的項目不會寫入";
    root->addWidget(new QLabel(hint));

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget;
    auto* lay = new QVBoxLayout(content);

    for (const auto& rec : def.records) {
        RecordBox rb;
        rb.rec = &rec;

        const QString title = rec.rdId >= 0 ? "RD ID " + QString::fromStdString(toHex(Bytes{ (uint8_t)rec.rdId }))
            : QString("記錄內容");
        rb.box = new QGroupBox(title);
        auto* form = new QFormLayout(rb.box);

        if (rec.rdId >= 0) {
            rb.box->setCheckable(true);
            rb.box->setChecked(mode == Mode::Write);   // 寫入預設全勾;更新預設不勾,避免不小心把資料蓋掉
        }
        else if (mode == Mode::Update) {
            rb.recNo = new QSpinBox;                   // 沒有 RD ID 的檔案,更新時要指定記錄編號
            rb.recNo->setRange(1, 255);
            form->addRow("記錄編號", rb.recNo);
        }

        for (const auto& f : rec.fields) {
            auto* edit = new QLineEdit(QString(f.minLen * 2, '0'));   // 預設全部補 0
            edit->setValidator(new QRegularExpressionValidator(QRegularExpression("[0-9A-Fa-f ]*"), edit));
            auto* info = new QLabel;
            info->setMinimumWidth(uiscale::u(6.9));

            auto* row = new QWidget;
            auto* h = new QHBoxLayout(row);
            h->setContentsMargins(0, 0, 0, 0);
            h->addWidget(edit, 1);
            h->addWidget(info);

            const personal::Field* fp = &f;
            auto refresh = [edit, info, fp]() {
                Bytes b;
                const bool ok = fromHex(edit->text().toStdString(), b);
                const int n = static_cast<int>(b.size());
                const bool valid = ok && n >= fp->minLen && n <= fp->maxLen;
                QString t = ok ? QString("%1 bytes").arg(n) : QString("格式錯誤");
                info->setText(t);
                edit->setProperty("invalid", !valid);
                edit->style()->unpolish(edit);
                edit->style()->polish(edit);
                };
            connect(edit, &QLineEdit::textChanged, this, [refresh] { refresh(); });
            refresh();

            form->addRow(QString("%1  (%2, %3)").arg(QString::fromStdString(f.name),
                QString::fromStdString(f.type), lenText(f)), row);
            rb.fields.push_back({ edit, fp });
        }
        lay->addWidget(rb.box);
        boxes_.push_back(rb);
    }
    lay->addStretch();
    scroll->setWidget(content);
    root->addWidget(scroll, 1);

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    bb->button(QDialogButtonBox::Ok)->setText("送出");
    bb->button(QDialogButtonBox::Cancel)->setText("取消");
    connect(bb, &QDialogButtonBox::accepted, this, &RecordDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &RecordDialog::reject);
    root->addWidget(bb);

    resize(uiscale::fit(QSize(uiscale::u(37.5), uiscale::u(35)), 0.9));
}

void RecordDialog::accept() {
    std::vector<CardPersonal::RecordInput> out;

    for (const auto& rb : boxes_) {
        if (rb.box->isCheckable() && !rb.box->isChecked()) continue;   // 沒勾選的不送

        CardPersonal::RecordInput ri;
        ri.rdId = rb.rec->rdId;
        ri.recNo = rb.recNo ? rb.recNo->value() : 1;

        for (const auto& fe : rb.fields) {
            Bytes b;
            const QString name = QString::fromStdString(fe.field->name);
            if (!fromHex(fe.edit->text().toStdString(), b)) {
                QMessageBox::warning(this, "輸入錯誤", QString("「%1」格式錯誤,請輸入十六進位字元").arg(name));
                fe.edit->setFocus();
                return;
            }
            if (static_cast<int>(b.size()) < fe.field->minLen || static_cast<int>(b.size()) > fe.field->maxLen) {
                QMessageBox::warning(this, "輸入錯誤",
                    QString("「%1」長度必須是 %2,目前是 %3 bytes").arg(name, lenText(*fe.field)).arg(b.size()));
                fe.edit->setFocus();
                return;
            }
            ri.data.insert(ri.data.end(), b.begin(), b.end());
        }
        out.push_back(ri);
    }

    if (out.empty()) {
        QMessageBox::warning(this, "尚未選擇", mode_ == Mode::Update ? "請至少勾選一個要更新的項目"
            : "請至少勾選一個要寫入的項目");
        return;
    }
    result_ = out;
    QDialog::accept();
}

// ======================= 查看視窗 =======================
void showRecords(QWidget* parent, const personal::FileDef& def,
    const std::vector<CardPersonal::RecordData>& recs) {
    QString text = QString("EF %1").arg(QString::fromStdString(def.name));
    if (!def.title.empty()) text += "  " + QString::fromStdString(def.title);
    text += "\n";
    if (recs.empty()) text += "\n(卡片內沒有資料)\n";

    for (const auto& r : recs) {
        const personal::Record* dr = nullptr;
        for (const auto& d : def.records)
            if (d.rdId == r.rdId) dr = &d;
        if (!dr && r.rdId < 0 && !def.records.empty()) dr = &def.records[0];

        text += r.rdId >= 0
            ? QString("\n[RD ID %1]  %2 bytes\n").arg(QString::fromStdString(toHex(Bytes{ (uint8_t)r.rdId }))).arg(r.data.size())
            : QString("\n[原始資料]  %1 bytes\n").arg(r.data.size());

        // 欄位長度都是固定的、而且總長度吻合,才依欄位切開顯示
        bool canSplit = dr != nullptr;
        size_t total = 0;
        if (dr) for (const auto& f : dr->fields) { canSplit = canSplit && f.minLen == f.maxLen; total += f.minLen; }

        if (canSplit && total == r.data.size()) {
            size_t pos = 0;
            for (const auto& f : dr->fields) {
                const Bytes part(r.data.begin() + pos, r.data.begin() + pos + f.minLen);
                pos += f.minLen;
                text += QString("  %1 (%2, %3):  %4").arg(QString::fromStdString(f.name), QString::fromStdString(f.type),
                    lenText(f), QString::fromStdString(toHex(part)));
                text += "\n";
            }
        }
        else {
            text += "  " + QString::fromStdString(toHex(r.data)) + "\n";
            text += dr ? "  (長度與欄位定義不符,以原始資料顯示)\n" : "  (未定義的 RD ID)\n";
        }
    }

    QDialog dlg(parent);
    dlg.setWindowTitle(QString("查看個人化資料 %1").arg(QString::fromStdString(def.name)));
    auto* lay = new QVBoxLayout(&dlg);
    auto* view = new QPlainTextEdit(text);
    view->setReadOnly(true);
    view->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    lay->addWidget(view, 1);
    auto* close = new QPushButton("關閉");
    QObject::connect(close, &QPushButton::clicked, &dlg, &QDialog::accept);
    lay->addWidget(close, 0, Qt::AlignRight);
    dlg.resize(uiscale::fit(QSize(uiscale::u(40), uiscale::u(32.5)), 0.9));
    dlg.exec();
}