#pragma once

#include <QGroupBox>
#include <QFlags>
#include "CardUtils.h"   // CustomerData

class QLineEdit;

// ─────────────────────────────────────────────────────────────────────────────
// CustomerDataForm — 客戶資料欄位（原「製卡發卡流程」頁籤中的那一大組欄位）
//
//   「製作母卡」「母卡轉換」兩個頁面各自擁有一份，可依需求只顯示需要的欄位：
//     • 製作母卡 → AllFields
//     • 母卡轉換 → Name | Exchange | PinA | PinB
//   欄位間的連動邏輯與原本完全相同：
//     Code A / Code B 改變 → 重算兩者檢查碼；A、B 皆為完整 48 碼時 Exchange = A XOR B
//     Exchange Code 改變   → 重算 CCode
// ─────────────────────────────────────────────────────────────────────────────
class CustomerDataForm : public QGroupBox
{
    Q_OBJECT

public:
    enum Field {
        Name = 0x001,
        C3 = 0x002,
        C3Code = 0x004,
        C1 = 0x008,
        C1Code = 0x010,
        CodeA = 0x020,      // 含 Code A 檢查碼
        CodeB = 0x040,      // 含 Code B 檢查碼
        Exchange = 0x080,   // 含 CCode
        PinA = 0x100,
        PinB = 0x200,
        AllFields = 0x3FF
    };
    Q_DECLARE_FLAGS(Fields, Field)

    explicit CustomerDataForm(Fields shown, QWidget* parent = nullptr);

    CustomerData data() const;
    void setData(const CustomerData& d);
    void clear();

    // C3 / C1 是否都有值（製作母卡用來決定按鈕是否可按）
    bool hasC3AndC1() const;

signals:
    void changed();                          // 任何欄位被修改
    void warning(const QString& message);    // 載入資料時發現的提醒

private slots:
    void onCodeABChanged();
    void onExchangeChanged();

private:
    QString joinSegs(QLineEdit* const* segs) const;
    void    setSegs(QLineEdit* const* segs, const QString& value);

    QLineEdit* m_nameEdit = nullptr;
    QLineEdit* m_c3Edit = nullptr;
    QLineEdit* m_c3CodeEdit = nullptr;
    QLineEdit* m_c1Edit = nullptr;
    QLineEdit* m_c1CodeEdit = nullptr;
    QLineEdit* m_codeAEdit[3] = { nullptr, nullptr, nullptr };
    QLineEdit* m_codeACheckEdit = nullptr;
    QLineEdit* m_codeBEdit[3] = { nullptr, nullptr, nullptr };
    QLineEdit* m_codeBCheckEdit = nullptr;
    QLineEdit* m_exchEdit[3] = { nullptr, nullptr, nullptr };
    QLineEdit* m_ccodeEdit = nullptr;
    QLineEdit* m_pinAEdit = nullptr;
    QLineEdit* m_pinBEdit = nullptr;

    bool m_updatingKeyFields = false;   // 程式自動填值時，避免 textChanged 連鎖觸發
};

Q_DECLARE_OPERATORS_FOR_FLAGS(CustomerDataForm::Fields)
