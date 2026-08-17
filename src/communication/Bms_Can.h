#ifndef BMS_CAN_H
#define BMS_CAN_H

#include "Std_Types.h"

Std_ReturnType Bms_Can_Init(void);

void Bms_Can_SendTest(void);

/*
 * Poll CAN RX mailbox.
 *
 * Call periodically from the BMS scheduler.
 */
void Bms_Can_MainFunction(void);

#endif