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

uint16 Bms_Adc_GetPackV1VoltageMv(void);
uint16 Bms_Adc_GetPackV2VoltageMv(void);
uint16 Bms_Adc_GetPackV3VoltageMv(void);

uint16 Bms_Adc_GetBus1Raw(void);
uint16 Bms_Adc_GetBus2Raw(void);
uint16 Bms_Adc_GetBus3Raw(void);
uint16 Bms_Adc_GetBusSpareRaw(void);

uint16 Bms_Adc_GetBus1VoltageMv(void);
uint16 Bms_Adc_GetBus2VoltageMv(void);
uint16 Bms_Adc_GetBus3VoltageMv(void);
uint16 Bms_Adc_GetBusSpareVoltageMv(void);


/**
 * @brief Returns the latest ADC1_P1 voltage in millivolts.
 */
uint16 Bms_Adc_GetNtc1VoltageMv(void);
uint16 Bms_Adc_GetNtc2VoltageMv(void);
uint16 Bms_Adc_GetNtc3VoltageMv(void);

/**
 * @brief Returns TRUE if the latest ADC acquisition round was fully valid.
 */
boolean Bms_Adc_IsPackValid(void);

/**
 * @brief Returns TRUE if the latest bus (ADC0) acquisition round was fully valid.
 */
boolean Bms_Adc_IsBusValid(void);



#ifdef __cplusplus
}
#endif

#endif /* BMS_ADC_H */
