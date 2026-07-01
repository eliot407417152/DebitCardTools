#pragma once

#include <QWidget>
#include "CardUtils.h"
#include "LodLoader.h"
#include "OperationMode.h"

class QLineEdit;
class QPushButton;
class QTextEdit;
class QLabel;
class QComboBox;
class QGroupBox;
class QVBoxLayout;

// ─────────────────────────────────────────────────────────────────────────────
// ApduFields — Select File / Write Record 共用的通用 APDU 欄位
// ─────────────────────────────────────────────────────────────────────────────
struct ApduFields
{
    QLineEdit* cla = nullptr;
    QLineEdit* ins = nullptr;
    QLineEdit* p1 = nullptr;
    QLineEdit* p2 = nullptr;
    QLineEdit* lc = nullptr;
    QLineEdit* le = nullptr;      // 可能為 nullptr（不需要 Le）
    QLineEdit* dataIn = nullptr;      // 可能為 nullptr（不需要 DataIn）

    QString buildApdu() const;
};

// ─────────────────────────────────────────────────────────────────────────────
// CreateFileFields — Create File 專用（固定 16 bytes DataIn）
// ─────────────────────────────────────────────────────────────────────────────
struct CreateFileFields
{
    QLineEdit* cla = nullptr;
    QLineEdit* ins = nullptr;
    QLineEdit* p1 = nullptr;
    QLineEdit* p2 = nullptr;
    QLineEdit* lcDisplay = nullptr;   // 唯讀，依 DataIn 長度自動計算

    QLineEdit* efid = nullptr;
    QLineEdit* ac1 = nullptr;
    QLineEdit* ac2 = nullptr;
    QLineEdit* ac3 = nullptr;
    QLineEdit* sob = nullptr;
    QLineEdit* tof = nullptr;
    QLineEdit* sor = nullptr;
    QLineEdit* rc = nullptr;
    QLineEdit* index = nullptr;
    QLineEdit* tok = nullptr;
    QLineEdit* rfu = nullptr;

    QString buildDataIn() const;
    QString buildApdu() const;
};

// ─────────────────────────────────────────────────────────────────────────────
// VerifyPinFields — Verify Pin 專用
//   CLA:00 INS:20 P1:00 P2:Pq（輸入後自動邏輯左移 3 位） Lc:08 DATA:PIN
// ─────────────────────────────────────────────────────────────────────────────
struct VerifyPinFields
{
    QLineEdit* cla = nullptr;
    QLineEdit* ins = nullptr;
    QLineEdit* p1 = nullptr;
    QLineEdit* pq = nullptr;   // 使用者輸入的 Pq（原始值，未位移）
    QLineEdit* p2Display = nullptr;   // 唯讀，顯示 Pq << 3 後的 P2
    QLineEdit* lc = nullptr;   // 唯讀，固定 08
    QLineEdit* pin = nullptr;   // DATA：PIN code（8 bytes / 16 HEX）

    QString computedP2() const;       // 計算 Pq 邏輯左移 3 位後的結果
    QString buildApdu() const;
};

// ─────────────────────────────────────────────────────────────────────────────
// ReadBinaryFields — Read Binary / Read All Binary 共用
//   Read Binary     : CLA:00 INS:B2 P1:RecN P2:04 Le:L（預設 00）
//   Read All Binary : CLA:00 INS:B2 P1:RecN P2:05 Le:L（預設 00 00 00）
//   （兩者以 le 欄位的 maxLength 決定 Le 要補齊/截斷成幾個 HEX 字元）
// ─────────────────────────────────────────────────────────────────────────────
struct ReadBinaryFields
{
    QLineEdit* cla = nullptr;
    QLineEdit* ins = nullptr;
    QLineEdit* recN = nullptr;   // P1，Record Id，自行輸入
    QLineEdit* p2 = nullptr;   // 唯讀：04（Read Binary）或 05（Read All Binary）
    QLineEdit* le = nullptr;   // Le

    QString buildApdu() const;
};

class PageCardOperation : public QWidget
{
    Q_OBJECT

public:
    PageCardOperation(SCARDHANDLE isamHandle, SCARDHANDLE masterHandle,
        OperationMode mode, QWidget* parent = nullptr);

signals:
    void backRequested();

private slots:
    // ISAM Tab
    void onIsamKeySelected(int index);
    void onUpdateIsam();

    // LOD Tab
    void onLoadFile();
    void onAuthenticate();
    void onMakeMaster();
    void onClearFields();

    // APDU Tab
    void onCreateFile();
    void onUpdateCreateLc();
    void onSelectFile();
    void onWriteRecord();
    void onListFile();
    void onVerifyPin();
    void onUpdateVerifyPinP2();
    void onReadBinary();
    void onReadAllBinary();

private:
    void setupUI();
    void setupIsamTab(QWidget* tab);
    void setupLodTab(QWidget* tab);
    void setupApduTab(QWidget* tab);
    void setupCreateFileGroup(QWidget* parent, QVBoxLayout* layout);

    QGroupBox* makeApduGroup(QWidget* parent,
        const QString& title,
        const QString& defaultCla,
        const QString& defaultIns,
        const QString& defaultP1,
        const QString& defaultP2,
        bool hasDataIn,
        bool hasLe,
        ApduFields& out);

    QLineEdit* makeHexField(QWidget* parent, int maxLen, const QString& placeholder = QString());
    QLineEdit* makeEditField(QWidget* parent, bool readOnly);

    SCARDHANDLE getTargetHandle() const;
    bool sendApduFromFields(SCARDHANDLE hCard, const ApduFields& fields, const QString& label);

    void displayData(const CustomerData& d);
    CustomerData collectDataFromFields() const;

    void log(const QString& msg, bool isError = false);

private:
    SCARDHANDLE   m_isamHandle = 0;
    SCARDHANDLE   m_masterHandle = 0;
    OperationMode m_mode = OperationMode::MasterCardProduction;

    // ISAM Tab
    QComboBox* m_isamKeyCombo = nullptr;
    QLabel* m_isamC3CodeLbl = nullptr;
    QLabel* m_isamC1CodeLbl = nullptr;
    QPushButton* m_updateIsamBtn = nullptr;

    // LOD Tab
    QPushButton* m_loadBtn = nullptr;
    QPushButton* m_clearBtn = nullptr;
    QPushButton* m_makeMasterBtn = nullptr;
    QPushButton* m_authBtn = nullptr;
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

    CustomerData m_data;
    bool m_loaded = false;
    bool m_authenticated = false;

    // APDU Tab
    CreateFileFields  m_createFields;
    ApduFields        m_selectFields;
    ApduFields        m_writeFields;
    VerifyPinFields   m_verifyPinFields;
    ReadBinaryFields  m_readBinaryFields;
    ReadBinaryFields  m_readAllBinaryFields;

    QTextEdit* m_logEdit = nullptr;
};
