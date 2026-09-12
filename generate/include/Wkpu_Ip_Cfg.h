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

#ifndef WKPU_IP_CFG_H
#define WKPU_IP_CFG_H

/**
 *   @file    Wkpu_Ip_Cfg.h
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
#include "Wkpu_Ip_SA_PBcfg.h"

/*==================================================================================================
 *                              SOURCE FILE VERSION INFORMATION
 *================================================================================================*/
#define WKPU_IP_CFG_VENDOR_ID                       43
#define WKPU_IP_CFG_AR_RELEASE_MAJOR_VERSION        4
#define WKPU_IP_CFG_AR_RELEASE_MINOR_VERSION        9
#define WKPU_IP_CFG_AR_RELEASE_REVISION_VERSION     0
#define WKPU_IP_CFG_SW_MAJOR_VERSION                7
#define WKPU_IP_CFG_SW_MINOR_VERSION                0
#define WKPU_IP_CFG_SW_PATCH_VERSION                1

/*==================================================================================================
 *                                      FILE VERSION CHECKS
 *================================================================================================*/
/* Check if header file and Std_Types.h file are of the same Autosar version */
#ifndef DISABLE_MCAL_INTERMODULE_ASR_CHECK
    #if ((WKPU_IP_CFG_AR_RELEASE_MAJOR_VERSION != STD_AR_RELEASE_MAJOR_VERSION) || \
         (WKPU_IP_CFG_AR_RELEASE_MINOR_VERSION != STD_AR_RELEASE_MINOR_VERSION))
        #error "AutoSar Version Numbers of Wkpu_Ip_Cfg.h and Std_Types.h are different"
    #endif
#endif

/* Check if header file and Wkpu_Ip_Cfg.h and Wkpu_Ip_SA_PBcfg.h file are of the same vendor */
#if (WKPU_IP_CFG_VENDOR_ID != WKPU_IP_SA_PBCFG_VENDOR_ID)
    #error "Wkpu_Ip_Cfg.h and Wkpu_Ip_SA_PBcfg.h have different vendor IDs"
#endif
/* Check if header file and Wkpu_Ip_SA_PBcfg.h file are of the same Autosar version */
#if ((WKPU_IP_CFG_AR_RELEASE_MAJOR_VERSION != WKPU_IP_SA_PBCFG_AR_RELEASE_MAJOR_VERSION) || \
     (WKPU_IP_CFG_AR_RELEASE_MINOR_VERSION != WKPU_IP_SA_PBCFG_AR_RELEASE_MINOR_VERSION) || \
     (WKPU_IP_CFG_AR_RELEASE_REVISION_VERSION != WKPU_IP_SA_PBCFG_AR_RELEASE_REVISION_VERSION))
    #error "AutoSar Version Numbers of Wkpu_Ip_Cfg.h and Wkpu_Ip_SA_PBcfg.h are different"
#endif
/* Check if header file and Wkpu_Ip_SA_PBcfg.h file are of the same Software version */
#if ((WKPU_IP_CFG_SW_MAJOR_VERSION != WKPU_IP_SA_PBCFG_SW_MAJOR_VERSION) || \
     (WKPU_IP_CFG_SW_MINOR_VERSION != WKPU_IP_SA_PBCFG_SW_MINOR_VERSION) || \
     (WKPU_IP_CFG_SW_PATCH_VERSION != WKPU_IP_SA_PBCFG_SW_PATCH_VERSION))
    #error "Software Version Numbers of Wkpu_Ip_Cfg.h and Wkpu_Ip_SA_PBcfg.h are different"
#endif
/*==================================================================================================
                                       DEFINES AND MACROS
==================================================================================================*/
#if (STD_ON == WKPU_IP_USED)
#define WKPU_IP_DEV_ERROR_DETECT          (STD_OFF)

#define WKPU_ICU_SINGLE_INTERRUPT         (STD_ON)

/* Macro used to export generated configuration. */
#define WKPU_CONFIG_EXT \
        WKPU_CONFIG_SA_PB

#endif /* WKPU_IP_USED */

#ifdef __cplusplus
}
#endif

/** @} */

#endif  /* WKPU_IP_CFG_H */

