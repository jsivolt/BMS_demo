/**
 *  @file   sil_main.c
 *  @brief  SIL harness: init order, task scheduling and the observation surface.
 *
 *  Replaces main.c for the host build. The task ordering below mirrors
 *  Bms_MainFunction_100ms() / Bms_MainFunction_1000ms() for the modules in
 *  scope; the CAN transmit calls and the state machine are omitted because
 *  Bms_Can and Bms_StateMachine are not part of the SOC slice.
 */

#include "sil_api.h"

#include "Bms_Soc.h"
#include "Battery_Monitor.h"
#include "Bms_Nvm.h"
#include "Bms_Vafe.h"
#include "Bms_Vpack.h"
#include "Fault_Manager.h"
#include "Lib_Interp.h"
#include "C40_Ip.h"

#include <string.h>

/* Provided by Sil_AppDoubles.c */
extern void Sil_ResetAppDoubles(void);

/* ================================================================================================
 * Harness state
 * ============================================================================================== */

static uint32  g_ElapsedMs      = 0U;
static uint32  g_MsInSecond     = 0U;

static uint8   g_VpackAlive     = 0U;
static boolean g_SkipAliveOnce  = FALSE;
static boolean g_FreezeAlive    = FALSE;

/* ================================================================================================
 * Lifecycle
 * ============================================================================================== */

void Sil_FlashWipe(void)
{
    Sil_C40_Wipe();
}

void Sil_PowerOn(void)
{
    g_ElapsedMs     = 0U;
    g_MsInSecond    = 0U;
    g_VpackAlive    = 0U;
    g_SkipAliveOnce = FALSE;
    g_FreezeAlive   = FALSE;

    Sil_ResetAppDoubles();

    /* Same order as main.c, restricted to the modules in the SOC slice. */
    FaultManager_Init();
    Bms_Vafe_Init();
    Bms_Vpack_Init();
    BatteryMonitor_Init();
    Bms_Nvm_Init();
    Bms_Soc_Init();
}

/* ================================================================================================
 * Task execution
 * ============================================================================================== */

void Sil_Run100ms(void)
{
    /*
     * main.c order: CAN RX -> vPACK comm health -> battery monitor -> SOC.
     * CAN RX is performed by the test injecting frames before this call.
     */
    Bms_Vpack_MainFunction();
    BatteryMonitor_MainFunction();
    Bms_Soc_MainFunction();
}

void Sil_Run1000ms(void)
{
    Bms_Soc_1sFunction();
}

void Sil_AdvanceMs(uint32 ms)
{
    uint32 remaining = ms;

    while (remaining >= 100U)
    {
        Sil_Run100ms();

        remaining    -= 100U;
        g_ElapsedMs  += 100U;
        g_MsInSecond += 100U;

        if (g_MsInSecond >= 1000U)
        {
            g_MsInSecond -= 1000U;
            Sil_Run1000ms();
        }
    }

    g_ElapsedMs += remaining;
}

uint32 Sil_ElapsedMs(void)
{
    return g_ElapsedMs;
}

/* ================================================================================================
 * Stimulus
 * ============================================================================================== */

void Sil_VpackSkipAlive(void)
{
    g_SkipAliveOnce = TRUE;
}

void Sil_VpackFreezeAlive(boolean enable)
{
    g_FreezeAlive = enable;
}

void Sil_InjectVpackCurrent(sint32 current_mA, sint16 shunt_uV, uint8 status)
{
    uint8  data[8];
    uint32 raw = (uint32)current_mA;
    uint16 sh  = (uint16)shunt_uV;

    data[0] = (uint8)(raw & 0xFFU);
    data[1] = (uint8)((raw >> 8U) & 0xFFU);
    data[2] = (uint8)((raw >> 16U) & 0xFFU);
    data[3] = (uint8)((raw >> 24U) & 0xFFU);

    data[4] = (uint8)(sh & 0xFFU);
    data[5] = (uint8)((sh >> 8U) & 0xFFU);

    if (g_FreezeAlive == TRUE)
    {
        /* Counter stuck: leave g_VpackAlive untouched. */
    }
    else if (g_SkipAliveOnce == TRUE)
    {
        /* Deliberately wrong: jump the counter so the alive check fails. */
        g_VpackAlive    = (uint8)((g_VpackAlive + 2U) & BMS_VPACK_ALIVE_MAX_VALUE);
        g_SkipAliveOnce = FALSE;
    }
    else
    {
        g_VpackAlive = (uint8)((g_VpackAlive + 1U) & BMS_VPACK_ALIVE_MAX_VALUE);
    }

    data[6] = g_VpackAlive;
    data[7] = status;

    Bms_Vpack_ProcessFrame(BMS_VPACK_CAN_ID_CURRENT, 8U, data);
}

void Sil_InjectVpackVoltage(uint32 packVoltage_mV, uint32 busVoltage_mV)
{
    uint8 data[8];

    data[0] = (uint8)(packVoltage_mV & 0xFFU);
    data[1] = (uint8)((packVoltage_mV >> 8U) & 0xFFU);
    data[2] = (uint8)((packVoltage_mV >> 16U) & 0xFFU);
    data[3] = (uint8)((packVoltage_mV >> 24U) & 0xFFU);

    data[4] = (uint8)(busVoltage_mV & 0xFFU);
    data[5] = (uint8)((busVoltage_mV >> 8U) & 0xFFU);
    data[6] = (uint8)((busVoltage_mV >> 16U) & 0xFFU);
    data[7] = (uint8)((busVoltage_mV >> 24U) & 0xFFU);

    Bms_Vpack_ProcessFrame(BMS_VPACK_CAN_ID_VOLTAGE, 8U, data);
}

void Sil_InjectVafeCycle(const uint16 *cells_mV)
{
    static uint8 measurementCounter = 0U;

    uint8 header[8];
    uint8 frame[8];
    uint8 f;
    uint8 c;

    if (cells_mV == NULL_PTR)
    {
        return;
    }

    memset(header, 0, sizeof(header));
    header[0] = measurementCounter;
    measurementCounter++;

    /* 0x405 opens the measurement cycle. */
    Bms_Vafe_ProcessFrame(0x405U, header, 8U);

    for (f = 0U; f < 4U; f++)
    {
        for (c = 0U; c < 4U; c++)
        {
            uint16 mv = cells_mV[(f * 4U) + c];

            frame[c * 2U]        = (uint8)(mv & 0xFFU);
            frame[(c * 2U) + 1U] = (uint8)((mv >> 8U) & 0xFFU);
        }

        Bms_Vafe_ProcessFrame((uint32)(0x401U + f), frame, 8U);
    }
}

void Sil_InjectVafeUniform(uint16 cell_mV)
{
    uint16 cells[16];
    uint8  i;

    for (i = 0U; i < 16U; i++)
    {
        cells[i] = cell_mV;
    }

    Sil_InjectVafeCycle(cells);
}

/* ================================================================================================
 * Observation — SOC
 * ============================================================================================== */

uint16  Sil_SocPack_pct_x10(void)    { return Bms_Soc_GetPackData()->PackSoc_pct_x10; }
boolean Sil_SocPackValid(void)       { return Bms_Soc_GetPackData()->Valid; }

uint16  Sil_SocMin_pct_x10(void)     { return Bms_Soc_GetPackData()->Min.Soc_pct_x10; }
uint16  Sil_SocMax_pct_x10(void)     { return Bms_Soc_GetPackData()->Max.Soc_pct_x10; }
uint16  Sil_SocAvg_pct_x10(void)     { return Bms_Soc_GetPackData()->Avg.Soc_pct_x10; }

float32 Sil_SocMinCapacity_mAh(void) { return Bms_Soc_GetPackData()->Min.RemainingCapacity_mAh; }
float32 Sil_SocMaxCapacity_mAh(void) { return Bms_Soc_GetPackData()->Max.RemainingCapacity_mAh; }
float32 Sil_SocAvgCapacity_mAh(void) { return Bms_Soc_GetPackData()->Avg.RemainingCapacity_mAh; }

boolean Sil_SocMinValid(void)        { return Bms_Soc_GetPackData()->Min.Valid; }
boolean Sil_SocMaxValid(void)        { return Bms_Soc_GetPackData()->Max.Valid; }
boolean Sil_SocAvgValid(void)        { return Bms_Soc_GetPackData()->Avg.Valid; }

uint8   Sil_SocInitSource(void)      { return (uint8)Bms_Soc_GetPackData()->InitSource; }

uint16  Sil_SocLegacy_pct_x10(void)  { return Bms_Soc_GetData()->Soc_pct_x10; }
boolean Sil_SocLegacyValid(void)     { return Bms_Soc_GetData()->Valid; }

void    Sil_SetSoc_pct_x10(uint16 soc_pct_x10) { Bms_Soc_SetSoc_pct_x10(soc_pct_x10); }
uint16  Sil_OcvToSoc(uint16 voltage_mV)        { return Bms_Soc_OcvToSoc(voltage_mV); }

/* ================================================================================================
 * Observation — Battery_Monitor
 * ============================================================================================== */

sint32  Sil_PackCurrent_mA(void)     { return BatteryMonitor_GetData()->PackCurrent_mA[0]; }
boolean Sil_PackCurrentValid(void)   { return BatteryMonitor_GetData()->PackCurrentValid[0]; }
float32 Sil_MinCellVoltage(void)     { return BatteryMonitor_GetData()->MinCellVoltage; }
float32 Sil_MaxCellVoltage(void)     { return BatteryMonitor_GetData()->MaxCellVoltage; }
float32 Sil_AverageCellVoltage(void) { return BatteryMonitor_GetData()->AverageCellVoltage; }
float32 Sil_DeltaCellVoltage(void)   { return BatteryMonitor_GetData()->DeltaCellVoltage; }
boolean Sil_CellVoltageValid(void)   { return BatteryMonitor_GetData()->CellVoltageValid; }
float32 Sil_PackV1(void)             { return BatteryMonitor_GetData()->PackV1; }

/* ================================================================================================
 * Observation / control — flash
 * ============================================================================================== */

uint32 Sil_FlashEraseCount(void)                  { return Sil_C40_EraseCount(); }
uint32 Sil_FlashWriteCount(void)                  { return Sil_C40_WriteCount(); }
uint8  Sil_FlashByte(uint32 offset)               { return Sil_C40_ReadByte(offset); }
void   Sil_FlashWriteByte(uint32 offset, uint8 v) { Sil_C40_WriteByteRaw(offset, v); }
void   Sil_FlashFailNextWrite(boolean enable)     { Sil_C40_FailNextWrite(enable); }
void   Sil_FlashFailNextErase(boolean enable)     { Sil_C40_FailNextErase(enable); }
void   Sil_FlashPowerLossAfterBytes(uint32 bytes) { Sil_C40_PowerLossAfterBytes(bytes); }

uint32 Sil_NvmRecordCount(void)
{
    /* Mirrors Bms_Nvm's on-flash layout: 24-byte records, magic "SOC2". */
    const uint32 recordSize = 24U;
    const uint32 magic      = 0x534F4332UL;

    uint32 count = 0U;
    uint32 index;

    for (index = 0U; (index * recordSize) + recordSize <= SIL_FLASH_SIZE; index++)
    {
        uint32 offset = index * recordSize;
        uint32 value  =
            ((uint32)Sil_C40_ReadByte(offset)) |
            (((uint32)Sil_C40_ReadByte(offset + 1U)) << 8U) |
            (((uint32)Sil_C40_ReadByte(offset + 2U)) << 16U) |
            (((uint32)Sil_C40_ReadByte(offset + 3U)) << 24U);

        if (value == magic)
        {
            count++;
        }
    }

    return count;
}

boolean Sil_NvmLoad(uint16 *socMin, uint16 *socMax, uint16 *socAvg)
{
    return Bms_Nvm_LoadSoc(socMin, socMax, socAvg);
}

/* ================================================================================================
 * Direct unit access
 * ============================================================================================== */

uint16 Sil_InterpLookup(const uint16 *flatTable, uint16 rows, uint16 x)
{
    return Lib_Interp_Lookup_1D_uint16((const uint16 (*)[2])flatTable, rows, x);
}
