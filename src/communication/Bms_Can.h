#ifndef BMS_CAN_H
#define BMS_CAN_H

#include "Std_Types.h"

Std_ReturnType Bms_Can_Init(void);

void Bms_Can_SendStatus(void);
void Bms_Can_SendPackStatus(void);
void Bms_Can_SendContactorStatus(void);
void Bms_Can_SendFaultStatus1(void);
void Bms_Can_SendFaultStatus2(void);
void Bms_Can_SendLastFaultStatus1(void);
void Bms_Can_SendLastFaultStatus2(void);

void Bms_Can1_SendTest(void);

void Bms_Can_SendCellSummary(void);
void Bms_Can_SendCellVoltage1_4(void);
void Bms_Can_SendCellVoltage5_8(void);
void Bms_Can_SendCellVoltage9_12(void);
void Bms_Can_SendCellVoltage13_16(void);
void Bms_Can_SendPackCurrent(void);
void Bms_Can_SendPackPower(void);
void Bms_Can_SendSocStatus(void);

/**
 * @brief Transmits the per-cell-extreme SOC estimates (min / max / avg) on 0x30B.
 */
void Bms_Can_SendCellSoc(void);

/**
 * @brief Transmits the Pack 1 state-of-power current limits on 0x30C.
 *
 * Reads Bms_Sop_GetData(). Limits are unsigned magnitudes in 0.1 A; the signal
 * name carries the direction. A limit that does not apply to the active mode
 * is published as zero.
 *
 * The frame carries no validity signal. Bms_Sop does not check whether its
 * inputs are valid, so there is nothing truthful to publish (SOP_DESIGN.md
 * section 5.6). Byte 6 bits 3-7 are reserved and would be the place for one.
 */
void Bms_Can_SendSopLimits(void);

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