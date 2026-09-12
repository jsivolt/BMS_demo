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
#ifndef WKPU_IP_DEFINES_H
#define WKPU_IP_DEFINES_H

/**
 *   @file    Wkpu_Ip_Defines.h
 *   @version 7.0.1
 *
 *   @brief   AUTOSAR Icu - contains the data exported by the Icu module
 *   @details Contains the information that will be exported by the module, as requested by Autosar.
 *
 *   @addtogroup wkpu_icu_ip WKPU IPL
 *   @{
 */

#ifdef __cplusplus
extern "C"{
#endif

 /*==================================================================================================
 *                                         INCLUDE FILES
 * 1) system and project includes
 * 2) needed interfaces from external units
 * 3) internal and external interfaces from this unit
 *================================================================================================*/
#include "Std_Types.h"

#include "S32K344_WKPU.h"

/*==================================================================================================
 *                              SOURCE FILE VERSION INFORMATION
 *================================================================================================*/

#define WKPU_IP_DEFINES_VENDOR_ID                    43
#define WKPU_IP_DEFINES_AR_RELEASE_MAJOR_VERSION     4
#define WKPU_IP_DEFINES_AR_RELEASE_MINOR_VERSION     9
#define WKPU_IP_DEFINES_AR_RELEASE_REVISION_VERSION  0
#define WKPU_IP_DEFINES_SW_MAJOR_VERSION             7
#define WKPU_IP_DEFINES_SW_MINOR_VERSION             0
#define WKPU_IP_DEFINES_SW_PATCH_VERSION             1

/*==================================================================================================
 *                                      FILE VERSION CHECKS
 *================================================================================================*/
/* Check if header file and Std_Types.h file are of the same Autosar version */
#ifndef DISABLE_MCAL_INTERMODULE_ASR_CHECK
    #if ((WKPU_IP_DEFINES_AR_RELEASE_MAJOR_VERSION != STD_AR_RELEASE_MAJOR_VERSION) || \
         (WKPU_IP_DEFINES_AR_RELEASE_MINOR_VERSION != STD_AR_RELEASE_MINOR_VERSION))
    #error "AutoSar Version Numbers of Wkpu_Ip_Defines.h and Std_Types.h are different"
    #endif
#endif

/*==================================================================================================
                                       DEFINES AND MACROS
==================================================================================================*/

#define WKPU_IP_USED               (STD_ON)

#if (STD_ON == WKPU_IP_USED)
#define WKPU_IP_NMI_API            (STD_ON)
#if (defined (WKPU_IP_NMI_API) && (STD_ON == WKPU_IP_NMI_API))
/*! @brief The distance between cores */
#define WKPU_IP_CORE_OFFSET_SIZE  (8U)
#define WKPU_IP_SUPPORT_NONE_REQUEST
#define WKPU_IP_SUPPORT_NON_MASK_INT
/** @brief The WKPU core array */
#define WKPU_IP_CORE_ARRAY \
{ \
    WKPU_CORE0    /*!< Core 0 */, \
    WKPU_CORE1    /*!< Core 1 */ \
}
#define WKPU_IP_NMI_CORE_CNT      (1U)
#define WKPU_IP_NMI_NUM_CORES     (2U)
#endif

#define WKPU_IP_NUM_OF_CHANNELS               (64U)

/** @brief The number of Wkpu channels are used in configuration */
#define WKPU_IP_NUM_OF_CHANNELS_USED          ((uint8)1U)

#define WKPU_IP_NUM_OF_CHANNELS_IN_ONE_REG_32 (32U)

#define WKPU_IP_64_CH_USED                (STD_ON)

#define WKPU_IP_NUM_OF_CHANNELS_IN_ONE_REG_64 (32U)

/** @brief Define if global variables need to be placed in non-cache area or not */
#define WKPU_IP_NO_CACHE_USED                 (STD_OFF)

#define WKPU_IP_INITIAL_INDEX_OF_CHANNELS \
    {255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 0U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U, 255U}

#endif /* WKPU_IP_USED */

#ifdef __cplusplus
}
#endif

/** @} */

#endif  /* WKPU_IP_DEFINES_H */

