#ifndef BMS_CAN_H
#define BMS_CAN_H

#include "Std_Types.h"

Std_ReturnType Bms_Can_Init(void);

void Bms_Can_SendStatus(void);
void Bms_Can_SendPackStatus(void);
void Bms_Can_SendContactorStatus(void);
void Bms_Can_SendFaultStatus1(void);
void Bms_Can_SendFaultStatus2(void);

void Bms_Can1_SendTest(void);

void Bms_Can_SendCellSummary(void);
void Bms_Can_SendPackCurrent(void);
void Bms_Can_SendPackPower(void);
void Bms_Can_SendSocStatus(void);

/*
 * Poll CAN RX mailbox.
 *
 * Call periodically from the BMS scheduler.
 */
void Bms_Can_MainFunction(void);

extern volatile float32 g_CanPack1Voltage_V;
extern volatile boolean g_CanPack1VoltageValid;
extern volatile uint32 g_CanPack1VoltageRxCount;
extern volatile uint32 g_CanPack1VoltageAgeMs;

#endif