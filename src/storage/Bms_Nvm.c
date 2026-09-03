#include "Bms_Nvm.h"

#include "C40_Ip.h"
#include "C40_Ip_Cfg.h"

/* ================================================================================================
 * Configuration
 * ============================================================================================== */

#define BMS_NVM_SOC_SECTOR             C40_DATA_ARRAY_0_BLOCK_4_S000

#define BMS_NVM_SOC_BASE_ADDRESS       (0x10000000UL)
#define BMS_NVM_SOC_SECTOR_SIZE        (0x2000UL)

#define BMS_NVM_SOC_MAGIC              (0x534F4331UL) /* "SOC1" */

#define BMS_NVM_DOMAIN_ID              (0U)

#define BMS_NVM_RECORD_SIZE            (16UL)

#define BMS_NVM_RECORD_COUNT \
    (BMS_NVM_SOC_SECTOR_SIZE / BMS_NVM_RECORD_SIZE)

/* ================================================================================================
 * Types
 * ============================================================================================== */

typedef struct
{
    uint32 Magic;
    uint32 Sequence;

    uint16 Soc_pct_x10;
    uint16 Reserved;

    uint32 Checksum;

} Bms_NvmSocRecordType;

/* Compile-time assumption:
 * sizeof(Bms_NvmSocRecordType) must remain 16 bytes.
 */

/* ================================================================================================
 * Local variables
 * ============================================================================================== */

static uint32 g_BmsNvmNextAddress;
static uint32 g_BmsNvmNextSequence;

static boolean g_BmsNvmInitialized;

/* ================================================================================================
 * Local functions
 * ============================================================================================== */

static uint32 Bms_Nvm_CalculateChecksum(
    const Bms_NvmSocRecordType *record)
{
    uint32 value;

    value  = record->Magic;
    value ^= record->Sequence;
    value ^= (uint32)record->Soc_pct_x10;
    value ^= 0xA5A55A5AUL;

    return value;
}

static boolean Bms_Nvm_IsErasedRecord(
    const Bms_NvmSocRecordType *record)
{
    if ((record->Magic == 0xFFFFFFFFUL) &&
        (record->Sequence == 0xFFFFFFFFUL) &&
        (record->Soc_pct_x10 == 0xFFFFU) &&
        (record->Reserved == 0xFFFFU) &&
        (record->Checksum == 0xFFFFFFFFUL))
    {
        return TRUE;
    }

    return FALSE;
}

static boolean Bms_Nvm_IsValidRecord(
    const Bms_NvmSocRecordType *record)
{
    uint32 checksum;

    if (record->Magic != BMS_NVM_SOC_MAGIC)
    {
        return FALSE;
    }

    if (record->Soc_pct_x10 > 1000U)
    {
        return FALSE;
    }

    checksum = Bms_Nvm_CalculateChecksum(record);

    if (record->Checksum != checksum)
    {
        return FALSE;
    }

    return TRUE;
}

/* ================================================================================================
 * Global functions
 * ============================================================================================== */

void Bms_Nvm_Init(void)
{
    C40_Ip_StatusType status;

    Bms_NvmSocRecordType record;

    uint32 address;
    uint32 index;

    g_BmsNvmInitialized = FALSE;

    g_BmsNvmNextAddress  = BMS_NVM_SOC_BASE_ADDRESS;
    g_BmsNvmNextSequence = 1UL;

    status = C40_Ip_Init(&C40_Ip_InitCfg);

    if (status != C40_IP_STATUS_SUCCESS)
    {
        return;
    }

    for (index = 0UL;
         index < BMS_NVM_RECORD_COUNT;
         index++)
    {
        address =
            BMS_NVM_SOC_BASE_ADDRESS +
            (index * BMS_NVM_RECORD_SIZE);

        status = C40_Ip_Read(
            address,
            sizeof(record),
            (uint8 *)&record);

        if (status != C40_IP_STATUS_SUCCESS)
        {
            return;
        }

        /*
         * First unused record.
         */
        if (Bms_Nvm_IsErasedRecord(&record) == TRUE)
        {
            g_BmsNvmNextAddress = address;
            g_BmsNvmInitialized = TRUE;
            return;
        }

        /*
         * Track the next sequence number.
         */
        if (Bms_Nvm_IsValidRecord(&record) == TRUE)
        {
            if (record.Sequence >= g_BmsNvmNextSequence)
            {
                g_BmsNvmNextSequence =
                    record.Sequence + 1UL;
            }
        }
    }

    /*
     * Sector full.
     */
    g_BmsNvmNextAddress =
        BMS_NVM_SOC_BASE_ADDRESS +
        BMS_NVM_SOC_SECTOR_SIZE;

    g_BmsNvmInitialized = TRUE;
}

boolean Bms_Nvm_LoadSoc(uint16 *Soc_pct_x10)
{
    C40_Ip_StatusType status;

    Bms_NvmSocRecordType record;

    uint32 address;
    uint32 index;

    uint32 latestSequence = 0UL;
    uint16 latestSoc      = 0U;

    boolean found = FALSE;

    if ((g_BmsNvmInitialized == FALSE) ||
        (Soc_pct_x10 == NULL_PTR))
    {
        return FALSE;
    }

    for (index = 0UL;
         index < BMS_NVM_RECORD_COUNT;
         index++)
    {
        address =
            BMS_NVM_SOC_BASE_ADDRESS +
            (index * BMS_NVM_RECORD_SIZE);

        status = C40_Ip_Read(
            address,
            sizeof(record),
            (uint8 *)&record);

        if (status != C40_IP_STATUS_SUCCESS)
        {
            return FALSE;
        }

        if (Bms_Nvm_IsErasedRecord(&record) == TRUE)
        {
            break;
        }

        if (Bms_Nvm_IsValidRecord(&record) == TRUE)
        {
            if ((found == FALSE) ||
                (record.Sequence > latestSequence))
            {
                latestSequence = record.Sequence;
                latestSoc      = record.Soc_pct_x10;

                found = TRUE;
            }
        }
    }

    if (found == TRUE)
    {
        *Soc_pct_x10 = latestSoc;

        return TRUE;
    }

    return FALSE;
}

boolean Bms_Nvm_SaveSoc(uint16 Soc_pct_x10)
{
    C40_Ip_StatusType status;

    Bms_NvmSocRecordType record;
    Bms_NvmSocRecordType readBack;

    if (g_BmsNvmInitialized == FALSE)
    {
        return FALSE;
    }

    if (Soc_pct_x10 > 1000U)
    {
        return FALSE;
    }

    /*
     * First implementation:
     * do NOT erase automatically when sector becomes full.
     */
    if (g_BmsNvmNextAddress >=
        (BMS_NVM_SOC_BASE_ADDRESS +
         BMS_NVM_SOC_SECTOR_SIZE))
    {
        return FALSE;
    }

    record.Magic       = BMS_NVM_SOC_MAGIC;
    record.Sequence    = g_BmsNvmNextSequence;
    record.Soc_pct_x10 = Soc_pct_x10;
    record.Reserved    = 0xFFFFU;

    record.Checksum =
        Bms_Nvm_CalculateChecksum(&record);

    /*
     * C40 API can automatically request sector unlock during program,
     * but explicitly clear lock here because we enabled the lock API.
     */
    status = C40_Ip_ClearLock(
        (C40_Ip_VirtualSectorsType)BMS_NVM_SOC_SECTOR,
        BMS_NVM_DOMAIN_ID);

    if (status != C40_IP_STATUS_SUCCESS)
    {
        return FALSE;
    }

    status = C40_Ip_MainInterfaceWrite(
        g_BmsNvmNextAddress,
        sizeof(record),
        (const uint8 *)&record,
        BMS_NVM_DOMAIN_ID);

    if (status != C40_IP_STATUS_SUCCESS)
    {
        return FALSE;
    }

    do
    {
        status = C40_Ip_MainInterfaceWriteStatus();
    }
    while (status == C40_IP_STATUS_BUSY);

    if (status != C40_IP_STATUS_SUCCESS)
    {
        return FALSE;
    }

    /*
     * Do not trust C40 internal program verify: read back and compare.
     */
    status = C40_Ip_Read(
        g_BmsNvmNextAddress,
        sizeof(readBack),
        (uint8 *)&readBack);

    if (status != C40_IP_STATUS_SUCCESS)
    {
        return FALSE;
    }

    if ((readBack.Magic       != record.Magic) ||
        (readBack.Sequence    != record.Sequence) ||
        (readBack.Soc_pct_x10 != record.Soc_pct_x10) ||
        (readBack.Reserved    != record.Reserved) ||
        (readBack.Checksum    != record.Checksum))
    {
        return FALSE;
    }

    g_BmsNvmNextAddress += BMS_NVM_RECORD_SIZE;
    g_BmsNvmNextSequence++;

    return TRUE;
}