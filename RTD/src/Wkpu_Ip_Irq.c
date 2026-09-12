/*==================================================================================================
*   Project              : RTD AUTOSAR 4.9
*   Platform             : CORTEXM
*   Peripheral           : Emios Siul2 Wkpu LpCmp
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
 *     @file
 *
 *     @addtogroup wkpu_icu_ip WKPU IPL
 *     @{
 */

#ifdef __cplusplus
extern "C"{
#endif

/*==================================================================================================
*                                          INCLUDE FILES
*  1) system and project includes
*  2) needed interfaces from external units
*  3) internal and external interfaces from this unit
==================================================================================================*/
#include "Std_Types.h"
#include "Wkpu_Ip.h"
#include "Wkpu_Ip_Irq.h"

#include "SchM_Icu.h"
/*==================================================================================================
*                                         SOURCE FILE VERSION INFORMATION
==================================================================================================*/

#define WKPU_IP_IRQ_VENDOR_ID_C                    43
#define WKPU_IP_IRQ_AR_RELEASE_MAJOR_VERSION_C     4
#define WKPU_IP_IRQ_AR_RELEASE_MINOR_VERSION_C     9
#define WKPU_IP_IRQ_AR_RELEASE_REVISION_VERSION_C  0
#define WKPU_IP_IRQ_SW_MAJOR_VERSION_C             7
#define WKPU_IP_IRQ_SW_MINOR_VERSION_C             0
#define WKPU_IP_IRQ_SW_PATCH_VERSION_C             1

/*==================================================================================================
*                                       FILE VERSION CHECKS
==================================================================================================*/
#ifndef DISABLE_MCAL_INTERMODULE_ASR_CHECK
    /* Check if header file and Std_Types.h file are of the same Autosar version */
    #if ((WKPU_IP_IRQ_AR_RELEASE_MAJOR_VERSION_C != STD_AR_RELEASE_MAJOR_VERSION) || \
         (WKPU_IP_IRQ_AR_RELEASE_MINOR_VERSION_C != STD_AR_RELEASE_MINOR_VERSION))
        #error "AutoSar Version Numbers of Wkpu_Ip_Irq.c and Std_Types.h are different"
    #endif
    /* Check if this header file and SchM_Icu.h file are of the same Autosar version */
    #if ((WKPU_IP_IRQ_AR_RELEASE_MAJOR_VERSION_C != SCHM_ICU_AR_RELEASE_MAJOR_VERSION) || \
        (WKPU_IP_IRQ_AR_RELEASE_MINOR_VERSION_C != SCHM_ICU_AR_RELEASE_MINOR_VERSION))
        #error "AutoSar Version Numbers of Wkpu_Ip_Irq.c and SchM_Icu.h are different"
    #endif
#endif

/* Check if source file and ICU header file are of the same vendor */
#if (WKPU_IP_IRQ_VENDOR_ID_C != WKPU_IP_VENDOR_ID)
    #error "Wkpu_Ip_Irq.c and Wkpu_Ip.h have different vendor IDs"
#endif
/* Check if source file and ICU header file are of the same AutoSar version */
#if ((WKPU_IP_IRQ_AR_RELEASE_MAJOR_VERSION_C  != WKPU_IP_AR_RELEASE_MAJOR_VERSION) || \
     (WKPU_IP_IRQ_AR_RELEASE_MINOR_VERSION_C  != WKPU_IP_AR_RELEASE_MINOR_VERSION) || \
     (WKPU_IP_IRQ_AR_RELEASE_REVISION_VERSION_C   != WKPU_IP_AR_RELEASE_REVISION_VERSION))
    #error "AutoSar Version Numbers of Wkpu_Ip_Irq.c and Wkpu_Ip.h are different"
#endif
/* Check if source file and ICU header file are of the same Software version */
#if ((WKPU_IP_IRQ_SW_MAJOR_VERSION_C  != WKPU_IP_SW_MAJOR_VERSION) || \
     (WKPU_IP_IRQ_SW_MINOR_VERSION_C  != WKPU_IP_SW_MINOR_VERSION) || \
     (WKPU_IP_IRQ_SW_PATCH_VERSION_C  != WKPU_IP_SW_PATCH_VERSION))
#error "Software Version Numbers of Wkpu_Ip_Irq.c and Wkpu_Ip.h are different"
#endif

/* Check if source file and ICU header file are of the same vendor */
#if (WKPU_IP_IRQ_VENDOR_ID_C != WKPU_IP_IRQ_VENDOR_ID)
    #error "Wkpu_Ip_Irq.c and Wkpu_Ip_Irq.h have different vendor IDs"
#endif
/* Check if source file and ICU header file are of the same AutoSar version */
#if ((WKPU_IP_IRQ_AR_RELEASE_MAJOR_VERSION_C  != WKPU_IP_IRQ_AR_RELEASE_MAJOR_VERSION) || \
     (WKPU_IP_IRQ_AR_RELEASE_MINOR_VERSION_C  != WKPU_IP_IRQ_AR_RELEASE_MINOR_VERSION) || \
     (WKPU_IP_IRQ_AR_RELEASE_REVISION_VERSION_C   != WKPU_IP_IRQ_AR_RELEASE_REVISION_VERSION))
    #error "AutoSar Version Numbers of Wkpu_Ip_Irq.c and Wkpu_Ip_Irq.h are different"
#endif
/* Check if source file and ICU header file are of the same Software version */
#if ((WKPU_IP_IRQ_SW_MAJOR_VERSION_C  != WKPU_IP_IRQ_SW_MAJOR_VERSION) || \
     (WKPU_IP_IRQ_SW_MINOR_VERSION_C  != WKPU_IP_IRQ_SW_MINOR_VERSION) || \
     (WKPU_IP_IRQ_SW_PATCH_VERSION_C  != WKPU_IP_IRQ_SW_PATCH_VERSION))
#error "Software Version Numbers of Wkpu_Ip_Irq.c and Wkpu_Ip_Irq.h are different"
#endif

/*==================================================================================================
*                                        GLOBAL CONSTANTS
==================================================================================================*/

/*==================================================================================================
*                                        GLOBAL FUNCTIONS
==================================================================================================*/

/*==================================================================================================
*                                        GLOBAL VARIABLES
==================================================================================================*/

/*==================================================================================================
*                                    LOCAL FUNCTION PROTOTYPES
==================================================================================================*/
#if (STD_ON == WKPU_IP_USED)

#define ICU_START_SEC_CODE
#include "Icu_MemMap.h"
#if (defined(WKPU_ICU_SINGLE_INTERRUPT) && (WKPU_ICU_SINGLE_INTERRUPT == STD_ON))
/**
 * @brief        Handles callback functions for both HLD and IPL
 * @details      This function manages callback execution for WKPU channel interrupts by:
 *               - First checking if a callback function is registered and calling it
 *               - If no callback is registered, checking for IPL notification function
 *               - Only executes notification if notificationEnable status is TRUE
 *               The function supports both High Level Driver (HLD) and IP Layer (IPL) callbacks.
 *
 * @param[in]    hwChannel   WKPU hardware channel
 *
 * @return       void
 *
 */
static inline void Wkpu_Ip_Callback(uint8 hwChannel);

/**
 * @brief        Processes single interrupt for all WKPU channels
 * @details      This function handles interrupt processing for platforms with only one interrupt line
 *               by checking all WKPU channels and servicing the ones with pending interrupts.
 *               For each channel, it clears the interrupt flag and calls the appropriate callback.
 *
 * @param        void
 *
 * @return       void
 *
 */
static inline void Wkpu_Ip_ProcessSingleInterrupt(void);

#endif /*defined WKPU_ICU_SINGLE_INTERRUPT*/

/*==================================================================================================
*                                        LOCAL FUNCTIONS
==================================================================================================*/


#if (defined(WKPU_ICU_SINGLE_INTERRUPT) && (WKPU_ICU_SINGLE_INTERRUPT == STD_ON))
static inline void Wkpu_Ip_Callback(uint8 hwChannel)
{
    /* Check if callback function (in HLD) is registered and call it */
    if (NULL_PTR != Wkpu_Ip_u32ChState[Wkpu_Ip_IndexInChState[hwChannel]].callback)
    {
        /* Execute registered callback function with stored parameters */
        Wkpu_Ip_u32ChState[Wkpu_Ip_IndexInChState[hwChannel]].callback(Wkpu_Ip_u32ChState[Wkpu_Ip_IndexInChState[hwChannel]].callbackParam, FALSE);
    }
    else
    {
        /* No callback registered - check for IPL notification function */
        if ((NULL_PTR != Wkpu_Ip_u32ChState[Wkpu_Ip_IndexInChState[(uint8)hwChannel]].WkpuChannelNotification) && \
            ((boolean)TRUE == Wkpu_Ip_u32ChState[Wkpu_Ip_IndexInChState[hwChannel]].notificationEnable))
        {
            Wkpu_Ip_u32ChState[Wkpu_Ip_IndexInChState[(uint8)hwChannel]].WkpuChannelNotification();
        }
    }
}

/** @implements   Wkpu_Ip_ProcessSingleInterrupt_Activity */
static inline void Wkpu_Ip_ProcessSingleInterrupt(void)
{
    uint8  u8WkpuChannel;
    uint32 u32ChannelMask   = 0x1U;
    uint32 u32reg_WKPU_WISR = Wkpu_Ip_apxBase[0U]->WISR;
    uint32 u32reg_WKPU_WIER = Wkpu_Ip_apxBase[0U]->IRER;

    /* Select which channels will be serviced - only the enabled irq ones*/
    u32reg_WKPU_WISR &= u32reg_WKPU_WIER;

    /* Process channels 0-31 */
    for (u8WkpuChannel = 0U; u8WkpuChannel < (uint8)WKPU_IP_NUM_OF_CHANNELS_IN_ONE_REG_32; u8WkpuChannel++)
    {
        if ((boolean)TRUE == Wkpu_Ip_u32ChState[Wkpu_Ip_IndexInChState[u8WkpuChannel]].chInit)
        {
            if (0x0U != (u32reg_WKPU_WISR & u32ChannelMask))
            {
                /* Clear pending interrupt serviced */
                Wkpu_Ip_apxBase[0U]->WISR = u32reg_WKPU_WISR & u32ChannelMask;

                /* Execute callback for this channel */
                Wkpu_Ip_Callback(u8WkpuChannel);
            }
        }
        else
        {
            /* If the driver is not initialized, clear the interrupt status flag only for the enabled interrupts and return immediately. */
            if (0x0U != (u32reg_WKPU_WISR & u32ChannelMask))
            {
                /* Clear pending interrupt serviced */
                Wkpu_Ip_apxBase[0U]->WISR = u32reg_WKPU_WISR & u32ChannelMask;
            }
        }
        /* Move to next channel bit */
        u32ChannelMask <<= (uint32)1U;
    }

#ifdef WKPU_IP_64_CH_USED
    /* Channel not initialized - clear pending interrupt without callback */
    u32ChannelMask   = 0x1U;
    u32reg_WKPU_WISR = Wkpu_Ip_apxBase[0U]->WISR_64;
    u32reg_WKPU_WIER = Wkpu_Ip_apxBase[0U]->IRER_64;

    /* Select which channels will be serviced - only the enabled irq ones*/
    u32reg_WKPU_WISR &= u32reg_WKPU_WIER;

    for (u8WkpuChannel = 0U; u8WkpuChannel < (uint8)WKPU_IP_NUM_OF_CHANNELS_IN_ONE_REG_64; u8WkpuChannel++)
    {
        if ((boolean)TRUE == Wkpu_Ip_u32ChState[Wkpu_Ip_IndexInChState[u8WkpuChannel + 32U]].chInit)
        {
            if (0x0U != (u32reg_WKPU_WISR & u32ChannelMask))
            {
                /* Clear pending interrupt serviced */
                Wkpu_Ip_apxBase[0U]->WISR_64 = u32reg_WKPU_WISR & u32ChannelMask;

                /* Execute callback for this channel */
                Wkpu_Ip_Callback(u8WkpuChannel + 32U);
            }
        }
        else
        {
            /* If the driver is not initialized, clear the interrupt status flag only for the enabled interrupts and return immediately. */
            if (0x0U != (u32reg_WKPU_WISR & u32ChannelMask))
            {
                /* Clear pending interrupt serviced */
                Wkpu_Ip_apxBase[0U]->WISR_64 = u32reg_WKPU_WISR & u32ChannelMask;
            }
        }
        u32ChannelMask <<= (uint32)1U;
    }
#endif

}
#endif /*defined WKPU_ICU_SINGLE_INTERRUPT*/
/*==================================================================================================
*                                        GLOBAL FUNCTIONS
==================================================================================================*/

#if (defined(WKPU_ICU_SINGLE_INTERRUPT) && (WKPU_ICU_SINGLE_INTERRUPT == STD_ON))
ISR(WKPU_EXT_IRQ_SINGLE_ISR)
{
    /* Call Processes single interrupt for all WKPU channels. */
    Wkpu_Ip_ProcessSingleInterrupt();
}
#endif   /* defined WKPU_ICU_SINGLE_INTERRUPT */

#define ICU_STOP_SEC_CODE
#include "Icu_MemMap.h"

#endif /* WKPU_IP_USED */

#ifdef __cplusplus
}
#endif

/** @} */

