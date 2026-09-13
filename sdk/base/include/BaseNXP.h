/*==================================================================================================
* Project : RTD AUTOSAR 4.9
* Platform : CORTEXM
* Peripheral : S32K3XX
* Dependencies : none
*
* Autosar Version : 4.9.0
* Autosar Revision : ASR_REL_4_9_REV_0000
* Autosar Conf.Variant :
* SW Version : 7.0.1
* Build Version : S32K3_RTD_7_0_1_D2602_ASR_REL_4_9_REV_0000_20260206
*
* Copyright 2020 - 2026 NXP
*
* NXP Confidential and Proprietary. This software is owned or controlled by NXP and may only be
*   used strictly in accordance with the applicable license terms. By expressly
*   accepting such terms or by downloading, installing, activating and/or otherwise
*   using the software, you are agreeing that you have read, and that you agree to
*   comply with and are bound by, such license terms. If you do not agree to be
*   bound by the applicable license terms, then you may not retain, install,
*   activate or otherwise use the software.
==================================================================================================*/
/**
*   @file    BaseNXP.h
*
*   @version 7.0.1
*
*   @addtogroup BASENXP_COMPONENT
*   @{
*/
#ifndef BASENXP_H
#define BASENXP_H

#ifdef __cplusplus
extern "C"{
#endif

/*==================================================================================================
*                                          INCLUDE FILES
* 1) system and project includes
* 2) needed interfaces from external units
* 3) internal and external interfaces from this unit
==================================================================================================*/
#include "Osif.h"

/*==================================================================================================
*                               SOURCE FILE VERSION INFORMATION
==================================================================================================*/
#define BASENXP_VENDOR_ID                    43
#define BASENXP_AR_RELEASE_MAJOR_VERSION     4
#define BASENXP_AR_RELEASE_MINOR_VERSION     9
#define BASENXP_AR_RELEASE_REVISION_VERSION  0
#define BASENXP_SW_MAJOR_VERSION             7
#define BASENXP_SW_MINOR_VERSION             0
#define BASENXP_SW_PATCH_VERSION             1

/*==================================================================================================
*                                       FILE VERSION CHECKS
==================================================================================================*/
/* Check if BaseNXP.h file and OsIf.h file are of the same vendor */
#if (BASENXP_VENDOR_ID != OSIF_VENDOR_ID)
    #error "BaseNXP.h and OsIf.h have different vendor ids"
#endif
/* Check if BaseNXP.h file and OsIf.h file are of the same Autosar version */
#if ((BASENXP_AR_RELEASE_MAJOR_VERSION    != OSIF_AR_RELEASE_MAJOR_VERSION) || \
     (BASENXP_AR_RELEASE_MINOR_VERSION    != OSIF_AR_RELEASE_MINOR_VERSION) || \
     (BASENXP_AR_RELEASE_REVISION_VERSION != OSIF_AR_RELEASE_REVISION_VERSION))
    #error "AUTOSAR Version Numbers of BaseNXP.h and OsIf.h are different"
#endif
/* Check if BaseNXP.h file and OsIf.h file are of the same Software version */
#if ((BASENXP_SW_MAJOR_VERSION != OSIF_SW_MAJOR_VERSION) || \
     (BASENXP_SW_MINOR_VERSION != OSIF_SW_MINOR_VERSION) || \
     (BASENXP_SW_PATCH_VERSION != OSIF_SW_PATCH_VERSION) \
    )
    #error "Software Version Numbers of BaseNXP.h and OsIf.h are different"
#endif

/*==================================================================================================
*                                           CONSTANTS
==================================================================================================*/

/*==================================================================================================
*                                       DEFINES AND MACROS
==================================================================================================*/

#endif /* BASENXP_H */

/** @} */

