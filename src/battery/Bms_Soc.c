/**
 *  @file       Bms_Soc.c
 *  @brief      Pack 1 State-of-Charge estimation using Coulomb counting.
 *
 *  SOC(t) = SOC(t-1) - I(t) * dt / Capacity
 *
 *  Pack 1 current sign convention (see Battery_Monitor.h):
 *    Positive = discharge -> remaining capacity decreases
 *    Negative = charge     -> remaining capacity increases
 */

#include "Bms_Soc.h"
#include "Battery_Monitor.h"

/*==================================================================================================
*                                       LOCAL VARIABLES
==================================================================================================*/

static Bms_Soc_DataType g_BmsSocData;

/*==================================================================================================
*                                       GLOBAL FUNCTIONS
==================================================================================================*/

void Bms_Soc_Init(void)
{
    g_BmsSocData.RemainingCapacity_mAh =
        ((float)BMS_SOC_PACK1_CAPACITY_MAH * (float)BMS_SOC_INITIAL_PCT_X10) / 1000.0f;

    g_BmsSocData.Soc_pct_x10 = (uint16)BMS_SOC_INITIAL_PCT_X10;

    g_BmsSocData.Valid = FALSE;
}

void Bms_Soc_MainFunction(void)
{
    const BatteryMonitor_DataType *batteryData = BatteryMonitor_GetData();
    float dt_h;
    float deltaCapacity_mAh;
    float socPercent;

    if ((batteryData == NULL_PTR) || (batteryData->PackCurrentValid[0] == FALSE))
    {
        g_BmsSocData.Valid = FALSE;
        return;
    }

    /* Hours elapsed per sample, based on the fixed calling period. */
    dt_h = (float)BMS_SOC_SAMPLE_PERIOD_MS / 3600000.0f;

    deltaCapacity_mAh = ((float)batteryData->PackCurrent_mA[0]) * dt_h;

    g_BmsSocData.RemainingCapacity_mAh += deltaCapacity_mAh;

    if (g_BmsSocData.RemainingCapacity_mAh > (float)BMS_SOC_PACK1_CAPACITY_MAH)
    {
        g_BmsSocData.RemainingCapacity_mAh = (float)BMS_SOC_PACK1_CAPACITY_MAH;
    }
    else if (g_BmsSocData.RemainingCapacity_mAh < 0.0f)
    {
        g_BmsSocData.RemainingCapacity_mAh = 0.0f;
    }
    else
    {
        /* Within range, no clamping needed. */
    }

    socPercent = (g_BmsSocData.RemainingCapacity_mAh * 100.0f) / (float)BMS_SOC_PACK1_CAPACITY_MAH;

    g_BmsSocData.Soc_pct_x10 = (uint16)((socPercent * 10.0f) + 0.5f);

    g_BmsSocData.Valid = TRUE;
}

const Bms_Soc_DataType *Bms_Soc_GetData(void)
{
    return &g_BmsSocData;
}

void Bms_Soc_SetSoc_pct_x10(uint16 NewSoc_pct_x10)
{
    uint16 clampedSoc = NewSoc_pct_x10;

    if (clampedSoc > BMS_SOC_MAX_PCT_X10)
    {
        clampedSoc = BMS_SOC_MAX_PCT_X10;
    }

    g_BmsSocData.RemainingCapacity_mAh =
        ((float)BMS_SOC_PACK1_CAPACITY_MAH * (float)clampedSoc) / 1000.0f;

    g_BmsSocData.Soc_pct_x10 = clampedSoc;
}
