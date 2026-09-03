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
#include "OsIf.h"
#include "Adc_Sar_Ip_Irq.h"

/*==================================================================================================
*                              SOURCE FILE VERSION INFORMATION
==================================================================================================*/
#define ADC_SAR_IP_VENDOR_ID_IRQ_C                      43
#define ADC_SAR_IP_AR_RELEASE_MAJOR_VERSION_IRQ_C       4
#define ADC_SAR_IP_AR_RELEASE_MINOR_VERSION_IRQ_C       9
#define ADC_SAR_IP_AR_RELEASE_REVISION_VERSION_IRQ_C    0
#define ADC_SAR_IP_SW_MAJOR_VERSION_IRQ_C               7
#define ADC_SAR_IP_SW_MINOR_VERSION_IRQ_C               0
#define ADC_SAR_IP_SW_PATCH_VERSION_IRQ_C               1

/*==================================================================================================
*                                     FILE VERSION CHECKS
==================================================================================================*/
#ifndef DISABLE_MCAL_INTERMODULE_ASR_CHECK
/* Check if Adc_Sar_Ip_Irq.c file and OsIf.h file are of the same Autosar version */
#if ((ADC_SAR_IP_AR_RELEASE_MAJOR_VERSION_IRQ_C != OSIF_AR_RELEASE_MAJOR_VERSION) || \
     (ADC_SAR_IP_AR_RELEASE_MINOR_VERSION_IRQ_C != OSIF_AR_RELEASE_MINOR_VERSION)    \
    )
    #error "AutoSar Version Numbers of Adc_Sar_Ip_Irq.c and OsIf.h are different"
#endif
#endif /* DISABLE_MCAL_INTERMODULE_ASR_CHECK */

/* Check if Adc_Sar_Ip_Irq.c file and Adc_Sar_Ip_Irq.h file are of the same vendor */
#if (ADC_SAR_IP_VENDOR_ID_IRQ_C != ADC_SAR_IP_VENDOR_ID_IRQ)
    #error "Adc_Sar_Ip_Irq.c and Adc_Sar_Ip_Irq.h have different vendor ids"
#endif

/* Check if Adc_Sar_Ip_Irq.c file and Adc_Sar_Ip_Irq.h file are of the same Autosar version */
#if ((ADC_SAR_IP_AR_RELEASE_MAJOR_VERSION_IRQ_C != ADC_SAR_IP_AR_RELEASE_MAJOR_VERSION_IRQ) || \
     (ADC_SAR_IP_AR_RELEASE_MINOR_VERSION_IRQ_C != ADC_SAR_IP_AR_RELEASE_MINOR_VERSION_IRQ) || \
     (ADC_SAR_IP_AR_RELEASE_REVISION_VERSION_IRQ_C != ADC_SAR_IP_AR_RELEASE_REVISION_VERSION_IRQ) \
    )
    #error "AutoSar Version Numbers of Adc_Sar_Ip_Irq.c and Adc_Sar_Ip_Irq.h are different"
#endif

/* Check if Adc_Sar_Ip_Irq.c file and Adc_Sar_Ip_Irq.h file are of the same Software version */
#if ((ADC_SAR_IP_SW_MAJOR_VERSION_IRQ_C != ADC_SAR_IP_SW_MAJOR_VERSION_IRQ) || \
     (ADC_SAR_IP_SW_MINOR_VERSION_IRQ_C != ADC_SAR_IP_SW_MINOR_VERSION_IRQ) || \
     (ADC_SAR_IP_SW_PATCH_VERSION_IRQ_C != ADC_SAR_IP_SW_PATCH_VERSION_IRQ) \
    )
  #error "Software Version Numbers of Adc_Sar_Ip_Irq.c and Adc_Sar_Ip_Irq.h are different"
#endif

#if (ADC_SAR_IP_IS_USED == STD_ON)

/*==================================================================================================
*                                        GLOBAL CONSTANTS
==================================================================================================*/

/*==================================================================================================
*                                        GLOBAL FUNCTIONS
==================================================================================================*/
#define ADC_START_SEC_CODE
#include "Adc_MemMap.h"

#if (ADC_SAR_IP_INSTANCE_COUNT >= 1U)
ISR(Adc_Sar_0_Isr)
{
    /* Call IRQ handler with the correct instance */
    Adc_Sar_Ip_IRQHandler(0UL);
}
#endif /* (ADC_SAR_IP_INSTANCE_COUNT >= 1U) */
#if (ADC_SAR_IP_INSTANCE_COUNT >= 2U)
ISR(Adc_Sar_1_Isr)
{
    /* Call IRQ handler with the correct instance */
    Adc_Sar_Ip_IRQHandler(1UL);
}
#endif /* (ADC_SAR_IP_INSTANCE_COUNT >= 2U) */
#if (ADC_SAR_IP_INSTANCE_COUNT >= 3U)
ISR(Adc_Sar_2_Isr)
{
    /* Call IRQ handler with the correct instance */
    Adc_Sar_Ip_IRQHandler(2UL);
}
#endif /* (ADC_SAR_IP_INSTANCE_COUNT >= 3U) */
#if (ADC_SAR_IP_INSTANCE_COUNT >= 4U)
ISR(Adc_Sar_3_Isr)
{
    /* Call IRQ handler with the correct instance */
    Adc_Sar_Ip_IRQHandler(3UL);
}
#endif /* (ADC_SAR_IP_INSTANCE_COUNT >= 4U) */
#if (ADC_SAR_IP_INSTANCE_COUNT >= 5U)
ISR(Adc_Sar_4_Isr)
{
    /* Call IRQ handler with the correct instance */
    Adc_Sar_Ip_IRQHandler(4UL);
}
#endif /* (ADC_SAR_IP_INSTANCE_COUNT >= 5U) */
#if (ADC_SAR_IP_INSTANCE_COUNT >= 6U)
ISR(Adc_Sar_5_Isr)
{
    /* Call IRQ handler with the correct instance */
    Adc_Sar_Ip_IRQHandler(5UL);
}
#endif /* (ADC_SAR_IP_INSTANCE_COUNT >= 6U) */
#if (ADC_SAR_IP_INSTANCE_COUNT >= 7U)
ISR(Adc_Sar_6_Isr)
{
    /* Call IRQ handler with the correct instance */
    Adc_Sar_Ip_IRQHandler(6UL);
}
#endif /* (ADC_SAR_IP_INSTANCE_COUNT >= 7U) */


#define ADC_STOP_SEC_CODE
#include "Adc_MemMap.h"

/*==================================================================================================
*                                        GLOBAL VARIABLES
==================================================================================================*/

/*==================================================================================================
*                                        LOCAL FUNCTIONS
==================================================================================================*/

#endif /* (ADC_SAR_IP_IS_USED == STD_ON) */

#ifdef __cplusplus
}
#endif

/** @} */
