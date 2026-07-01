#pragma once

#include "CardUtils.h"
#include <QWidget>
#include <winscard.h>

class QLineEdit;
class QPushButton;
class QTextEdit;
class QLabel;
class QComboBox;
class QTabWidget;
class QGroupBox;
class QVBoxLayout;

// ─────────────────────────────────────────────────────────────────────────────
// ApduFields — 單組 APDU 欄位組合 (CLA INS P1 P2 Lc Le DataIn)
// ─────────────────────────────────────────────────────────────────────────────
struct ApduFields {
    QLineEdit* cla = nullptr;
    QLineEdit* ins = nullptr;
    QLineEdit* p1 = nullptr;
    QLineEdit* p2 = nullptr;
    QLineEdit* lc = nullptr;
    QLineEdit* le = nullptr;
    QLineEdit* dataIn = nullptr;

    QString buildApdu() const;
};

// ─────────────────────────────────────────────────────────────────────────────
// CreateFileFields — Create File DataIn 的 16 個子欄位
//
//  DataIn 結構 (16 bytes = 32 HEX 字元)：
//  #1~#2   EFID / DFID    (2 bytes)
//  #3~#4   AC1            (2 bytes)
//  #5~#6   AC2            (2 bytes)
//  #7~#8   AC3            (2 bytes)
//  #9~#10  SoB 檔案空間    (2 bytes)
//  #11     ToF 檔案類型    (1 byte)
//  #12     SoR 資料錄長度  (1 byte)
//  #13     RC  錯誤限制次數(1 byte)
//  #14     Index 基碼認證  (1 byte)
//  #15     ToK 基碼類別    (1 byte)
//  #16     RFU 保留        (1 byte)
// ─────────────────────────────────────────────────────────────────────────────
struct CreateFileFields {
    // Header
    QLineEdit* cla = nullptr;
    QLineEdit* ins = nullptr;
    QLineEdit* p1 = nullptr;
    QLineEdit* p2 = nullptr;
    QLineEdit* lcDisplay = nullptr;  // 唯讀，自動計算

    // DataIn 子欄位
    QLineEdit* efid = nullptr;   // #1~#2  2 bytes
    QLineEdit* ac1 = nullptr;   // #3~#4  2 bytes
    QLineEdit* ac2 = nullptr;   // #5~#6  2 bytes
    QLineEdit* ac3 = nullptr;   // #7~#8  2 bytes
    QLineEdit* sob = nullptr;   // #9~#10 2 bytes
    QLineEdit* tof = nullptr;   // #11    1 byte
    QLineEdit* sor = nullptr;   // #12    1 byte
    QLineEdit* rc = nullptr;   // #13    1 byte
    QLineEdit* index = nullptr;   // #14    1 byte
    QLineEdit* tok = nullptr;   // #15    1 byte
    QLineEdit* rfu = nullptr;   // #16    1 byte

    // 組成完整 APDU（Lc 自動計算 = 16 = 0x10）
    QString buildApdu() const;

    // 組成 DataIn HEX（32字元）
    QString buildDataIn() const;
};

// ─────────────────────────────────────────────────────────────────────────────
// PageCardOperation — 主操作頁
//   Tab1：ISAM 金鑰管理
//   Tab2：LOD 資料 / 母卡製作
//   Tab3：卡片指令（Create File / Select File / Write Record / List File）
// ─────────────────────────────────────────────────────────────────────────────
class PageCardOperation : public QWidget
{
    Q_OBJECT

public:
    PageCardOperation(SCARDHANDLE isamHandle, SCARDHANDLE masterHandle, QWidget* parent = nullptr);

signals:
    void backRequested();

private slots:
    void onIsamKeySelected(int index);
    void onUpdateIsam();
    void onLoadFile();
    void onMakeMaster();
    void onAuthenticate();
    void onClearFields();
    void onCreateFile();
    void onSelectFile();
    void onWriteRecord();
    void onListFile();
    void onUpdateCreateLc();  // 子欄位變動時自動更新 Lc

private:
    void       setupUI();
    void       setupIsamTab(QWidget* tab);
    void       setupLodTab(QWidget* tab);
    void       setupApduTab(QWidget* tab);
    void       setupCreateFileGroup(QWidget* parent, QVBoxLayout* layout);
    QGroupBox* makeApduGroup(QWidget* parent,
        const QString& title,
        const QString& defaultCla,
        const QString& defaultIns,
        const QString& defaultP1,
        const QString& defaultP2,
        bool           hasDataIn,
        bool           hasLe,
        ApduFields& out);
    QLineEdit* makeHexField(QWidget* parent, int maxLen, const QString& placeholder = {});
    QLineEdit* makeEditField(QWidget* parent, bool readOnly = false);
    SCARDHANDLE  getTargetHandle() const;
    void       displayData(const CustomerData& d);
    void       log(const QString& msg, bool isError = false);
    bool       sendApduFromFields(SCARDHANDLE hCard,
        const ApduFields& fields,
        const QString& label);
    CustomerData collectDataFromFields() const;

    SCARDHANDLE  m_isamHandle;
    SCARDHANDLE  m_masterHandle;
    CustomerData m_data;
    bool         m_loaded = false;
    bool         m_authenticated = false;   // 母卡認證狀態

    // ── ISAM Tab ──
    QComboBox* m_isamKeyCombo = nullptr;
    QLabel* m_isamC3CodeLbl = nullptr;   // 只顯示 Code，不顯示 Key
    QLabel* m_isamC1CodeLbl = nullptr;
    QPushButton* m_updateIsamBtn = nullptr;

    // ── LOD Tab ──
    QPushButton* m_loadBtn = nullptr;
    QPushButton* m_makeMasterBtn = nullptr;
    QPushButton* m_authBtn = nullptr;
    QPushButton* m_clearBtn = nullptr;
    QLabel* m_fileLabel = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QLineEdit* m_c3Edit = nullptr;
    QLineEdit* m_c3CodeEdit = nullptr;
    QLineEdit* m_c1Edit = nullptr;
    QLineEdit* m_c1CodeEdit = nullptr;
    QLineEdit* m_codeAEdit = nullptr;
    QLineEdit* m_codeBEdit = nullptr;
    QLineEdit* m_exchEdit = nullptr;
    QLineEdit* m_pinAEdit = nullptr;
    QLineEdit* m_pinBEdit = nullptr;

    // ── APDU Tab ──
    CreateFileFields m_createFields;
    ApduFields       m_selectFields;
    ApduFields       m_writeFields;

    // ── 日誌 ──
    QTextEdit* m_logEdit = nullptr;
};
