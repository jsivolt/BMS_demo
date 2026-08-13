#ifndef BMS_APP_H
#define BMS_APP_H

typedef enum
{
    BMS_STATE_INIT = 0,
    BMS_STATE_NORMAL,
    BMS_STATE_WARNING,
    BMS_STATE_FAULT
} Bms_StateType;

void Bms_Init(void);
void Bms_MainFunction(void);

void Bms_SetState(Bms_StateType state);
Bms_StateType Bms_GetState(void);

#endif /* BMS_APP_H */
