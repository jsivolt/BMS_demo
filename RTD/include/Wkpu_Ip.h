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

#ifndef WKPU_IP_H
#define WKPU_IP_H

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
==================================================================================================*/
#include "Wkpu_Ip_Cfg.h"

#if (STD_ON == WKPU_IP_USED)
    #if (defined (WKPU_IP_ENABLE_USER_MODE_SUPPORT))
        #if (STD_ON == WKPU_IP_ENABLE_USER_MODE_SUPPORT)
            #include "Reg_eSys.h"
        #endif
    #endif
#endif /* WKPU_IP_USED */
/*==================================================================================================
*                                SOURCE FILE VERSION INFORMATION
==================================================================================================*/
#define WKPU_IP_VENDOR_ID                      43
#define WKPU_IP_AR_RELEASE_MAJOR_VERSION       4
#define WKPU_IP_AR_RELEASE_MINOR_VERSION       9
#define WKPU_IP_AR_RELEASE_REVISION_VERSION    0
#define WKPU_IP_SW_MAJOR_VERSION               7
#define WKPU_IP_SW_MINOR_VERSION               0
#define WKPU_IP_SW_PATCH_VERSION               1

/*==================================================================================================
*                                       FILE VERSION CHECKS
==================================================================================================*/
/* Check if source file and ICU header file are of the same vendor */
#if (WKPU_IP_VENDOR_ID != WKPU_IP_CFG_VENDOR_ID)
    #error "Wkpu_Ip.h and Wkpu_Ip_Cfg.h have different vendor IDs"
#endif
/* Check if source file and ICU header file are of the same AutoSar version */
#if ((WKPU_IP_AR_RELEASE_MAJOR_VERSION  != WKPU_IP_CFG_AR_RELEASE_MAJOR_VERSION) || \
     (WKPU_IP_AR_RELEASE_MINOR_VERSION  != WKPU_IP_CFG_AR_RELEASE_MINOR_VERSION) || \
     (WKPU_IP_AR_RELEASE_REVISION_VERSION   != WKPU_IP_CFG_AR_RELEASE_REVISION_VERSION))
    #error "AutoSar Version Numbers of Wkpu_Ip.h and Wkpu_Ip_Cfg.h are different"
#endif
/* Check if source file and ICU header file are of the same Software version */
#if ((WKPU_IP_SW_MAJOR_VERSION  != WKPU_IP_CFG_SW_MAJOR_VERSION) || \
     (WKPU_IP_SW_MINOR_VERSION  != WKPU_IP_CFG_SW_MINOR_VERSION) || \
     (WKPU_IP_SW_PATCH_VERSION  != WKPU_IP_CFG_SW_PATCH_VERSION))
#error "Software Version Numbers of Wkpu_Ip.h and Wkpu_Ip_Cfg.h are different"
#endif

#ifndef DISABLE_MCAL_INTERMODULE_ASR_CHECK
    #if (STD_ON == WKPU_IP_USED)
        #if (defined (WKPU_IP_ENABLE_USER_MODE_SUPPORT))
            #if (STD_ON == WKPU_IP_ENABLE_USER_MODE_SUPPORT)
            /* Check if header file and Reg_eSys.h file are of the same Autosar version */
                #if ((WKPU_IP_AR_RELEASE_MAJOR_VERSION != REG_ESYS_AR_RELEASE_MAJOR_VERSION) || \
                    (WKPU_IP_AR_RELEASE_MINOR_VERSION != REG_ESYS_AR_RELEASE_MINOR_VERSION))
                    #error "AutoSar Version Numbers of Ftm_Icu_Ip.h and Reg_eSys.h are different"
                #endif
            #endif
        #endif
    #endif /* WKPU_IP_USED */
#endif

/*==================================================================================================
*                                            CONSTANTS
==================================================================================================*/

/*==================================================================================================
*                                        DEFINES AND MACROS
==================================================================================================*/
#if (STD_ON == WKPU_IP_USED)

#if (defined WKPU_CONFIG_EXT)
#define ICU_START_SEC_CONFIG_DATA_UNSPECIFIED
#include "Icu_MemMap.h"

/* Macro used to import WKPU generated configurations. */
WKPU_CONFIG_EXT

#define ICU_STOP_SEC_CONFIG_DATA_UNSPECIFIED
#include "Icu_MemMap.h"
#endif
/*==================================================================================================
*                                              ENUMS
==================================================================================================*/

/*==================================================================================================
*                                  STRUCTURES AND OTHER TYPEDEFS
==================================================================================================*/

/*==================================================================================================
*                                  GLOBAL VARIABLE DECLARATIONS
==================================================================================================*/

#if (WKPU_IP_NO_CACHE_USED == STD_ON)
    #define ICU_START_SEC_VAR_CLEARED_UNSPECIFIED_NO_CACHEABLE
#else
    #define ICU_START_SEC_VAR_CLEARED_UNSPECIFIED
#endif
#include "Icu_MemMap.h"

/* Table of initialized WKPU channels */
extern Wkpu_Ip_State Wkpu_Ip_u32ChState[WKPU_IP_NUM_OF_CHANNELS_USED];

#if (WKPU_IP_NO_CACHE_USED == STD_ON)
    #define ICU_STOP_SEC_VAR_CLEARED_UNSPECIFIED_NO_CACHEABLE
#else
    #define ICU_STOP_SEC_VAR_CLEARED_UNSPECIFIED
#endif
#include "Icu_MemMap.h"

#if (WKPU_IP_NO_CACHE_USED == STD_ON)
    #define ICU_START_SEC_VAR_INIT_8_NO_CACHEABLE
#else
    #define ICU_START_SEC_VAR_INIT_8
#endif
#include "Icu_MemMap.h"

/* This array stores the positions in the Wkpu_Ip_u32ChState array of the configured Wkpu channels. */
extern uint8 Wkpu_Ip_IndexInChState[WKPU_IP_NUM_OF_CHANNELS];

#if (WKPU_IP_NO_CACHE_USED == STD_ON)
    #define ICU_STOP_SEC_VAR_INIT_8_NO_CACHEABLE
#else
    #define ICU_STOP_SEC_VAR_INIT_8
#endif
#include "Icu_MemMap.h"

#define ICU_START_SEC_CONST_UNSPECIFIED
#include "Icu_MemMap.h"

extern WKPU_Type * const Wkpu_Ip_apxBase[];

#define ICU_STOP_SEC_CONST_UNSPECIFIED
#include "Icu_MemMap.h"
/*==================================================================================================
*                                      FUNCTION PROTOTYPES
==================================================================================================*/
#define ICU_START_SEC_CODE
#include "Icu_MemMap.h"

/**
 * @brief        Enables the interrupt of WKPU channel
 * @details      This function enables WKPU Channel Interrupt by:
 *               - Clearing any pending interrupt status flag
 *               - Enabling interrupt request via IRER register
 *               - Enabling wakeup request via WRER register
 *
 * @param[in]    instance    WKPU instance
 * @param[in]    hwChannel   WKPU Hardware channel
 *
 * @return       void
 *
 */
void Wkpu_Ip_EnableInterrupt(uint8 instance, uint8 hwChannel);

/**
 * @brief        Disables the interrupt of WKPU channel
 * @details      This function disables WKPU Channel Interrupt by:
 *               - Disabling interrupt request via IRER register
 *               - Disabling wakeup request via WRER register
 *
 * @param[in]    instance    WKPU instance
 * @param[in]    hwChannel   WKPU Hardware channel
 *
 * @return       void
 *
 */
void Wkpu_Ip_DisableInterrupt(uint8 instance, uint8 hwChannel);

/**
 * @brief        Initializes WKPU peripheral with user configuration
 * @details      This function initializes WKPU by performing the following steps for each channel:
 *               - Saves callback information to state structure
 *               - Handles standby wakeup support (if enabled)
 *               - Disables interrupt request (except when waking from standby)
 *               - Configures filter enable/disable
 *               - Sets activation condition (edge events)
 *               - Marks channel as initialized
 *
 * @param[in]    instance     WKPU instance number
 * @param[in]    userConfig   Pointer to channel configuration structure
 *
 * @return       Wkpu_Ip_StatusType
 *
 */
Wkpu_Ip_StatusType Wkpu_Ip_Init(uint8 instance, const Wkpu_Ip_IrqConfigType* userConfig);

/**
 * @brief        Resets WKPU configuration to default state
 * @details      This function resets WKPU configuration by performing the following steps for all channels:
 *               - Disables IRQ interrupt for each channel
 *               - Clears Wakeup/Interrupt Filter Enable Register (WIFER)
 *               - Clears edge event enable registers (WIREER, WIFEER)
 *               - Clears interrupt status flags (WISR)
 *
 * @param[in]    instance    WKPU instance number
 *
 * @return       Wkpu_Ip_StatusType
 *
 */
Wkpu_Ip_StatusType Wkpu_Ip_DeInit(uint8 instance);

/**
 * @brief        Sets activation condition of WKPU channel
 * @details      This function enables the requested activation condition (rising, falling or both edges)
 *               for corresponding WKPU channels by configuring the edge event enable registers:
 *               - WKPU_IP_RISING_EDGE: Enables rising edge detection only
 *               - WKPU_IP_FALLING_EDGE: Enables falling edge detection only
 *               - WKPU_IP_BOTH_EDGES: Enables both rising and falling edge detection
 *               - WKPU_IP_NONE_EDGE: Disables all edge detection
 *
 * @param[in]    instance    WKPU instance
 * @param[in]    hwChannel   WKPU Hardware channel
 * @param[in]    edge        Edge type for activation (Wkpu_Ip_EdgeType)
 *
 * @return       void
 *
 * @note         This function calls Wkpu_Ip_EnableRisingEdge and Wkpu_Ip_EnableFallingEdge
 *               to configure the appropriate edge detection registers
 *
 */
void Wkpu_Ip_SetActivationCondition(uint8 instance, uint8 hwChannel, Wkpu_Ip_EdgeType edge);

/**
 * @brief        Gets the input state of WKPU channel
 * @details      This function checks if interrupt flags for corresponding WKPU channel is set then
 *               clears the interrupt flag and returns the value as TRUE. The function performs:
 *               - Reads the interrupt status flag (WISR) for the specified channel
 *               - Checks if interrupt request is disabled (IRER = 0) to avoid conflict
 *               - Clears the status flag if conditions are met
 *               - Returns channel active state
 *
 * @param[in]    instance    WKPU instance
 * @param[in]    hwChannel   WKPU Hardware channel
 *
 * @return       boolean
 *               - TRUE: Channel is active (interrupt flag was set and cleared)
 *               - FALSE: Channel is in idle state
 *
 * @note         Function only returns TRUE when WISR flag is set and IRER is disabled
 *               Status flag is automatically cleared when function returns TRUE
 *
 */
boolean Wkpu_Ip_GetInputState(uint8 instance, uint8 hwChannel);

/**
 * @brief      ICU driver function that enables notification for WKPU hardware channel.
 * @details    This function:
 *             - Sets the notification Enable to TRUE
 *             - Allows calling notification function when an interrupt occurs
 *
 * @param[in]    hwChannel   WKPU hardware channel
 *
 * @return     void
 *
 */
void Wkpu_Ip_EnableNotification(uint8 hwChannel);

/**
 * @brief      ICU driver function that disables notification for WKPU hardware channel.
 * @details    This function:
 *             - Sets the notification Enable to FALSE
 *             - Do not Allow calling notification function when an interrupt occurs
 *
 * @param[in]    hwChannel   WKPU hardware channel
 *
 * @return     void
 *
 */
void Wkpu_Ip_DisableNotification(uint8 hwChannel);


#if (defined (WKPU_IP_NMI_API) && (STD_ON == WKPU_IP_NMI_API))
/**
 * @brief        Initializes NMI part of WKPU peripheral
 * @details      This function initializes the NMI functionality of WKPU by performing the following steps:
 *               - Deinitializes existing NMI configuration to ensure clean state
 *               - Calculates the number of channels to configure (limited by available cores)
 *               - Configures each NMI core with specified settings including:
 *                 • Destination source configuration
 *                 • Wake-up request enable/disable
 *                 • Glitch filter configuration
 *                 • Edge event activation condition
 *                 • Configuration lock settings
 *
 * @param[in]    instance    WKPU instance number
 * @param[in]    userConfig  Pointer to NMI configuration structure containing channel settings
 *
 * @return       Wkpu_Ip_StatusType
 *               - WKPU_IP_SUCCESS: NMI initialization completed successfully
 *               - WKPU_IP_ERROR: Initialization failed (from deinit step)
 *
 */
Wkpu_Ip_StatusType Wkpu_Ip_InitNMI(uint8 instance, const Wkpu_Ip_IrqConfigType* userConfig);

/**
 * @brief        Deinitializes NMI part of WKPU peripheral
 * @details      This function resets NMI configuration to default state by performing the following steps
 *               for each supported NMI core:
 *               - Checks if NMI configuration is locked (cannot modify if locked)
 *               - Clears status flags and overrun flags (NSR register)
 *               - Disables edge event detection (NREE/NFEE bits)
 *               - Disables glitch filter (NFE bits)
 *               - Disables wake-up request (NWRE bits)
 *               - Resets destination source to NONE (NDSS bits, if supported)
 *               Supports different core ranges with appropriate register sets.
 *
 * @param[in]    instance    WKPU instance number
 *
 * @return       Wkpu_Ip_StatusType
 *               - WKPU_IP_SUCCESS: NMI deinitialization completed successfully
 *               - WKPU_IP_ERROR: Deinitialization failed (one or more cores are locked)
 *
 */
Wkpu_Ip_StatusType Wkpu_Ip_DeinitNMI(uint8 instance);
#endif /* STD_ON == WKPU_IP_NMI_API */

#define ICU_STOP_SEC_CODE
#include "Icu_MemMap.h"

#endif /* WKPU_IP_USED */

#ifdef __cplusplus
}
#endif

/** @} */

#endif  /* WKPU_IP_H */
