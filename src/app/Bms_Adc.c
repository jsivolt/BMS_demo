#include "Bms_Adc.h"

#include "Adc_Sar_Ip.h"
#include "Adc_Sar_Ip_PBcfg.h"
#include "Adc_Sar_Ip_Cfg.h"
#include "Adc_Sar_Ip_CfgDefines.h"

#define BMS_ADC_POT_CHANNEL    (34U)

/* Latest ADC raw value */
static volatile uint16 g_BmsAdcPotRaw = 0U;

/* TRUE when the last acquisition produced a valid conversion result */
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

    return (status == ADC_SAR_IP_STATUS_SUCCESS) ? (Std_ReturnType)E_OK : (Std_ReturnType)E_NOT_OK;
}


void Bms_Adc_MainFunction(void)
{
    Adc_Sar_Ip_ChanResultType result;

    if (g_BmsAdcConversionStarted == TRUE)
    {
        Adc_Sar_Ip_GetConvResult(
            ADCHWUNIT_0_INSTANCE,
            BMS_ADC_POT_CHANNEL,
            ADC_SAR_IP_CONV_CHAIN_NORMAL,
            &result
        );

        if (result.ValidFlag == TRUE)
        {
            g_BmsAdcPotRaw = result.ConvData;
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
