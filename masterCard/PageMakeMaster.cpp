#include "PageMakeMaster.h"
#include "ReaderBar.h"
#include "CustomerDataForm.h"
#include "UiHelpers.h"
#include "MasterCardUpdater.h"
#include "LodLoader.h"
#include <QGroupBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>

PageMakeMaster::PageMakeMaster(ReaderManager* manager, QWidget* parent)
    : FlowPageBase(manager, parent)
{
    auto* layout = createContentLayout();

    // ── 讀卡機選擇：ISAM / 母卡（兩台不同）──────────────────────────────────
    auto* readerGrp = new QGroupBox("讀卡機選擇", this);
    auto* readerForm = new QFormLayout(readerGrp);
    m_isamBar = new ReaderBar(manager, 0, readerGrp);
    m_masterBar = new ReaderBar(manager, 1, readerGrp);
    readerForm->addRow("🔑 ISAM 讀卡機：", m_isamBar);
    readerForm->addRow("💳 母卡 讀卡機：", m_masterBar);
    layout->addWidget(readerGrp);

    // ── 操作按鈕 ─────────────────────────────────────────────────────────────
    auto* btnRow = new QHBoxLayout();
    m_loadBtn = new QPushButton("📂 載入 LOD", this);
    m_loadBtn->setStyleSheet("QPushButton { background:#4CAF50; color:white; padding:8px 16px; font-weight:bold; border-radius:4px; }");
    m_clearBtn = new QPushButton("🗑 清除", this);
    m_clearBtn->setStyleSheet("QPushButton { background:#9E9E9E; color:white; padding:8px 12px; font-weight:bold; border-radius:4px; }");
    m_makeBtn = UiHelpers::makeActionButton("🔮 製作母卡", "#0288D1", this);
    m_makeBtn->setToolTip("一次完成：ISAM 寫入 Exchange Key → 母卡認證 → 寫入母卡資料（C8/C9/0001/0002）");
    m_makeBtn->setEnabled(false);
    m_fileLabel = new QLabel("尚未載入 — 或直接手動輸入", this);
    m_fileLabel->setStyleSheet("color:#888; font-style:italic;");

    btnRow->addWidget(m_loadBtn);
    btnRow->addWidget(m_clearBtn);
    btnRow->addWidget(m_makeBtn);
    btnRow->addWidget(m_fileLabel, 1);
    layout->addLayout(btnRow);

    // ── 客戶資料欄位（全部欄位）─────────────────────────────────────────────
    m_form = new CustomerDataForm(CustomerDataForm::AllFields, this);
    layout->addWidget(m_form);
    layout->addStretch();

    connect(m_loadBtn, &QPushButton::clicked, this, &PageMakeMaster::onLoadFile);
    connect(m_clearBtn, &QPushButton::clicked, this, &PageMakeMaster::onClearFields);
    connect(m_makeBtn, &QPushButton::clicked, this, &PageMakeMaster::onMakeMaster);
    connect(m_form, &CustomerDataForm::changed, this, &PageMakeMaster::onFieldsChanged);
    connect(m_form, &CustomerDataForm::warning, this, [this](const QString& m) { log("⚠️ " + m, true); });
}

void PageMakeMaster::onLoadFile()
{
    QString path = QFileDialog::getOpenFileName(
        this, "選擇客戶資料檔案", {},
        "LOD 檔案 (*.lod *.LOD);;所有檔案 (*.*)");
    if (path.isEmpty()) return;

    QStringList warnings;
    CustomerData tmp;
    if (!LodLoader::load(path, tmp, &warnings)) {
        QMessageBox::critical(this, "載入失敗", "無法解析 LOD 檔案（至少需包含 C3 與 C1）。");
        return;
    }

    m_form->setData(tmp);
    m_fileLabel->setText("✔ " + path.section('/', -1));
    m_fileLabel->setStyleSheet("color:#2E7D32; font-weight:bold;");
    log("✔ LOD 載入：" + tmp.name);
    for (const auto& w : warnings) log("⚠️ " + w, true);
}

void PageMakeMaster::onClearFields()
{
    m_form->clear();
    m_fileLabel->setText("尚未載入 — 或直接手動輸入");
    m_fileLabel->setStyleSheet("color:#888; font-style:italic;");
    m_makeBtn->setEnabled(false);
    log("欄位已清除。");
}

// 不論是載入 LOD 或手動輸入，只要 C3 / C1 都有值，即可啟用「製作母卡」按鈕
void PageMakeMaster::onFieldsChanged()
{
    m_makeBtn->setEnabled(m_form->hasC3AndC1());
}

void PageMakeMaster::onMakeMaster()
{
    SCARDHANDLE hIsam = 0, hMaster = 0;
    if (!acquireHandles(m_isamBar, m_masterBar, "母卡", hIsam, hMaster))
        return;

    CustomerData d = m_form->data();
    if (d.c3.isEmpty() || d.c1.isEmpty()) {
        QMessageBox::warning(this, "資料不足", "至少需要填入 C3 / C1 金鑰！");
        return;
    }
    if (d.exchangeCode.isEmpty() || d.ccode.isEmpty()) {
        QMessageBox::warning(this, "資料不足", "請確認 Exchange Code 與 CCode 皆已填寫！\n（Code A、Code B 填好後會自動計算）");
        return;
    }

    // 一次完成：ISAM 寫入 Exchange Key → 母卡認證 → 寫入 C8/C9/0001/0002
    log("====== 開始製作母卡 ======");
    MasterCardUpdater mc(hMaster, hIsam);
    bool ok = mc.makeMasterCard(d, logFn());

    if (ok) QMessageBox::information(this, "完成", "🎉 母卡製作完成！");
    else    QMessageBox::critical(this, "失敗", "❌ 製作母卡失敗，請查看日誌。");
}
