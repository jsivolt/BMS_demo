/**
 *  @file       Bms_Soc.h
 *  @brief      Pack 1 State-of-Charge estimation using Coulomb counting.
 *
 *  Integrates BatteryMonitor's Pack 1 current (g_BmsVpackData / PackCurrent_mA[0])
 *  over time to track remaining capacity and SOC percentage.
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

/*==================================================================================================
*                                       TYPE DEFINITIONS
==================================================================================================*/

/** @brief Pack 1 Coulomb-counting SOC state. */
typedef struct
{
    float RemainingCapacity_mAh;

    /** @brief State of charge. Unit: 0.1 %, range 0-1000. */
    uint16 Soc_pct_x10;

    /** @brief TRUE while Pack 1 current is valid and the SOC estimate is being integrated. */
    boolean Valid;

} Bms_Soc_DataType;

/*==================================================================================================
*                                       FUNCTION PROTOTYPES
==================================================================================================*/

/**
 * @brief Initializes the SOC estimator to BMS_SOC_INITIAL_PCT_X10.
 */
void Bms_Soc_Init(void);

/**
 * @brief Integrates Pack 1 current over one BMS_SOC_SAMPLE_PERIOD_MS period.
 *        Must be called from BatteryMonitor_MainFunction's caller at a fixed
 *        BMS_SOC_SAMPLE_PERIOD_MS rate, after BatteryMonitor_MainFunction().
 */
void Bms_Soc_MainFunction(void);

/**
 * @brief Returns the current SOC / remaining capacity snapshot.
 */
const Bms_Soc_DataType *Bms_Soc_GetData(void);

/**
 * @brief Recalibrates the Coulomb counter to a known SOC (e.g. at rest / full charge).
 * @param[in] NewSoc_pct_x10 New SOC, unit 0.1 %, clamped to [BMS_SOC_MIN_PCT_X10, BMS_SOC_MAX_PCT_X10].
 */
void Bms_Soc_SetSoc_pct_x10(uint16 NewSoc_pct_x10);

/**
 * @brief Periodic (1 s) SOC persistence: saves to NVM when due and changed enough.
 */
void Bms_Soc_1sFunction(void);

#ifdef __cplusplus
}
#endif

#endif /* BMS_SOC_H */
