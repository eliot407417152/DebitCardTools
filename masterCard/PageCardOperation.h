#pragma once

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QMap>
#include <winscard.h>
#include "CardUtils.h"
#include "IsamKeyChecker.h"   // CustomerData / KeyCheckResult
#include "CardWasher.h"       // 洗卡
#include "CardIssuer.h"       // 發卡

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

// 卡片指令 — Terminal Authentication 專用：P2 由 Kq 邏輯左移 3 位自動運算（比照 Verify Pin），
// 但 DATA 為 16 bytes（32 HEX），Lc 隨之固定為 10h
struct TerminalAuthFields
{
    QLineEdit* cla = nullptr;
    QLineEdit* ins = nullptr;
    QLineEdit* p1 = nullptr;
    QLineEdit* pq = nullptr;
    QLineEdit* p2Display = nullptr;
    QLineEdit* lc = nullptr;
    QLineEdit* pin = nullptr;   // DATA（16 bytes / 32 HEX）

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

// ISAM 指令頁 — Set Session Key（Div-Data 可變長度 + R2 固定 8 bytes，Lc = DL + 8）
struct SetSessionKeyIsamFields
{
    QLineEdit* cla = nullptr;
    QLineEdit* ins = nullptr;
    QLineEdit* p1 = nullptr;      // KID
    QLineEdit* p2 = nullptr;      // 00
    QLineEdit* lcDisplay = nullptr;
    QLineEdit* le = nullptr;      // --（固定無 Le，僅顯示用）

    QLineEdit* divData = nullptr; // Div-Data（多樣化資料，可變長度）
    QLineEdit* r2 = nullptr;      // R2（卡片指令 Set Session Key 回應之 R2，8 bytes）
    QLineEdit* dataDisplay = nullptr; // DATA = Div-Data + R2（自動組合、唯讀顯示）

    QString buildDataIn() const;
    QString buildApdu() const;
};

// ISAM 指令頁 — Get Diversify Key with Session Key（Lc = DL，Le 固定 00）
struct DiversifyKeyFields
{
    QLineEdit* cla = nullptr;
    QLineEdit* ins = nullptr;
    QLineEdit* p1 = nullptr;      // KID（WK 或 SK）
    QLineEdit* p2 = nullptr;      // 00
    QLineEdit* lcDisplay = nullptr;
    QLineEdit* le = nullptr;      // 00（固定）

    QLineEdit* divData = nullptr; // Div-Data（多樣化資料，可變長度）

    QString buildApdu() const;
};

// 卡片指令 — Set Session Key（P2 由 Kq 邏輯左移三位運算，比照 Verify Pin 作法）
struct SetSessionKeyCardFields
{
    QLineEdit* cla = nullptr;
    QLineEdit* ins = nullptr;
    QLineEdit* p1 = nullptr;
    QLineEdit* pq = nullptr;
    QLineEdit* p2Display = nullptr;
    QLineEdit* lc = nullptr;
    QLineEdit* r1 = nullptr;
    QLineEdit* le = nullptr;

    QString computedP2() const;
    QString buildApdu() const;
};

// 卡片指令 — Update Record Ciphered (Write Record Ciphered)
//   Lc 依 DATA 長度自動計算（08/10/18），DATA 為自行輸入
struct UpdateRecordCipheredFields
{
    QLineEdit* cla = nullptr;
    QLineEdit* ins = nullptr;
    QLineEdit* p1 = nullptr;
    QLineEdit* p2 = nullptr;
    QLineEdit* lcDisplay = nullptr;

    QLineEdit* data = nullptr;   // DATA（自行輸入）

    QString buildApdu() const;
};

// 卡片指令 — Card Authentication
//   P2 為自行輸入的 Kq（不做左移運算），DATA = R1 + R3（各 8 bytes），Lc/Le 固定 10h
// 卡片指令 — Card Authentication
//   P2 由 Kq 邏輯左移 3 位運算（比照 Set Session Key 作法），DATA = R1 + R3（各 8 bytes），Lc/Le 固定 10h
//   回應（DataOut，16 bytes）= [MAC 8 bytes][R2 8 bytes]
//   驗證用的 Key 由使用者於 manualKey 欄位自行輸入；若該欄位填寫則直接使用該值，
//   不再自動向 ISAM 索取（僅在 manualKey 留空時，才會向 ISAM 送 Get Key Check Code, P1=25 取得 Key），
//   計算 DES(DES(DES(Key,R1),R2),R3) 並與回應的 MAC（前 8 bytes）比對
struct CardAuthFields
{
    QLineEdit* cla = nullptr;
    QLineEdit* ins = nullptr;
    QLineEdit* p1 = nullptr;
    QLineEdit* pq = nullptr;        // Kq 原始值（十進位輸入）
    QLineEdit* p2Display = nullptr; // P2 = Kq 左移 3 位（自動計算、唯讀）
    QLineEdit* lc = nullptr;
    QLineEdit* le = nullptr;

    QLineEdit* r1 = nullptr;    // R1（8 bytes，自行輸入）
    QLineEdit* r3 = nullptr;    // R3（8 bytes，自行輸入）
    QLineEdit* manualKey = nullptr;   // Key（自行輸入；若填寫則直接使用，不再向 ISAM 索取）

    QString computedP2() const;
    QString buildApdu() const;
};

// 卡片指令 — Write Record With TAC
//   P2 為自行輸入的 Kq，DATA 依 P2 而長度不同，皆為自行輸入
//   Lc 依 DATA 實際長度自動計算，Le 固定 08
struct WriteRecordTacFields
{
    QLineEdit* cla = nullptr;
    QLineEdit* ins = nullptr;
    QLineEdit* p1 = nullptr;
    QLineEdit* p2 = nullptr;     // Kq（自行輸入）
    QLineEdit* lcDisplay = nullptr;
    QLineEdit* le = nullptr;

    QLineEdit* data = nullptr;   // DATA（自行輸入，長度依 P2 而定）

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

private slots:
    // ISAM 金鑰 Tab
    void onIsamKeySelected(int index);
    void onViewIsam();
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
    void onUpdateSetSessionKeyLc();
    void onIsamSetSessionKey();
    void onUpdateDiversifyKeyLc();
    void onIsamGetDiversifyKey();
    void onIsamGetWorkingKey();

    // 製卡發卡流程 Tab
    void onLoadFile();
    void onClearFields();
    void onAuthenticate();
    void onMakeMaster();
    void onConvertMaster();
    void onCustomerFieldChanged();
    void onWashCard();
    void onIssueCard();

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
    void onUpdateTerminalAuthP2();
    void onTerminalAuthentication();
    void onUpdateSessionKeyCardP2();
    void onSetSessionKeyCard();
    void onUpdateRecordCipheredLc();
    void onUpdateRecordCiphered();
    void onReadBinary();
    void onGenerateRandomCard();
    void onCardAuthentication();
    void onUpdateCardAuthP2();
    void onWriteRecordWithTacLcUpdate();
    void onWriteRecordWithTac();
    void onExportLog();

private:
    void setupUI();
    void setupIsamTab(QWidget* tab);
    void setupIsamCmdTab(QWidget* tab);
    void setupLodTab(QWidget* tab);
    void setupApduTab(QWidget* tab);
    QGroupBox* setupCreateFileGroup(QWidget* parent, QVBoxLayout* layout);

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
    QPushButton* m_viewIsamBtn = nullptr;
    QLabel* m_isamViewC3Lbl = nullptr;   // 查看 ISAM：讀取自卡片的 C3 對照碼
    QLabel* m_isamViewC1Lbl = nullptr;   // 查看 ISAM：讀取自卡片的 C1 對照碼
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
    SetSessionKeyIsamFields m_setSessionKeyIsamFields;
    DiversifyKeyFields m_diversifyKeyFields;
    IsamSimpleFields m_workingKeyFields;

    // ── 製卡發卡流程 Tab ────────────────────────────────────────────────────
    QComboBox* m_masterIsamReaderCombo = nullptr;   // 製作母卡：ISAM 讀卡機
    QComboBox* m_masterCardReaderCombo = nullptr;   // 製作母卡：母卡 讀卡機
    QPushButton* m_loadBtn = nullptr;
    QPushButton* m_clearBtn = nullptr;
    QPushButton* m_makeMasterBtn = nullptr;
    QPushButton* m_authBtn = nullptr;
    QPushButton* m_convertBtn = nullptr;   // 母卡轉換
    QPushButton* m_washCardBtn = nullptr;  // 洗卡
    QPushButton* m_issueCardBtn = nullptr; // 發卡
    QLineEdit* m_issueC2DataEdit = nullptr; // 發卡：00C2 Write Record 自行輸入 PIN 碼（HEX，DataIn 會自動組成 01+長度+PIN）
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
    TerminalAuthFields m_terminalAuthFields;   // Terminal Authentication（P1固定/Kq左移3位/Lc自動/Data=16bytes）
    SetSessionKeyCardFields m_setSessionKeyCardFields;
    UpdateRecordCipheredFields m_updateRecordCipheredFields;
    IsamSimpleFields m_readBinaryFields;   // Read Binary：CLA/INS/P1 固定，P2/Le 自行輸入
    IsamSimpleFields m_generateRandomCardFields;   // Generate Random Number：CLA/INS/P1/P2/Le 固定
    CardAuthFields m_cardAuthFields;   // Card Authentication
    QComboBox* m_cardAuthIsamReaderCombo = nullptr;   // Card Authentication：取得 Key 用的 ISAM 讀卡機
    WriteRecordTacFields m_writeRecordTacFields;   // Write Record With TAC

    // ── 共用 ────────────────────────────────────────────────────────────────
    QTextEdit* m_logEdit = nullptr;
    QPushButton* m_exportLogBtn = nullptr;
};
