#ifndef BMS_CONTACTOR_H
#define BMS_CONTACTOR_H

#include "Std_Types.h"


typedef enum
{
    BMS_CONTACTOR_OFF = 0,

    BMS_CONTACTOR_NEG_ON,

    BMS_CONTACTOR_PRECHARGE,

    BMS_CONTACTOR_POS_ON,

    BMS_CONTACTOR_RUN,

    BMS_CONTACTOR_OPENING,

    BMS_CONTACTOR_FAULT

} Bms_ContactorStateType;


typedef struct
{
    boolean negative;
    boolean positive;
    boolean precharge;

} Bms_ContactorOutputType;


void Bms_Contactor_Init(void);

void Bms_Contactor_MainFunction(void);

void Bms_Contactor_RequestClose(void);

void Bms_Contactor_RequestOpen(void);

Bms_ContactorStateType Bms_Contactor_GetState(void);

Bms_ContactorOutputType Bms_Contactor_GetOutputs(void);


/*
 * Temporary test interface.
 *
 * Later PackVoltage / BusVoltage will come from ADC / AFE.
 */
void Bms_Contactor_SetPackVoltage(float voltage);
void Bms_Contactor_SetBusVoltage(float voltage);


#endif