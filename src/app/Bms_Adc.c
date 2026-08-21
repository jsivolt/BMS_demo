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

#define BMS_ADC_BUS1_CHANNEL          (0U)  /* ADC0_P0 */
#define BMS_ADC_BUS2_CHANNEL          (1U)  /* ADC0_P1 */
#define BMS_ADC_BUS3_CHANNEL          (3U)  /* ADC0_P3 */
#define BMS_ADC_BUS_SPARE_CHANNEL     (4U)  /* ADC0_P4 */

/* Latest ADC raw values */
static volatile uint16 g_BmsAdcPotRaw = 0U;

static volatile uint16 g_BmsAdcNtc1Raw = 0U;
static volatile uint16 g_BmsAdcNtc2Raw = 0U;
static volatile uint16 g_BmsAdcNtc3Raw = 0U;

static volatile uint16 g_BmsAdcPackV1Raw = 0U;
static volatile uint16 g_BmsAdcPackV2Raw = 0U;
static volatile uint16 g_BmsAdcPackV3Raw = 0U;

static volatile uint16 g_BmsAdcBus1Raw = 0U;
static volatile uint16 g_BmsAdcBus2Raw = 0U;
static volatile uint16 g_BmsAdcBus3Raw = 0U;
static volatile uint16 g_BmsAdcBusSpareRaw = 0U;

/* TRUE when the last acquisition produced valid conversion results, per ADC unit */
static volatile boolean g_BmsAdcPackValid = FALSE;
static volatile boolean g_BmsAdcBusValid = FALSE;

/* Indicates whether a conversion has already been started, per ADC unit */
static boolean g_BmsAdcAdc1ConversionStarted = FALSE;
static boolean g_BmsAdcAdc0ConversionStarted = FALSE;


Std_ReturnType Bms_Adc_Init(void)
{
    Adc_Sar_Ip_StatusType status;

    /* ADC1 */
    status = Adc_Sar_Ip_Init(
        ADCHWUNIT_0_INSTANCE,
        &AdcHwUnit_0
    );

    if (status != ADC_SAR_IP_STATUS_SUCCESS)
    {
        return (Std_ReturnType)E_NOT_OK;
    }

    status = Adc_Sar_Ip_DoCalibration(
        ADCHWUNIT_0_INSTANCE
    );

    if (status != ADC_SAR_IP_STATUS_SUCCESS)
    {
        return (Std_ReturnType)E_NOT_OK;
    }

    /* ADC0 */
    status = Adc_Sar_Ip_Init(
        ADCHWUNIT_1_INSTANCE,
        &AdcHwUnit_1
    );

    if (status != ADC_SAR_IP_STATUS_SUCCESS)
    {
        return (Std_ReturnType)E_NOT_OK;
    }

    status = Adc_Sar_Ip_DoCalibration(
        ADCHWUNIT_1_INSTANCE
    );

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

    Adc_Sar_Ip_ChanResultType bus1Result;
    Adc_Sar_Ip_ChanResultType bus2Result;
    Adc_Sar_Ip_ChanResultType bus3Result;
    Adc_Sar_Ip_ChanResultType busSpareResult;

    if (g_BmsAdcAdc1ConversionStarted == TRUE)
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

            g_BmsAdcPackValid = TRUE;
        }
        else
        {
            g_BmsAdcPackValid = FALSE;
        }
    }

    if (g_BmsAdcAdc0ConversionStarted == TRUE)
    {
        Adc_Sar_Ip_GetConvResult(
            ADCHWUNIT_1_INSTANCE,
            BMS_ADC_BUS1_CHANNEL,
            ADC_SAR_IP_CONV_CHAIN_NORMAL,
            &bus1Result
        );

        Adc_Sar_Ip_GetConvResult(
            ADCHWUNIT_1_INSTANCE,
            BMS_ADC_BUS2_CHANNEL,
            ADC_SAR_IP_CONV_CHAIN_NORMAL,
            &bus2Result
        );

        Adc_Sar_Ip_GetConvResult(
            ADCHWUNIT_1_INSTANCE,
            BMS_ADC_BUS3_CHANNEL,
            ADC_SAR_IP_CONV_CHAIN_NORMAL,
            &bus3Result
        );

        Adc_Sar_Ip_GetConvResult(
            ADCHWUNIT_1_INSTANCE,
            BMS_ADC_BUS_SPARE_CHANNEL,
            ADC_SAR_IP_CONV_CHAIN_NORMAL,
            &busSpareResult
        );

        if ((bus1Result.ValidFlag == TRUE) &&
            (bus2Result.ValidFlag == TRUE) &&
            (bus3Result.ValidFlag == TRUE) &&
            (busSpareResult.ValidFlag == TRUE))
        {
            g_BmsAdcBus1Raw = bus1Result.ConvData;
            g_BmsAdcBus2Raw = bus2Result.ConvData;
            g_BmsAdcBus3Raw = bus3Result.ConvData;
            g_BmsAdcBusSpareRaw = busSpareResult.ConvData;

            g_BmsAdcBusValid = TRUE;
        }
        else
        {
            g_BmsAdcBusValid = FALSE;
        }
    }

    Adc_Sar_Ip_StartConversion(
        ADCHWUNIT_0_INSTANCE,
        ADC_SAR_IP_CONV_CHAIN_NORMAL
    );

    g_BmsAdcAdc1ConversionStarted = TRUE;

    Adc_Sar_Ip_StartConversion(
        ADCHWUNIT_1_INSTANCE,
        ADC_SAR_IP_CONV_CHAIN_NORMAL
    );

    g_BmsAdcAdc0ConversionStarted = TRUE;
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

uint16 Bms_Adc_GetBus1Raw(void)
{
    return g_BmsAdcBus1Raw;
}

uint16 Bms_Adc_GetBus2Raw(void)
{
    return g_BmsAdcBus2Raw;
}

uint16 Bms_Adc_GetBus3Raw(void)
{
    return g_BmsAdcBus3Raw;
}

uint16 Bms_Adc_GetBusSpareRaw(void)
{
    return g_BmsAdcBusSpareRaw;
}

uint16 Bms_Adc_GetBus1VoltageMv(void)
{
    uint32 raw = (uint32)g_BmsAdcBus1Raw;

    return (uint16)((raw * 3300U) / 16383U);
}

uint16 Bms_Adc_GetBus2VoltageMv(void)
{
    uint32 raw = (uint32)g_BmsAdcBus2Raw;

    return (uint16)((raw * 3300U) / 16383U);
}

uint16 Bms_Adc_GetBus3VoltageMv(void)
{
    uint32 raw = (uint32)g_BmsAdcBus3Raw;

    return (uint16)((raw * 3300U) / 16383U);
}

uint16 Bms_Adc_GetBusSpareVoltageMv(void)
{
    uint32 raw = (uint32)g_BmsAdcBusSpareRaw;

    return (uint16)((raw * 3300U) / 16383U);
}

boolean Bms_Adc_IsPackValid(void)
{
    return g_BmsAdcPackValid;
}

boolean Bms_Adc_IsBusValid(void)
{
    return g_BmsAdcBusValid;
}
