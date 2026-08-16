#include "Bms_Adc.h"

#include "Adc_Sar_Ip.h"
#include "Adc_Sar_Ip_PBcfg.h"
#include "Adc_Sar_Ip_Cfg.h"
#include "Adc_Sar_Ip_CfgDefines.h"
#include "Bms_Ntc_Cfg.h"


#define BMS_ADC_NTC1_CHANNEL      (1U)
#define BMS_ADC_NTC2_CHANNEL      (32U)
#define BMS_ADC_NTC3_CHANNEL      (33U)

#define BMS_ADC_PACK_V1_CHANNEL   (4U)
#define BMS_ADC_PACK_V2_CHANNEL   (35U)
#define BMS_ADC_PACK_V3_CHANNEL   (36U)

#define BMS_ADC_POT_CHANNEL       (34U)

/* Latest ADC raw values */
static volatile uint16 g_BmsAdcPotRaw = 0U;

static volatile uint16 g_BmsAdcNtc1Raw = 0U;
static volatile uint16 g_BmsAdcNtc2Raw = 0U;
static volatile uint16 g_BmsAdcNtc3Raw = 0U;

static volatile uint16 g_BmsAdcPackV1Raw = 0U;
static volatile uint16 g_BmsAdcPackV2Raw = 0U;
static volatile uint16 g_BmsAdcPackV3Raw = 0U;

/* TRUE when the last acquisition produced valid conversion results */
static volatile boolean g_BmsAdcValid = FALSE;

/* Indicates whether a conversion has already been started */
static boolean g_BmsAdcConversionStarted = FALSE;


Std_ReturnType Bms_Adc_Init(void)
{
    Adc_Sar_Ip_StatusType status;

    status = Adc_Sar_Ip_Init(
        ADCHWUNIT_0_INSTANCE,
        &AdcHwUnit_0
    );

    if (status == ADC_SAR_IP_STATUS_SUCCESS)
    {
        status = Adc_Sar_Ip_DoCalibration(
            ADCHWUNIT_0_INSTANCE
        );
    }

    return (status == ADC_SAR_IP_STATUS_SUCCESS)
        ? (Std_ReturnType)E_OK
        : (Std_ReturnType)E_NOT_OK;
}


void Bms_Adc_MainFunction(void)
{
    Adc_Sar_Ip_ChanResultType potResult;

    Adc_Sar_Ip_ChanResultType ntc1Result;
    Adc_Sar_Ip_ChanResultType ntc2Result;
    Adc_Sar_Ip_ChanResultType ntc3Result;

    Adc_Sar_Ip_ChanResultType packV1Result;
    Adc_Sar_Ip_ChanResultType packV2Result;
    Adc_Sar_Ip_ChanResultType packV3Result;

    if (g_BmsAdcConversionStarted == TRUE)
    {
        /*
         * Read potentiometer:
         * ADC1_S10 / Channel 34
         */
        Adc_Sar_Ip_GetConvResult(
            ADCHWUNIT_0_INSTANCE,
            BMS_ADC_POT_CHANNEL,
            ADC_SAR_IP_CONV_CHAIN_NORMAL,
            &potResult
        );

        /*
         * Read second ADC input:
         * ADC1_P1 / Channel 1
         * NTC2: ADC1_S8  / Channel 32 */
        /* NTC3: ADC1_S9  / Channel 33 */

        /* PackV1: ADC1_P4  / Channel 4  */
        /* PackV2: ADC1_S11 / Channel 35 */
        /* PackV3: ADC1_S12 / Channel 36 */

        /* POT: ADC1_S10 / Channel 34 */
        Adc_Sar_Ip_GetConvResult(
            ADCHWUNIT_0_INSTANCE,
            BMS_ADC_NTC1_CHANNEL,
            ADC_SAR_IP_CONV_CHAIN_NORMAL,
            &ntc1Result
        );

        Adc_Sar_Ip_GetConvResult(
            ADCHWUNIT_0_INSTANCE,
            BMS_ADC_NTC2_CHANNEL,
            ADC_SAR_IP_CONV_CHAIN_NORMAL,
            &ntc2Result
        );

        Adc_Sar_Ip_GetConvResult(
            ADCHWUNIT_0_INSTANCE,
            BMS_ADC_NTC3_CHANNEL,
            ADC_SAR_IP_CONV_CHAIN_NORMAL,
            &ntc3Result
        );

        Adc_Sar_Ip_GetConvResult(
            ADCHWUNIT_0_INSTANCE,
            BMS_ADC_PACK_V1_CHANNEL,
            ADC_SAR_IP_CONV_CHAIN_NORMAL,
            &packV1Result
        );

        Adc_Sar_Ip_GetConvResult(
            ADCHWUNIT_0_INSTANCE,
            BMS_ADC_PACK_V2_CHANNEL,
            ADC_SAR_IP_CONV_CHAIN_NORMAL,
            &packV2Result
        );

        Adc_Sar_Ip_GetConvResult(
            ADCHWUNIT_0_INSTANCE,
            BMS_ADC_PACK_V3_CHANNEL,
            ADC_SAR_IP_CONV_CHAIN_NORMAL,
            &packV3Result
        );


        if ((potResult.ValidFlag == TRUE) &&
            (ntc1Result.ValidFlag == TRUE) &&
            (ntc2Result.ValidFlag == TRUE) &&
            (ntc3Result.ValidFlag == TRUE) &&
            (packV1Result.ValidFlag == TRUE) &&
            (packV2Result.ValidFlag == TRUE) &&
            (packV3Result.ValidFlag == TRUE))
        {
            g_BmsAdcPotRaw = potResult.ConvData;

            g_BmsAdcNtc1Raw = ntc1Result.ConvData;
            g_BmsAdcNtc2Raw = ntc2Result.ConvData;
            g_BmsAdcNtc3Raw = ntc3Result.ConvData;

            g_BmsAdcPackV1Raw = packV1Result.ConvData;
            g_BmsAdcPackV2Raw = packV2Result.ConvData;
            g_BmsAdcPackV3Raw = packV3Result.ConvData;

            g_BmsAdcValid = TRUE;
        }
        else
        {
            g_BmsAdcValid = FALSE;
        }
    }

    Adc_Sar_Ip_StartConversion(
        ADCHWUNIT_0_INSTANCE,
        ADC_SAR_IP_CONV_CHAIN_NORMAL
    );

    g_BmsAdcConversionStarted = TRUE;
}


uint16 Bms_Adc_GetPotRaw(void)
{
    return g_BmsAdcPotRaw;
}

uint16 Bms_Adc_GetPotVoltageMv(void)
{
    uint32 raw = (uint32)g_BmsAdcPotRaw;

    return (uint16)((raw * 3300U) / 16383U);
}

uint16 Bms_Adc_GetNtc1Raw(void)
{
    return g_BmsAdcNtc1Raw;
}

uint16 Bms_Adc_GetNtc2Raw(void)
{
    return g_BmsAdcNtc2Raw;
}

uint16 Bms_Adc_GetNtc3Raw(void)
{
    return g_BmsAdcNtc3Raw;
}

uint16 Bms_Adc_GetPackV1Raw(void)
{
    return g_BmsAdcPackV1Raw;
}

uint16 Bms_Adc_GetPackV1VoltageMv(void)
{
    uint32 raw = (uint32)g_BmsAdcPackV1Raw;

    return (uint16)((raw * 3300U) / 16383U);
}

uint16 Bms_Adc_GetPackV2Raw(void)
{
    return g_BmsAdcPackV2Raw;
}

uint16 Bms_Adc_GetPackV2VoltageMv(void)
{
    uint32 raw = (uint32)g_BmsAdcPackV2Raw;

    return (uint16)((raw * 3300U) / 16383U);
}

uint16 Bms_Adc_GetPackV3Raw(void)
{
    return g_BmsAdcPackV3Raw;
}

uint16 Bms_Adc_GetPackV3VoltageMv(void)
{
    uint32 raw = (uint32)g_BmsAdcPackV3Raw;

    return (uint16)((raw * 3300U) / 16383U);
}

uint16 Bms_Adc_GetNtc1VoltageMv(void)
{
    uint32 raw = (uint32)g_BmsAdcNtc1Raw;

    return (uint16)(
        (raw * BMS_NTC_CFG_ADC_VREF_MV) /
        BMS_NTC_CFG_ADC_FULL_SCALE
    );
}
