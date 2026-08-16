/**
 *  @file       Bms_Ntc.c
 *  @brief      NTC thermistor temperature conversion for the battery pack.
 */

#include "Bms_Ntc.h"
#include "Bms_Ntc_Cfg.h"
#include "Bms_Adc.h"

#include <math.h>

/*==================================================================================================
*                                       LOCAL VARIABLES
==================================================================================================*/

static volatile sint16 g_BmsNtcTemperature_dC = 0;
static volatile boolean g_BmsNtcValid = FALSE;

/*==================================================================================================
*                                       LOCAL FUNCTION DEFINITIONS
==================================================================================================*/

static float Bms_Ntc_RawToResistance(uint16 adcRaw)
{
    float ratio;
    float resistance;

    /* Fraction of full-scale represented by the NTC divider node. */
    ratio = (float)adcRaw / (float)BMS_NTC_CFG_ADC_FULL_SCALE;

    if (ratio <= 0.0f)
    {
        ratio = 1.0e-6f;
    }
    else if (ratio >= 1.0f)
    {
        ratio = 1.0f - 1.0e-6f;
    }
    else
    {
        /* Ratio already within the open interval (0, 1). */
    }

#if (BMS_NTC_CFG_NTC_TO_GND == 1U)
    /* Vadc = Vref * Rntc / (Rseries + Rntc)  =>  Rntc = Rseries * ratio / (1 - ratio). */
    resistance = (float)BMS_NTC_CFG_SERIES_RESISTOR_OHM * (ratio / (1.0f - ratio));
#else
    /* NTC to VREF: Rntc = Rseries * (1 - ratio) / ratio. */
    resistance = (float)BMS_NTC_CFG_SERIES_RESISTOR_OHM * ((1.0f - ratio) / ratio);
#endif

    return resistance;
}

Std_ReturnType Bms_Ntc_RawToTemperature(uint16 adcRaw, sint16 * const temperature_dC)
{
    Std_ReturnType retVal;
    float resistance;
    float invTemp;
    float tempKelvin;
    float tempDeciCelsius;
    sint16 result;

    if (temperature_dC == NULL_PTR)
    {
        return (Std_ReturnType)E_NOT_OK;
    }

    resistance = Bms_Ntc_RawToResistance(adcRaw);

    /* Beta equation: 1/T = 1/T0 + (1/Beta) * ln(R / R0). */
    invTemp = (1.0f / (float)BMS_NTC_CFG_T25_KELVIN) +
              ((1.0f / (float)BMS_NTC_CFG_BETA_KELVIN) *
               logf(resistance / (float)BMS_NTC_CFG_R25_OHM));

    tempKelvin = 1.0f / invTemp;

    /* Convert to tenths of a degree Celsius. */
    tempDeciCelsius = ((tempKelvin - 273.15f) * 10.0f);

    result = (sint16)tempDeciCelsius;
    retVal = (Std_ReturnType)E_OK;

    if (result < BMS_NTC_CFG_MIN_TEMP_DC)
    {
        result = BMS_NTC_CFG_MIN_TEMP_DC;
        retVal = (Std_ReturnType)E_NOT_OK;
    }
    else if (result > BMS_NTC_CFG_MAX_TEMP_DC)
    {
        result = BMS_NTC_CFG_MAX_TEMP_DC;
        retVal = (Std_ReturnType)E_NOT_OK;
    }
    else
    {
        /* Result within valid range. */
    }

    *temperature_dC = result;

    return retVal;
}

void Bms_Ntc_MainFunction(void)
{
    uint16 adcRaw;
    sint16 temperature_dC;

    adcRaw = Bms_Adc_GetNtcRaw();

    if (Bms_Ntc_RawToTemperature(
            adcRaw,
            &temperature_dC) == (Std_ReturnType)E_OK)
    {
        g_BmsNtcTemperature_dC = temperature_dC;
        g_BmsNtcValid = TRUE;
    }
    else
    {
        /*
         * Keep the last valid temperature value.
         * Only invalidate the latest NTC measurement.
         */
        g_BmsNtcValid = FALSE;
    }
}


sint16 Bms_Ntc_GetTemperature_dC(void)
{
    return g_BmsNtcTemperature_dC;
}


boolean Bms_Ntc_IsValid(void)
{
    return g_BmsNtcValid;
}
