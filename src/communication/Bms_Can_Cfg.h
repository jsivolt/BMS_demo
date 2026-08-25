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
#define BMS_CAN_CFG_INSTANCE              (0U)

/** @brief FlexCAN hardware instance 1 (CAN1) used by the BMS application. */
#define BMS_CAN1_CFG_INSTANCE             (1U)

/** @brief Message buffer index reserved for transmission. */
#define BMS_CAN_CFG_TX_MB_INDEX           (0U)

/** @brief CAN1 message buffer index reserved for transmission. */
#define BMS_CAN1_CFG_TX_MB_INDEX          (0U)

/** @brief CAN1 test CAN identifier used for transmission. */
#define BMS_CAN1_CFG_TX_TEST_ID           (0x400U)

/** @brief CAN1 message buffer index reserved for reception. */
#define BMS_CAN1_CFG_RX_MB_INDEX          (1U)

/** @brief CAN1 test CAN identifier used for reception. */
#define BMS_CAN1_CFG_RX_TEST_ID           (0x401U)

/** @brief Message buffer index reserved for debug reception. */
#define BMS_CAN_CFG_RX_DEBUG_MB_INDEX     (1U)

/** @brief Message buffer index reserved for control reception. */
#define BMS_CAN_CFG_RX_CONTROL_MB_INDEX   (2U)

/** @brief Nominal CAN bit rate, in bits per second. */
#define BMS_CAN_CFG_BAUDRATE_BPS        (500000U)

/** @brief CAN identifier used to broadcast BMS status frames. */
#define BMS_CAN_CFG_TX_STATUS_ID          (0x300U)

/** @brief CAN identifier used to broadcast pack voltage/status frames. */
#define BMS_CAN_CFG_TX_PACK_STATUS_ID     (0x301U)

/** @brief CAN identifier used to broadcast contactor status frames. */
#define BMS_CAN_CFG_TX_CONTACTOR_STATUS_ID    (0x302U)

/** @brief CAN identifier used to broadcast Pack1/Pack2 fault mask frames. */
#define BMS_CAN_CFG_TX_FAULT_12_ID             (0x303U)

/** @brief CAN identifier used to broadcast Pack3/system fault mask frames. */
#define BMS_CAN_CFG_TX_FAULT_3_SYSTEM_ID       (0x304U)

/** @brief CAN identifier used for incoming debug frames. */
#define BMS_CAN_CFG_RX_DEBUG_ID           (0x200U)

/** @brief CAN identifier used for incoming control frames. */
#define BMS_CAN_CFG_RX_CONTROL_ID         (0x201U)

/** @brief Acceptance mask applied to the reception message buffer. */
#define BMS_CAN_CFG_RX_MASK             (0x7FFU)

/** @brief Set to 1 to use extended (29-bit) identifiers, 0 for standard (11-bit). */
#define BMS_CAN_CFG_USE_EXTENDED_ID     (0U)

#ifdef __cplusplus
}
#endif

#endif /* BMS_CAN_CFG_H */
