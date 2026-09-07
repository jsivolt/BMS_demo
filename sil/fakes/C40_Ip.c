/**
 *  @file   C40_Ip.c  (SIL fake)
 *  @brief  RAM-backed Data Flash model with real NOR semantics.
 */

#include "C40_Ip.h"
#include "C40_Ip_Cfg.h"

#include <string.h>

const C40_Ip_ConfigType C40_Ip_InitCfg = { 0U };

static uint8   g_Flash[SIL_FLASH_SIZE];
static boolean g_Initialized      = FALSE;

static uint32  g_EraseCount       = 0U;
static uint32  g_WriteCount       = 0U;

static boolean g_FailNextWrite    = FALSE;
static boolean g_FailNextErase    = FALSE;

static uint32  g_PowerLossBudget  = 0U;   /* 0 = disabled */
static boolean g_PowerLost        = FALSE;

/* ================================================================================================
 * Helpers
 * ============================================================================================== */

static boolean Sil_C40_InRange(uint32 address, uint32 length)
{
    if (address < SIL_FLASH_BASE)
    {
        return FALSE;
    }

    if ((address - SIL_FLASH_BASE) > SIL_FLASH_SIZE)
    {
        return FALSE;
    }

    if (((address - SIL_FLASH_BASE) + length) > SIL_FLASH_SIZE)
    {
        return FALSE;
    }

    return TRUE;
}

/* ================================================================================================
 * Driver API
 * ============================================================================================== */

C40_Ip_StatusType C40_Ip_Init(const C40_Ip_ConfigType *ConfigPtr)
{
    (void)ConfigPtr;

    if (g_Initialized == FALSE)
    {
        /* First power-up of the simulated device: sector comes up erased. */
        memset(g_Flash, 0xFF, sizeof(g_Flash));
        g_Initialized = TRUE;
    }

    return C40_IP_STATUS_SUCCESS;
}

C40_Ip_StatusType C40_Ip_Read(uint32 LogicalAddress,
                              uint32 Length,
                              uint8 *Dest)
{
    if ((Dest == NULL_PTR) || (Sil_C40_InRange(LogicalAddress, Length) == FALSE))
    {
        return C40_IP_STATUS_ERROR_INPUT_PARAM;
    }

    memcpy(Dest, &g_Flash[LogicalAddress - SIL_FLASH_BASE], Length);

    return C40_IP_STATUS_SUCCESS;
}

C40_Ip_StatusType C40_Ip_ClearLock(C40_Ip_VirtualSectorsType VirtualSector,
                                   uint8 DomainIdValue)
{
    (void)DomainIdValue;

    if (VirtualSector != (C40_Ip_VirtualSectorsType)C40_DATA_ARRAY_0_BLOCK_4_S000)
    {
        return C40_IP_STATUS_ERROR_INPUT_PARAM;
    }

    return C40_IP_STATUS_SUCCESS;
}

C40_Ip_StatusType C40_Ip_MainInterfaceWrite(uint32 LogicalAddress,
                                            uint32 Length,
                                            const uint8 *Src,
                                            uint8 DomainIdValue)
{
    uint32 i;
    uint32 offset;

    (void)DomainIdValue;

    if (Src == NULL_PTR)
    {
        return C40_IP_STATUS_ERROR_INPUT_PARAM;
    }

    /* The real C40 requires an 8-byte-aligned length, max 128 bytes. */
    if ((Length == 0U) || ((Length % 8U) != 0U) || (Length > 128U))
    {
        return C40_IP_STATUS_ERROR_INPUT_PARAM;
    }

    if (Sil_C40_InRange(LogicalAddress, Length) == FALSE)
    {
        return C40_IP_STATUS_ERROR_INPUT_PARAM;
    }

    if (g_FailNextWrite == TRUE)
    {
        g_FailNextWrite = FALSE;
        return C40_IP_STATUS_ERROR;
    }

    offset = LogicalAddress - SIL_FLASH_BASE;

    for (i = 0U; i < Length; i++)
    {
        if ((g_PowerLossBudget != 0U) || (g_PowerLost == TRUE))
        {
            if (g_PowerLossBudget == 0U)
            {
                /* Supply already gone: remaining bytes are simply not written. */
                continue;
            }

            g_PowerLossBudget--;

            if (g_PowerLossBudget == 0U)
            {
                g_PowerLost = TRUE;
            }
        }

        /* NOR program can only clear bits, never set them. */
        g_Flash[offset + i] &= Src[i];
    }

    g_WriteCount++;

    return C40_IP_STATUS_SUCCESS;
}

C40_Ip_StatusType C40_Ip_MainInterfaceWriteStatus(void)
{
    /* The model completes synchronously; never report BUSY. */
    return C40_IP_STATUS_SUCCESS;
}

C40_Ip_StatusType C40_Ip_MainInterfaceSectorErase(C40_Ip_VirtualSectorsType VirtualSector,
                                                  uint8 DomainIdValue)
{
    (void)DomainIdValue;

    if (VirtualSector != (C40_Ip_VirtualSectorsType)C40_DATA_ARRAY_0_BLOCK_4_S000)
    {
        return C40_IP_STATUS_ERROR_INPUT_PARAM;
    }

    if (g_FailNextErase == TRUE)
    {
        g_FailNextErase = FALSE;
        return C40_IP_STATUS_ERROR;
    }

    if (g_PowerLost == TRUE)
    {
        return C40_IP_STATUS_ERROR;
    }

    memset(g_Flash, 0xFF, sizeof(g_Flash));
    g_EraseCount++;

    return C40_IP_STATUS_SUCCESS;
}

C40_Ip_StatusType C40_Ip_MainInterfaceSectorEraseStatus(void)
{
    return C40_IP_STATUS_SUCCESS;
}

/* ================================================================================================
 * SIL control surface
 * ============================================================================================== */

void Sil_C40_Wipe(void)
{
    memset(g_Flash, 0xFF, sizeof(g_Flash));

    g_EraseCount      = 0U;
    g_WriteCount      = 0U;
    g_FailNextWrite   = FALSE;
    g_FailNextErase   = FALSE;
    g_PowerLossBudget = 0U;
    g_PowerLost       = FALSE;
    g_Initialized     = TRUE;
}

uint8 Sil_C40_ReadByte(uint32 offset)
{
    if (offset >= SIL_FLASH_SIZE)
    {
        return 0U;
    }

    return g_Flash[offset];
}

void Sil_C40_WriteByteRaw(uint32 offset, uint8 value)
{
    if (offset < SIL_FLASH_SIZE)
    {
        g_Flash[offset] = value;
    }
}

uint32 Sil_C40_EraseCount(void)
{
    return g_EraseCount;
}

uint32 Sil_C40_WriteCount(void)
{
    return g_WriteCount;
}

void Sil_C40_FailNextWrite(boolean enable)
{
    g_FailNextWrite = enable;
}

void Sil_C40_FailNextErase(boolean enable)
{
    g_FailNextErase = enable;
}

void Sil_C40_PowerLossAfterBytes(uint32 bytes)
{
    g_PowerLossBudget = bytes;
    g_PowerLost       = (boolean)(bytes == 0U ? FALSE : FALSE);
}
