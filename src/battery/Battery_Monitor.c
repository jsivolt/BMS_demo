#include "Battery_Monitor.h"
#include "Bms_Adc.h"


static BatteryMonitor_DataType g_BatteryData;


void BatteryMonitor_Init(void)
{
    uint8 i;

    g_BatteryData.PackV1 = 0.0f;
    g_BatteryData.PackV2 = 0.0f;
    g_BatteryData.PackV3 = 0.0f;

    g_BatteryData.Temperature = 0.0f;

    for (i = 0U; i < 16U; i++)
    {
        g_BatteryData.CellVoltage[i] = 0.0f;
    }

    g_BatteryData.Valid = FALSE;

    g_BatteryData.Status = BAT_MON_STATUS_INVALID;
}


void BatteryMonitor_MainFunction(void)
{
    if (Bms_Adc_IsPackValid() == TRUE)
    {
        g_BatteryData.PackV1 =
            (float)Bms_Adc_GetPackV1VoltageMv() / 1000.0f;

        g_BatteryData.PackV2 =
            (float)Bms_Adc_GetPackV2VoltageMv() / 1000.0f;

        g_BatteryData.PackV3 =
            (float)Bms_Adc_GetPackV3VoltageMv() / 1000.0f;

        g_BatteryData.Valid = TRUE;
        g_BatteryData.Status = BAT_MON_STATUS_OK;
    }
    else
    {
        g_BatteryData.Valid = FALSE;
        g_BatteryData.Status = BAT_MON_STATUS_INVALID;
    }
}


const BatteryMonitor_DataType *BatteryMonitor_GetData(void)
{
    return &g_BatteryData;
}