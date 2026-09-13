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
*   @file           ComStack_Types.h
*   @implements     ComStack_Types.h_Artifact
*   @version 7.0.1
*
*   @brief   AUTOSAR BaseNXP - Communication stack types header file.
*   @details AUTOSAR communication stack type header file.
*            This file contains sample code only. It is not part of the production code deliverables
*   @addtogroup BASENXP_COMPONENT
*   @{
*/

#ifndef COMSTACKTYPES_H
#define COMSTACKTYPES_H

#ifdef __cplusplus
extern "C"{
#endif


/*==================================================================================================
*                                         INCLUDE FILES
==================================================================================================*/
#include "ComStack_Types.h"

/*==================================================================================================
*                              SOURCE FILE VERSION INFORMATION
==================================================================================================*/

/*==================================================================================================
*                                      FILE VERSION CHECKS
==================================================================================================*/

/*==================================================================================================
*                                           CONSTANTS
==================================================================================================*/

/*==================================================================================================
*                                       DEFINES AND MACROS
==================================================================================================*/

/*==================================================================================================
*                                            ENUMS
==================================================================================================*/
/**
 * @brief Depending on the HW, quality information regarding the evaluated timestamp might be supported.
 *        If not supported, the value shall be always Valid. For Uncertain and Invalid values, the upper layer
 *        shall discard the time stamp.
 * @implements TimeStampQualType_enum
 */
typedef enum
{
    VALID   = 0,   /**< @brief Timestamp is valid and synchronized */
    INVALID = 1,   /**< @brief Timestamp is invalid */
    UNCERTAIN = 2    /**< @brief Timestamp validity is uncertain */
} TimeStampQualType;

/*==================================================================================================
*                                STRUCTURES AND OTHER TYPEDEFS
==================================================================================================*/
/**
 * @brief Variables of this type are used for expressing time stamps including relative time and absolute
 *        calendar time. The absolute time starts at 1970-01-01.
 *        Value range of Seconds part:
 *          - 0 .. (2^48 -1), i.e. 0 to 3257812230d [0xFFFF FFFF FFFF]
 *        Value range of Nanoseconds part:
 *          - 0 to 999999999ns [0x3B9AC9FF]
 *          - invalid value in nanoseconds: [0x3B9ACA00] to [0x3FFFFFFF]
 *          - Bit 30 and 31 reserved, default: 0
 * @implements TimeStampType_struct
 */
typedef struct
{
    uint32 seconds;       /**< @brief Seconds part of the timestamp */
    uint32 nanoseconds;   /**< @brief Nanoseconds part of the timestamp */
    uint16 secondsHi;     /**< @brief 16 bit MSB of the 48 bits Seconds part of the time */
} TimeStampType;

/**
 * @brief The Time Tuple represents the clock values of two related HW clocks
 *          - the value of the clock used for timestamping of frames
 *          - and the corresponding value of the adjustable HW clock, derived by cross-timestamping
 * @implements TimeTupleType_struct
 */
typedef struct
{
    TimeStampType     timestampClockValue;    /**< @brief Value of the clock, which is used of ingress/egress timestamping */
    TimeStampType     disciplinedClockValue;  /**< @brief Value of the adjustable HW clock */
    TimeStampQualType timeQuality;            /**< @brief Status of time tuple */
} TimeTupleType;


/*==================================================================================================
*                                STRUCTURES AND OTHER TYPEDEFS
==================================================================================================*/

/*==================================================================================================
*                                 GLOBAL VARIABLE DECLARATIONS
==================================================================================================*/

/*==================================================================================================
*                                     FUNCTION PROTOTYPES
==================================================================================================*/


#ifdef __cplusplus
}
#endif

#endif /* COMSTACKTYPES_H */

/** @} */
