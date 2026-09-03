/**
 *  @file       Xcp_Can.h
 *  @brief      CAN5 transport layer for XCP: owns the FlexCAN instance 5 hardware
 *              (init, mailboxes, RX polling, TX responses).
 */

#ifndef XCP_CAN_H
#define XCP_CAN_H

#include "Std_Types.h"

Std_ReturnType Xcp_Can_Init(void);

/*
 * Poll CAN5 RX mailbox and dispatch received XCP commands.
 *
 * Call periodically from the BMS scheduler, independently of Bms_Can_MainFunction().
 */
void Xcp_Can_MainFunction(void);

/* Sends an XCP response/error packet (up to 8 bytes) on CAN5. */
void Xcp_Can_SendResponse(const uint8 *data, uint8 length);

#endif
