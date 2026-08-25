#include "Bms_Vafe.h"

Bms_Vafe_DataType g_BmsVafeData;

static void Bms_Vafe_Decode4Cells(
    uint8 startIndex,
    const uint8 *data
)
{
    g_BmsVafeData.CellVoltage_mV[startIndex + 0U] =
        ((uint16)data[1] << 8U) | (uint16)data[0];

    g_BmsVafeData.CellVoltage_mV[startIndex + 1U] =
        ((uint16)data[3] << 8U) | (uint16)data[2];

    g_BmsVafeData.CellVoltage_mV[startIndex + 2U] =
        ((uint16)data[5] << 8U) | (uint16)data[4];

    g_BmsVafeData.CellVoltage_mV[startIndex + 3U] =
        ((uint16)data[7] << 8U) | (uint16)data[6];
}

void Bms_Vafe_Init(void)
{
    uint8 i;

    for (i = 0U; i < BMS_VAFE_CELL_COUNT; i++)
    {
        g_BmsVafeData.CellVoltage_mV[i] = 0U;
    }

    g_BmsVafeData.DataValid = FALSE;
}

void Bms_Vafe_ProcessFrame(uint32 canId, const uint8 *data, uint8 dlc)
{
    if ((data == NULL_PTR) || (dlc < 8U))
    {
        return;
    }

    switch (canId)
    {
        case 0x401U:
            Bms_Vafe_Decode4Cells(0U, data);
            g_BmsVafeData.DataValid = TRUE;
            break;

        case 0x402U:
            Bms_Vafe_Decode4Cells(4U, data);
            g_BmsVafeData.DataValid = TRUE;
            break;

        case 0x403U:
            Bms_Vafe_Decode4Cells(8U, data);
            g_BmsVafeData.DataValid = TRUE;
            break;

        case 0x404U:
            Bms_Vafe_Decode4Cells(12U, data);
            g_BmsVafeData.DataValid = TRUE;
            break;

        default:
            break;
    }
}