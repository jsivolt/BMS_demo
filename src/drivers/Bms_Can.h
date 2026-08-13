/**
 *  @file       Bms_Can.h
 *  @brief      BMS CAN communication driver abstraction.
 */

#ifndef BMS_CAN_H
#define BMS_CAN_H

#ifdef __cplusplus
extern "C"{
#endif

#include "Std_Types.h"

/*==================================================================================================
*                                       LOCAL DEFINES
==================================================================================================*/

#define BMS_CAN_MAX_DATA_LENGTH 8U

/*==================================================================================================
*                                       TYPE DEFINITIONS
==================================================================================================*/

/** @brief A single classic CAN frame used by the BMS application. */
typedef struct
{
    uint32 id;
    uint8  length;
    uint8  data[BMS_CAN_MAX_DATA_LENGTH];
} Bms_Can_MessageType;

/*==================================================================================================
*                                       FUNCTION PROTOTYPES
==================================================================================================*/

/**
 * @brief Initializes the CAN controller and transceiver.
 * @return E_OK if initialization succeeded, E_NOT_OK otherwise.
 */
Std_ReturnType Bms_Can_Init(void);

/**
 * @brief Transmits a CAN message.
 * @param[in] message The message to transmit.
 * @return E_OK if the message was queued for transmission, E_NOT_OK otherwise.
 */
Std_ReturnType Bms_Can_Transmit(const Bms_Can_MessageType * const message);

/**
 * @brief Retrieves the next received CAN message, if any.
 * @param[out] message Destination structure filled with the received message.
 * @return E_OK if a message was available, E_NOT_OK otherwise.
 */
Std_ReturnType Bms_Can_Receive(Bms_Can_MessageType * const message);

#ifdef __cplusplus
}
#endif

#endif /* BMS_CAN_H */
