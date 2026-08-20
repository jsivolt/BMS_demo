#ifndef BMS_CONTACTOR_H
#define BMS_CONTACTOR_H

#include "Std_Types.h"


#define BMS_PACK_COUNT   (3U)

typedef enum
{
    BMS_PACK_1 = 0U,
    BMS_PACK_2,
    BMS_PACK_3

} Bms_PackIdType;


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

void Bms_Contactor_RequestClose(Bms_PackIdType packId);

void Bms_Contactor_RequestOpen(Bms_PackIdType packId);

Bms_ContactorStateType Bms_Contactor_GetState(Bms_PackIdType packId);

Bms_ContactorOutputType Bms_Contactor_GetOutputs(Bms_PackIdType packId);


/*
 * Temporary test interface.
 *
 * Later PackVoltage / BusVoltage will come from ADC / AFE.
 */
void Bms_Contactor_SetPackVoltage(
        Bms_PackIdType packId,
        float voltage);

/*
 * All packs share the same DC bus.
 */
void Bms_Contactor_SetBusVoltage(float voltage);


#endif