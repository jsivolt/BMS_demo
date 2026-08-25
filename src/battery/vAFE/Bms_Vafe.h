#ifndef BMS_VAFE_H
#define BMS_VAFE_H

#include "Std_Types.h"

#define BMS_VAFE_CELL_COUNT      (16U)
#define BMS_VAFE_FRAME_COUNT     (4U)

typedef struct
{
    uint16 CellVoltage_mV[BMS_VAFE_CELL_COUNT];

    uint16 MinCellVoltage_mV;
    uint16 MaxCellVoltage_mV;
    uint16 DeltaCellVoltage_mV;

    uint8 MinCellIndex;
    uint8 MaxCellIndex;

    boolean DataValid;
} Bms_Vafe_DataType;

extern Bms_Vafe_DataType g_BmsVafeData;

void Bms_Vafe_Init(void);
void Bms_Vafe_ProcessFrame(uint32 canId, const uint8 *data, uint8 dlc);

#endif