/**
 *  @file       Xcp.h
 *  @brief      XCP protocol command processing (CONNECT, GET_STATUS, SET_MTA,
 *              UPLOAD, SHORT_UPLOAD, DOWNLOAD). Transport-agnostic.
 */

#ifndef XCP_H
#define XCP_H

#include "Std_Types.h"

/* Processes one received XCP command packet and sends the response via Xcp_Can. */
void Xcp_ProcessCommand(const uint8 *data, uint8 dlc);

#endif
