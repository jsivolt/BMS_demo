#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include "Std_Types.h"

#define BATTERY_MONITOR_PACK_COUNT    (3U)

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
    /* Pack voltage */
    float PackV1;
    float PackV2;
    float PackV3;

    /* Cell voltages from AFE / vAFE. Unit: V */
    float CellVoltage[16];

    /* Cell voltage summary. Unit: V */
    float MinCellVoltage;
    float MaxCellVoltage;
    float DeltaCellVoltage;

    /* 0-based cell index */
    uint8 MinCellIndex;
    uint8 MaxCellIndex;

    /* TRUE when a complete valid AFE cell dataset is available */
    boolean CellVoltageValid;

    /*
     * Pack temperature.
     *
     * [0] = Pack 1 / NTC1
     * [1] = Pack 2 / NTC2
     * [2] = Pack 3 / NTC3
     *
     * Unit: 0.1 degC
     *
     * Example:
     * 253 = 25.3 degC
     */
    sint16 PackTemperature_dC[BATTERY_MONITOR_PACK_COUNT];

    /*
     * TRUE when corresponding NTC temperature is valid.
     */
    boolean PackTemperatureValid[BATTERY_MONITOR_PACK_COUNT];

    /*
     * Temperature summary across VALID packs only.
     */
    sint16 MinPackTemperature_dC;
    sint16 MaxPackTemperature_dC;
    sint16 DeltaPackTemperature_dC;

    /*
     * TRUE if at least one valid pack temperature is available.
     */
    boolean TemperatureSummaryValid;

    /*
     * Existing overall pack-voltage validity.
     */
    boolean Valid;

    BatteryMonitor_StatusType Status;

} BatteryMonitor_DataType;


void BatteryMonitor_Init(void);

void BatteryMonitor_MainFunction(void);

const BatteryMonitor_DataType *BatteryMonitor_GetData(void);


#endif /* BATTERY_MONITOR_H */