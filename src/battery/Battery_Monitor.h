/**
 *  @file       Battery_Monitor.h
 *  @brief      Battery pack monitoring - voltage, current and temperature.
 */

#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#ifdef __cplusplus
extern "C"{
#endif

#include "Std_Types.h"

/*==================================================================================================
*                                       TYPE DEFINITIONS
==================================================================================================*/

/** @brief Snapshot of the battery pack measurements. */
typedef struct
{
    sint32 packVoltage_mV;
    sint32 packCurrent_mA;
    sint16 packTemperature_dC;   /**< Tenths of a degree Celsius. */
    uint8  stateOfCharge_pct;
} Battery_Monitor_DataType;

/*==================================================================================================
*                                       FUNCTION PROTOTYPES
==================================================================================================*/

/**
 * @brief Initializes the battery monitor internal state.
 */
void Battery_Monitor_Init(void);

/**
 * @brief Acquires a new set of measurements and evaluates them against safety limits.
 *        Must be called periodically from the application main function.
 */
void Battery_Monitor_Update(void);

/**
 * @brief Retrieves the latest battery pack measurements.
 * @param[out] data Destination structure filled with the latest measurements.
 */
void Battery_Monitor_GetData(Battery_Monitor_DataType * const data);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_MONITOR_H */
