[README.md](https://github.com/user-attachments/files/30455641/README.md)
# DebitCardTools（智慧卡產製系統）

以 Qt Widgets 撰寫的金融智慧卡（ISAM／母卡／發卡）產製與除錯工具。提供從讀卡機偵測、ISAM 金鑰管理、單筆 APDU 手動測試，到製作母卡、母卡轉換、洗卡、發卡等完整流程的圖形化操作介面。

> 本文件依目前程式碼與歷次調整內容整理，供後續維護者快速掌握專案結構與各功能的實作邏輯。

---

## 目錄

- [系統需求](#系統需求)
- [專案結構](#專案結構)
- [畫面與功能](#畫面與功能)
  - [讀卡機選擇頁](#讀卡機選擇頁-pageselectreader)
  - [ISAM 金鑰頁](#isam-金鑰頁)
  - [ISAM 指令頁](#isam-指令頁)
  - [卡片指令頁](#卡片指令頁)
  - [製卡發卡流程頁](#製卡發卡流程頁)
- [核心工具類別](#核心工具類別)
- [開發慣例](#開發慣例)
- [已知議題 / 待確認事項](#已知議題--待確認事項)
- [建置方式](#建置方式)

---

## 系統需求

- **Qt** 5 或 6（Widgets 模組）
- **OpenSSL**（`libdes` / `DES_*` 系列函式，用於 DES / 3DES 運算）
- **PC/SC**（Windows 內建 `winscard.h` / `WinSCard.lib`，或對應平台的 PC/SC Lite）
- 支援 PC/SC 的讀卡機（測試環境含 Gemplus USB SmartCard Reader 等）

---

## 專案結構

```
main.cpp                     程式進入點，建立 QApplication + MainWindow
MainWindow.h/.cpp            主視窗，管理「選擇讀卡機」↔「操作工作區」兩個分頁的切換與視窗大小

PageSelectReader.h/.cpp      啟動頁：偵測目前插著的讀卡機，簡單列出清單，按「進入」後交棒給操作頁

PageCardOperation.h          操作頁主標頭（所有分頁共用的類別定義／欄位 struct／成員宣告）
PageCardOperation.cpp        建構子、共用工具（連線讀卡機、通用欄位建構、log、匯出日誌等）
PageCardOperation_Fields.cpp 各欄位 struct 的 buildApdu() / computedP2() 等純運算實作
PageCardOperation_IsamTab.cpp     Tab「ISAM 金鑰」的畫面與邏輯
PageCardOperation_IsamCmdTab.cpp  Tab「ISAM 指令」的畫面與邏輯
PageCardOperation_ApduTab.cpp     Tab「卡片指令」的畫面與邏輯
PageCardOperation_LodTab.cpp      Tab「製卡發卡流程」的畫面與邏輯

CardUtils.h/.cpp             APDU 傳輸、DES/3DES 加解密、HEX 轉換、ISAM CD Key 對照表等共用工具
IsamKeyData.h/.cpp           各銀行/機構 ISAM 金鑰對照資料庫（C3/C1 金鑰、對照碼）
IsamKeyUpdater.h/.cpp        寫入 ISAM C3/C1 金鑰流程
IsamKeyChecker.h/.cpp        檢查卡片 KEY（雙向認證 + Session Key 覆寫 C3）流程
LodLoader.h/.cpp             解析 LOD 客戶資料檔（CSV 格式），對應到 CustomerData
MasterCardUpdater.h/.cpp     母卡雙向認證、寫入資料（C8/C9/0001/0002），以及母卡轉換（母卡 → 銀行 ISAM）
CardWasher.h/.cpp            「洗卡」完整流程（Session Key 建立 + 00C3/00C1/00C0 金鑰檔案處理）
CardIssuer.h/.cpp            「發卡」完整流程（洗卡步驟 + 00C2/00C4/00C5/00C6/00D6/00D7/00C8 應用檔案 + 個人化資料寫入）
```

`PageCardOperation` 的實作原本集中在單一 `.cpp`（約 3000 行以上），為了方便維護已依分頁拆成多個 `.cpp`，全部共用同一份 `PageCardOperation.h`，彼此邏輯完全沒有變動，純粹是實作分檔。**若使用 Visual Studio 專案或 CMake，記得把上述所有 `PageCardOperation_*.cpp` 都加進編譯清單。**

---

## 畫面與功能

### 讀卡機選擇頁（`PageSelectReader`）

- 啟動時自動呼叫 `SCardListReadersA` 偵測目前插著的讀卡機
- 畫面精簡：狀態文字 + 讀卡機清單（`QListWidget`）+「🔄 重新偵測」「➡️ 進入」
- 偵測到至少 1 台即可進入操作頁；實際各功能要用哪一台讀卡機，改到操作頁裡各自的下拉選單選擇
- 進入操作頁後，`context()` / `readers()` 會交給 `PageCardOperation` 使用

### ISAM 金鑰頁

- 下拉選單選擇銀行／機構（資料來源：`IsamKeyData`），顯示對照用的 C3/C1 Code（金鑰本體不顯示）
- **🔎 查看 ISAM**：直接讀取卡片目前的 C3／C1 對照碼（不需先選銀行）
- **⚡ 寫入 ISAM 金鑰**：呼叫 `IsamKeyUpdater` 寫入所選銀行的 C3/C1 金鑰
- **🔍 檢查卡片 KEY**：呼叫 `IsamKeyChecker` 執行雙向認證與 List File 比對

### ISAM 指令頁

左側指令清單 + 右側對應欄位（點選清單項目切換），涵蓋：

1. Select ISAM Object
2. Get Key Check Code
3. Generate Random No
4. Get TerminalAuthCode
5. Get Encrypted Key
6. Write Key
7. Store Manufacturer Key
8. Set Session Key（Div-Data + R2，Lc = DL+8）
9. Get Diversify Key with Session Key
10. Get Working Key with Session Key

### 卡片指令頁

同樣是左側清單 + 右側欄位的排版，涵蓋：

1. Create File
2. Select File
3. List File
4. Read Record
5. Read All Record（回應依 `[Block編號][長度][資料]` 格式自動切開顯示每個 block）
6. Read Binary
7. Write Record
8. Update Record Ciphered
9. Verify Pin（P2 由 Pq 邏輯左移 3 位自動運算）
10. Terminal Authentication（P2 由 Kq 邏輯左移 3 位自動運算，Kq 十進位輸入）
11. Set Session Key（同上左移邏輯）
12. Generate Random Number
13. **Card Authentication**（`88` 指令）：DATA=R1+R3，P2 由 Kq 左移 3 位；回應 16 bytes 拆成 `[MAC 前8 bytes][R2 後8 bytes]`，可自動計算 `E(E(E(K,R1),R2),R3)` 與回應 MAC 比對（詳見〈已知議題〉）
14. Write Record With TAC

### 製卡發卡流程頁

- **載入 LOD**／手動輸入客戶資料欄位（C3、C1、Exchange Code、CCode、Pin A/B 等）
- **🔐 母卡認證 / 🔮 製作母卡**：透過 `MasterCardUpdater` 對兩台讀卡機（ISAM + 母卡）執行雙向認證與資料寫入
- **🔄 母卡轉換**：把製作好的母卡與銀行 ISAM 做轉換（Write Key、Verify PIN、0001/0002 金鑰區塊轉換）
- **🧼 洗卡**：`CardWasher`，對「卡片」+ ISAM 建立 Session Key，並處理 00C3/00C1/00C0 三個金鑰檔案
- **🎫 發卡**：`CardIssuer`，包含洗卡的全部步驟，並延伸建立 00C2/00C4/00C5/00C6/00D6/00D7/00C8 應用檔案與個人化資料檔案（0001/0002/1001/1002 等）的實際資料寫入
- **💾 匯出日誌**：把畫面上的通訊日誌存成文字檔

---

## 核心工具類別

| 類別 | 說明 |
|---|---|
| `CardUtils` | `sendApdu`（APDU 收送）、`encrypt3DES`/`desHex`/`desSingleBlockHex`/`tripleDesHex`（依金鑰長度自動判斷單一 DES 或 3DES EDE）、`divKey`、`getSessionKey`、`lookupIsamCdKey`（依 ISAM 回應前 2 bytes 對照 CD Key）等 |
| `IsamKeyData` | 各銀行/機構的 C3/C1 金鑰與對照碼硬編碼資料表 |
| `MasterCardUpdater` | 母卡雙向認證、寫入 C8/C9/0001/0002、母卡轉換（Write Key + Store Manufacturer Key） |
| `CardWasher` | 洗卡：Session Key 建立 + 00C3/00C1/00C0 |
| `CardIssuer` | 發卡：洗卡步驟 + 應用檔案（00C2 等）+ 個人化資料寫入 |

---

## 開發慣例

專案中反覆出現以下約定，維護時請盡量比照辦理：

1. **Lc 一律自動計算**：凡文件標示的 Lc 數字與實際 DataIn 長度對不上時，一律改用「依實際組出的 DataIn 長度自動計算」，不採用文件上的數字（多次驗證過文件本身有筆誤）。
2. **Kq／Pq 左移 3 位邏輯**：出現在 Verify Pin、Terminal Authentication、Set Session Key（卡片）、Card Authentication 等指令。**Kq 一律以十進位解析**（例如輸入 `17` 得到 `P2=0x88`），不要用十六進位解析，這是先前修正過的重點。
3. **ISAM 對照碼／檢索碼**：多處指令（如 `Get Key Check Code`）回應的只是「檢索碼」，需再取其前 4 碼查 `CardUtils::lookupIsamCdKey()` 才是實際可用的金鑰／CD Key，不能直接把回應原文當金鑰使用。
4. **UI 排版**：ISAM 指令／卡片指令兩頁採「左側指令清單 + 右側 `QStackedWidget`」設計，新增指令時比照既有的 `addCmdPage()` / `addApduCmdPage()` 小工具把群組加進清單即可。

---

## 已知議題 / 待確認事項

- **Card Authentication 的 Key 推導方式尚未最終確認。** 目前程式邏輯為：
  1. DATA = R1 + R3 送出，P2 由 Kq 左移 3 位。
  2. 回應 16 bytes 拆成 `MAC = 前 8 bytes`、`R2 = 後 8 bytes`。
  3. 计算 `R = E(DES)(E(DES)(E(DES/3DES)(K,R1),R2),R3)` 並與 MAC 比對。
  
  第 3 步中的金鑰 `K`（卡片認證基碼）該如何取得，仍在依實際刷卡回應資料反覆核對中，曾嘗試「Key 直接自行輸入」與「Key 檢索碼查表換算」兩種方式，請在確認最終正確做法後更新本節與對應程式碼／註解。
- `CardWasher` 步驟 6「Generate Random Number（ISAM）」固定使用 `8F 86 00 00`（INS=86）；文件曾多次寫成 `INS=06`，已確認以 `86` 為準，若日後規格書更新請留意是否為筆誤。

---

## 建置方式

專案未附上 `.pro` / `CMakeLists.txt`（依實際採用的建置系統為準，例如 Visual Studio `.vcxproj`）。建置時請確認：

1. 專案來源檔清單包含所有 `PageCardOperation_*.cpp` 分檔
2. 連結 Qt Widgets、OpenSSL（DES）、`WinSCard.lib`（或對應平台 PC/SC 函式庫）
3. Include 路徑包含 `winscard.h` 與 OpenSSL 標頭
