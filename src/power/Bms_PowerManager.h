#ifndef BMS_POWER_MANAGER_H
#define BMS_POWER_MANAGER_H

#include "Std_Types.h"
#include "Power_Ip.h"

typedef enum
{
    BMS_POWER_BOOT_COLD = 0,
    BMS_POWER_BOOT_STANDBY_WAKE
} Bms_PowerBootReasonType;

void Bms_PowerManager_Init(Power_Ip_ResetType resetReason);

void Bms_PowerManager_RequestSleep(void);

void Bms_PowerManager_MainFunction(void);

boolean Bms_PowerManager_IsSleepRequested(void);

Bms_PowerBootReasonType Bms_PowerManager_GetBootReason(void);

#endif
