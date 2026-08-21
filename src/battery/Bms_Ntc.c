/**
 *  @file       Bms_Ntc.c
 *  @brief      3-channel NTC thermistor temperature conversion for the battery pack.
 */

#include "Bms_Ntc.h"
#include "Bms_Ntc_Cfg.h"
#include "Bms_Adc.h"

#include <math.h>

/*==================================================================================================
*                                       LOCAL VARIABLES
==================================================================================================*/

static sint16 g_BmsNtcTemperature_dC[BMS_NTC_COUNT];
static boolean g_BmsNtcValid[BMS_NTC_COUNT];

/*==================================================================================================
*                                       LOCAL FUNCTION PROTOTYPES
==================================================================================================*/

static float Bms_Ntc_RawToResistance(uint16 adcRaw);

static uint16 Bms_Ntc_GetAdcRaw(
    Bms_NtcChannelType channel
);

static boolean Bms_Ntc_IsChannelValid(
    Bms_NtcChannelType channel
);

/*==================================================================================================
*                                       LOCAL FUNCTION DEFINITIONS
==================================================================================================*/

static boolean Bms_Ntc_IsChannelValid(
    Bms_NtcChannelType channel
)
{
    return (((uint8)channel) < BMS_NTC_COUNT)
               ? TRUE
               : FALSE;
}


static uint16 Bms_Ntc_GetAdcRaw(
    Bms_NtcChannelType channel
)
{
    uint16 adcRaw = 0U;

    switch (channel)
    {
        case BMS_NTC_1:
            adcRaw = Bms_Adc_GetNtc1Raw();
            break;

        case BMS_NTC_2:
            adcRaw = Bms_Adc_GetNtc2Raw();
            break;

        case BMS_NTC_3:
            adcRaw = Bms_Adc_GetNtc3Raw();
            break;

        default:
            adcRaw = 0U;
            break;
    }

    return adcRaw;
}


static float Bms_Ntc_RawToResistance(
    uint16 adcRaw
)
{
    float ratio;
    float resistance;

    ratio =
        (float)adcRaw /
        (float)BMS_NTC_CFG_ADC_FULL_SCALE;

    /*
     * Prevent divide-by-zero at ADC rails.
     */
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
        /* Ratio is valid. */
    }

#if (BMS_NTC_CFG_NTC_TO_GND == 1U)

    /*
     * Divider:
     *
     * VREF
     *   |
     * Rseries
     *   |
     *   +------ ADC
     *   |
     *  NTC
     *   |
     *  GND
     *
     * Vadc =
     * Vref * Rntc / (Rseries + Rntc)
     *
     * Rntc =
     * Rseries * ratio / (1 - ratio)
     */

    resistance =
        (float)BMS_NTC_CFG_SERIES_RESISTOR_OHM *
        (ratio / (1.0f - ratio));

#else

    /*
     * Opposite topology:
     *
     * VREF
     *   |
     *  NTC
     *   |
     *   +------ ADC
     *   |
     * Rseries
     *   |
     *  GND
     */

    resistance =
        (float)BMS_NTC_CFG_SERIES_RESISTOR_OHM *
        ((1.0f - ratio) / ratio);

#endif

    return resistance;
}


/*==================================================================================================
*                                       GLOBAL FUNCTION DEFINITIONS
==================================================================================================*/

void Bms_Ntc_Init(void)
{
    uint8 i;

    for (i = 0U; i < BMS_NTC_COUNT; i++)
    {
        g_BmsNtcTemperature_dC[i] = 0;
        g_BmsNtcValid[i] = FALSE;
    }
}


Std_ReturnType Bms_Ntc_RawToTemperature(
    uint16 adcRaw,
    sint16 * const temperature_dC
)
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

    /*
     * ADC raw -> NTC resistance
     */
    resistance =
        Bms_Ntc_RawToResistance(adcRaw);

    /*
     * Beta equation:
     *
     * 1/T =
     * 1/T0 +
     * (1/Beta) * ln(R/R0)
     */

    invTemp =
        (1.0f /
         (float)BMS_NTC_CFG_T25_KELVIN)
        +
        (
            (1.0f /
             (float)BMS_NTC_CFG_BETA_KELVIN)
            *
            logf(
                resistance /
                (float)BMS_NTC_CFG_R25_OHM
            )
        );

    tempKelvin =
        1.0f / invTemp;

    /*
     * Kelvin -> 0.1 °C
     *
     * Example:
     * 25.3 °C -> 253
     */
    tempDeciCelsius =
        (tempKelvin - 273.15f) * 10.0f;

    result =
        (sint16)tempDeciCelsius;

    retVal =
        (Std_ReturnType)E_OK;

    /*
     * Range validation
     */
    if (result < BMS_NTC_CFG_MIN_TEMP_DC)
    {
        result =
            BMS_NTC_CFG_MIN_TEMP_DC;

        retVal =
            (Std_ReturnType)E_NOT_OK;
    }
    else if (result > BMS_NTC_CFG_MAX_TEMP_DC)
    {
        result =
            BMS_NTC_CFG_MAX_TEMP_DC;

        retVal =
            (Std_ReturnType)E_NOT_OK;
    }
    else
    {
        /* Temperature is valid. */
    }

    *temperature_dC = result;

    return retVal;
}


void Bms_Ntc_MainFunction(void)
{
    uint8 i;

    uint16 adcRaw;

    sint16 temperature_dC;

    Bms_NtcChannelType channel;

    /*
     * Process NTC1 / NTC2 / NTC3
     */
    for (i = 0U; i < BMS_NTC_COUNT; i++)
    {
        channel =
            (Bms_NtcChannelType)i;

        /*
         * Get corresponding ADC result
         */
        adcRaw =
            Bms_Ntc_GetAdcRaw(channel);

        /*
         * ADC -> temperature
         */
        if (
            Bms_Ntc_RawToTemperature(
                adcRaw,
                &temperature_dC
            )
            ==
            (Std_ReturnType)E_OK
        )
        {
            /*
             * Only update temperature
             * when conversion is valid.
             */
            g_BmsNtcTemperature_dC[i] =
                temperature_dC;

            g_BmsNtcValid[i] =
                TRUE;
        }
        else
        {
            /*
             * Keep last valid temperature.
             *
             * Mark newest reading invalid.
             */
            g_BmsNtcValid[i] =
                FALSE;
        }
    }
}


sint16 Bms_Ntc_GetTemperature_dC(
    Bms_NtcChannelType channel
)
{
    if (
        Bms_Ntc_IsChannelValid(channel)
        ==
        FALSE
    )
    {
        return 0;
    }

    return
        g_BmsNtcTemperature_dC[
            (uint8)channel
        ];
}


boolean Bms_Ntc_IsValid(
    Bms_NtcChannelType channel
)
{
    if (
        Bms_Ntc_IsChannelValid(channel)
        ==
        FALSE
    )
    {
        return FALSE;
    }

    return
        g_BmsNtcValid[
            (uint8)channel
        ];
}