/**
 *  @file       Bms_BattCfg.h
 *  @brief      Shared battery data configuration.
 *
 *  Single owner of the cell and pack constants the rest of the firmware works
 *  from: the capacity, the OCV curve and the cell safety envelope that the
 *  Battery_Monitor fault path trips on. This module holds constant data and
 *  the functions that return it: no
 *  state, no runtime input and no algorithm (CFG-FR-03), so any component may
 *  call it from any task at any time.
 *
 *  The read functions hand back const pointers into flash rather than copying
 *  table data into a caller buffer (CFG-FR-02).
 *
 *  There is deliberately no Bms_BattCfg_Init(). Nothing here needs
 *  initializing, so an init would be dead code (SOP_DESIGN.md section 7.5).
 *
 *  See SOP_DESIGN.md for the full design.
 */

#ifndef BMS_BATTCFG_H
#define BMS_BATTCFG_H

#ifdef __cplusplus
extern "C"{
#endif

#include "Std_Types.h"

/*==================================================================================================
*                                       TYPE DEFINITIONS
==================================================================================================*/

/**
 * @brief Cell safety envelope: the band the cell must stay inside.
 *
 * Every threshold is hysteretic, so each limit carries the value that raises
 * the fault and the value that clears it again. The clear value always sits
 * on the safe side of the set value, which is what makes the fault latch until
 * the condition has genuinely receded.
 *
 * This is the single definition of these numbers (CFG-FR-01). Battery_Monitor
 * trips its faults on them; once Bms_Sop exists, its derate windows are
 * calibrated against the same struct so a reviewer can check that derating
 * finishes before protection starts (CFG-FR-06, SOP-FR-06).
 */
typedef struct
{
    /** @brief Cell over-voltage. Unit: mV. Set >= Max, clear <= MaxClear. */
    uint16 CellVoltageMax_mV;
    uint16 CellVoltageMaxClear_mV;

    /** @brief Cell under-voltage. Unit: mV. Set <= Min, clear >= MinClear. */
    uint16 CellVoltageMin_mV;
    uint16 CellVoltageMinClear_mV;

    /** @brief Cell imbalance, max minus min cell. Unit: mV. */
    uint16 CellImbalanceMax_mV;
    uint16 CellImbalanceMaxClear_mV;

    /** @brief Pack over-temperature. Unit: 0.1 degC. Set >= Max. */
    sint16 TemperatureMax_dC;
    sint16 TemperatureMaxClear_dC;

    /** @brief Pack under-temperature. Unit: 0.1 degC. Set <= Min. */
    sint16 TemperatureMin_dC;
    sint16 TemperatureMinClear_dC;

    /** @brief Pack-to-pack temperature spread. Unit: 0.1 degC. */
    sint16 TemperatureDeltaMax_dC;
    sint16 TemperatureDeltaMaxClear_dC;

    /*
     * Derate windows for Bms_Sop. Each ramp runs from Start (no derate) to
     * End (DerateFloor, 0 as calibrated), so a limit reaches 0 at End.
     * SOP-FR-06 requires every End to sit on the safe side of the matching
     * fault threshold above, so that the limit is 0 before protection starts.
     * Test SP-12 checks exactly that relation.
     */

    /** @brief Charge and regen derate on rising cell voltage. End < CellVoltageMax_mV. */
    uint16 DerateVHighStart_mV;
    uint16 DerateVHighEnd_mV;

    /** @brief Discharge derate on falling cell voltage. End > CellVoltageMin_mV. */
    uint16 DerateVLowStart_mV;
    uint16 DerateVLowEnd_mV;

    /** @brief All limits derate on rising temperature. End < TemperatureMax_dC. */
    sint16 DerateTHighStart_dC;
    sint16 DerateTHighEnd_dC;

    /** @brief Factor at and past each End. 0 cuts the limit to 0. Unit: 0.001, range 0-1000. */
    uint16 DerateFloor;

} Bms_BattCfg_CellLimitsType;

/**
 * @brief Which static limit table to read.
 *
 * Owned here, not by Bms_Sop. Bms_BattCfg is a foundation module that Bms_Soc
 * and Battery_Monitor already depend on; naming a Bms_Sop type in this header
 * would make Bms_BattCfg.h include Bms_Sop.h while Bms_Sop.h includes this
 * one. The identifier alone picks the table: the operating mode decides which
 * limits Bms_Sop publishes, not which table each limit reads.
 */
typedef enum
{
    BMS_BATTCFG_LIMIT_DISCHARGE = 0U,
    BMS_BATTCFG_LIMIT_REGEN     = 1U,
    BMS_BATTCFG_LIMIT_CHARGE    = 2U
} Bms_BattCfg_LimitIdType;

/*==================================================================================================
*                                       FUNCTION PROTOTYPES
==================================================================================================*/

/**
 * @brief Nominal Pack 1 capacity.
 *
 * TODO: tune to actual cell/pack spec. This is the fixed total the Coulomb
 * counter scales its remaining capacity against; there is no capacity fade.
 *
 * @return Nominal capacity. Unit: mAh.
 */
uint32 Bms_BattCfg_GetNominalCapacity_mAh(void);

/**
 * @brief Open-circuit-voltage curve.
 *
 * Rows are {X, Y} = {cell voltage (mV), SOC (0.1 %)}, sorted ascending by X,
 * in the layout Lib_Interp_Lookup_1D_uint16() expects.
 *
 * Placeholder curve: must be replaced with characterization data for the
 * actual cell chemistry before the OCV reset is trusted.
 *
 * @return Pointer to the first row. Never NULL_PTR.
 */
const uint16 (*Bms_BattCfg_GetOcvTable(void))[2];

/**
 * @brief Number of rows in the OCV curve.
 *
 * @return Row count. Always >= 1.
 */
uint16 Bms_BattCfg_GetOcvTableSize(void);

/**
 * @brief Cell safety envelope.
 *
 * Simulation-stage values: tune per cell chemistry before this is trusted.
 *
 * @return Pointer to the constant envelope. Never NULL_PTR.
 */
const Bms_BattCfg_CellLimitsType *Bms_BattCfg_GetCellLimits(void);

/**
 * @brief Static current limit for one direction, by SOC and temperature.
 *
 * Bilinear interpolation over the calibration map for @p limitId, clamped at
 * every edge. PLACEHOLDER DATA: the maps are shaped plausibly but are not
 * derived from the cell, contactor or fuse datasheets. They must be replaced
 * with real ratings before any limit published from them is trusted.
 *
 * @param[in] limitId     Which map to read.
 * @param[in] soc_pct_x10 State of charge. Unit: 0.1 %, range 0-1000.
 * @param[in] temp_dC     Temperature. Unit: 0.1 degC.
 * @return Current limit magnitude. Unit: 0.1 A. Returns 0 for an unknown id.
 */
uint16 Bms_BattCfg_GetStaticLimit_dA(Bms_BattCfg_LimitIdType limitId,
                                     uint16                  soc_pct_x10,
                                     sint16                  temp_dC);

#ifdef __cplusplus
}
#endif

#endif /* BMS_BATTCFG_H */
