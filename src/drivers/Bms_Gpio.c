/**
 *  @file       Bms_Gpio.c
 *  @brief      BMS digital I/O driver, built on top of the SIUL2 port IP.
 *
 *  @note       This RTD delivery only includes the SIUL2 pin-muxing IP
 *              (Siul2_Port_Ip), not a GPIO data-register (DIO) IP. Pin
 *              direction is configured through Siul2_Port_Ip, while the
 *              actual output/input level is tracked in a shadow state
 *              until a DIO driver is integrated for this board.
 */

#include "Bms_Gpio.h"
#include "Siul2_Port_Ip.h"

/*==================================================================================================
*                                       LOCAL VARIABLES
==================================================================================================*/

/** @brief Shadow state for every logical pin, used until a real DIO driver is wired in. */
static Bms_Gpio_StateType Bms_Gpio_ShadowState[BMS_GPIO_PIN_COUNT];

/*==================================================================================================
*                                       FUNCTION DEFINITIONS
==================================================================================================*/

void Bms_Gpio_Init(void)
{
    uint32 index;

    for (index = 0U; index < (uint32)BMS_GPIO_PIN_COUNT; index++)
    {
        Bms_Gpio_ShadowState[index] = BMS_GPIO_STATE_LOW;
    }

    /* TODO: Call Siul2_Port_Ip_SetPinDirection() here for each physical pin
     *       once the pin-to-port mapping for this board is defined in
     *       board/Siul2_Port_Ip_Cfg.c, and wire in a DIO IP for level control. */
}

void Bms_Gpio_WritePin(Bms_Gpio_PinIdType pinId, Bms_Gpio_StateType state)
{
    if (pinId < BMS_GPIO_PIN_COUNT)
    {
        /* TODO: Replace with a real GPIO data register write once a DIO IP is available. */
        Bms_Gpio_ShadowState[pinId] = state;
    }
}

Bms_Gpio_StateType Bms_Gpio_ReadPin(Bms_Gpio_PinIdType pinId)
{
    Bms_Gpio_StateType state = BMS_GPIO_STATE_LOW;

    if (pinId < BMS_GPIO_PIN_COUNT)
    {
        /* TODO: Replace with a real GPIO data register read once a DIO IP is available. */
        state = Bms_Gpio_ShadowState[pinId];
    }

    return state;
}
