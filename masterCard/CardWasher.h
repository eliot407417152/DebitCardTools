#pragma once
#include "CardFlow.h"

// 洗卡流程
// 功能 1、2:DataIN 直接使用卡片 Read Binary 的回應
// 功能 3、4、5:DataIN 的 Read Binary 改為外部傳入的多樣化資料 div(8 bytes)
class CardWasher : public CardFlow {
public:
    using CardFlow::CardFlow;

    bool authTerminal();                         // 步驟 1~5   認證端末設備
    bool createSessionKey();                     // 步驟 6~8   建立交談基碼
    bool updateIssuerAuthKey(const Bytes& div);  // 步驟 9~11  更新製造商生產認證基碼為發卡端末認證基碼
    bool updateCardRandomKey(const Bytes& div);  // 步驟 12~14 更新卡片發行亂碼基碼
    bool addMasterKey(const Bytes& div);         // 步驟 15~17 新增發卡單位主控基碼
};