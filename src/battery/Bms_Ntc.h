/**
 *  @file       Bms_Ntc.h
 *  @brief      NTC thermistor temperature conversion for the battery pack.
 */

#ifndef BMS_NTC_H
#define BMS_NTC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "Std_Types.h"

/*==================================================================================================
*                                       FUNCTION PROTOTYPES
==================================================================================================*/

/**
 * @brief Converts a raw NTC ADC value into a temperature.
 * @param[in]  adcRaw       Raw ADC conversion result of the NTC divider node.
 * @param[out] temperature_dC Temperature in tenths of a degree Celsius,
 *                            clamped to the configured valid range.
 * @return E_OK if the conversion produced a value inside the valid range,
 *         E_NOT_OK otherwise (result is clamped).
 */
Std_ReturnType Bms_Ntc_RawToTemperature(uint16 adcRaw, sint16 * const temperature_dC);

/**
 * @brief Executes periodic NTC acquisition and temperature update.
 */
void Bms_Ntc_MainFunction(void);

/**
 * @brief Returns the latest computed NTC temperature.
 * @return Temperature in tenths of a degree Celsius.
 */
sint16 Bms_Ntc_GetTemperature_dC(void);

/**
 * @brief Indicates whether the latest temperature is within the valid range.
 */
boolean Bms_Ntc_IsValid(void);

#ifdef __cplusplus
}
#endif

#endif /* BMS_NTC_H */
