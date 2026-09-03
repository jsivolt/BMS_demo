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

/** @brief CAN1 RX slot 0 - Cell Voltage frame 0. */
#define BMS_CAN1_CFG_RX_MB0_INDEX         (1U)
#define BMS_CAN1_CFG_RX_MB0_ID            (0x401U)

/** @brief CAN1 RX slot 1 - Cell Voltage frame 1. */
#define BMS_CAN1_CFG_RX_MB1_INDEX         (2U)
#define BMS_CAN1_CFG_RX_MB1_ID            (0x402U)

/** @brief CAN1 RX slot 2 - Cell Voltage frame 2. */
#define BMS_CAN1_CFG_RX_MB2_INDEX         (3U)
#define BMS_CAN1_CFG_RX_MB2_ID            (0x403U)

/** @brief CAN1 RX slot 3 - Cell Voltage frame 3. */
#define BMS_CAN1_CFG_RX_MB3_INDEX         (4U)
#define BMS_CAN1_CFG_RX_MB3_ID            (0x404U)

/** @brief CAN1 RX slot 4 - AFE Measurement Header. */
#define BMS_CAN1_CFG_RX_MB4_INDEX         (5U)
#define BMS_CAN1_CFG_RX_MB4_ID            (0x405U)

/** @brief Number of CAN1 RX mailboxes. */
#define BMS_CAN1_CFG_RX_MB_COUNT          (5U)

/** @brief FlexCAN hardware instance 2 (CAN2), used for ADBMS2950 simulation. */
#define BMS_CAN2_CFG_INSTANCE             (2U)

/** @brief CAN2 RX message buffer for the ADBMS2950 current frame. */
#define BMS_CAN2_CFG_RX_CURRENT_MB_INDEX     (1U)

/** @brief CAN2 RX message buffer for the ADBMS2950 voltage frame. */
#define BMS_CAN2_CFG_RX_VOLTAGE_MB_INDEX     (2U)

/** @brief CAN2 RX CAN identifier for the ADBMS2950 current frame. */
#define BMS_CAN2_CFG_RX_CURRENT_ID           (0x410U)

/** @brief CAN2 RX CAN identifier for the ADBMS2950 voltage frame. */
#define BMS_CAN2_CFG_RX_VOLTAGE_ID           (0x411U)

/** @brief Message buffer index reserved for debug reception. */
#define BMS_CAN_CFG_RX_DEBUG_MB_INDEX     (1U)

/** @brief Message buffer index reserved for control reception. */
#define BMS_CAN_CFG_RX_CONTROL_MB_INDEX   (2U)

/** @brief CAN0 nominal bit rate, in bits per second. */
#define BMS_CAN0_CFG_BAUDRATE_BPS        (500000U)

/** @brief CAN1 nominal bit rate, in bits per second. */
#define BMS_CAN1_CFG_BAUDRATE_BPS       (1000000U)

/** @brief CAN2 nominal bit rate, in bits per second. */
#define BMS_CAN2_CFG_BAUDRATE_BPS       (1000000U)

/** @brief FlexCAN hardware instance 5, reserved for XCP. */
#define BMS_CAN5_CFG_INSTANCE              (5U)

/** @brief CAN5 RX mailbox for XCP/test commands. */
#define BMS_CAN5_CFG_RX_MB_INDEX           (0U)

/** @brief CAN5 TX mailbox for XCP/test responses. */
#define BMS_CAN5_CFG_TX_MB_INDEX           (1U)

/** @brief CAN5 test/XCP command CAN ID. */
#define BMS_CAN5_CFG_RX_ID                 (0x600U)

/** @brief CAN5 test/XCP response CAN ID. */
#define BMS_CAN5_CFG_TX_ID                 (0x601U)

/** @brief CAN5 nominal bit rate. */
#define BMS_CAN5_CFG_BAUDRATE_BPS          (1000000U)

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

/** @brief CAN identifier used to broadcast cell voltage summary. */
#define BMS_CAN_CFG_TX_CELL_SUMMARY_ID    (0x305U)

/** @brief CAN identifier used to broadcast cell 1-4 individual voltages. */
#define BMS_CAN_CFG_TX_CELL_VOLTAGE_1_4_ID     (0x310U)

/** @brief CAN identifier used to broadcast cell 5-8 individual voltages. */
#define BMS_CAN_CFG_TX_CELL_VOLTAGE_5_8_ID     (0x311U)

/** @brief CAN identifier used to broadcast cell 9-12 individual voltages. */
#define BMS_CAN_CFG_TX_CELL_VOLTAGE_9_12_ID    (0x312U)

/** @brief CAN identifier used to broadcast cell 13-16 individual voltages. */
#define BMS_CAN_CFG_TX_CELL_VOLTAGE_13_16_ID   (0x313U)

/** @brief CAN identifier used to broadcast pack current status. */
#define BMS_CAN_CFG_TX_PACK_CURRENT_ID      (0x306U)

/** @brief CAN identifier used to broadcast pack power status. */
#define BMS_CAN_CFG_TX_PACK_POWER_ID      (0x307U)

/** @brief CAN identifier used to broadcast SOC status. */
#define BMS_CAN_CFG_TX_SOC_STATUS_ID    (0x308U)

/** @brief CAN identifier used to broadcast latched Pack1/Pack2 fault history. */
#define BMS_CAN_CFG_TX_LAST_FAULT_12_ID         (0x309U)

/** @brief CAN identifier used to broadcast latched Pack3/system fault history. */
#define BMS_CAN_CFG_TX_LAST_FAULT_3_SYSTEM_ID   (0x30AU)

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
