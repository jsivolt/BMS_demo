/**
 *  @file       Bms_BattCfg.c
 *  @brief      Shared battery data configuration.
 *
 *  The OCV curve and the capacity moved here from Bms_Soc.c unchanged
 *  (CFG-FR-04), and the cell safety envelope from Battery_Monitor.c
 *  (CFG-FR-06). Every value is identical to the one its old owner held, so
 *  both moves keep behavior the same (CFG-FR-05).
 *
 *  See SOP_DESIGN.md sections 3.4 and 7.4.
 */

#include "Bms_BattCfg.h"

/*==================================================================================================
*                                       LOCAL CONSTANTS
==================================================================================================*/

/** @brief Nominal Pack 1 capacity. TODO: tune to actual cell/pack spec. Unit: mAh. */
#define BMS_BATTCFG_PACK1_CAPACITY_MAH      (100000UL)

/**
 * @brief Open-circuit-voltage curve. X = cell voltage (mV), Y = SOC (0.1 %).
 *
 * Placeholder curve: must be replaced with characterization data for the
 * actual cell chemistry before the OCV reset is trusted.
 */
static const uint16 g_BmsBattCfgOcvTable[][2] =
{
    { 3000U,    0U },
    { 3300U,  200U },
    { 3450U,  500U },
    { 3600U,  800U },
    { 3700U,  900U },
    { 3900U, 1000U }
};

#define BMS_BATTCFG_OCV_TABLE_SIZE \
    (sizeof(g_BmsBattCfgOcvTable) / sizeof(g_BmsBattCfgOcvTable[0]))

/**
 * @brief Cell safety envelope. Moved from Battery_Monitor.c, values unchanged.
 *
 * Simulation-stage thresholds: tune per cell chemistry later.
 *
 * The over-temperature pair is knowingly unreachable as it stands. Bms_Ntc
 * clamps and invalidates above 125.0 degC (BMS_NTC_CFG_MAX_TEMP_DC), so a
 * 200.0 degC set point can never be met and a real over-temperature surfaces
 * as FAULT_TEMP_SENSOR instead. That is PROJECT_PLAN.md finding F2; the move
 * does not fix it, it only puts the two numbers where the mismatch is visible.
 */
static const Bms_BattCfg_CellLimitsType g_BmsBattCfgCellLimits =
{
    .CellVoltageMax_mV           = 4250U,      /* 4.250 V */
    .CellVoltageMaxClear_mV      = 4150U,      /* 4.150 V */

    .CellVoltageMin_mV           = 2500U,      /* 2.500 V */
    .CellVoltageMinClear_mV      = 2700U,      /* 2.700 V */

    .CellImbalanceMax_mV         = 300U,
    .CellImbalanceMaxClear_mV    = 200U,

    .TemperatureMax_dC           = 2000,       /* 200.0 degC - see F2 above */
    .TemperatureMaxClear_dC      = 1950,       /* 195.0 degC */

    .TemperatureMin_dC           = -200,       /* -20.0 degC */
    .TemperatureMinClear_dC      = -150,       /* -15.0 degC */

    .TemperatureDeltaMax_dC      = 500,        /* 50.0 degC */
    .TemperatureDeltaMaxClear_dC = 100         /* 10.0 degC */
};


/*==================================================================================================
*                                       GLOBAL FUNCTIONS
==================================================================================================*/

uint32 Bms_BattCfg_GetNominalCapacity_mAh(void)
{
    return BMS_BATTCFG_PACK1_CAPACITY_MAH;
}

const uint16 (*Bms_BattCfg_GetOcvTable(void))[2]
{
    return g_BmsBattCfgOcvTable;
}

uint16 Bms_BattCfg_GetOcvTableSize(void)
{
    return (uint16)BMS_BATTCFG_OCV_TABLE_SIZE;
}

const Bms_BattCfg_CellLimitsType *Bms_BattCfg_GetCellLimits(void)
{
    return &g_BmsBattCfgCellLimits;
}
