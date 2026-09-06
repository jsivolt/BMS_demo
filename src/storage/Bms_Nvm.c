#include "Bms_Nvm.h"

#include "C40_Ip.h"
#include "C40_Ip_Cfg.h"

/* ================================================================================================
 * Configuration
 * ============================================================================================== */

#define BMS_NVM_SOC_SECTOR             C40_DATA_ARRAY_0_BLOCK_4_S000

#define BMS_NVM_SOC_BASE_ADDRESS       (0x10000000UL)
#define BMS_NVM_SOC_SECTOR_SIZE        (0x2000UL)

#define BMS_NVM_SOC_MAGIC              (0x534F4332UL) /* "SOC2" - triple-SOC layout */

#define BMS_NVM_DOMAIN_ID              (0U)

#define BMS_NVM_RECORD_SIZE            (24UL)

#define BMS_NVM_RECORD_COUNT \
    (BMS_NVM_SOC_SECTOR_SIZE / BMS_NVM_RECORD_SIZE)

/* ================================================================================================
 * Types
 * ============================================================================================== */

typedef struct
{
    uint32 Magic;             /* @0  */
    uint32 Sequence;          /* @4  */

    uint16 SocMin_pct_x10;    /* @8  weak / lowest-cell SOC   */
    uint16 SocMax_pct_x10;    /* @10 strong / highest-cell SOC */
    uint16 SocAvg_pct_x10;    /* @12 average-cell SOC          */
    uint16 Reserved;          /* @14 */

    uint32 Checksum;          /* @16 */
    uint32 Reserved2;         /* @20 */

} Bms_NvmSocRecordType;

/* Compile-time assumption: sizeof(Bms_NvmSocRecordType) must stay 24 bytes -
 * a multiple of 8, as C40_Ip_MainInterfaceWrite requires an 8-byte-aligned
 * length. The "SOC1" 16-byte layout that preceded this is rejected by the
 * magic check; a sector still holding those records reads back as "no valid
 * record" until the next save (one boot at the default SOC).
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
    value ^= (uint32)record->SocMin_pct_x10;
    value ^= ((uint32)record->SocMax_pct_x10 << 16U);
    value ^= (uint32)record->SocAvg_pct_x10;
    value ^= 0xA5A55A5AUL;

    return value;
}

static boolean Bms_Nvm_IsErasedRecord(
    const Bms_NvmSocRecordType *record)
{
    if ((record->Magic == 0xFFFFFFFFUL) &&
        (record->Sequence == 0xFFFFFFFFUL) &&
        (record->SocMin_pct_x10 == 0xFFFFU) &&
        (record->SocMax_pct_x10 == 0xFFFFU) &&
        (record->SocAvg_pct_x10 == 0xFFFFU) &&
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

    if ((record->SocMin_pct_x10 > 1000U) ||
        (record->SocMax_pct_x10 > 1000U) ||
        (record->SocAvg_pct_x10 > 1000U))
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

/*
 * Erases the whole persistence sector and blocks until it completes.
 *
 * Blocking: a data-flash sector erase takes on the order of tens of
 * milliseconds. It happens at most once per BMS_NVM_RECORD_COUNT saves
 * (see Bms_Nvm_SaveSoc), so the amortised cost is negligible, but the caller
 * is stalled for that one call.
 */
static boolean Bms_Nvm_EraseSector(void)
{
    C40_Ip_StatusType status;

    status = C40_Ip_ClearLock(
        (C40_Ip_VirtualSectorsType)BMS_NVM_SOC_SECTOR,
        BMS_NVM_DOMAIN_ID);

    if (status != C40_IP_STATUS_SUCCESS)
    {
        return FALSE;
    }

    status = C40_Ip_MainInterfaceSectorErase(
        (C40_Ip_VirtualSectorsType)BMS_NVM_SOC_SECTOR,
        BMS_NVM_DOMAIN_ID);

    if (status != C40_IP_STATUS_SUCCESS)
    {
        return FALSE;
    }

    do
    {
        status = C40_Ip_MainInterfaceSectorEraseStatus();
    }
    while (status == C40_IP_STATUS_BUSY);

    if (status != C40_IP_STATUS_SUCCESS)
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
     * No erased slot found: the sector is full. Point past the end so the next
     * Bms_Nvm_SaveSoc erases and wraps.
     */
    g_BmsNvmNextAddress =
        BMS_NVM_SOC_BASE_ADDRESS +
        BMS_NVM_SOC_SECTOR_SIZE;

    g_BmsNvmInitialized = TRUE;
}

boolean Bms_Nvm_LoadSoc(
    uint16 *SocMin_pct_x10,
    uint16 *SocMax_pct_x10,
    uint16 *SocAvg_pct_x10)
{
    C40_Ip_StatusType status;

    Bms_NvmSocRecordType record;

    uint32 address;
    uint32 index;

    uint32 latestSequence = 0UL;
    uint16 latestMin      = 0U;
    uint16 latestMax      = 0U;
    uint16 latestAvg      = 0U;

    boolean found = FALSE;

    if ((g_BmsNvmInitialized == FALSE) ||
        (SocMin_pct_x10 == NULL_PTR) ||
        (SocMax_pct_x10 == NULL_PTR) ||
        (SocAvg_pct_x10 == NULL_PTR))
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
                latestMin      = record.SocMin_pct_x10;
                latestMax      = record.SocMax_pct_x10;
                latestAvg      = record.SocAvg_pct_x10;

                found = TRUE;
            }
        }
    }

    if (found == TRUE)
    {
        *SocMin_pct_x10 = latestMin;
        *SocMax_pct_x10 = latestMax;
        *SocAvg_pct_x10 = latestAvg;

        return TRUE;
    }

    return FALSE;
}

boolean Bms_Nvm_SaveSoc(
    uint16 SocMin_pct_x10,
    uint16 SocMax_pct_x10,
    uint16 SocAvg_pct_x10)
{
    C40_Ip_StatusType status;

    Bms_NvmSocRecordType record;
    Bms_NvmSocRecordType readBack;

    if (g_BmsNvmInitialized == FALSE)
    {
        return FALSE;
    }

    if ((SocMin_pct_x10 > 1000U) ||
        (SocMax_pct_x10 > 1000U) ||
        (SocAvg_pct_x10 > 1000U))
    {
        return FALSE;
    }

    /*
     * If the next record would not fit, erase the sector and wrap to the
     * start. The values being written now are the newest, so there is nothing
     * from the old sector contents that needs to be carried over.
     *
     * g_BmsNvmNextSequence keeps climbing across the wrap: the record written
     * just below is still the highest sequence in the sector, so Bms_Nvm_Init
     * and Bms_Nvm_LoadSoc still pick it as newest. uint32 will not wrap in any
     * realistic lifetime (>4e9 saves).
     */
    if ((g_BmsNvmNextAddress + BMS_NVM_RECORD_SIZE) >
        (BMS_NVM_SOC_BASE_ADDRESS + BMS_NVM_SOC_SECTOR_SIZE))
    {
        if (Bms_Nvm_EraseSector() == FALSE)
        {
            return FALSE;
        }

        g_BmsNvmNextAddress = BMS_NVM_SOC_BASE_ADDRESS;
    }

    record.Magic          = BMS_NVM_SOC_MAGIC;
    record.Sequence       = g_BmsNvmNextSequence;
    record.SocMin_pct_x10 = SocMin_pct_x10;
    record.SocMax_pct_x10 = SocMax_pct_x10;
    record.SocAvg_pct_x10 = SocAvg_pct_x10;
    record.Reserved       = 0xFFFFU;
    record.Reserved2      = 0xFFFFFFFFUL;

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

    if ((readBack.Magic          != record.Magic) ||
        (readBack.Sequence       != record.Sequence) ||
        (readBack.SocMin_pct_x10 != record.SocMin_pct_x10) ||
        (readBack.SocMax_pct_x10 != record.SocMax_pct_x10) ||
        (readBack.SocAvg_pct_x10 != record.SocAvg_pct_x10) ||
        (readBack.Checksum       != record.Checksum))
    {
        return FALSE;
    }

    g_BmsNvmNextAddress += BMS_NVM_RECORD_SIZE;
    g_BmsNvmNextSequence++;

    return TRUE;
}