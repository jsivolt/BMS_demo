/**
 *  @file       Bms_Sop.h
 *  @brief      Pack 1 State-of-Power estimation: the present current limits.
 *
 *  State of power (SOP) is the largest current the pack can carry right now
 *  without breaking a cell limit. This module publishes three limits for
 *  Pack 1: discharge, regenerative braking and charge.
 *
 *  Each limit comes from one static lookup table indexed by SOC and
 *  temperature, then trimmed by a feedback derate that watches the measured
 *  cell voltages and temperature. All tables and constants come from
 *  Bms_BattCfg; this module holds no battery data of its own.
 *
 *  The module is stateless. Every field is recomputed from the present inputs
 *  on each call, so there is no integration, no filter and no rate limit.
 *
 *  See SOP_DESIGN.md for the full design.
 */

#ifndef BMS_SOP_H
#define BMS_SOP_H

#ifdef __cplusplus
extern "C"{
#endif

#include "Std_Types.h"

/*==================================================================================================
*                                       DEFINES
==================================================================================================*/

/**
 * @brief Operating mode used until a mode-provider component exists.
 *
 * Tune at runtime through g_BmsSopMode.
 */
#define BMS_SOP_DEFAULT_MODE            (BMS_SOP_MODE_DISCHARGE)

/*==================================================================================================
*                                       TYPE DEFINITIONS
==================================================================================================*/

/** @brief Operating mode. Determines which limits are active. */
typedef enum
{
    BMS_SOP_MODE_DISCHARGE = 0U,   /**< Not charging: discharge + regen limits active. */
    BMS_SOP_MODE_CHARGE    = 1U    /**< Charger attached: charge limit active. */
} Bms_Sop_ModeType;

/** @brief One limit, with the intermediate steps kept for calibration. */
typedef struct
{
    /** @brief Raw static-table result, before derating. Unit: 0.1 A. */
    uint16 Table_dA;

    /** @brief Applied derate factor. Unit: 0.001, range 0-1000. */
    uint16 DerateFactor;

    /**
     * @brief Published limit, after derating. Unit: 0.1 A.
     *
     * Forced to 0 when this limit does not apply to the active mode. The two
     * fields above keep their computed values either way, so a calibration
     * tool can still see what the inactive direction would have allowed.
     */
    uint16 Final_dA;

} Bms_Sop_LimitType;

/** @brief Published SOP snapshot. */
typedef struct
{
    Bms_Sop_LimitType Discharge;   /**< Active in Discharge mode. */
    Bms_Sop_LimitType Regen;       /**< Active in Discharge mode. */
    Bms_Sop_LimitType Charge;      /**< Active in Charge mode. */

    /** @brief The mode this cycle ran in. Not published on CAN. */
    Bms_Sop_ModeType Mode;

    /** @brief Which feedback term is currently governing. Diagnostic. */
    boolean DerateActiveVLow;
    boolean DerateActiveVHigh;
    boolean DerateActiveTHigh;

} Bms_Sop_DataType;

/*==================================================================================================
*                                       GLOBAL VARIABLES
==================================================================================================*/

/**
 * @brief Calibratable operating mode. Values of Bms_Sop_ModeType.
 *
 * The design has the mode arriving from a separate mode-provider component
 * (SOP-IR-06). That component does not exist yet, so the mode is a variable
 * here instead: initialized to BMS_SOP_DEFAULT_MODE and left non-static and
 * volatile so it can be overwritten live from a debugger or an XCP master.
 * Same pattern as g_BmsSocOcvWaitTimeout_ms.
 *
 * This is a stand-in, not the final interface. When the mode provider lands,
 * this variable goes away and Bms_Sop reads that component instead.
 *
 * Anything other than a valid Bms_Sop_ModeType is treated as Discharge, which
 * is the safe reading: it publishes no charge limit.
 */
extern volatile uint8 g_BmsSopMode;

/*==================================================================================================
*                                       FUNCTION PROTOTYPES
==================================================================================================*/

/**
 * @brief Initializes the module. Run after Bms_Soc_Init().
 *
 * All limits start at zero. The first MainFunction call publishes real limits
 * 100 ms later. A brief window of zero limit is harmless; a plausible limit
 * published before any measurement was checked is not.
 */
void Bms_Sop_Init(void);

/**
 * @brief Scheduler entry point for the 100 ms update.
 *
 * Must run after BatteryMonitor_MainFunction() and Bms_Soc_MainFunction(),
 * because it reads the outputs of both.
 */
void Bms_Sop_MainFunction(void);

/**
 * @brief Read-only access to the published limits.
 *
 * @return Pointer to the snapshot. Never NULL_PTR.
 */
const Bms_Sop_DataType *Bms_Sop_GetData(void);

#ifdef __cplusplus
}
#endif

#endif /* BMS_SOP_H */
