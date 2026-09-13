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
#include "../common/Lib_Interp.h"

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
    .TemperatureDeltaMaxClear_dC = 100,        /* 10.0 degC */

    /* Derate windows. PLACEHOLDER calibration, see the map note below. */
    .DerateVHighStart_mV         = 4100U,      /* 4.100 V, inside the 4.250 V trip */
    .DerateVHighEnd_mV           = 4200U,      /* 4.200 V */
    .DerateVLowStart_mV          = 2900U,      /* 2.900 V, inside the 2.500 V trip */
    .DerateVLowEnd_mV            = 2600U,      /* 2.600 V */
    .DerateTHighStart_dC         = 450,        /* 45.0 degC */
    .DerateTHighEnd_dC           = 600,        /* 60.0 degC */
    .DerateFloor                 = 0U          /* 0.000 - each ramp ends at a zero limit */
};

/*==================================================================================================
*                           STATIC SOP LIMIT MAPS - PLACEHOLDER DATA
==================================================================================================*/

/**
 * @brief Shared breakpoints for all three limit maps.
 *
 * The axes are the ones SOP_DESIGN.md section 3.5 proposes. Both are sint32
 * because Lib_Interp_Lookup_2D_uint16() takes signed axes, which is what lets
 * the temperature axis hold negative breakpoints directly.
 */
static const sint32 g_BmsBattCfgSocAxis_pct_x10[] =
{
    0, 100, 200, 400, 600, 800, 900, 1000
};

static const sint32 g_BmsBattCfgTempAxis_dC[] =
{
    -200, -100, 0, 100, 250, 400, 500, 600
};

#define BMS_BATTCFG_SOC_AXIS_SIZE     (sizeof(g_BmsBattCfgSocAxis_pct_x10) / sizeof(g_BmsBattCfgSocAxis_pct_x10[0]))

#define BMS_BATTCFG_TEMP_AXIS_SIZE     (sizeof(g_BmsBattCfgTempAxis_dC) / sizeof(g_BmsBattCfgTempAxis_dC[0]))

/**
 * @brief PLACEHOLDER limit maps. Unit: 0.1 A. Row-major, one row per
 *        temperature breakpoint, SOC along the row.
 *
 * These numbers are NOT from the cell, contactor or fuse datasheets. Nobody
 * measured them. They are shaped to be plausible and to exercise the lookup:
 *
 *   - discharge falls to zero at empty and rolls off at both temperature ends
 *   - charge and regen fall to zero at full
 *   - charge and regen are zero at -20 degC and tiny at -10 degC, because
 *     charging a cold lithium cell plates metal on the anode. This is the
 *     cold protection SOP_DESIGN.md section 3.6.2 says the map carries, since
 *     there is no separate low-temperature derate factor
 *   - every value stays inside the over-current trips in the cell envelope
 *     above: discharge peaks at 90.0 A against a 100.0 A trip, charge at
 *     70.0 A and regen at 75.0 A against an 80.0 A trip
 *
 * Replace all three maps with characterization data before any published
 * limit is trusted. Same status as the OCV curve.
 */
static const uint16 g_BmsBattCfgDischargeMap_dA[] =
{
    /* SOC:   0%   10%   20%   40%   60%   80%   90%  100%          temp */
              0U, 100U, 180U, 250U, 280U, 300U, 300U, 300U,  /* -20.0 degC */
              0U, 150U, 280U, 400U, 450U, 480U, 480U, 480U,  /* -10.0 degC */
              0U, 220U, 400U, 600U, 680U, 720U, 720U, 720U,  /*   0.0 degC */
              0U, 280U, 500U, 750U, 850U, 880U, 900U, 900U,  /*  10.0 degC */
              0U, 300U, 550U, 800U, 880U, 900U, 900U, 900U,  /*  25.0 degC */
              0U, 280U, 520U, 760U, 840U, 880U, 880U, 880U,  /*  40.0 degC */
              0U, 200U, 380U, 550U, 600U, 620U, 620U, 620U,  /*  50.0 degC */
              0U, 100U, 180U, 260U, 300U, 300U, 300U, 300U   /*  60.0 degC */
};

static const uint16 g_BmsBattCfgRegenMap_dA[] =
{
    /* SOC:   0%   10%   20%   40%   60%   80%   90%  100%          temp */
              0U,   0U,   0U,   0U,   0U,   0U,   0U,   0U,  /* -20.0 degC */
             80U,  80U,  70U,  50U,  30U,  15U,   5U,   0U,  /* -10.0 degC */
            200U, 200U, 180U, 150U, 120U,  70U,  35U,   0U,  /*   0.0 degC */
            600U, 600U, 570U, 500U, 400U, 230U, 110U,   0U,  /*  10.0 degC */
            750U, 750U, 720U, 640U, 520U, 320U, 160U,   0U,  /*  25.0 degC */
            700U, 700U, 670U, 590U, 480U, 300U, 150U,   0U,  /*  40.0 degC */
            450U, 450U, 430U, 370U, 300U, 180U,  90U,   0U,  /*  50.0 degC */
            180U, 180U, 170U, 140U, 110U,  70U,  35U,   0U   /*  60.0 degC */
};

static const uint16 g_BmsBattCfgChargeMap_dA[] =
{
    /* SOC:   0%   10%   20%   40%   60%   80%   90%  100%          temp */
              0U,   0U,   0U,   0U,   0U,   0U,   0U,   0U,  /* -20.0 degC */
             50U,  50U,  40U,  30U,  20U,  10U,   5U,   0U,  /* -10.0 degC */
            150U, 150U, 140U, 120U, 100U,  60U,  30U,   0U,  /*   0.0 degC */
            500U, 500U, 480U, 420U, 350U, 200U, 100U,   0U,  /*  10.0 degC */
            700U, 700U, 680U, 600U, 500U, 300U, 150U,   0U,  /*  25.0 degC */
            650U, 650U, 620U, 550U, 450U, 280U, 140U,   0U,  /*  40.0 degC */
            400U, 400U, 380U, 330U, 280U, 170U,  80U,   0U,  /*  50.0 degC */
            150U, 150U, 140U, 120U, 100U,  60U,  30U,   0U   /*  60.0 degC */
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

uint16 Bms_BattCfg_GetStaticLimit_dA(Bms_BattCfg_LimitIdType limitId,
                                     uint16                  soc_pct_x10,
                                     sint16                  temp_dC)
{
    const uint16 *map;

    switch (limitId)
    {
        case BMS_BATTCFG_LIMIT_DISCHARGE:
            map = g_BmsBattCfgDischargeMap_dA;
            break;

        case BMS_BATTCFG_LIMIT_REGEN:
            map = g_BmsBattCfgRegenMap_dA;
            break;

        case BMS_BATTCFG_LIMIT_CHARGE:
            map = g_BmsBattCfgChargeMap_dA;
            break;

        default:
            map = NULL_PTR;
            break;
    }

    if (map == NULL_PTR)
    {
        return 0U;
    }

    return Lib_Interp_Lookup_2D_uint16(
        g_BmsBattCfgSocAxis_pct_x10,
        (uint16)BMS_BATTCFG_SOC_AXIS_SIZE,
        g_BmsBattCfgTempAxis_dC,
        (uint16)BMS_BATTCFG_TEMP_AXIS_SIZE,
        map,
        (sint32)soc_pct_x10,
        (sint32)temp_dC);
}
