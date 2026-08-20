/**
 *  @file       Bms_Afe.c
 *  @brief      Driver for the battery Analog Front-End (AFE) IC.
 *
 *  Communication layer only: all bus access goes through Bms_Spi_Write(),
 *  Bms_Spi_Read() and Bms_Spi_WriteRead(). This file has no knowledge of
 *  LPSPI or any other peripheral.
 *
 *  Frame format assumed for the AFE register protocol:
 *    - Write : [ RegAddr (bit7=0) | DataHigh | DataLow ]
 *    - Read  : [ RegAddr (bit7=1) | DataHigh | DataLow ] (data returned in the same transfer)
 */

#include "Bms_Afe.h"
#include "../communication/Bms_Spi.h"

/*==================================================================================================
*                                       DEFINES
==================================================================================================*/

#define BMS_AFE_READ_BIT            (0x80U)

#define BMS_AFE_REG_CONFIG          (0x01U)
#define BMS_AFE_REG_CELL_CTRL       (0x02U)
#define BMS_AFE_REG_CELL1_BASE      (0x10U)   /* Cell N voltage register = CELL1_BASE + N */

#define BMS_AFE_CFG_DEFAULT         (0x0001U) /* Default config: device active, all cells enabled */
#define BMS_AFE_CELL_CTRL_START_CONV (0x0001U)

#define BMS_AFE_FRAME_LEN           (3U)      /* RegAddr + 16-bit data */

/*==================================================================================================
*                                       LOCAL FUNCTIONS
==================================================================================================*/

Std_ReturnType Bms_Afe_WriteRegister(uint8 RegAddr, uint16 Value)
{
    uint8 txFrame[BMS_AFE_FRAME_LEN];

    txFrame[0] = (uint8)(RegAddr & (uint8)~BMS_AFE_READ_BIT);
    txFrame[1] = (uint8)(Value >> 8U);
    txFrame[2] = (uint8)(Value & 0xFFU);

    return Bms_Spi_Write(txFrame, BMS_AFE_FRAME_LEN);
}

Std_ReturnType Bms_Afe_ReadRegister(uint8 RegAddr, uint16 *Value)
{
    uint8 txFrame[BMS_AFE_FRAME_LEN];
    uint8 rxFrame[BMS_AFE_FRAME_LEN];
    Std_ReturnType status;

    if (Value == NULL_PTR)
    {
        return (Std_ReturnType)E_NOT_OK;
    }

    txFrame[0] = (uint8)(RegAddr | BMS_AFE_READ_BIT);
    txFrame[1] = 0U;
    txFrame[2] = 0U;

    status = Bms_Spi_WriteRead(txFrame, rxFrame, BMS_AFE_FRAME_LEN);

    if (status != (Std_ReturnType)E_OK)
    {
        return (Std_ReturnType)E_NOT_OK;
    }

    *Value = (uint16)(((uint16)rxFrame[1] << 8U) | (uint16)rxFrame[2]);

    return (Std_ReturnType)E_OK;
}

/*==================================================================================================
*                                       GLOBAL FUNCTIONS
==================================================================================================*/

Std_ReturnType Bms_Afe_Init(void)
{
    return Bms_Afe_WriteRegister(BMS_AFE_REG_CONFIG, BMS_AFE_CFG_DEFAULT);
}

Std_ReturnType Bms_Afe_ReadCellVoltages(Bms_Afe_DataType * const Data)
{
    uint8 cellIdx;
    uint16 regValue;

    if (Data == NULL_PTR)
    {
        return (Std_ReturnType)E_NOT_OK;
    }

    if (Bms_Afe_WriteRegister(BMS_AFE_REG_CELL_CTRL, BMS_AFE_CELL_CTRL_START_CONV) != (Std_ReturnType)E_OK)
    {
        return (Std_ReturnType)E_NOT_OK;
    }

    for (cellIdx = 0U; cellIdx < BMS_AFE_NUM_CELLS; cellIdx++)
    {
        if (Bms_Afe_ReadRegister((uint8)(BMS_AFE_REG_CELL1_BASE + cellIdx), &regValue) != (Std_ReturnType)E_OK)
        {
            return (Std_ReturnType)E_NOT_OK;
        }

        /* Register value is assumed to already be expressed in mV. */
        Data->cellVoltage_mV[cellIdx] = regValue;
    }

    Data->dieTemperature_dC = 0;

    return (Std_ReturnType)E_OK;
}
