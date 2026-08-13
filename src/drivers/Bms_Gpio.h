/**
 *  @file       Bms_Gpio.h
 *  @brief      BMS digital I/O driver, built on top of the SIUL2 port IP.
 */

#ifndef BMS_GPIO_H
#define BMS_GPIO_H

#ifdef __cplusplus
extern "C"{
#endif

#include "Std_Types.h"

/*==================================================================================================
*                                       TYPE DEFINITIONS
==================================================================================================*/

/** @brief Logical BMS digital I/O identifiers, mapped to physical pins in Bms_Gpio.c. */
typedef enum
{
    BMS_GPIO_PIN_STATUS_LED = 0,
    BMS_GPIO_PIN_FAULT_LED,
    BMS_GPIO_PIN_CONTACTOR_CTRL,
    BMS_GPIO_PIN_COUNT
} Bms_Gpio_PinIdType;

/** @brief Digital pin level. */
typedef enum
{
    BMS_GPIO_STATE_LOW = 0,
    BMS_GPIO_STATE_HIGH = 1
} Bms_Gpio_StateType;

/*==================================================================================================
*                                       FUNCTION PROTOTYPES
==================================================================================================*/

/**
 * @brief Configures the direction of every BMS digital I/O pin.
 */
void Bms_Gpio_Init(void);

/**
 * @brief Drives an output pin to the requested level.
 * @param[in] pinId The logical pin identifier.
 * @param[in] state The level to drive.
 */
void Bms_Gpio_WritePin(Bms_Gpio_PinIdType pinId, Bms_Gpio_StateType state);

/**
 * @brief Reads the current level of a pin.
 * @param[in] pinId The logical pin identifier.
 * @return The current pin level.
 */
Bms_Gpio_StateType Bms_Gpio_ReadPin(Bms_Gpio_PinIdType pinId);

#ifdef __cplusplus
}
#endif

#endif /* BMS_GPIO_H */
