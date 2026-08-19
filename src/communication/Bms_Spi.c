#include "Bms_Spi.h"

#include "Lpspi_Ip.h"
#include "Lpspi_Ip_Sa_PBcfg.h"


#define BMS_SPI_CFG_INSTANCE       (1U)
#define BMS_SPI_CFG_TIMEOUT_US      (1000U)


/*
 * Debug status, kept non-static so it can be watched in S32DS Expressions.
 */
volatile Lpspi_Ip_StatusType g_BmsSpiInitStatus;
volatile Lpspi_Ip_StatusType g_BmsSpiTransferStatus;


Std_ReturnType Bms_Spi_Init(void)
{
    g_BmsSpiInitStatus = Lpspi_Ip_Init(&Lpspi_Ip_PhyUnitConfig_SpiPhyUnit_1_Instance_1);

    if (g_BmsSpiInitStatus != LPSPI_IP_STATUS_SUCCESS)
    {
        return (Std_ReturnType)E_NOT_OK;
    }

    return (Std_ReturnType)E_OK;
}


Std_ReturnType Bms_Spi_Transfer(const uint8 *TxBuffer, uint8 *RxBuffer, uint16 Length)
{
    if ((Length == 0U) || ((TxBuffer == NULL_PTR) && (RxBuffer == NULL_PTR)))
    {
        return (Std_ReturnType)E_NOT_OK;
    }

    g_BmsSpiTransferStatus = Lpspi_Ip_SyncTransmit(
        &Lpspi_Ip_DeviceAttributes_SpiExternalDevice_0_Instance_1,
        TxBuffer,
        RxBuffer,
        Length,
        BMS_SPI_CFG_TIMEOUT_US
    );

    if (g_BmsSpiTransferStatus != LPSPI_IP_STATUS_SUCCESS)
    {
        return (Std_ReturnType)E_NOT_OK;
    }

    return (Std_ReturnType)E_OK;
}

Std_ReturnType Bms_Spi_Test(void)
{
    uint8 txData[4] =
    {
        0xAAU,
        0x55U,
        0xA5U,
        0x5AU
    };

    uint8 rxData[4] = {0U};

    return Bms_Spi_Transfer(
        txData,
        rxData,
        (uint16)sizeof(txData)
    );
}