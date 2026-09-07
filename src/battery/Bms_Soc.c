/**
 *  @file       Bms_Soc.c
 *  @brief      Pack 1 State-of-Charge estimation using Coulomb counting.
 *
 *  Pack 1 current sign convention:
 *    Positive = charge    -> remaining capacity increases
 *    Negative = discharge -> remaining capacity decreases
 *
 *  Three estimators (Min / Max / Avg) are integrated from the same pack
 *  current. They are seeded independently at startup - from the OCV table when
 *  the pack slept long enough for the cells to relax, otherwise from the last
 *  NVM-saved SOC - and the reported pack SOC is a weighted blend of the Min and
 *  Max estimates. See SOC_DESIGN.md.
 */


#include "Bms_Soc.h"
#include "Battery_Monitor.h"
#include "Bms_SleepTime.h"
#include "../storage/Bms_Nvm.h"
#include "../common/Lib_Interp.h"

/*==================================================================================================
*                                       LOCAL CONSTANTS
==================================================================================================*/

/**
 * @brief Open-circuit-voltage curve. X = cell voltage (mV), Y = SOC (0.1 %).
 *
 * Placeholder curve: must be replaced with characterization data for the
 * actual cell chemistry before the OCV reset is trusted.
 */
static const uint16 g_BmsSocOcvTable[][2] =
{
    { 3000U,    0U },
    { 3300U,  200U },
    { 3450U,  500U },
    { 3600U,  800U },
    { 3700U,  900U },
    { 3900U, 1000U }
};

#define BMS_SOC_OCV_TABLE_SIZE \
    (sizeof(g_BmsSocOcvTable) / sizeof(g_BmsSocOcvTable[0]))

/*==================================================================================================
*                                       LOCAL VARIABLES
==================================================================================================*/

static Bms_Soc_PackType g_BmsSocPack;

/** @brief Legacy single-value view of the blended pack SOC. */
static Bms_Soc_DataType g_BmsSocData;

static uint16 g_LastSavedSocMin_pct_x10;
static uint16 g_LastSavedSocMax_pct_x10;
static uint16 g_LastSavedSocAvg_pct_x10;
static uint32 g_SocSaveTimer_ms;

/**
 * @brief TRUE while an eligible OCV reset is waiting for CellVoltageValid.
 *
 * Set by Bms_Soc_Init(), cleared by Bms_Soc_ResolvePendingInit() once the wait
 * ends - either because cell voltages became valid or because the timeout
 * expired. The estimators hold no usable anchor while this is TRUE.
 */
static boolean g_BmsSocInitPending;

/*==================================================================================================
*                                       GLOBAL VARIABLES
==================================================================================================*/

volatile uint16 g_BmsSocOcvWaitTimeout_ms = BMS_SOC_OCV_WAIT_TIMEOUT_MS;

volatile uint32 g_BmsSocOcvWaitElapsed_ms = 0U;

/*==================================================================================================
*                                       LOCAL FUNCTIONS
==================================================================================================*/

/**
 * @brief Converts a cell voltage in volts to whole millivolts.
 */
static uint16 Bms_Soc_VoltsToMilliVolts(float voltage_V)
{
    float voltage_mV;

    if (voltage_V <= 0.0f)
    {
        return 0U;
    }

    voltage_mV = (voltage_V * 1000.0f) + 0.5f;

    if (voltage_mV >= 65535.0f)
    {
        return 65535U;
    }

    return (uint16)voltage_mV;
}

/**
 * @brief Remaining capacity (mAh) equivalent to a SOC value in 0.1 % units.
 */
static float Bms_Soc_SocX10ToCapacity_mAh(uint16 soc_pct_x10)
{
    return ((float)BMS_SOC_PACK1_CAPACITY_MAH * (float)soc_pct_x10) / 1000.0f;
}

/**
 * @brief Absolute difference between two SOC values in 0.1 % units.
 */
static uint16 Bms_Soc_AbsDiff_pct_x10(uint16 a, uint16 b)
{
    return (a >= b) ? (uint16)(a - b) : (uint16)(b - a);
}

/**
 * @brief Stores a new remaining capacity in one estimator, clamped to
 *        [0, BMS_SOC_PACK1_CAPACITY_MAH], and derives Soc_pct_x10 from it.
 *
 * Does not touch est->Valid; that is the caller's decision.
 */
static void Bms_Soc_SetEstimatorCapacity(
    Bms_Soc_EstimatorType *est,
    float capacity_mAh)
{
    if (capacity_mAh < 0.0f)
    {
        capacity_mAh = 0.0f;
    }
    else if (capacity_mAh > (float)BMS_SOC_PACK1_CAPACITY_MAH)
    {
        capacity_mAh = (float)BMS_SOC_PACK1_CAPACITY_MAH;
    }
    else
    {
        /* Within range, no clamping needed. */
    }

    est->RemainingCapacity_mAh = capacity_mAh;

    est->Soc_pct_x10 = (uint16)(((capacity_mAh * 1000.0f)
                                 / (float)BMS_SOC_PACK1_CAPACITY_MAH) + 0.5f);
}

/**
 * @brief Resets one estimator from a relaxed cell voltage.
 *
 * A voltage outside the OCV table's range is clamped to the nearest end point
 * by Bms_Soc_OcvToSoc(), not rejected.
 */
static void Bms_Soc_ApplyOcvReset(
    Bms_Soc_EstimatorType *est,
    uint16 voltage_mV)
{
    uint16 ocvSoc;

    ocvSoc = Bms_Soc_OcvToSoc(voltage_mV);

    Bms_Soc_SetEstimatorCapacity(est, Bms_Soc_SocX10ToCapacity_mAh(ocvSoc));
}

/**
 * @brief Blends Min and Max into the final pack SOC and refreshes the legacy view.
 *
 * The weight comes from the Avg estimate: near empty the result converges to
 * the weak cell, near full to the strong cell.
 */
static void Bms_Soc_ComputePackSoc(void)
{
    uint32 weightMax_x10;
    uint32 weightMin_x10;
    uint32 blended_x10;

    weightMax_x10 = (uint32)g_BmsSocPack.Avg.Soc_pct_x10;

    if (weightMax_x10 > (uint32)BMS_SOC_MAX_PCT_X10)
    {
        weightMax_x10 = (uint32)BMS_SOC_MAX_PCT_X10;
    }

    weightMin_x10 = (uint32)BMS_SOC_MAX_PCT_X10 - weightMax_x10;

    blended_x10 =
        (((uint32)g_BmsSocPack.Min.Soc_pct_x10 * weightMin_x10) +
         ((uint32)g_BmsSocPack.Max.Soc_pct_x10 * weightMax_x10)) /
        (uint32)BMS_SOC_MAX_PCT_X10;

    if (blended_x10 > (uint32)BMS_SOC_MAX_PCT_X10)
    {
        blended_x10 = (uint32)BMS_SOC_MAX_PCT_X10;
    }

    g_BmsSocPack.PackSoc_pct_x10 = (uint16)blended_x10;

    if ((g_BmsSocPack.Min.Valid == TRUE) &&
        (g_BmsSocPack.Max.Valid == TRUE) &&
        (g_BmsSocPack.Avg.Valid == TRUE))
    {
        g_BmsSocPack.Valid = TRUE;
    }
    else
    {
        g_BmsSocPack.Valid = FALSE;
    }

    /* Legacy single-value view consumed by the CAN SOC status frame. */
    g_BmsSocData.RemainingCapacity_mAh = g_BmsSocPack.Avg.RemainingCapacity_mAh;
    g_BmsSocData.Soc_pct_x10           = g_BmsSocPack.PackSoc_pct_x10;
    g_BmsSocData.Valid                 = g_BmsSocPack.Valid;
}

/**
 * @brief Puts the estimators in the "no anchor yet" state used while a deferred
 *        OCV reset is waiting for cell voltages.
 *
 * Capacities are zeroed rather than pre-seeded so that a consumer reading SOC
 * during the wait sees an explicitly invalid value instead of a plausible
 * number that is about to be replaced.
 */
static void Bms_Soc_MarkUnseeded(void)
{
    Bms_Soc_SetEstimatorCapacity(&g_BmsSocPack.Min, 0.0f);
    Bms_Soc_SetEstimatorCapacity(&g_BmsSocPack.Max, 0.0f);
    Bms_Soc_SetEstimatorCapacity(&g_BmsSocPack.Avg, 0.0f);

    g_BmsSocPack.Min.Valid = FALSE;
    g_BmsSocPack.Max.Valid = FALSE;
    g_BmsSocPack.Avg.Valid = FALSE;

    g_BmsSocPack.InitSource = BMS_SOC_INIT_SOURCE_PENDING;

    Bms_Soc_ComputePackSoc();
}

/**
 * @brief Ends the deferred wait and runs the normal 3-tier initialization.
 */
static void Bms_Soc_FinishPendingInit(void)
{
    g_BmsSocInitPending = FALSE;

    Bms_Soc_InitPack();
}

/**
 * @brief Advances the deferred-initialization wait by one task period.
 *
 * @return TRUE when the estimators are initialized and integration may run,
 *         FALSE while the wait is still in progress.
 *
 * Two inputs have to arrive before an OCV reset can be applied, and both are
 * covered by one shared g_BmsSocOcvWaitTimeout_ms budget:
 *
 *   Stage 1 - the elapsed sleep time must become readable. Until
 *             Bms_Soc_IsElapsedSleepTimeReady() reports TRUE there is no way to
 *             tell whether an OCV reset is even eligible, so nothing else can
 *             be decided.
 *   Stage 2 - once the sleep time is readable it decides the outcome:
 *             below the relax threshold there is nothing to wait for and
 *             initialization runs immediately (tier 2 / tier 3); at or above it
 *             the wait continues for CellVoltageValid, which is what makes the
 *             tier 1 OCV branch reachable.
 *
 * If the budget expires in either stage, Bms_Soc_InitPack() is called anyway:
 * its tier 1 condition fails on whichever input is still missing, so it falls
 * through to NVM restore and then to the default guess.
 *
 * The elapsed counter is advanced before the timeout test so that a 500 ms
 * timeout expires on the fifth 100 ms call, i.e. 500 ms after Bms_Soc_Init().
 */
static boolean Bms_Soc_ResolvePendingInit(void)
{
    const BatteryMonitor_DataType *batteryData;

    if (g_BmsSocInitPending == FALSE)
    {
        return TRUE;
    }

    if (Bms_Soc_IsElapsedSleepTimeReady() == TRUE)
    {
        if (Bms_Soc_GetElapsedSleepTime_s() <
                BMS_SOC_OCV_RESET_SLEEP_THRESHOLD_S)
        {
            /*
             * Sleep time known and too short for relaxed cells: an OCV reset is
             * not eligible, so there is nothing left to wait for.
             */
            Bms_Soc_FinishPendingInit();

            return TRUE;
        }

        batteryData = BatteryMonitor_GetData();

        if ((batteryData != NULL_PTR) &&
            (batteryData->CellVoltageValid == TRUE))
        {
            /* Both inputs present: tier 1 OCV reset can now be applied. */
            Bms_Soc_FinishPendingInit();

            return TRUE;
        }
    }

    g_BmsSocOcvWaitElapsed_ms += (uint32)BMS_SOC_SAMPLE_PERIOD_MS;

    if (g_BmsSocOcvWaitElapsed_ms >= (uint32)g_BmsSocOcvWaitTimeout_ms)
    {
        Bms_Soc_FinishPendingInit();

        return TRUE;
    }

    return FALSE;
}

/*==================================================================================================
*                                       GLOBAL FUNCTIONS
==================================================================================================*/

boolean Bms_Soc_IsElapsedSleepTimeReady(void)
{
    return Bms_SleepTime_IsReady();
}

uint32 Bms_Soc_GetElapsedSleepTime_s(void)
{
    return Bms_SleepTime_GetElapsed_s();
}

uint16 Bms_Soc_OcvToSoc(uint16 voltage_mV)
{
    return Lib_Interp_Lookup_1D_uint16(
        g_BmsSocOcvTable,
        (uint16)BMS_SOC_OCV_TABLE_SIZE,
        voltage_mV);
}

void Bms_Soc_InitPack(void)
{
    const BatteryMonitor_DataType *batteryData = BatteryMonitor_GetData();
    uint16 restoredMin;
    uint16 restoredMax;
    uint16 restoredAvg;
    boolean initialized = FALSE;

    /*
     * 1. OCV reset. Taken only when the elapsed sleep time is READABLE, is long
     *    enough for the cells to have relaxed, AND a valid cell-voltage dataset
     *    is available. When taken, it fully initializes all three estimators and
     *    NVM restore is skipped.
     *
     *    None of those three inputs is guaranteed on the call made from
     *    Bms_Soc_Init() itself, so that call defers instead and
     *    Bms_Soc_ResolvePendingInit() calls back in here once they have arrived
     *    - or after the shared wait times out, in which case this condition
     *    fails on whichever input is still missing and tier 2 takes over.
     *
     *    The readiness test comes first deliberately: an unready timekeeping
     *    source has no meaningful elapsed time to compare against the threshold.
     *
     *    It still does not fire today: Bms_Soc_GetElapsedSleepTime_s() is
     *    hardcoded to 0 (SOC_DESIGN.md 5.2), so the reset is never eligible and
     *    every boot initializes immediately from NVM.
     */
    if ((Bms_Soc_IsElapsedSleepTimeReady() == TRUE) &&
        (Bms_Soc_GetElapsedSleepTime_s() >= BMS_SOC_OCV_RESET_SLEEP_THRESHOLD_S) &&
        (batteryData != NULL_PTR) &&
        (batteryData->CellVoltageValid == TRUE))
    {
        Bms_Soc_ApplyOcvReset(
            &g_BmsSocPack.Min,
            Bms_Soc_VoltsToMilliVolts(batteryData->MinCellVoltage));

        Bms_Soc_ApplyOcvReset(
            &g_BmsSocPack.Max,
            Bms_Soc_VoltsToMilliVolts(batteryData->MaxCellVoltage));

        Bms_Soc_ApplyOcvReset(
            &g_BmsSocPack.Avg,
            Bms_Soc_VoltsToMilliVolts(batteryData->AverageCellVoltage));

        g_BmsSocPack.InitSource = BMS_SOC_INIT_SOURCE_OCV;

        initialized = TRUE;
    }

    /*
     * 2. NVM restore. Used whenever the OCV reset was not taken. Each estimator
     *    is restored from its own persisted value.
     */
    if ((initialized == FALSE) &&
        (Bms_Nvm_LoadSoc(&restoredMin, &restoredMax, &restoredAvg) == TRUE))
    {
        Bms_Soc_SetEstimatorCapacity(&g_BmsSocPack.Min,
            Bms_Soc_SocX10ToCapacity_mAh(restoredMin));
        Bms_Soc_SetEstimatorCapacity(&g_BmsSocPack.Max,
            Bms_Soc_SocX10ToCapacity_mAh(restoredMax));
        Bms_Soc_SetEstimatorCapacity(&g_BmsSocPack.Avg,
            Bms_Soc_SocX10ToCapacity_mAh(restoredAvg));

        g_BmsSocPack.InitSource = BMS_SOC_INIT_SOURCE_NVM;

        initialized = TRUE;
    }

    /*
     * 3. Neither reference available: start from a default SOC and flag the
     *    estimates invalid so downstream consumers know the value is a guess.
     */
    if (initialized == FALSE)
    {
        float defaultCapacity_mAh =
            Bms_Soc_SocX10ToCapacity_mAh((uint16)BMS_SOC_INITIAL_PCT_X10);

        Bms_Soc_SetEstimatorCapacity(&g_BmsSocPack.Min, defaultCapacity_mAh);
        Bms_Soc_SetEstimatorCapacity(&g_BmsSocPack.Max, defaultCapacity_mAh);
        Bms_Soc_SetEstimatorCapacity(&g_BmsSocPack.Avg, defaultCapacity_mAh);

        g_BmsSocPack.InitSource = BMS_SOC_INIT_SOURCE_DEFAULT;
    }

    g_BmsSocPack.Min.Valid = initialized;
    g_BmsSocPack.Max.Valid = initialized;
    g_BmsSocPack.Avg.Valid = initialized;

    Bms_Soc_ComputePackSoc();

    g_LastSavedSocMin_pct_x10 = g_BmsSocPack.Min.Soc_pct_x10;
    g_LastSavedSocMax_pct_x10 = g_BmsSocPack.Max.Soc_pct_x10;
    g_LastSavedSocAvg_pct_x10 = g_BmsSocPack.Avg.Soc_pct_x10;

    g_SocSaveTimer_ms = 0U;
}

void Bms_Soc_Init(void)
{
    g_BmsSocInitPending = FALSE;
    g_BmsSocOcvWaitElapsed_ms = 0U;

    /*
     * A tier 1 OCV reset depends on two inputs that are not necessarily
     * available at this point in startup:
     *
     *   - the elapsed sleep time, which decides whether the cells are relaxed
     *     enough to be read as OCV at all, and which a real timekeeping source
     *     may not have acquired yet (Bms_Soc_IsElapsedSleepTimeReady());
     *   - the cell voltages themselves, which arrive over CAN1 from the vAFE.
     *     This function runs before the scheduler starts, so no CAN poll has
     *     executed and CellVoltageValid is necessarily FALSE here.
     *
     * Initialization can only complete immediately in the one case where both
     * are already settled against tier 1: the sleep time is readable AND too
     * short for an OCV reset. Anything else is deferred to
     * Bms_Soc_ResolvePendingInit(), which waits on the 100 ms task and falls
     * back to NVM / default when the shared timeout expires.
     */
    if ((Bms_Soc_IsElapsedSleepTimeReady() == TRUE) &&
        (Bms_Soc_GetElapsedSleepTime_s() <
             BMS_SOC_OCV_RESET_SLEEP_THRESHOLD_S))
    {
        Bms_Soc_InitPack();
    }
    else
    {
        g_BmsSocInitPending = TRUE;

        Bms_Soc_MarkUnseeded();
    }
}

void Bms_Soc_1sFunction(void)
{
    uint16 socMin;
    uint16 socMax;
    uint16 socAvg;
    boolean changedEnough;

    if (g_BmsSocPack.Valid == FALSE)
    {
        return;
    }

    g_SocSaveTimer_ms += 1000U;

    if (g_SocSaveTimer_ms < BMS_SOC_SAVE_PERIOD_MS)
    {
        return;
    }

    g_SocSaveTimer_ms = 0U;

    socMin = g_BmsSocPack.Min.Soc_pct_x10;
    socMax = g_BmsSocPack.Max.Soc_pct_x10;
    socAvg = g_BmsSocPack.Avg.Soc_pct_x10;

    changedEnough =
        (Bms_Soc_AbsDiff_pct_x10(socMin, g_LastSavedSocMin_pct_x10) >= BMS_SOC_SAVE_DELTA_X10) ||
        (Bms_Soc_AbsDiff_pct_x10(socMax, g_LastSavedSocMax_pct_x10) >= BMS_SOC_SAVE_DELTA_X10) ||
        (Bms_Soc_AbsDiff_pct_x10(socAvg, g_LastSavedSocAvg_pct_x10) >= BMS_SOC_SAVE_DELTA_X10);

    if (changedEnough == TRUE)
    {
        if (Bms_Nvm_SaveSoc(socMin, socMax, socAvg) == TRUE)
        {
            g_LastSavedSocMin_pct_x10 = socMin;
            g_LastSavedSocMax_pct_x10 = socMax;
            g_LastSavedSocAvg_pct_x10 = socAvg;
        }
    }
}

void Bms_Soc_MainFunctionPack(void)
{
    const BatteryMonitor_DataType *batteryData;
    sint32 current_mA;

    /*
     * Finish a deferred OCV initialization first. While that wait is pending the
     * estimators hold no anchor, so no current sample may be accumulated onto
     * them - integration resumes on the cycle the wait resolves.
     */
    if (Bms_Soc_ResolvePendingInit() == FALSE)
    {
        return;
    }

    batteryData = BatteryMonitor_GetData();

    if ((batteryData == NULL_PTR) || (batteryData->PackCurrentValid[0] == FALSE))
    {
        g_BmsSocPack.Min.Valid = FALSE;
        g_BmsSocPack.Max.Valid = FALSE;
        g_BmsSocPack.Avg.Valid = FALSE;

        Bms_Soc_ComputePackSoc();

        return;
    }

    current_mA = batteryData->PackCurrent_mA[0];

    /* Charge added this 100 ms tick: dt_h = period_ms / 3600000. */
    {
        float deltaCapacity_mAh = (float)current_mA *
            ((float)BMS_SOC_SAMPLE_PERIOD_MS / 3600000.0f);

        Bms_Soc_SetEstimatorCapacity(&g_BmsSocPack.Min,
            g_BmsSocPack.Min.RemainingCapacity_mAh + deltaCapacity_mAh);
        Bms_Soc_SetEstimatorCapacity(&g_BmsSocPack.Max,
            g_BmsSocPack.Max.RemainingCapacity_mAh + deltaCapacity_mAh);
        Bms_Soc_SetEstimatorCapacity(&g_BmsSocPack.Avg,
            g_BmsSocPack.Avg.RemainingCapacity_mAh + deltaCapacity_mAh);
    }

    g_BmsSocPack.Min.Valid = TRUE;
    g_BmsSocPack.Max.Valid = TRUE;
    g_BmsSocPack.Avg.Valid = TRUE;

    Bms_Soc_ComputePackSoc();
}

void Bms_Soc_MainFunction(void)
{
    Bms_Soc_MainFunctionPack();
}

const Bms_Soc_DataType *Bms_Soc_GetData(void)
{
    return &g_BmsSocData;
}

const Bms_Soc_PackType *Bms_Soc_GetPackData(void)
{
    return &g_BmsSocPack;
}

void Bms_Soc_SetSoc_pct_x10(uint16 NewSoc_pct_x10)
{
    float capacity_mAh = Bms_Soc_SocX10ToCapacity_mAh(NewSoc_pct_x10);

    Bms_Soc_SetEstimatorCapacity(&g_BmsSocPack.Min, capacity_mAh);
    Bms_Soc_SetEstimatorCapacity(&g_BmsSocPack.Max, capacity_mAh);
    Bms_Soc_SetEstimatorCapacity(&g_BmsSocPack.Avg, capacity_mAh);

    Bms_Soc_ComputePackSoc();
}
