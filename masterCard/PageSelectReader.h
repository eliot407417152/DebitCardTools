#pragma once

#include <QWidget>
#include <winscard.h>
#include "OperationMode.h"

class QComboBox;
class QPushButton;
class QLabel;
class QTextEdit;
class QRadioButton;
class QButtonGroup;
class QFormLayout;

class PageSelectReader : public QWidget
{
    Q_OBJECT

public:
    explicit PageSelectReader(QWidget* parent = nullptr);
    ~PageSelectReader();

    SCARDHANDLE   isamHandle()   const;
    SCARDHANDLE   masterHandle() const;
    OperationMode mode()         const;   // 目前選擇的操作模式

signals:
    void readerSelected();

private slots:
    void onRefresh();
    void onConfirm();
    void onModeChanged();   // 母卡製作 / 一般操作 切換

private:
    void setupUI();
    void applyModeUI();     // 依模式顯示/隱藏「母卡讀卡機」欄位、調整文字
    bool connectCard(const QString& readerName, SCARDHANDLE& outHandle);
    void log(const QString& msg, bool isError = false);

private:
    SCARDCONTEXT m_context = 0;
    SCARDHANDLE  m_isamHandle = 0;
    SCARDHANDLE  m_masterHandle = 0;

    OperationMode m_mode = OperationMode::MasterCardProduction;

    // 模式選擇
    QRadioButton* m_modeMasterRadio = nullptr;
    QRadioButton* m_modeGeneralRadio = nullptr;
    QButtonGroup* m_modeGroup = nullptr;

    // 讀卡機指派
    QLabel* m_statusLabel = nullptr;
    QFormLayout* m_readerForm = nullptr;
    QComboBox* m_isamCombo = nullptr;   // 母卡製作模式：ISAM 讀卡機／一般操作模式：唯一讀卡機
    QComboBox* m_cardCombo = nullptr;   // 僅母卡製作模式使用
    QLabel* m_isamRowLabel = nullptr;   // 隨模式切換文字
    QLabel* m_cardRowLabel = nullptr;   // 一般操作模式隱藏

    QPushButton* m_refreshBtn = nullptr;
    QPushButton* m_confirmBtn = nullptr;
    QTextEdit* m_logEdit = nullptr;
};
