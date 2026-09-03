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

#ifndef ADC_SAR_IP_TRUSTEDFUNCTIONS_H
#define ADC_SAR_IP_TRUSTEDFUNCTIONS_H

/**
*   @file
*
*   @addtogroup adc_sar_ip Adc Sar IPL
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
#include "Adc_Sar_Ip_CfgDefines.h"

/*==================================================================================================
*                              SOURCE FILE VERSION INFORMATION
==================================================================================================*/
#define ADC_SAR_IP_VENDOR_ID_TRUSTEDFUNCTIONS                      43
#define ADC_SAR_IP_AR_RELEASE_MAJOR_VERSION_TRUSTEDFUNCTIONS       4
#define ADC_SAR_IP_AR_RELEASE_MINOR_VERSION_TRUSTEDFUNCTIONS       9
#define ADC_SAR_IP_AR_RELEASE_REVISION_VERSION_TRUSTEDFUNCTIONS    0
#define ADC_SAR_IP_SW_MAJOR_VERSION_TRUSTEDFUNCTIONS               7
#define ADC_SAR_IP_SW_MINOR_VERSION_TRUSTEDFUNCTIONS               0
#define ADC_SAR_IP_SW_PATCH_VERSION_TRUSTEDFUNCTIONS               1
/*==================================================================================================
*                                     FILE VERSION CHECKS
==================================================================================================*/
/* Check if Adc_Sar_Ip_TrustedFunctions.h file and Adc_Sar_Ip_CfgDefines.h file are of the same vendor */
#if (ADC_SAR_IP_VENDOR_ID_TRUSTEDFUNCTIONS != ADC_SAR_IP_VENDOR_ID_CFGDEFINES)
    #error "Adc_Sar_Ip_TrustedFunctions.h and Adc_Sar_Ip_CfgDefines.h have different vendor ids"
#endif

/* Check if Adc_Sar_Ip_TrustedFunctions.h file and Adc_Sar_Ip_CfgDefines.h file are of the same Autosar version */
#if ((ADC_SAR_IP_AR_RELEASE_MAJOR_VERSION_TRUSTEDFUNCTIONS != ADC_SAR_IP_AR_RELEASE_MAJOR_VERSION_CFGDEFINES) || \
     (ADC_SAR_IP_AR_RELEASE_MINOR_VERSION_TRUSTEDFUNCTIONS != ADC_SAR_IP_AR_RELEASE_MINOR_VERSION_CFGDEFINES) || \
     (ADC_SAR_IP_AR_RELEASE_REVISION_VERSION_TRUSTEDFUNCTIONS != ADC_SAR_IP_AR_RELEASE_REVISION_VERSION_CFGDEFINES) \
    )
    #error "AutoSar Version Numbers of Adc_Sar_Ip_TrustedFunctions.h and Adc_Sar_Ip_CfgDefines.h are different"
#endif

/* Check if Adc_Sar_Ip_TrustedFunctions.h file and Adc_Sar_Ip_CfgDefines.h file are of the same Software version */
#if ((ADC_SAR_IP_SW_MAJOR_VERSION_TRUSTEDFUNCTIONS != ADC_SAR_IP_SW_MAJOR_VERSION_CFGDEFINES) || \
     (ADC_SAR_IP_SW_MINOR_VERSION_TRUSTEDFUNCTIONS != ADC_SAR_IP_SW_MINOR_VERSION_CFGDEFINES) || \
     (ADC_SAR_IP_SW_PATCH_VERSION_TRUSTEDFUNCTIONS != ADC_SAR_IP_SW_PATCH_VERSION_CFGDEFINES) \
    )
  #error "Software Version Numbers of Adc_Sar_Ip_TrustedFunctions.h and Adc_Sar_Ip_CfgDefines.h are different"
#endif

#if (ADC_SAR_IP_IS_USED == STD_ON)

/*==================================================================================================
*                                     FUNCTION PROTOTYPES
==================================================================================================*/
#define ADC_START_SEC_CODE
#include "Adc_MemMap.h"

#define ADC_STOP_SEC_CODE
#include "Adc_MemMap.h"

#endif /* (ADC_SAR_IP_IS_USED == STD_ON) */

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* ADC_SAR_IP_TRUSTEDFUNCTIONS_H */
