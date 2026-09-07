/**
 *  @file   Sil_AppDoubles.c
 *  @brief  Test doubles for the two hardware-backed application modules the
 *          SOC signal chain reads through Battery_Monitor.
 *
 *  Bms_Adc and Bms_Ntc are thin wrappers over the ADC IP driver with no logic
 *  worth exercising in SIL, so they are replaced wholesale by settable values
 *  rather than faked at the driver boundary.
 *
 *  The real headers are included deliberately: a signature change in the
 *  production API breaks the SIL build instead of silently drifting.
 */

#include "sil_api.h"

#include "Bms_Adc.h"
#include "Bms_Ntc.h"
#include "Bms_SleepTime.h"

/* ================================================================================================
 * Backing values, driven from Python
 * ============================================================================================== */

static uint16  g_PackV2_mV   = 0U;
static uint16  g_PackV3_mV   = 0U;
static boolean g_PackValid   = FALSE;

static sint16  g_Ntc_dC[3]   = { 250, 250, 250 };   /* 25.0 degC */
static boolean g_NtcValid[3] = { TRUE, TRUE, TRUE };

/*
 * Sleep-time provider. Defaults mirror the production placeholder in
 * Bms_SleepTime.c - ready immediately, zero elapsed - so a test that does not
 * care about the OCV path sees exactly the target's behaviour.
 */
static uint32  g_SleepElapsed_s = 0U;
static boolean g_SleepReady     = TRUE;

/* ================================================================================================
 * Bms_Adc — only the entry points Battery_Monitor actually calls
 * ============================================================================================== */

uint16 Bms_Adc_GetPackV2VoltageMv(void)
{
    return g_PackV2_mV;
}

uint16 Bms_Adc_GetPackV3VoltageMv(void)
{
    return g_PackV3_mV;
}

boolean Bms_Adc_IsPackValid(void)
{
    return g_PackValid;
}

/* ================================================================================================
 * Bms_Ntc
 * ============================================================================================== */

sint16 Bms_Ntc_GetTemperature_dC(Bms_NtcChannelType channel)
{
    if ((uint32)channel >= 3U)
    {
        return 0;
    }

    return g_Ntc_dC[(uint32)channel];
}

boolean Bms_Ntc_IsValid(Bms_NtcChannelType channel)
{
    if ((uint32)channel >= 3U)
    {
        return FALSE;
    }

    return g_NtcValid[(uint32)channel];
}

/* ================================================================================================
 * Bms_SleepTime — elapsed power-off time provider
 * ============================================================================================== */

boolean Bms_SleepTime_IsReady(void)
{
    return g_SleepReady;
}

uint32 Bms_SleepTime_GetElapsed_s(void)
{
    return g_SleepElapsed_s;
}

/* ================================================================================================
 * SIL control surface
 * ============================================================================================== */

void Sil_SetSleepTime(uint32 elapsed_s, boolean ready)
{
    g_SleepElapsed_s = elapsed_s;
    g_SleepReady     = ready;
}

void Sil_SetAdcPackVoltages(uint16 v2_mV, uint16 v3_mV, boolean valid)
{
    g_PackV2_mV = v2_mV;
    g_PackV3_mV = v3_mV;
    g_PackValid = valid;
}

void Sil_SetNtc(sint16 t1_dC, sint16 t2_dC, sint16 t3_dC, boolean valid)
{
    g_Ntc_dC[0] = t1_dC;
    g_Ntc_dC[1] = t2_dC;
    g_Ntc_dC[2] = t3_dC;

    g_NtcValid[0] = valid;
    g_NtcValid[1] = valid;
    g_NtcValid[2] = valid;
}

void Sil_ResetAppDoubles(void)
{
    Sil_SetAdcPackVoltages(0U, 0U, FALSE);
    Sil_SetNtc(250, 250, 250, TRUE);
    Sil_SetSleepTime(0U, TRUE);
}
