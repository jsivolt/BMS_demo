/**
 *  @file       Bms_Adc.h
 *  @brief      BMS ADC driver, built on top of the ADC_SAR IP.
 */

#ifndef BMS_ADC_H
#define BMS_ADC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "Std_Types.h"

/**
 * @brief Initializes and calibrates the ADC hardware unit used by the BMS.
 * @return E_OK if init and calibration succeeded, E_NOT_OK otherwise.
 */
Std_ReturnType Bms_Adc_Init(void);

/**
 * @brief Executes periodic ADC acquisition.
 */
void Bms_Adc_MainFunction(void);

/**
 * @brief Returns the latest raw potentiometer conversion value.
 */
uint16 Bms_Adc_GetPotRaw(void);

uint16 Bms_Adc_GetPotVoltageMv(void);

/**
 * @brief Returns the latest raw value of ADC1_P1.
 */
uint16 Bms_Adc_GetNtc1Raw(void);
uint16 Bms_Adc_GetNtc2Raw(void);
uint16 Bms_Adc_GetNtc3Raw(void);

uint16 Bms_Adc_GetPackV1Raw(void);
uint16 Bms_Adc_GetPackV2Raw(void);
uint16 Bms_Adc_GetPackV3Raw(void);

/**
 * @brief Returns the latest ADC1_P1 voltage in millivolts.
 */
uint16 Bms_Adc_GetNtc1VoltageMv(void);



#ifdef __cplusplus
}
#endif

#endif /* BMS_ADC_H */
