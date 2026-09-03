/**
 *  @file       Xcp_Cfg.h
 *  @brief      Compile-time configuration for the XCP-over-CAN module.
 */

#ifndef XCP_CFG_H
#define XCP_CFG_H

#include "Std_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*==================================================================================================
*                                  CAN5 TRANSPORT CONFIGURATION
==================================================================================================*/

/** @brief FlexCAN hardware instance 5, reserved for XCP. */
#define XCP_CAN_CFG_INSTANCE              (5U)

/** @brief CAN5 RX mailbox for XCP commands. */
#define XCP_CAN_CFG_RX_MB_INDEX           (0U)

/** @brief CAN5 TX mailbox for XCP responses. */
#define XCP_CAN_CFG_TX_MB_INDEX           (1U)

/** @brief CAN5 XCP command CAN ID. */
#define XCP_CAN_CFG_RX_ID                 (0x600U)

/** @brief CAN5 XCP response CAN ID. */
#define XCP_CAN_CFG_TX_ID                 (0x601U)

/** @brief CAN5 nominal bit rate. */
#define XCP_CAN_CFG_BAUDRATE_BPS          (1000000U)

/*==================================================================================================
*                                   XCP PROTOCOL CONFIGURATION
==================================================================================================*/

/** @brief Maximum length of a CTO (command/response) packet. */
#define XCP_CFG_MAX_CTO                    (8U)

/** @brief Maximum length of a DTO packet. */
#define XCP_CFG_MAX_DTO                    (8U)

/** @brief Reported XCP protocol layer version (BCD 1.0). */
#define XCP_CFG_PROTOCOL_VERSION           (0x10U)

/** @brief Reported XCP transport layer version (BCD 1.0). */
#define XCP_CFG_TRANSPORT_VERSION          (0x10U)

/**
 * @brief First bring-up whitelist for UPLOAD/SHORT_UPLOAD/DOWNLOAD.
 *
 * Allow only SRAM around the current application RAM area.
 * Can be tightened/expanded later based on the linker map.
 */
#define XCP_CFG_VALID_RAM_START             (0x20400000U)
#define XCP_CFG_VALID_RAM_END               (0x2047FFFFU)

#ifdef __cplusplus
}
#endif

#endif /* XCP_CFG_H */
