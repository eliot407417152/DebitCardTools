#pragma once

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QMap>
#include <winscard.h>
#include "CardUtils.h"
#include "IsamKeyChecker.h"   // CustomerData / KeyCheckResult

class QLineEdit;
class QPushButton;
class QTextEdit;
class QLabel;
class QComboBox;
class QVBoxLayout;
class QGroupBox;

// ─────────────────────────────────────────────────────────────────────────────
// 各功能區塊的欄位集合（維持原本邏輯不變）
// ─────────────────────────────────────────────────────────────────────────────
struct ApduFields
{
    QLineEdit* cla = nullptr;
    QLineEdit* ins = nullptr;
    QLineEdit* p1 = nullptr;
    QLineEdit* p2 = nullptr;
    QLineEdit* lc = nullptr;
    QLineEdit* dataIn = nullptr;
    QLineEdit* le = nullptr;

    QString buildApdu() const;
};

// CLA INS P1 P2 (+ 可選 Le)，無 Lc、無 DataIn（ISAM 指令頁的簡易指令使用）
struct IsamSimpleFields
{
    QLineEdit* cla = nullptr;
    QLineEdit* ins = nullptr;
    QLineEdit* p1 = nullptr;
    QLineEdit* p2 = nullptr;
    QLineEdit* le = nullptr;

    QString buildApdu() const;
};

// Create File 專用：16 bytes DataIn 由多個子欄位組成
struct CreateFileFields
{
    QLineEdit* cla = nullptr;
    QLineEdit* ins = nullptr;
    QLineEdit* p1 = nullptr;
    QLineEdit* p2 = nullptr;
    QLineEdit* lcDisplay = nullptr;

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

// Verify Pin 專用：P2 由 Pq 邏輯左移 3 位自動運算
struct VerifyPinFields
{
    QLineEdit* cla = nullptr;
    QLineEdit* ins = nullptr;
    QLineEdit* p1 = nullptr;
    QLineEdit* pq = nullptr;
    QLineEdit* p2Display = nullptr;
    QLineEdit* lc = nullptr;
    QLineEdit* pin = nullptr;

    QString computedP2() const;
    QString buildApdu() const;
};

// Read Record / Read All Record 共用
struct ReadRecordFields
{
    QLineEdit* cla = nullptr;
    QLineEdit* ins = nullptr;
    QLineEdit* recN = nullptr;
    QLineEdit* p2 = nullptr;
    QLineEdit* le = nullptr;

    QString buildApdu() const;
};

// ISAM 指令頁 — Write Key
struct WriteKeyFields
{
    QLineEdit* cla = nullptr;
    QLineEdit* ins = nullptr;
    QLineEdit* p1 = nullptr;
    QLineEdit* p2 = nullptr;
    QLineEdit* lcDisplay = nullptr;

    QLineEdit* cdKey = nullptr;
    QLineEdit* exchangeKey = nullptr;
    QLineEdit* eKey = nullptr;
    QLineEdit* ccode = nullptr;
    QLineEdit* limit = nullptr;

    QString buildDataIn() const;
    QString buildApdu() const;
};

// ISAM 指令頁 — Store Manufacturer Key
struct StoreManufacturerKeyFields
{
    QLineEdit* cla = nullptr;
    QLineEdit* ins = nullptr;
    QLineEdit* p1 = nullptr;
    QLineEdit* p2 = nullptr;
    QLineEdit* lcDisplay = nullptr;

    QLineEdit* kt = nullptr;
    QLineEdit* eKey = nullptr;
    QLineEdit* ccode = nullptr;
    QLineEdit* limit = nullptr;

    QString buildDataIn() const;
    QString buildApdu() const;
};

// ─────────────────────────────────────────────────────────────────────────────
// PageCardOperation
//
//【已依需求調整】
//   1. 不再由建構子傳入固定的 ISAM / 母卡 SCARDHANDLE，改傳入
//      SCARDCONTEXT + 偵測到的讀卡機名稱清單 (QStringList)。
//   2. 「ISAM 金鑰」「ISAM 指令」兩個頁籤、以及「製卡發卡流程」頁籤，
//      現在都各自提供「讀卡機選擇」下拉選單（比照原本「卡片指令」頁籤的
//      「發送對象」下拉選單做法），使用者可自行選擇要用哪一台讀卡機。
//      實際連線 (SCardConnect) 改為「使用到某台讀卡機時才連線」，並快取
//      連線代碼，避免重複連線。
//   3. 「製作母卡」需要選擇兩台不同的讀卡機；若偵測到的讀卡機不足兩台，
//      「製卡發卡流程」頁籤會直接停用，避免誤觸。
// ─────────────────────────────────────────────────────────────────────────────
class PageCardOperation : public QWidget
{
    Q_OBJECT

public:
    explicit PageCardOperation(SCARDCONTEXT context,
        const QStringList& readers,
        QWidget* parent = nullptr);
    ~PageCardOperation();

signals:
    void backRequested();

private slots:
    // ISAM 金鑰 Tab
    void onIsamKeySelected(int index);
    void onUpdateIsam();
    void onCheckIsamKey();

    // ISAM 指令 Tab
    void onIsamSelectObject();
    void onIsamGetKeyCheckCode();
    void onIsamGenerateRandom();
    void onIsamGetTerminalAuthCode();
    void onIsamGetEncryptedKey();
    void onUpdateWriteKeyEKey();
    void onUpdateWriteKeyLc();
    void onIsamWriteKey();
    void onUpdateStoreMfgKeyLc();
    void onIsamStoreManufacturerKey();

    // 製卡發卡流程 Tab
    void onLoadFile();
    void onClearFields();
    void onAuthenticate();
    void onMakeMaster();
    void onConvertMaster();
    void onCustomerFieldChanged();

    // 卡片指令 Tab
    void onCreateFile();
    void onUpdateCreateLc();
    void onSelectFile();
    void onWriteRecord();
    void onListFile();
    void onUpdateVerifyPinP2();
    void onVerifyPin();
    void onReadRecord();
    void onReadAllRecord();

private:
    void setupUI();
    void setupIsamTab(QWidget* tab);
    void setupIsamCmdTab(QWidget* tab);
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

    // 讀卡機連線輔助：依讀卡機名稱取得（必要時建立並快取）SCARDHANDLE
    SCARDHANDLE connectReader(const QString& readerName) const;
    // 供「卡片指令」頁籤使用：讀取該頁籤的「發送對象」下拉選單目前選擇的讀卡機
    SCARDHANDLE getTargetHandle() const;

    bool sendApduFromFields(SCARDHANDLE hCard, const ApduFields& fields, const QString& label);

    void displayData(const CustomerData& d);
    CustomerData collectDataFromFields() const;

    void log(const QString& msg, bool isError = false);

    // ── 讀卡機環境 ──────────────────────────────────────────────────────────
    SCARDCONTEXT m_context = 0;
    QStringList  m_readers;
    bool         m_canMakeMaster = false;   // 讀卡機數量 >= 2 才允許製作母卡

    // key: 讀卡機名稱, value: 已連線的 SCARDHANDLE（惰性連線快取）
    mutable QMap<QString, SCARDHANDLE> m_handleCache;

    // ── ISAM 金鑰 Tab ───────────────────────────────────────────────────────
    QComboBox* m_isamKeyReaderCombo = nullptr;   // 本頁使用哪一台讀卡機
    QComboBox* m_isamKeyCombo = nullptr;
    QLabel* m_isamC3CodeLbl = nullptr;
    QLabel* m_isamC1CodeLbl = nullptr;
    QPushButton* m_updateIsamBtn = nullptr;
    QPushButton* m_checkIsamKeyBtn = nullptr;

    // ── ISAM 指令 Tab ───────────────────────────────────────────────────────
    QComboBox* m_isamCmdReaderCombo = nullptr;   // 本頁使用哪一台讀卡機
    ApduFields m_isamSelectObjFields;
    IsamSimpleFields m_isamKeyCheckFields;
    IsamSimpleFields m_isamRandomFields;
    ApduFields m_isamTermAuthFields;
    IsamSimpleFields m_isamEncKeyFields;
    WriteKeyFields m_writeKeyFields;
    StoreManufacturerKeyFields m_storeMfgKeyFields;

    // ── 製卡發卡流程 Tab ────────────────────────────────────────────────────
    QComboBox* m_masterIsamReaderCombo = nullptr;   // 製作母卡：ISAM 讀卡機
    QComboBox* m_masterCardReaderCombo = nullptr;   // 製作母卡：母卡 讀卡機
    QPushButton* m_loadBtn = nullptr;
    QPushButton* m_clearBtn = nullptr;
    QPushButton* m_makeMasterBtn = nullptr;
    QPushButton* m_authBtn = nullptr;
    QPushButton* m_convertBtn = nullptr;   // 母卡轉換
    QLabel* m_fileLabel = nullptr;

    QLineEdit* m_nameEdit = nullptr;
    QLineEdit* m_c3Edit = nullptr;
    QLineEdit* m_c3CodeEdit = nullptr;
    QLineEdit* m_c1Edit = nullptr;
    QLineEdit* m_c1CodeEdit = nullptr;
    QLineEdit* m_codeAEdit = nullptr;
    QLineEdit* m_codeBEdit = nullptr;
    QLineEdit* m_exchEdit = nullptr;
    QLineEdit* m_ccodeEdit = nullptr;       // 母卡轉換用 CCode（手動填入）
    QLineEdit* m_pinAEdit = nullptr;
    QLineEdit* m_pinBEdit = nullptr;

    CustomerData m_data;
    bool m_loaded = false;
    bool m_authenticated = false;

    // ── 卡片指令 Tab ────────────────────────────────────────────────────────
    CreateFileFields m_createFields;
    ApduFields m_selectFields;
    ApduFields m_writeFields;
    VerifyPinFields m_verifyPinFields;
    ReadRecordFields m_readRecordFields;
    ReadRecordFields m_readAllRecordFields;

    // ── 共用 ────────────────────────────────────────────────────────────────
    QTextEdit* m_logEdit = nullptr;
};
