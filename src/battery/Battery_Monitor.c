#include "Battery_Monitor.h"
#include "Bms_Adc.h"



BatteryMonitor_DataType g_BatteryData;


void BatteryMonitor_Init(void)
{

    g_BatteryData.PackVoltage = 0.0f;

    g_BatteryData.Temperature = 0.0f;


    for(uint8 i=0;i<16;i++)
    {
        g_BatteryData.CellVoltage[i]=0.0f;
    }


    g_BatteryData.Valid = FALSE;

}






void BatteryMonitor_MainFunction(void)
{
    if (Bms_Adc_IsPackValid() == TRUE)
    {
        g_BatteryData.PackVoltage =
            (float)Bms_Adc_GetPackV1VoltageMv() / 1000.0f;

        g_BatteryData.Valid = TRUE;
    }
    else
    {
        g_BatteryData.Valid = FALSE;
    }
}



const BatteryMonitor_DataType*
BatteryMonitor_GetData(void)
{

    return &g_BatteryData;

}