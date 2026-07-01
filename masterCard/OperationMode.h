#pragma once

// 操作模式：
//   MasterCardProduction — 母卡製作，需要兩台讀卡機（ISAM + 母卡）
//   GeneralOperation     — 一般操作，僅需一台讀卡機，且不提供「製作母卡」相關功能
enum class OperationMode
{
    MasterCardProduction,
    GeneralOperation
};
