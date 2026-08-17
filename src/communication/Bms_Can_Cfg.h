/**
 *  @file       Bms_Can_Cfg.h
 *  @brief      Compile-time configuration for the BMS CAN communication module.
 */

#ifndef BMS_CAN_CFG_H
#define BMS_CAN_CFG_H

#include "Std_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*==================================================================================================
*                                       CONFIGURATION
==================================================================================================*/

/** @brief FlexCAN hardware instance used by the BMS application. */
#define BMS_CAN_CFG_INSTANCE            (0U)

/** @brief Message buffer index reserved for transmission. */
#define BMS_CAN_CFG_TX_MB_INDEX         (0U)

/** @brief Message buffer index reserved for reception. */
#define BMS_CAN_CFG_RX_MB_INDEX         (1U)

/** @brief Nominal CAN bit rate, in bits per second. */
#define BMS_CAN_CFG_BAUDRATE_BPS        (500000U)

/** @brief CAN identifier used to broadcast BMS status frames. */
#define BMS_CAN_CFG_TX_STATUS_ID        (0x100U)

/** @brief CAN identifier the BMS listens to for incoming command frames. */
#define BMS_CAN_CFG_RX_COMMAND_ID       (0x200U)

/** @brief Acceptance mask applied to the reception message buffer. */
#define BMS_CAN_CFG_RX_MASK             (0x7FFU)

/** @brief Set to 1 to use extended (29-bit) identifiers, 0 for standard (11-bit). */
#define BMS_CAN_CFG_USE_EXTENDED_ID     (0U)

#ifdef __cplusplus
}
#endif

#endif /* BMS_CAN_CFG_H */
