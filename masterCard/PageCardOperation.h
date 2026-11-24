#pragma once

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QMap>
#include <winscard.h>
#include "CardUtils.h"
#include "IsamKeyChecker.h"   // CustomerData / KeyCheckResult
#include "KeySetLoader.h"     // Card Authentication 用：外部 Key Set 檔案
#include "DataSetLoader.h"    // 外部 Data Set 檔案

class QLineEdit;
class QPushButton;
class QTextEdit;
class QLabel;
class QComboBox;
class QVBoxLayout;
class QGroupBox;
class QDialog;
class QRadioButton;
class ReaderManager;
class ReaderBar;

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

// 卡片指令 — Update Record（INS=D2, P2=04）
//   在已 Select 的基本資料檔內更新一筆既存資料錄（含 PIN/KEY EF）；
//   不得用於 Transaction EF、Special Value EF 及交易授權明細紀錄檔。
//   RecN（P1）= X'01'~X'FE'；Lc = Data 長度（max X'FD' bytes），依 DATA 自動計算
struct UpdateRecordFields
{
    QLineEdit* cla = nullptr;
    QLineEdit* ins = nullptr;
    QLineEdit* recN = nullptr;       // P1：資料錄號碼（自行輸入）
    QLineEdit* p2 = nullptr;         // 固定 04
    QLineEdit* lcDisplay = nullptr;  // Lc（唯讀，自動計算）

    QLineEdit* data = nullptr;       // DATA（自行輸入）

    QString buildApdu() const;
};

// 卡片指令 — Raw APDU（自訂指令測試）
//   CLA/INS/P1/P2/Lc/Data/Le 全部由使用者自行輸入，程式不自動計算、不補值、不改值，
//   Lc/Data/Le 留空則該欄位不組入 APDU。
struct RawApduFields
{
    QLineEdit* cla = nullptr;
    QLineEdit* ins = nullptr;
    QLineEdit* p1 = nullptr;
    QLineEdit* p2 = nullptr;
    QLineEdit* lc = nullptr;
    QLineEdit* data = nullptr;
    QLineEdit* le = nullptr;
    QLabel* preview = nullptr;   // 即時顯示組出來的完整 APDU

    QString buildApdu() const;
};

// 卡片指令 — Card Authentication
//   P2 由 Kq 邏輯左移 3 位運算（比照 Set Session Key 作法），DATA = R1 + R3（各 8 bytes），Lc/Le 固定 10h
//   回應（DataOut，16 bytes）= [MAC 8 bytes][R2 8 bytes]
//   驗證用的 Key（K，卡片認證基碼）改由「Key Set」選取：需先載入外部 Key Set 檔案
//   （格式見 KeySetLoader.h），執行前於下拉選單選擇要用的那一筆，直接使用該筆的金鑰本體，
//   不再向 ISAM 查詢或自行輸入。
//   計算 R = E(DES)(E(DES)(E(DES/3DES)(K,R1),R2),R3) 並與回應的 MAC（前 8 bytes）比對
//   K 可能是 8/16/24 bytes（單/雙/三長度金鑰），第一步依長度自動判斷單一 DES 或 3DES，
//   第二、三步固定為單一 DES（S 與中間結果恆為 8 bytes）
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

// 卡片指令 — Write Record With S/N TAC
//   CLA 固定 00 / INS 固定 E6，P1、P2、Lc、DATA 皆自行輸入，Le 固定 00
struct WriteRecordWSNumTacFields
{
    QLineEdit* cla = nullptr;
    QLineEdit* ins = nullptr;
    QLineEdit* p1 = nullptr;     // 自行輸入
    QLineEdit* p2 = nullptr;     // 自行輸入
    QLineEdit* lc = nullptr;     // 自行輸入
    QLineEdit* le = nullptr;

    QLineEdit* data = nullptr;   // DATA（自行輸入）

    QString buildApdu() const;
};

// ─────────────────────────────────────────────────────────────────────────────
// 加密工具 Tab — DES / 3DES 通用計算（ECB / CBC，Key / IV / 資料皆自行輸入）
// 左側指令清單（DES / 3DES）＋右側各自獨立頁面，比照「卡片指令」頁籤的呈現方式
// ─────────────────────────────────────────────────────────────────────────────
struct CryptoToolFields
{
    // ── DES 頁面 ──
    QRadioButton* desModeEcb = nullptr;   // 模式：ECB
    QRadioButton* desModeCbc = nullptr;   // 模式：CBC
    QLineEdit* desIv = nullptr;      // IV（僅 CBC 模式可輸入，8 bytes / 16 hex）
    QLineEdit* desKey = nullptr;     // Key（8 bytes / 16 hex）
    QLineEdit* desDataIn = nullptr;  // 輸入資料（HEX）
    QLineEdit* desDataOut = nullptr; // 計算結果（HEX，唯讀）

    // ── 3DES 頁面 ──
    QRadioButton* tdesModeEcb = nullptr;  // 模式：ECB
    QRadioButton* tdesModeCbc = nullptr;  // 模式：CBC
    QLineEdit* tdesIv = nullptr;     // IV（僅 CBC 模式可輸入，8 bytes / 16 hex）
    QLineEdit* tdesK1 = nullptr;     // K1（8 bytes / 16 hex）
    QLineEdit* tdesK2 = nullptr;     // K2（8 bytes / 16 hex）
    QLineEdit* tdesK3 = nullptr;     // K3（8 bytes / 16 hex，留空＝雙長度 3DES：K1,K2,K1）
    QLineEdit* tdesDataIn = nullptr;  // 輸入資料（HEX）
    QLineEdit* tdesDataOut = nullptr; // 計算結果（HEX，唯讀）
};

// ─────────────────────────────────────────────────────────────────────────────
// 日誌顏色設定（背景 / 一般文字 / 錯誤文字，皆為 #RRGGBB）
//   預設為亮色（白底、黑字、紅色錯誤）；kDarkPreset 為原本的黑底綠字
// ─────────────────────────────────────────────────────────────────────────────
struct LogColors
{
    QString background = "#FFFFFF";
    QString text = "#000000";
    QString error = "#C62828";

    static LogColors lightPreset() { return LogColors{ "#FFFFFF", "#000000", "#C62828" }; }
    static LogColors darkPreset() { return LogColors{ "#121212", "#00FF66", "#FF3333" }; }
};

// ─────────────────────────────────────────────────────────────────────────────
// PageCardOperation — 操作工作區（程式啟動後直接顯示，不再有「偵測讀卡機」頁面）
//
//   • 讀卡機的偵測 / 連線 / 斷線 / 重新整理 統一交給 ReaderManager，
//     每個需要選讀卡機的地方都放一個 ReaderBar（下拉選單 + 連線 / 斷線 / 重新整理）。
//     重插卡片後按「重新整理」即可重新讀取，不必重開程式。
//   • 頁籤：
//       🔑 ISAM 金鑰 / 🔧 ISAM 指令 / 📡 卡片指令 / 🔐 加密工具 / 🧪 Raw APDU   （本檔 + PageCardOperation_*.cpp）
//       🔮 製作母卡 / 🔄 母卡轉換 / 🧼 洗卡 / 💳 發卡               （各自獨立的頁面類別，
//         見 PageMakeMaster / PageConvertMaster / PageWashCard / PageIssueCard，
//         之後調整任一流程只需要改對應的那一個頁面）
// ─────────────────────────────────────────────────────────────────────────────
class PageCardOperation : public QWidget
{
    Q_OBJECT

public:
    explicit PageCardOperation(QWidget* parent = nullptr);
    ~PageCardOperation();

    // 設定選單「日誌顯示設定」使用：套用日誌字體大小(px)、行距(百分比，100=預設單行高)與顏色
    void applyLogAppearance(int fontSizePx, int lineSpacingPercent, const LogColors& colors);

    // 設定選單「Key Set / Data Set 管理」使用：開啟（或喚起）Key Set / Data Set 管理視窗，
    // 不再放在「卡片指令」頁籤裡，改由選單集中管理，避免混在卡片指令操作中。
    void openKeySetDataSetManager();

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
    void onUpdateRecordLc();
    void onUpdateRecord();
    void onRawApduPreview();
    void onRawApdu();
    void onReadBinary();
    void onGenerateRandomCard();
    void onCardAuthentication();
    void onUpdateCardAuthP2();
    void onLoadKeySet();
    void onKeySetIdApply();
    void onKeySetSelectionChanged(int index);
    void onLoadDataSet();
    void onDataSetSelectionChanged(int index);
    void onWriteRecordWithTacLcUpdate();
    void onWriteRecordWithTac();
    void onWriteRecordWSNumTac();
    void onExportLog();
    void onClearLog();

    // 加密工具 Tab
    void onCryptoDesEncrypt();
    void onCryptoDesDecrypt();
    void onCryptoTdesEncrypt();
    void onCryptoTdesDecrypt();

private:
    void setupUI();
    void setupIsamTab(QWidget* tab);
    void setupIsamCmdTab(QWidget* tab);
    void setupApduTab(QWidget* tab);
    void setupCryptoTab(QWidget* tab);
    void setupRawApduTab(QWidget* tab);
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

    // 讀卡機連線輔助：委派給 ReaderManager（已連線且有效就直接用，失效或未連線則自動（重新）連線）
    SCARDHANDLE connectReader(const QString& readerName) const;
    // 供「卡片指令」頁籤使用：讀取該頁籤的「發送對象」下拉選單目前選擇的讀卡機
    SCARDHANDLE getTargetHandle() const;

    bool sendApduFromFields(SCARDHANDLE hCard, const ApduFields& fields, const QString& label);

    void log(const QString& msg, bool isError = false);

    // 加密工具：DES/3DES 共用計算邏輯（isDes=false 代表 3DES）
    void cryptoCalculate(bool isDes, bool encrypt);

    // 嘗試從程式所在目錄自動載入 KeySet.txt / DataSet.txt（找不到就靜默略過，
    // 使用者仍可透過「載入」按鈕手動指定檔案）
    void autoLoadKeySetAndDataSet();
    void populateKeySetCombo();
    void populateDataSetCombo();
    // 建立「Key Set / Data Set 管理」視窗（僅第一次呼叫 openKeySetDataSetManager() 時建立一次，
    // 之後重複開啟只是顯示同一個視窗，內容/已選取狀態會保留）
    void buildKeySetDataSetDialog();

    // ── 日誌外觀設定 ────────────────────────────────────────────────────────
    int m_logFontSize = 11;            // px
    int m_logLineSpacingPercent = 100; // 100 = 預設單行行高
    LogColors m_logColors;             // 日誌顏色（預設亮色）

    // 日誌內容保留一份，改顏色時可整份重新上色（已顯示的舊訊息也會一起變）
    struct LogEntry { QString text; bool isError; };
    QList<LogEntry> m_logEntries;
    void appendLogEntry(const LogEntry& e);
    QString logStyleSheet() const;

    // ── 讀卡機環境（ISAM 金鑰 / ISAM 指令 / 卡片指令 / 四個流程頁面共用同一個）────
    ReaderManager* m_readerMgr = nullptr;

    // ── ISAM 金鑰 Tab ───────────────────────────────────────────────────────
    QComboBox* m_isamKeyReaderCombo = nullptr;   // 本頁使用哪一台讀卡機（來自 ReaderBar）
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
    UpdateRecordFields m_updateRecordFields;   // Update Record（00 D2 RecN 04 Lc Data）
    IsamSimpleFields m_readBinaryFields;   // Read Binary：CLA/INS/P1 固定，P2/Le 自行輸入
    IsamSimpleFields m_generateRandomCardFields;   // Generate Random Number：CLA/INS/P1/P2/Le 固定
    CardAuthFields m_cardAuthFields;   // Card Authentication
    QComboBox* m_keySetCombo = nullptr;      // Key Set 選擇（Card Authentication 用的 K）
    QPushButton* m_loadKeySetBtn = nullptr;  // 重新載入 Key Set 檔案
    QLabel* m_keySetFileLbl = nullptr;       // 目前已載入的 Key Set 檔名
    QList<KeySetEntry> m_keySetEntries;      // 已載入的 Key Set 資料
    QLineEdit* m_keySetIdEdit = nullptr;     // 依 ID 查詢用的輸入欄位
    QPushButton* m_keySetApplyBtn = nullptr; // 依 ID 套用
    QLabel* m_keySetValueLbl = nullptr;      // 顯示目前選取項目對應的 Set Key

    QComboBox* m_dataSetCombo = nullptr;     // Data Set 檢視
    QPushButton* m_loadDataSetBtn = nullptr; // 載入 Data Set 檔案
    QLabel* m_dataSetFileLbl = nullptr;      // 目前已載入的 Data Set 檔名
    QLabel* m_dataSetValueLbl = nullptr;     // 目前選取的 Data Set 內容顯示
    QList<DataSetEntry> m_dataSetEntries;    // 已載入的 Data Set 資料
    WriteRecordTacFields m_writeRecordTacFields;   // Write Record With TAC
    WriteRecordWSNumTacFields m_writeRecordWSNumTacFields;   // Write Record With S/N TAC

    // Key Set / Data Set 管理視窗（改由選單「設定 → Key Set / Data Set 管理...」開啟，
    // 不再放在「卡片指令」頁籤裡；視窗本身只建立一次，重複開啟只是喚起同一個視窗）
    QDialog* m_keySetDataSetDialog = nullptr;
    // 「卡片指令」頁籤 Card Authentication 區塊裡的唯讀狀態顯示，
    // 讓使用者不用開啟管理視窗也能看到目前套用的 Key Set 內容
    QLabel* m_caKeySetStatusLbl = nullptr;

    // ── Raw APDU Tab（獨立分類）────────────────────────────────────────────
    RawApduFields m_rawApduFields;             // Raw APDU（全欄位自行輸入）
    ReaderBar* m_rawApduReaderBar = nullptr;   // 本頁使用哪一台讀卡機

    // ── 加密工具 Tab ────────────────────────────────────────────────────────
    CryptoToolFields m_cryptoFields;

    // ── 共用 ────────────────────────────────────────────────────────────────
    QTextEdit* m_logEdit = nullptr;
    QPushButton* m_exportLogBtn = nullptr;
    QPushButton* m_clearLogBtn = nullptr;
};