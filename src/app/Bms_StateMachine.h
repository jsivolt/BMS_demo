#ifndef BMS_STATE_MACHINE_H
#define BMS_STATE_MACHINE_H

#include "Std_Types.h"

typedef enum
{
    BMS_STATE_INIT = 0U,
    BMS_STATE_STANDBY,
    BMS_STATE_ACTIVE,
    BMS_STATE_FAULT
} Bms_StateType;

void Bms_StateMachine_Init(void);
void Bms_StateMachine_MainFunction(void);

Bms_StateType Bms_StateMachine_GetState(void);

#endif