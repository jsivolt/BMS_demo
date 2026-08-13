/**
 *  @file       Bms_Can.c
 *  @brief      BMS CAN communication driver abstraction.
 *
 *  @note       This RTD delivery does not include a FlexCAN IP module yet.
 *              The functions below are stubs so the application layer can be
 *              developed against a stable interface; replace their bodies
 *              once the FlexCAN driver is integrated for this board.
 */

#include "Bms_Can.h"

/*==================================================================================================
*                                       FUNCTION DEFINITIONS
==================================================================================================*/

Std_ReturnType Bms_Can_Init(void)
{
    /* TODO: Initialize the FlexCAN controller once its driver is integrated. */
    return (Std_ReturnType)E_NOT_OK;
}

Std_ReturnType Bms_Can_Transmit(const Bms_Can_MessageType * const message)
{
    Std_ReturnType status = (Std_ReturnType)E_NOT_OK;

    if (message != NULL_PTR)
    {
        /* TODO: Queue the message for transmission once the FlexCAN driver is integrated. */
        status = (Std_ReturnType)E_NOT_OK;
    }

    return status;
}

Std_ReturnType Bms_Can_Receive(Bms_Can_MessageType * const message)
{
    Std_ReturnType status = (Std_ReturnType)E_NOT_OK;

    if (message != NULL_PTR)
    {
        /* TODO: Fetch a received message once the FlexCAN driver is integrated. */
        status = (Std_ReturnType)E_NOT_OK;
    }

    return status;
}
