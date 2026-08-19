#ifndef BMS_SPI_H
#define BMS_SPI_H

#include "Std_Types.h"

Std_ReturnType Bms_Spi_Init(void);
Std_ReturnType Bms_Spi_Test(void);

/*
 * Full-duplex synchronous SPI transfer (blocking, hardware CS on PCS0).
 *
 * TxBuffer/RxBuffer may each be NULL_PTR if not needed by the caller.
 */
Std_ReturnType Bms_Spi_Transfer(const uint8 *TxBuffer, uint8 *RxBuffer, uint16 Length);

#endif
