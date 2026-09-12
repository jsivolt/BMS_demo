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

#ifndef WKPU_IP_TYPES_H
#define WKPU_IP_TYPES_H

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
                                         INCLUDE FILES
==================================================================================================*/
#include "Wkpu_Ip_Defines.h"

/*==================================================================================================
                               SOURCE FILE VERSION INFORMATION
==================================================================================================*/
#define WKPU_IP_TYPES_VENDOR_ID                     43
#define WKPU_IP_TYPES_AR_RELEASE_MAJOR_VERSION      4
#define WKPU_IP_TYPES_AR_RELEASE_MINOR_VERSION      9
#define WKPU_IP_TYPES_AR_RELEASE_REVISION_VERSION   0
#define WKPU_IP_TYPES_SW_MAJOR_VERSION              7
#define WKPU_IP_TYPES_SW_MINOR_VERSION              0
#define WKPU_IP_TYPES_SW_PATCH_VERSION              1

/*==================================================================================================
                                      FILE VERSION CHECKS
==================================================================================================*/
/* Check if source file and ICU header file are of the same vendor */
#if (WKPU_IP_TYPES_VENDOR_ID != WKPU_IP_DEFINES_VENDOR_ID)
    #error "Wkpu_Ip_Types.h and Wkpu_Ip_Defines.h have different vendor IDs"
#endif
/* Check if source file and ICU header file are of the same AutoSar version */
#if ((WKPU_IP_TYPES_AR_RELEASE_MAJOR_VERSION  != WKPU_IP_DEFINES_AR_RELEASE_MAJOR_VERSION) || \
     (WKPU_IP_TYPES_AR_RELEASE_MINOR_VERSION  != WKPU_IP_DEFINES_AR_RELEASE_MINOR_VERSION) || \
     (WKPU_IP_TYPES_AR_RELEASE_REVISION_VERSION   != WKPU_IP_DEFINES_AR_RELEASE_REVISION_VERSION))
    #error "AutoSar Version Numbers of Wkpu_Ip_Types.h and Wkpu_Ip_Defines.h are different"
#endif
/* Check if source file and ICU header file are of the same Software version */
#if ((WKPU_IP_TYPES_SW_MAJOR_VERSION  != WKPU_IP_DEFINES_SW_MAJOR_VERSION) || \
     (WKPU_IP_TYPES_SW_MINOR_VERSION  != WKPU_IP_DEFINES_SW_MINOR_VERSION) || \
     (WKPU_IP_TYPES_SW_PATCH_VERSION  != WKPU_IP_DEFINES_SW_PATCH_VERSION))
#error "Software Version Numbers of Wkpu_Ip_Types.h and Wkpu_Ip_Defines.h are different"
#endif

/*==================================================================================================
                                           CONSTANTS
==================================================================================================*/

/*==================================================================================================
                                 GLOBAL VARIABLE DECLARATIONS
==================================================================================================*/

/*==================================================================================================
                                       DEFINES AND MACROS
==================================================================================================*/

/*==================================================================================================
                                             ENUMS
==================================================================================================*/
#if (STD_ON == WKPU_IP_USED)

#if (defined (WKPU_IP_NMI_API) && (STD_ON == WKPU_IP_NMI_API))
/**
 * @brief        WKPU NMI destination source types
 * @details      Enumeration defining the possible destination sources for NMI requests.
 *               Determines how the NMI signal is routed within the system.
 */
typedef enum
{
#ifdef WKPU_IP_SUPPORT_NONE_REQUEST
    WKPU_IP_NMI_NONE               = 3U,    /**< @brief No NMI, critical interrupt, or machine check request generated */
#endif
#ifdef WKPU_IP_SUPPORT_MACHINE_CHK_REQ
    WKPU_IP_NMI_MACHINE_CHK_REQ    = 2U,   /**< @brief Machine check request interrupt */
#endif
#ifdef WKPU_IP_SUPPORT_CRITICAL_INT
    WKPU_IP_NMI_CRITICAL_INT       = 1U,   /**< @brief Critical interrupt */
#endif
#ifdef WKPU_IP_SUPPORT_NON_MASK_INT
    WKPU_IP_NMI_NON_MASK_INT       = 0U   /**< @brief Non-maskable interrupt */
#endif
} Wkpu_Ip_NmiDestSrcType;

/**
 * @brief        WKPU NMI core identifiers
 * @details      Enumeration defining the available NMI cores that can be configured.
 *               Each core represents a separate NMI source with independent configuration.
 */
typedef enum
{
    WKPU_CORE0       = 0U,    /**< @brief NMI Core 0 */
    WKPU_CORE1       = 1U,    /**< @brief NMI Core 1 */
    WKPU_CORE2       = 2U,    /**< @brief NMI Core 2 */
    WKPU_CORE3       = 3U,    /**< @brief NMI Core 3 */
} Wkpu_Ip_CoreType;
#endif /* (defined (WKPU_IP_NMI_API) && (STD_ON == WKPU_IP_NMI_API)) */

/**
 * @brief        WKPU edge event types
 * @details      Enumeration defining the types of edge events that can trigger WKPU interrupts.
 *               These values are used to configure the activation condition for WKPU channels.
 */
typedef enum
{
    WKPU_IP_NONE_EDGE       = 0U,   /**< @brief None event */
    WKPU_IP_RISING_EDGE     = 1U,   /**< @brief Rising edge event */
    WKPU_IP_FALLING_EDGE    = 2U,   /**< @brief Falling edge event */
    WKPU_IP_BOTH_EDGES       = 3U    /**< @brief Both rising and falling edge event */
} Wkpu_Ip_EdgeType;

/**
 * @brief        WKPU IP layer status return values
 * @details      Enumeration defining the possible return status values for WKPU IP layer functions.
 *               Used to indicate success or failure of WKPU operations.
 */
typedef enum
{
    WKPU_IP_SUCCESS     = 0x0U,  /**< @brief Status for success operation return. */
    WKPU_IP_ERROR       = 0x1U,  /**< @brief General error return status.         */
} Wkpu_Ip_StatusType;
/*==================================================================================================
                                 STRUCTURES AND OTHER TYPEDEFS
==================================================================================================*/
/**
 * @typedef      Wkpu_Ip_NotifyType
 * @brief        Function pointer type for notification callbacks.
 * @details      The notification functions shall have no parameters and no return value.
 */
typedef void                             (*Wkpu_Ip_NotifyType)(void);

/**
 * @typedef      Wkpu_Ip_CallbackType
 * @brief        Function pointer type for interrupt callbacks.
 * @details      Callback signature used for each channel with an active interrupt.
 *               It receives two parameters: a 16-bit value and a boolean flag.
 */
typedef void (*Wkpu_Ip_CallbackType)(uint16 callbackParam1, boolean callbackParam2);

#if (defined (WKPU_IP_NMI_API) && (STD_ON == WKPU_IP_NMI_API))

/**
 * @brief        WKPU NMI core configuration structure
 * @details      Structure containing all configuration parameters for a single NMI core.
 *               Used to configure NMI behavior including destination, filtering, and edge detection.
 */
typedef struct
{
    Wkpu_Ip_CoreType            core;               /**< @brief WKPU core source */
    Wkpu_Ip_NmiDestSrcType      destination;        /**< @brief NMI destination source */
    boolean                     wkpReqEn;           /**< @brief NMI request enable */
    boolean                     filterEn;           /**< @brief NMI filter enable */
    Wkpu_Ip_EdgeType            edgeEvent;          /**< @brief NMI edge events */
    boolean                     lockEn;             /**< @brief NMI configuration lock register */
} Wkpu_Ip_NmiCfgType;
#endif /* (defined (WKPU_IP_NMI_API) && (STD_ON == WKPU_IP_NMI_API)) */

/**
 * @brief        WKPU channel configuration structure
 * @details      Structure containing all configuration parameters for a single WKPU channel.
 *               Used during initialization to set up channel behavior and callbacks.
 */
typedef struct
{
    uint8                   hwChannel;                 /**< @brief The WKPU hardware channel.                   */
    boolean                 filterEn;                  /**< @brief WKPU/interrupt filter enable.                */
    Wkpu_Ip_EdgeType        edgeEvent;                 /**< @brief WKPU/interrupt edge events.                  */
    Wkpu_Ip_CallbackType    callback;                  /**< @brief Pointer to the callback function.            */
    Wkpu_Ip_NotifyType      WkpuChannelNotification;   /**< @brief The notification functions shall have no parameters and no return value.*/
    uint16                  callbackParam;             /**< @brief The logic channel for which callback is set. */
} Wkpu_Ip_ChannelConfigType;

/**
 * @brief        WKPU driver configuration structure
 * @details      Main configuration structure containing all WKPU driver settings.
 *               Used during driver initialization to configure all channels and NMI cores.
 */
typedef struct
{
#if (defined (WKPU_IP_NMI_API) && (STD_ON == WKPU_IP_NMI_API))
    /** @brief Number of channels in NMI configuration */
    uint8                           numNMIChannels;
    /** @brief Pointer to channels configration */
    const Wkpu_Ip_NmiCfgType             (*pNMIChannelsConfig)[];
#endif
    /** @brief Number of channels in configuration */
    uint8                           numChannels;
    /** @brief Pointer to channels configration */
    const Wkpu_Ip_ChannelConfigType       (*pChannelsConfig)[];
} Wkpu_Ip_IrqConfigType;

/**
 * @brief        WKPU IP state structure.
 * @details      This structure is used by the IPL driver for internal logic.
 *               The content is populated at initialization time.
 */
typedef struct
{
    /** @brief Initialization state. */
    boolean                     chInit;
    /** @brief Pointer to the callback function. */
    Wkpu_Ip_CallbackType        callback;
    /** @brief The notification functions for SIGNAL_EDGE_DETECT mode. */
    Wkpu_Ip_NotifyType          WkpuChannelNotification;
    /** @brief The logic channel for which callback is set. */
    uint16                       callbackParam;
    /** @brief Store the initialization state that determines whether Notifications are enabled. */
    boolean                      notificationEnable;
} Wkpu_Ip_State;
#endif /* WKPU_IP_USED */
/*==================================================================================================
                                     FUNCTION PROTOTYPES
==================================================================================================*/

#ifdef __cplusplus
}
#endif

/**@}*/

#endif  /* WKPU_IP_TYPES_H */
