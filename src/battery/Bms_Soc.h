/**
 *  @file       Bms_Soc.h
 *  @brief      Pack 1 State-of-Charge estimation using Coulomb counting.
 *
 *  Three independent estimators are maintained for the weakest cell (Min),
 *  the strongest cell (Max) and the cell average (Avg). Each integrates
 *  BatteryMonitor's Pack 1 current (g_BmsVpackData / PackCurrent_mA[0]) over
 *  time to track remaining capacity and SOC percentage. The reported pack SOC
 *  is a weighted blend of the Min and Max estimates, weighted by how close the
 *  Avg estimate sits to empty or full.
 *
 *  See SOC_DESIGN.md for the full design.
 */

#ifndef BMS_SOC_H
#define BMS_SOC_H

#ifdef __cplusplus
extern "C"{
#endif

#include "Std_Types.h"

/*==================================================================================================
*                                       DEFINES
==================================================================================================*/

/** @brief Nominal Pack 1 capacity. TODO: tune to actual cell/pack spec. */
#define BMS_SOC_PACK1_CAPACITY_MAH      (100000UL)

/** @brief Calling period of Bms_Soc_MainFunction. Must match the actual scheduler task period. */
#define BMS_SOC_SAMPLE_PERIOD_MS        (100U)

/** @brief SOC used at Init() when no prior calibration is available. Unit: 0.1 %. */
#define BMS_SOC_INITIAL_PCT_X10         (500U)

#define BMS_SOC_MIN_PCT_X10             (0U)
#define BMS_SOC_MAX_PCT_X10             (1000U)

/** @brief Minimum interval between persisted SOC saves. */
#define BMS_SOC_SAVE_PERIOD_MS       (60000UL)

/** @brief Minimum SOC change (0.1 % units) required to trigger an early save. */
#define BMS_SOC_SAVE_DELTA_X10       (1U)

/**
 * @brief Off-time above which cell voltages are treated as relaxed OCV at startup.
 *
 * Placeholder value; tune after validation. Only relevant once
 * Bms_Soc_GetElapsedSleepTime_s() reports a real elapsed time.
 */
#define BMS_SOC_OCV_RESET_SLEEP_THRESHOLD_S  (28800UL)

/**
 * @brief Default budget for the whole startup wait that precedes an OCV reset.
 *
 * Covers both inputs tier 1 depends on, in order: first the elapsed sleep time
 * becoming readable (Bms_Soc_IsElapsedSleepTimeReady()), then BatteryMonitor
 * reporting CellVoltageValid. Neither can be satisfied while Bms_Soc_Init()
 * runs, because the scheduler that polls CAN has not started yet. If both have
 * not arrived within this one shared window, initialization falls back to NVM
 * restore and then to the default guess.
 *
 * Compile-time default only; tune at runtime through g_BmsSocOcvWaitTimeout_ms.
 */
#define BMS_SOC_OCV_WAIT_TIMEOUT_MS     (500U)

/*==================================================================================================
*                                       TYPE DEFINITIONS
==================================================================================================*/

/** @brief One Coulomb-counting estimator. */
typedef struct
{
    float RemainingCapacity_mAh;

    /** @brief State of charge. Unit: 0.1 %, range 0-1000. */
    uint16 Soc_pct_x10;

    /** @brief TRUE while Pack 1 current is valid and this estimate is being integrated. */
    boolean Valid;

} Bms_Soc_EstimatorType;

/**
 * @brief Legacy single-value SOC snapshot.
 *
 * Kept so existing consumers (CAN SOC status frame) stay unchanged.
 * Soc_pct_x10 mirrors the blended pack SOC.
 */
typedef Bms_Soc_EstimatorType Bms_Soc_DataType;

/**
 * @brief Which reference the estimators were initialized from at startup.
 *
 * Latched once by Bms_Soc_InitPack() and not changed by runtime integration:
 * it describes the provenance of the absolute anchor the Coulomb counter is
 * working from, which is what tells a consumer how far to trust the value.
 * Encoded on CAN 0x308, byte 2 bits 1-3.
 */
typedef enum
{
    /** @brief Tier 3: no reference available; SOC is a compile-time guess and flagged invalid. */
    BMS_SOC_INIT_SOURCE_DEFAULT = 0U,

    /** @brief Tier 1: seeded from relaxed cell voltages through the OCV table. */
    BMS_SOC_INIT_SOURCE_OCV     = 1U,

    /** @brief Tier 2: restored from the last values persisted in Data Flash. */
    BMS_SOC_INIT_SOURCE_NVM     = 2U,

    /**
     * @brief Transient: the estimators are waiting for the inputs a tier 1 OCV
     *        reset needs - the elapsed sleep time becoming readable, then a
     *        valid cell-voltage set. Estimates are invalid while this is
     *        reported; it is replaced by OCV, NVM or DEFAULT once the wait
     *        resolves (at the latest after g_BmsSocOcvWaitTimeout_ms).
     */
    BMS_SOC_INIT_SOURCE_PENDING = 3U

} Bms_Soc_InitSourceType;

/** @brief The three estimators plus the single blended pack result. */
typedef struct
{
    Bms_Soc_EstimatorType Min;
    Bms_Soc_EstimatorType Max;
    Bms_Soc_EstimatorType Avg;

    /** @brief Final blended pack SOC. Unit: 0.1 %, range 0-1000. */
    uint16 PackSoc_pct_x10;

    /** @brief TRUE while all three estimators are being integrated. */
    boolean Valid;

    /** @brief How the estimators were seeded at startup. Latched by Bms_Soc_InitPack(). */
    Bms_Soc_InitSourceType InitSource;

} Bms_Soc_PackType;

/*==================================================================================================
*                                       GLOBAL VARIABLES
==================================================================================================*/

/**
 * @brief Calibratable OCV-wait timeout. Unit: ms.
 *
 * Initialized to BMS_SOC_OCV_WAIT_TIMEOUT_MS. Left non-static so it can be
 * tuned live from a debugger or an XCP master. Effective resolution is the
 * Bms_Soc_MainFunction period (BMS_SOC_SAMPLE_PERIOD_MS), so the wait always
 * ends on a task boundary.
 */
extern volatile uint16 g_BmsSocOcvWaitTimeout_ms;

/**
 * @brief Time already spent waiting for CellVoltageValid. Unit: ms.
 *
 * Diagnostic only: stops advancing once initialization has resolved, so it also
 * records how long the OCV reset actually had to wait.
 */
extern volatile uint32 g_BmsSocOcvWaitElapsed_ms;

/*==================================================================================================
*                                       FUNCTION PROTOTYPES
==================================================================================================*/

/**
 * @brief Startup entry point for SOC initialization.
 *
 * Runs before the scheduler starts, where neither input a tier 1 OCV reset
 * needs is guaranteed: the elapsed sleep time may not be acquired yet, and cell
 * voltages arrive over CAN so no poll has produced them. It initializes
 * immediately only when the sleep time is readable AND too short for an OCV
 * reset - the one case where tier 1 is already ruled out. Otherwise
 * initialization is deferred to Bms_Soc_MainFunctionPack(), which waits for
 * both inputs and falls back to NVM / default after g_BmsSocOcvWaitTimeout_ms.
 */
void Bms_Soc_Init(void);

/**
 * @brief Initializes all three estimators once, at startup.
 *
 * Either OCV-resets each estimator from its own cell-voltage reference (when
 * the system slept long enough for the cells to relax) or restores the last
 * NVM-saved SOC into all three. See SOC_DESIGN.md 3.8.
 */
void Bms_Soc_InitPack(void);

/**
 * @brief Scheduler entry point for the 100 ms update. Wraps Bms_Soc_MainFunctionPack().
 */
void Bms_Soc_MainFunction(void);

/**
 * @brief Integrates Pack 1 current into all three estimators and recomputes the
 *        blended pack SOC. Must be called at a fixed BMS_SOC_SAMPLE_PERIOD_MS
 *        rate, after BatteryMonitor_MainFunction().
 *
 * Also resolves an initialization deferred by Bms_Soc_Init(). No integration
 * happens while that wait is still pending, so the Coulomb counters never
 * accumulate onto an unseeded anchor.
 */
void Bms_Soc_MainFunctionPack(void);

/**
 * @brief Returns the legacy single-value SOC snapshot (blended pack SOC).
 */
const Bms_Soc_DataType *Bms_Soc_GetData(void);

/**
 * @brief Returns the full three-estimator snapshot.
 */
const Bms_Soc_PackType *Bms_Soc_GetPackData(void);

/**
 * @brief Recalibrates all three Coulomb counters to a known SOC.
 * @param[in] NewSoc_pct_x10 New SOC, unit 0.1 %, clamped to [BMS_SOC_MIN_PCT_X10, BMS_SOC_MAX_PCT_X10].
 */
void Bms_Soc_SetSoc_pct_x10(uint16 NewSoc_pct_x10);

/**
 * @brief Converts a relaxed cell voltage to SOC using the OCV table.
 * @param[in] voltage_mV Cell voltage. Unit: mV.
 * @return SOC. Unit: 0.1 %. Clamped to the table's end points.
 */
uint16 Bms_Soc_OcvToSoc(uint16 voltage_mV);

/**
 * @brief Whether the elapsed sleep time has been acquired and may be read.
 *
 * Input to the startup OCV decision: Bms_Soc_GetElapsedSleepTime_s() carries no
 * meaningful value until this reports TRUE, so initialization waits for it
 * before judging whether an OCV reset is eligible. The current source is a
 * compile-time constant and is therefore ready immediately; a real timekeeping
 * source must report FALSE until its value has actually been acquired.
 * See SOC_DESIGN.md 5.2.
 */
boolean Bms_Soc_IsElapsedSleepTimeReady(void);

/**
 * @brief Elapsed time the system was powered off before this boot.
 *
 * Only meaningful once Bms_Soc_IsElapsedSleepTimeReady() reports TRUE. No
 * timekeeping source exists on this hardware yet, so this currently returns 0,
 * which disables the startup OCV reset. See SOC_DESIGN.md 5.2.
 */
uint32 Bms_Soc_GetElapsedSleepTime_s(void);

/**
 * @brief Periodic (1 s) SOC persistence: saves to NVM when due and changed enough.
 */
void Bms_Soc_1sFunction(void);

#ifdef __cplusplus
}
#endif

#endif /* BMS_SOC_H */
