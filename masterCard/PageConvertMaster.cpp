#include "PageConvertMaster.h"
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

PageConvertMaster::PageConvertMaster(ReaderManager* manager, QWidget* parent)
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
    m_convertBtn = UiHelpers::makeActionButton("🔄 母卡轉換", "#EF6C00", this);
    m_convertBtn->setToolTip("母卡 → 銀行 ISAM：Write Key、Verify PIN、0001/0002 金鑰區塊轉換");
    m_fileLabel = new QLabel("尚未載入 — 或直接手動輸入", this);
    m_fileLabel->setStyleSheet("color:#888; font-style:italic;");

    btnRow->addWidget(m_loadBtn);
    btnRow->addWidget(m_clearBtn);
    btnRow->addWidget(m_convertBtn);
    btnRow->addWidget(m_fileLabel, 1);
    layout->addLayout(btnRow);

    // ── 母卡轉換需要的資料：Exchange Code / CCode / Pin A / Pin B ───────────
    m_form = new CustomerDataForm(
        CustomerDataForm::Name | CustomerDataForm::Exchange
        | CustomerDataForm::PinA | CustomerDataForm::PinB, this);
    layout->addWidget(m_form);
    layout->addStretch();

    connect(m_loadBtn, &QPushButton::clicked, this, &PageConvertMaster::onLoadFile);
    connect(m_clearBtn, &QPushButton::clicked, this, &PageConvertMaster::onClearFields);
    connect(m_convertBtn, &QPushButton::clicked, this, &PageConvertMaster::onConvert);
    connect(m_form, &CustomerDataForm::warning, this, [this](const QString& m) { log("⚠️ " + m, true); });
}

void PageConvertMaster::onLoadFile()
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

void PageConvertMaster::onClearFields()
{
    m_form->clear();
    m_fileLabel->setText("尚未載入 — 或直接手動輸入");
    m_fileLabel->setStyleSheet("color:#888; font-style:italic;");
    log("欄位已清除。");
}

void PageConvertMaster::onConvert()
{
    SCARDHANDLE hIsam = 0, hMaster = 0;
    if (!acquireHandles(m_isamBar, m_masterBar, "母卡", hIsam, hMaster))
        return;

    CustomerData d = m_form->data();
    if (d.exchangeCode.isEmpty() || d.ccode.isEmpty()) {
        QMessageBox::warning(this, "資料不足", "請確認 Exchange Code 與 CCode 皆已填寫！");
        return;
    }
    if (d.pinA.isEmpty() || d.pinB.isEmpty()) {
        QMessageBox::warning(this, "資料不足", "請確認 Pin A / Pin B 皆已填寫！");
        return;
    }

    log("====== 開始母卡轉換 ======");
    MasterCardUpdater mc(hMaster, hIsam);
    bool ok = mc.convertToBankIsam(d, logFn());

    if (ok) QMessageBox::information(this, "完成", "🎉 母卡轉換完成！");
    else    QMessageBox::critical(this, "失敗", "❌ 母卡轉換失敗，請查看日誌。");
}
