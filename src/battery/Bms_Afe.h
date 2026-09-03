/**
 *  @file       Bms_Afe.h
 *  @brief      Driver for the battery Analog Front-End (AFE) IC.
 *              Talks to the AFE only through Bms_Spi_Write / Bms_Spi_Read / Bms_Spi_WriteRead,
 *              never touches LPSPI directly.
 */

#ifndef BMS_AFE_H
#define BMS_AFE_H

#ifdef __cplusplus
extern "C"{
#endif

#include "Std_Types.h"

/*==================================================================================================
*                                       DEFINES
==================================================================================================*/

/** @brief Number of series cells monitored by the AFE. */
#define BMS_AFE_NUM_CELLS       (12U)

/*==================================================================================================
*                                       TYPE DEFINITIONS
==================================================================================================*/

/** @brief Snapshot of one AFE acquisition cycle. */
typedef struct
{
    uint16 cellVoltage_mV[BMS_AFE_NUM_CELLS];
    sint16 dieTemperature_dC;   /**< Tenths of a degree Celsius. */
} Bms_Afe_DataType;

/*==================================================================================================
*                                       FUNCTION PROTOTYPES
==================================================================================================*/

/**
 * @brief Initializes the AFE IC (wakes it up and applies default register configuration).
 * @return E_OK on success, E_NOT_OK on communication or configuration failure.
 */
Std_ReturnType Bms_Afe_Init(void);

/**
 * @brief Writes a 16-bit value to an AFE register.
 * @param[in] RegAddr AFE register address.
 * @param[in] Value   Value to write.
 */
Std_ReturnType Bms_Afe_WriteRegister(uint8 RegAddr, uint16 Value);

/**
 * @brief Reads a 16-bit value from an AFE register.
 * @param[in]  RegAddr AFE register address.
 * @param[out] Value   Destination for the register content.
 */
Std_ReturnType Bms_Afe_ReadRegister(uint8 RegAddr, uint16 *Value);

/**
 * @brief Triggers a cell voltage conversion and reads back all cell voltages.
 * @param[out] Data Destination structure filled with the latest measurements.
 */
Std_ReturnType Bms_Afe_ReadCellVoltages(Bms_Afe_DataType * const Data);

#ifdef __cplusplus
}
#endif

#endif /* BMS_AFE_H */
