/**
 *  @file       Bms_Ntc_Cfg.h
 *  @brief      Compile-time configuration for the NTC temperature sensor module.
 */

#ifndef BMS_NTC_CFG_H
#define BMS_NTC_CFG_H

#include "Std_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*==================================================================================================
*                                       CONFIGURATION
==================================================================================================*/

/** @brief ADC reference voltage in millivolts. */
#define BMS_NTC_CFG_ADC_VREF_MV          (3300U)

/** @brief Maximum ADC raw code (14-bit resolution: 2^14 - 1). */
#define BMS_NTC_CFG_ADC_FULL_SCALE       (16383U)

/** @brief Fixed series (pull-up) resistor of the voltage divider, in ohms. */
#define BMS_NTC_CFG_SERIES_RESISTOR_OHM  (10000.0f)

/**
 * @brief   Divider topology.
 *          Set to 1 when the NTC is connected between the ADC node and GND
 *          (series resistor to VREF). Set to 0 for the opposite arrangement.
 */
#define BMS_NTC_CFG_NTC_TO_GND           (1U)

/** @brief NTC nominal resistance at the reference temperature, in ohms. */
#define BMS_NTC_CFG_R25_OHM              (10000.0f)

/** @brief NTC reference temperature, in kelvin (25 degC). */
#define BMS_NTC_CFG_T25_KELVIN           (298.15f)

/** @brief NTC Beta coefficient (B25/85), in kelvin. */
#define BMS_NTC_CFG_BETA_KELVIN          (3435.0f)

/** @brief Lower valid temperature bound, in tenths of a degree Celsius. */
#define BMS_NTC_CFG_MIN_TEMP_DC          ((sint16)-400)

/** @brief Upper valid temperature bound, in tenths of a degree Celsius. */
#define BMS_NTC_CFG_MAX_TEMP_DC          ((sint16)1250)

#ifdef __cplusplus
}
#endif

#endif /* BMS_NTC_CFG_H */
