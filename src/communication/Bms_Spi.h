#ifndef BMS_SPI_H
#define BMS_SPI_H

#include "Std_Types.h"

Std_ReturnType Bms_Spi_Init(void); 

Std_ReturnType Bms_Spi_Write(
    const uint8 *TxBuffer,
    uint16 Length
);

Std_ReturnType Bms_Spi_Read(
    uint8 *RxBuffer,
    uint16 Length
);

Std_ReturnType Bms_Spi_WriteRead(
    const uint8 *TxBuffer,
    uint8 *RxBuffer,
    uint16 Length
);

/*Std_ReturnType Bms_Spi_Test(void);*/

#endif /* BMS_SPI_H */