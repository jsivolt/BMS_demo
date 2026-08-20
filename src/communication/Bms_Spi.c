#include "Bms_Spi.h"
#include "Lpspi_Ip.h"
#include "Lpspi_Ip_Sa_PBcfg.h"

#define BMS_SPI_TIMEOUT_US    (1000U)

volatile Lpspi_Ip_StatusType g_BmsSpiInitStatus;
volatile Lpspi_Ip_StatusType g_BmsSpiTransferStatus;

volatile uint32 g_BmsSpiDebugStep = 0U;

static uint8 g_BmsSpiDummyTx[64];

Std_ReturnType Bms_Spi_Init(void)
{
    g_BmsSpiDebugStep = 1U;

    g_BmsSpiInitStatus =
        Lpspi_Ip_Init(
            &Lpspi_Ip_PhyUnitConfig_SpiPhyUnit_1_Instance_1
        );

    g_BmsSpiDebugStep = 2U;

    if (g_BmsSpiInitStatus != LPSPI_IP_STATUS_SUCCESS)
    {
        g_BmsSpiDebugStep = 3U;
        return (Std_ReturnType)E_NOT_OK;
    }

    g_BmsSpiDebugStep = 4U;

    return (Std_ReturnType)E_OK;
}


Std_ReturnType Bms_Spi_WriteRead(
    const uint8 *TxBuffer,
    uint8 *RxBuffer,
    uint16 Length
)
{
    if ((TxBuffer == NULL_PTR) ||
        (RxBuffer == NULL_PTR) ||
        (Length == 0U))
    {
        return (Std_ReturnType)E_NOT_OK;
    }

    g_BmsSpiTransferStatus =
        Lpspi_Ip_SyncTransmit(
            &Lpspi_Ip_DeviceAttributes_SpiExternalDevice_0_Instance_1,
            TxBuffer,
            RxBuffer,
            Length,
            BMS_SPI_TIMEOUT_US
        );

    if (g_BmsSpiTransferStatus != LPSPI_IP_STATUS_SUCCESS)
    {
        return (Std_ReturnType)E_NOT_OK;
    }

    return (Std_ReturnType)E_OK;
}


Std_ReturnType Bms_Spi_Write(
    const uint8 *TxBuffer,
    uint16 Length
)
{
    uint8 dummyRx[64];

    if ((TxBuffer == NULL_PTR) ||
        (Length == 0U) ||
        (Length > 64U))
    {
        return (Std_ReturnType)E_NOT_OK;
    }

    return Bms_Spi_WriteRead(
        TxBuffer,
        dummyRx,
        Length
    );
}


Std_ReturnType Bms_Spi_Read(
    uint8 *RxBuffer,
    uint16 Length
)
{
    uint16 i;

    if ((RxBuffer == NULL_PTR) ||
        (Length == 0U) ||
        (Length > 64U))
    {
        return (Std_ReturnType)E_NOT_OK;
    }

    for (i = 0U; i < Length; i++)
    {
        g_BmsSpiDummyTx[i] = 0xFFU;
    }

    return Bms_Spi_WriteRead(
        g_BmsSpiDummyTx,
        RxBuffer,
        Length
    );
}