/**
 *  @file       Battery_Monitor.c
 *  @brief      Battery pack monitoring - voltage, current and temperature.
 */

#include "Battery_Monitor.h"
#include "Fault_Manager.h"

/*==================================================================================================
*                                       LOCAL DEFINES
==================================================================================================*/

#define BATTERY_MONITOR_OVER_VOLTAGE_MV    ((sint32)4200 * 96)  /**< Pack-level over-voltage limit. */
#define BATTERY_MONITOR_UNDER_VOLTAGE_MV   ((sint32)2800 * 96)  /**< Pack-level under-voltage limit. */
#define BATTERY_MONITOR_OVER_CURRENT_MA    ((sint32)200000)     /**< Charge/discharge over-current limit. */
#define BATTERY_MONITOR_OVER_TEMP_DC       ((sint16)600)        /**< 60.0 degC over-temperature limit. */
#define BATTERY_MONITOR_UNDER_TEMP_DC      ((sint16)-200)       /**< -20.0 degC under-temperature limit. */

/*==================================================================================================
*                                       LOCAL VARIABLES
==================================================================================================*/

static Battery_Monitor_DataType Battery_Monitor_LatestData;

/*==================================================================================================
*                                       LOCAL FUNCTION PROTOTYPES
==================================================================================================*/

static void Battery_Monitor_AcquireMeasurements(Battery_Monitor_DataType * const data);
static void Battery_Monitor_CheckLimits(const Battery_Monitor_DataType * const data);

/*==================================================================================================
*                                       FUNCTION DEFINITIONS
==================================================================================================*/

void Battery_Monitor_Init(void)
{
    Battery_Monitor_LatestData.packVoltage_mV = 0;
    Battery_Monitor_LatestData.packCurrent_mA = 0;
    Battery_Monitor_LatestData.packTemperature_dC = 0;
    Battery_Monitor_LatestData.stateOfCharge_pct = 0U;
}

void Battery_Monitor_Update(void)
{
    Battery_Monitor_AcquireMeasurements(&Battery_Monitor_LatestData);
    Battery_Monitor_CheckLimits(&Battery_Monitor_LatestData);
}

void Battery_Monitor_GetData(Battery_Monitor_DataType * const data)
{
    if (data != NULL_PTR)
    {
        *data = Battery_Monitor_LatestData;
    }
}

/*==================================================================================================
*                                       LOCAL FUNCTION DEFINITIONS
==================================================================================================*/

/**
 * @brief Reads the cell voltages/current/temperature sensors.
 * @note  No analog front-end driver is integrated in this demo yet;
 *        replace this body with the actual sensor acquisition once available.
 */
static void Battery_Monitor_AcquireMeasurements(Battery_Monitor_DataType * const data)
{
    /* TODO: Replace with real ADC / AFE sensor readings. */
    data->packVoltage_mV = 0;
    data->packCurrent_mA = 0;
    data->packTemperature_dC = 0;
    data->stateOfCharge_pct = 0U;
}

static void Battery_Monitor_CheckLimits(const Battery_Monitor_DataType * const data)
{
    if (data->packVoltage_mV > BATTERY_MONITOR_OVER_VOLTAGE_MV)
    {
        Fault_Manager_ReportFault(FAULT_ID_OVER_VOLTAGE);
    }
    else
    {
        Fault_Manager_ClearFault(FAULT_ID_OVER_VOLTAGE);
    }

    if (data->packVoltage_mV < BATTERY_MONITOR_UNDER_VOLTAGE_MV)
    {
        Fault_Manager_ReportFault(FAULT_ID_UNDER_VOLTAGE);
    }
    else
    {
        Fault_Manager_ClearFault(FAULT_ID_UNDER_VOLTAGE);
    }

    if ((data->packCurrent_mA > BATTERY_MONITOR_OVER_CURRENT_MA) ||
        (data->packCurrent_mA < -BATTERY_MONITOR_OVER_CURRENT_MA))
    {
        Fault_Manager_ReportFault(FAULT_ID_OVER_CURRENT);
    }
    else
    {
        Fault_Manager_ClearFault(FAULT_ID_OVER_CURRENT);
    }

    if (data->packTemperature_dC > BATTERY_MONITOR_OVER_TEMP_DC)
    {
        Fault_Manager_ReportFault(FAULT_ID_OVER_TEMPERATURE);
    }
    else
    {
        Fault_Manager_ClearFault(FAULT_ID_OVER_TEMPERATURE);
    }

    if (data->packTemperature_dC < BATTERY_MONITOR_UNDER_TEMP_DC)
    {
        Fault_Manager_ReportFault(FAULT_ID_UNDER_TEMPERATURE);
    }
    else
    {
        Fault_Manager_ClearFault(FAULT_ID_UNDER_TEMPERATURE);
    }
}
