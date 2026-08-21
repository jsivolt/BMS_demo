/**
 *  @file       Bms_Ntc.h
 *  @brief      3-channel NTC thermistor temperature conversion for the battery pack.
 */

#ifndef BMS_NTC_H
#define BMS_NTC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "Std_Types.h"

/*==================================================================================================
*                                      DEFINES AND MACROS
==================================================================================================*/

#define BMS_NTC_COUNT    (3U)

/*==================================================================================================
*                                             TYPES
==================================================================================================*/

typedef enum
{
    BMS_NTC_1 = 0U,
    BMS_NTC_2,
    BMS_NTC_3
} Bms_NtcChannelType;

/*==================================================================================================
*                                       FUNCTION PROTOTYPES
==================================================================================================*/

void Bms_Ntc_Init(void);

Std_ReturnType Bms_Ntc_RawToTemperature(
    uint16 adcRaw,
    sint16 * const temperature_dC
);

void Bms_Ntc_MainFunction(void);

sint16 Bms_Ntc_GetTemperature_dC(
    Bms_NtcChannelType channel
);

boolean Bms_Ntc_IsValid(
    Bms_NtcChannelType channel
);

#ifdef __cplusplus
}
#endif

#endif /* BMS_NTC_H */