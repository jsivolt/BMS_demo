#ifndef BMS_VAFE_H
#define BMS_VAFE_H

#include "Std_Types.h"

#define BMS_VAFE_CELL_COUNT   (16U)

typedef struct
{
    uint16 CellVoltage_mV[BMS_VAFE_CELL_COUNT];
    boolean DataValid;
} Bms_Vafe_DataType;

extern Bms_Vafe_DataType g_BmsVafeData;

void Bms_Vafe_Init(void);
void Bms_Vafe_ProcessFrame(uint32 canId, const uint8 *data, uint8 dlc);

#endif