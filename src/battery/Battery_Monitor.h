#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include "Std_Types.h"

typedef enum
{
    BAT_MON_STATUS_OK = 0,
    BAT_MON_STATUS_INVALID,
    BAT_MON_STATUS_OVER_VOLTAGE,
    BAT_MON_STATUS_UNDER_VOLTAGE,
    BAT_MON_STATUS_VOLTAGE_MISMATCH

} BatteryMonitor_StatusType;

typedef struct
{
    float PackV1;
    float PackV2;
    float PackV3;

    float CellVoltage[16];

    float Temperature;

    boolean Valid;

    BatteryMonitor_StatusType Status;

} BatteryMonitor_DataType;


void BatteryMonitor_Init(void);

void BatteryMonitor_MainFunction(void);

const BatteryMonitor_DataType *BatteryMonitor_GetData(void);


#endif /* BATTERY_MONITOR_H */