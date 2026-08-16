/*==================================================================================================
*   Project              : RTD AUTOSAR 4.9
*   Platform             : CORTEXM
*   Peripheral           : ADC_SAR
*   Dependencies         : none
*
*   Autosar Version      : 4.9.0
*   Autosar Revision     : ASR_REL_4_9_REV_0000
*   Autosar Conf.Variant :
*   SW Version           : 7.0.1
*   Build Version        : S32K3_RTD_7_0_1_D2602_ASR_REL_4_9_REV_0000_20260206
*
*   Copyright 2020 - 2026 NXP
*
*   NXP Confidential and Proprietary. This software is owned or controlled by NXP and may only be 
*   used strictly in accordance with the applicable license terms. By expressly 
*   accepting such terms or by downloading, installing, activating and/or otherwise 
*   using the software, you are agreeing that you have read, and that you agree to 
*   comply with and are bound by, such license terms. If you do not agree to be 
*   bound by the applicable license terms, then you may not retain, install,
*   activate or otherwise use the software.
==================================================================================================*/

/**
*   @file
*
*   @addtogroup adc_sar_ip_config Adc Sar IPL Configuration
*   @{
*/

#ifdef __cplusplus
extern "C"{
#endif

/*==================================================================================================
*                                        INCLUDE FILES
* 1) system and project includes
* 2) needed interfaces from external units
* 3) internal and external interfaces from this unit
==================================================================================================*/
#include "Adc_Sar_Ip_PBcfg.h"

/*==================================================================================================
*                              SOURCE FILE VERSION INFORMATION
==================================================================================================*/
#define ADC_SAR_IP_VENDOR_ID_PBCFG_C                     43
#define ADC_SAR_IP_AR_RELEASE_MAJOR_VERSION_PBCFG_C      4
#define ADC_SAR_IP_AR_RELEASE_MINOR_VERSION_PBCFG_C      9
#define ADC_SAR_IP_AR_RELEASE_REVISION_VERSION_PBCFG_C   0
#define ADC_SAR_IP_SW_MAJOR_VERSION_PBCFG_C              7
#define ADC_SAR_IP_SW_MINOR_VERSION_PBCFG_C              0
#define ADC_SAR_IP_SW_PATCH_VERSION_PBCFG_C              1

/*==================================================================================================
*                                     FILE VERSION CHECKS
==================================================================================================*/
/* Check if Adc_Sar_Ip_PBcfg.c file and Adc_Sar_Ip_PBcfg.h file are of the same vendor */
#if (ADC_SAR_IP_VENDOR_ID_PBCFG_C != ADC_SAR_IP_VENDOR_ID_PBCFG)
    #error "Adc_Sar_Ip_PBcfg.c and Adc_Sar_Ip_PBcfg.h have different vendor ids"
#endif

/* Check if Adc_Sar_Ip_PBcfg.c file and Adc_Sar_Ip_PBcfg.h file are of the same Autosar version */
#if ((ADC_SAR_IP_AR_RELEASE_MAJOR_VERSION_PBCFG_C != ADC_SAR_IP_AR_RELEASE_MAJOR_VERSION_PBCFG) || \
     (ADC_SAR_IP_AR_RELEASE_MINOR_VERSION_PBCFG_C != ADC_SAR_IP_AR_RELEASE_MINOR_VERSION_PBCFG) || \
     (ADC_SAR_IP_AR_RELEASE_REVISION_VERSION_PBCFG_C != ADC_SAR_IP_AR_RELEASE_REVISION_VERSION_PBCFG) \
    )
    #error "AutoSar Version Numbers of Adc_Sar_Ip_PBcfg.c and Adc_Sar_Ip_PBcfg.h are different"
#endif

/* Check if Adc_Sar_Ip_PBcfg.c file and Adc_Sar_Ip_PBcfg.h file are of the same Software version */
#if ((ADC_SAR_IP_SW_MAJOR_VERSION_PBCFG_C != ADC_SAR_IP_SW_MAJOR_VERSION_PBCFG) || \
     (ADC_SAR_IP_SW_MINOR_VERSION_PBCFG_C != ADC_SAR_IP_SW_MINOR_VERSION_PBCFG) || \
     (ADC_SAR_IP_SW_PATCH_VERSION_PBCFG_C != ADC_SAR_IP_SW_PATCH_VERSION_PBCFG) \
    )
  #error "Software Version Numbers of Adc_Sar_Ip_PBcfg.c and Adc_Sar_Ip_PBcfg.h are different"
#endif

/*==================================================================================================
*                          LOCAL TYPEDEFS (STRUCTURES, UNIONS, ENUMS)
==================================================================================================*/

/*==================================================================================================
*                                       LOCAL MACROS
==================================================================================================*/

/*==================================================================================================
*                                      LOCAL CONSTANTS
==================================================================================================*/

#define ADC_START_SEC_CONFIG_DATA_UNSPECIFIED
#include "Adc_MemMap.h"



const Adc_Sar_Ip_ConfigType AdcHwUnit_0 =
{
    ADC_SAR_IP_CONV_MODE_ONESHOT, /* ConvMode */
#if (ADC_SAR_IP_SET_RESOLUTION == STD_ON)
    ADC_SAR_IP_RESOLUTION_14, /* AdcResolution */
    (boolean)FALSE, /* BypassResolution */
#endif /* (ADC_SAR_IP_SET_RESOLUTION == STD_ON) */
    ADC_SAR_IP_CLK_FULL_BUS, /* ClkSelect */
    ADC_SAR_IP_CLK_FULL_BUS, /* CalibrationClkSelect */
#if ADC_SAR_IP_HIGH_SPEED_ENABLE_AVAILABLE
    (boolean)FALSE, /* HighSpeedConvEn */
#endif /* ADC_SAR_IP_HIGH_SPEED_ENABLE_AVAILABLE */
#if ADC_SAR_IP_HAS_CTU
    ADC_SAR_IP_CTU_MODE_DISABLED, /* CtuMode */
#endif /* ADC_SAR_IP_HAS_CTU */
#if (ADC_SAR_IP_EXTERNAL_TRIGGER_ENABLE)
#if ADC_SAR_IP_INJ_EXT_TRIGGER_AVAILABLE
    ADC_SAR_IP_EXT_TRIG_EDGE_DISABLED, /* InjectedEdge */
#endif /* ADC_SAR_IP_INJ_EXT_TRIGGER_AVAILABLE */
#endif /* (ADC_SAR_IP_EXTERNAL_TRIGGER_ENABLE) */
#if (ADC_SAR_IP_EXTERNAL_TRIGGER_ENABLE)
#if ADC_SAR_IP_EXT_TRIGGER_AVAILABLE
    ADC_SAR_IP_EXT_TRIG_EDGE_DISABLED, /* ExtTrigger */
    (boolean)FALSE, /* NormalExtTrgEn */
#if ADC_SAR_IP_AUX_EXT_TRIGGER_AVAILABLE
    (boolean)FALSE, /* NormalAuxExtTrgEn */
#endif /* ADC_SAR_IP_AUX_EXT_TRIGGER_AVAILABLE */
#endif /* ADC_SAR_IP_EXT_TRIGGER_AVAILABLE */
#endif /* (ADC_SAR_IP_EXTERNAL_TRIGGER_ENABLE) */
#if ADC_SAR_IP_CONVERSION_TIMING_AVAILABLE
    { 22U, 22U, 22U }, /* SampleTimeArr */
#endif /* ADC_SAR_IP_CONVERSION_TIMING_AVAILABLE */
#if ADC_SAR_IP_PRESAMPLING_AVAILABLE
    (boolean)FALSE, /* BypassSampling */
    { ADC_SAR_IP_PRESAMPLE_VREFL, ADC_SAR_IP_PRESAMPLE_VREFL, ADC_SAR_IP_PRESAMPLE_VREFL }, /* PresamplingSourceArr */
#endif /* ADC_SAR_IP_PRESAMPLING_AVAILABLE */
    (boolean)FALSE, /* AutoClockOff */
    (boolean)FALSE, /* OverwriteEnable */
    ADC_SAR_IP_DATA_ALIGNED_RIGHT, /* DataAlign */
#if ADC_SAR_IP_DECODE_DELAY_AVAILABLE
    0U, /* DecodeDelay */
#endif /* ADC_SAR_IP_DECODE_DELAY_AVAILABLE */
    0U, /* PowerDownDelay */
#if (STD_ON == ADC_SAR_IP_SELFTEST_ENABLED)
    NULL_PTR, /* SelfTestThresholdConfig */
#endif /* (STD_ON == ADC_SAR_IP_SELFTEST_ENABLED) */
#if ADC_SAR_IP_AVERAGING_AVAILABLE
    (boolean)FALSE, /* AvgEn */
    ADC_SAR_IP_AVG_4_CONV, /* AvgSel */
#endif /* ADC_SAR_IP_AVERAGING_AVAILABLE */
    0U, /* UsrOffset */
    0U, /* UsrGain */
    (boolean)FALSE, /* DmaEnable */
    ADC_SAR_IP_DMA_REQ_CLEAR_ON_ACK, /* DmaClearSource */
    { 0x12U, 0x1FU, 0x0U }, /* ChanMaskNormal */
    { 0x0U, 0x0U, 0x0U }, /* ChanMaskInjected */
    0U, /* NumChannels */
    NULL_PTR, /* ChannelConfigsPtr */
#if (STD_ON == ADC_SAR_IP_WDG_ENABLED)
    0U, /* NumWdgThresholds */
    NULL_PTR, /* WdgThresholds */
#endif /* (STD_ON == ADC_SAR_IP_WDG_ENABLED) */
#if (STD_ON == ADC_SAR_IP_ECH_ENABLED)
    NULL_PTR, /* EndOfNormalChainNotification */
#endif /* (STD_ON == ADC_SAR_IP_ECH_ENABLED) */
#if (STD_ON == ADC_SAR_IP_JECH_ENABLED)
    NULL_PTR, /* EndOfInjectedChainNotification */
#endif /* (STD_ON == ADC_SAR_IP_JECH_ENABLED) */
#if ADC_SAR_IP_HAS_CTU
#if (STD_ON == ADC_SAR_IP_EOCTU_ENABLED)
    NULL_PTR, /* EndOfCtuConversionNotification */
#endif /* (STD_ON == ADC_SAR_IP_EOCTU_ENABLED) */
#endif /* ADC_SAR_IP_HAS_CTU */
#if (STD_ON == ADC_SAR_IP_EOC_ENABLED)
    NULL_PTR, /* EndOfConvNotification */
#endif /* (STD_ON == ADC_SAR_IP_EOC_ENABLED) */
#if (STD_ON == ADC_SAR_IP_WDG_ENABLED)
    NULL_PTR /* WdgOutOfRangeNotification */
#endif /* (STD_ON == ADC_SAR_IP_WDG_ENABLED) */
};


#define ADC_STOP_SEC_CONFIG_DATA_UNSPECIFIED
#include "Adc_MemMap.h"

/*==================================================================================================
*                                      LOCAL VARIABLES
==================================================================================================*/

/*==================================================================================================
*                                      GLOBAL CONSTANTS
==================================================================================================*/

/*==================================================================================================
*                                      GLOBAL VARIABLES
==================================================================================================*/

/*==================================================================================================
*                                   LOCAL FUNCTION PROTOTYPES
==================================================================================================*/

/*==================================================================================================
*                                       LOCAL FUNCTIONS
==================================================================================================*/

/*==================================================================================================
*                                       GLOBAL FUNCTIONS
==================================================================================================*/

#ifdef __cplusplus
}
#endif

/** @} */

