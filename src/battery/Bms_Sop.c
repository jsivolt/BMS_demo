/**
 *  @file       Bms_Sop.c
 *  @brief      Pack 1 State-of-Power estimation: the present current limits.
 *
 *  Two stages, in a fixed pipeline, with no state carried between calls:
 *
 *    1. Static lookup   - one calibration map per limit, on SOC x temperature.
 *    2. Feedback derate - three ramps on the measured cell voltage extremes
 *                         and the maximum pack temperature, combined by
 *                         minimum, never by product.
 *
 *  The mode then forces the published value of any limit that does not apply
 *  to zero, and the result is written to g_BmsSopData.
 *
 *  Published limits are unsigned magnitudes in 0.1 A. The direction lives in
 *  the field name, so no sign convention reaches the CAN interface.
 *
 *  See SOP_DESIGN.md.
 */

#include "Bms_Sop.h"
#include "Bms_BattCfg.h"
#include "Battery_Monitor.h"
#include "Bms_Soc.h"
#include "../common/Lib_Interp.h"

/*==================================================================================================
*                                       LOCAL CONSTANTS
==================================================================================================*/

/** @brief Derate factor meaning "no derate". Unit: 0.001. */
#define BMS_SOP_DERATE_NONE             (1000U)

/** @brief Millivolts per volt, for the Battery_Monitor float conversion. */
#define BMS_SOP_MV_PER_V                (1000.0f)

/** @brief Highest cell voltage representable in the uint16 mV pipeline. */
#define BMS_SOP_CELL_MV_MAX             (65535.0f)

/*==================================================================================================
*                                       GLOBAL VARIABLES
==================================================================================================*/

volatile uint8 g_BmsSopMode = (uint8)BMS_SOP_DEFAULT_MODE;

#if (BMS_SOP_TEST_OVERRIDE == 1U)
volatile Bms_Sop_TestOverrideType g_BmsSopTestOverride = { 0U, 0U, 0U, 0, 0U, 0U };
#endif

/*==================================================================================================
*                                       LOCAL VARIABLES
==================================================================================================*/

static Bms_Sop_DataType g_BmsSopData;

/*==================================================================================================
*                                       LOCAL FUNCTIONS
==================================================================================================*/

/**
 * @brief Converts a Battery_Monitor cell voltage from volts to millivolts.
 *
 * Battery_Monitor reports cell voltages as float volts. The rest of this
 * module works in uint16 millivolts, so the conversion happens once, here, on
 * the way in. Negative and out-of-range inputs saturate rather than wrap.
 */
static uint16 Bms_Sop_VoltsTo_mV(float volts)
{
    float mV = volts * BMS_SOP_MV_PER_V;

    if (mV <= 0.0f)
    {
        return 0U;
    }

    if (mV >= BMS_SOP_CELL_MV_MAX)
    {
        return (uint16)BMS_SOP_CELL_MV_MAX;
    }

    return (uint16)(mV + 0.5f);
}

/**
 * @brief Straight-line derate ramp between two breakpoints.
 *
 * Builds the two-point table the ramp needs and hands it to the existing 1-D
 * lookup, which already clamps at both ends and copes with a falling Y. The
 * caller passes the breakpoints in whichever order puts them ascending on X,
 * so a rising ramp and a falling one both use this one function.
 *
 * @param[in] x      Present value of the watched signal.
 * @param[in] xLow   Lower X breakpoint.
 * @param[in] yLow   Factor at xLow.
 * @param[in] xHigh  Upper X breakpoint.
 * @param[in] yHigh  Factor at xHigh.
 * @return Factor between the two Y values, clamped outside the window.
 */
static uint16 Bms_Sop_Ramp(uint16 x,
                           uint16 xLow,
                           uint16 yLow,
                           uint16 xHigh,
                           uint16 yHigh)
{
    const uint16 table[2][2] =
    {
        { xLow,  yLow  },
        { xHigh, yHigh }
    };

    return Lib_Interp_Lookup_1D_uint16(table, 2U, x);
}

/**
 * @brief Derate factor for a falling minimum cell voltage.
 *
 * Full factor at or above Start, DerateFloor at or below End. Start is the
 * higher voltage of the two, so the table is built End-first to keep X
 * ascending.
 */
static uint16 Bms_Sop_FactorVLow(uint16 minCell_mV,
                                 const Bms_BattCfg_CellLimitsType *limits)
{
    return Bms_Sop_Ramp(minCell_mV,
                        limits->DerateVLowEnd_mV,   limits->DerateFloor,
                        limits->DerateVLowStart_mV, BMS_SOP_DERATE_NONE);
}

/** @brief Derate factor for a rising maximum cell voltage. */
static uint16 Bms_Sop_FactorVHigh(uint16 maxCell_mV,
                                  const Bms_BattCfg_CellLimitsType *limits)
{
    return Bms_Sop_Ramp(maxCell_mV,
                        limits->DerateVHighStart_mV, BMS_SOP_DERATE_NONE,
                        limits->DerateVHighEnd_mV,   limits->DerateFloor);
}

/**
 * @brief Derate factor for a rising maximum pack temperature.
 *
 * The ramp runs in the uint16 domain but the temperature is signed. The whole
 * window sits above zero, so a temperature at or below the window start is in
 * the safe region by definition and skips the lookup, rather than being cast
 * to a large positive number.
 */
static uint16 Bms_Sop_FactorTHigh(sint16 temp_dC,
                                  const Bms_BattCfg_CellLimitsType *limits)
{
    if ((temp_dC <= 0) || (temp_dC <= limits->DerateTHighStart_dC))
    {
        return BMS_SOP_DERATE_NONE;
    }

    return Bms_Sop_Ramp((uint16)temp_dC,
                        (uint16)limits->DerateTHighStart_dC, BMS_SOP_DERATE_NONE,
                        (uint16)limits->DerateTHighEnd_dC,   limits->DerateFloor);
}

/** @brief Smaller of two derate factors. The rule is minimum, never product. */
static uint16 Bms_Sop_MinFactor(uint16 a, uint16 b)
{
    return (a < b) ? a : b;
}

/** @brief Fills one limit from its table value and its derate factor. */
static void Bms_Sop_ApplyLimit(Bms_Sop_LimitType *limit,
                               uint16 table_dA,
                               uint16 factor)
{
    limit->Table_dA     = table_dA;
    limit->DerateFactor = factor;
    limit->Final_dA     = (uint16)(((uint32)table_dA * (uint32)factor) / 1000U);
}

/*==================================================================================================
*                                       GLOBAL FUNCTIONS
==================================================================================================*/

void Bms_Sop_Init(void)
{
    g_BmsSopData.Discharge.Table_dA     = 0U;
    g_BmsSopData.Discharge.DerateFactor = 0U;
    g_BmsSopData.Discharge.Final_dA     = 0U;

    g_BmsSopData.Regen  = g_BmsSopData.Discharge;
    g_BmsSopData.Charge = g_BmsSopData.Discharge;

    /*
     * Init restores the configured default, as SOP_DESIGN.md section 3.8
     * requires. A mode a calibration tool wrote earlier does not survive a
     * restart: initialization establishes a known state, and Discharge is the
     * safe one because it publishes no charge limit.
     */
    g_BmsSopMode = (uint8)BMS_SOP_DEFAULT_MODE;

    g_BmsSopData.Mode = BMS_SOP_DEFAULT_MODE;

    g_BmsSopData.DerateActiveVLow  = FALSE;
    g_BmsSopData.DerateActiveVHigh = FALSE;
    g_BmsSopData.DerateActiveTHigh = FALSE;
}

void Bms_Sop_MainFunction(void)
{
    const BatteryMonitor_DataType *battery = BatteryMonitor_GetData();
    const Bms_Soc_PackType *soc = Bms_Soc_GetPackData();
    const Bms_BattCfg_CellLimitsType *limits = Bms_BattCfg_GetCellLimits();

    uint16 minCell_mV;
    uint16 maxCell_mV;
    sint16 temp_dC;
    uint16 socMin_pct_x10;
    uint16 socMax_pct_x10;
    uint16 kVLow;
    uint16 kVHigh;
    uint16 kTHigh;

    if ((battery == NULL_PTR) || (soc == NULL_PTR) || (limits == NULL_PTR))
    {
        return;
    }

    /*
     * No validity flag is read here. Bms_Soc and Battery_Monitor both publish
     * one and this module deliberately ignores both: there is no fallback and
     * no validity bit on the published frame. SOP_DESIGN.md section 5.6
     * records what that costs. Revisit before this reaches real hardware.
     */
    minCell_mV = Bms_Sop_VoltsTo_mV(battery->MinCellVoltage);
    maxCell_mV = Bms_Sop_VoltsTo_mV(battery->MaxCellVoltage);
    temp_dC    = battery->MaxPackTemperature_dC;
    socMin_pct_x10 = soc->Min.Soc_pct_x10;
    socMax_pct_x10 = soc->Max.Soc_pct_x10;

#if (BMS_SOP_TEST_OVERRIDE == 1U)
    {
        /* One read of the enable mask, so a write mid-cycle cannot split it. */
        uint8 enable = g_BmsSopTestOverride.Enable;

        if ((enable & BMS_SOP_OVR_MIN_CELL) != 0U)
        {
            minCell_mV = g_BmsSopTestOverride.MinCell_mV;
        }
        if ((enable & BMS_SOP_OVR_MAX_CELL) != 0U)
        {
            maxCell_mV = g_BmsSopTestOverride.MaxCell_mV;
        }
        if ((enable & BMS_SOP_OVR_MAX_TEMP) != 0U)
        {
            temp_dC = g_BmsSopTestOverride.MaxTemp_dC;
        }
        if ((enable & BMS_SOP_OVR_SOC_MIN) != 0U)
        {
            socMin_pct_x10 = g_BmsSopTestOverride.SocMin_pct_x10;
        }
        if ((enable & BMS_SOP_OVR_SOC_MAX) != 0U)
        {
            socMax_pct_x10 = g_BmsSopTestOverride.SocMax_pct_x10;
        }
    }
#endif

    kVLow  = Bms_Sop_FactorVLow(minCell_mV, limits);
    kVHigh = Bms_Sop_FactorVHigh(maxCell_mV, limits);
    kTHigh = Bms_Sop_FactorTHigh(temp_dC, limits);

    g_BmsSopData.DerateActiveVLow  = (kVLow  < BMS_SOP_DERATE_NONE) ? TRUE : FALSE;
    g_BmsSopData.DerateActiveVHigh = (kVHigh < BMS_SOP_DERATE_NONE) ? TRUE : FALSE;
    g_BmsSopData.DerateActiveTHigh = (kTHigh < BMS_SOP_DERATE_NONE) ? TRUE : FALSE;

    /*
     * The discharge map follows the weakest cell, which reaches the low cutoff
     * first under load. The regen and charge maps follow the strongest cell,
     * which reaches the high cutoff first under charge.
     */
    Bms_Sop_ApplyLimit(
        &g_BmsSopData.Discharge,
        Bms_BattCfg_GetStaticLimit_dA(BMS_BATTCFG_LIMIT_DISCHARGE,
                                      socMin_pct_x10, temp_dC),
        Bms_Sop_MinFactor(kVLow, kTHigh));

    Bms_Sop_ApplyLimit(
        &g_BmsSopData.Regen,
        Bms_BattCfg_GetStaticLimit_dA(BMS_BATTCFG_LIMIT_REGEN,
                                      socMax_pct_x10, temp_dC),
        Bms_Sop_MinFactor(kVHigh, kTHigh));

    Bms_Sop_ApplyLimit(
        &g_BmsSopData.Charge,
        Bms_BattCfg_GetStaticLimit_dA(BMS_BATTCFG_LIMIT_CHARGE,
                                      socMax_pct_x10, temp_dC),
        Bms_Sop_MinFactor(kVHigh, kTHigh));

    /*
     * Anything that is not a valid mode reads as Discharge. That is the safe
     * default, because it publishes no charge limit.
     */
    g_BmsSopData.Mode = (g_BmsSopMode == (uint8)BMS_SOP_MODE_CHARGE)
                        ? BMS_SOP_MODE_CHARGE
                        : BMS_SOP_MODE_DISCHARGE;

    /*
     * Only the published value is forced to zero. Table_dA and DerateFactor
     * keep their computed values, so the inactive direction stays visible to a
     * calibration tool (SOP-FR-03).
     */
    if (g_BmsSopData.Mode == BMS_SOP_MODE_CHARGE)
    {
        g_BmsSopData.Discharge.Final_dA = 0U;
        g_BmsSopData.Regen.Final_dA     = 0U;
    }
    else
    {
        g_BmsSopData.Charge.Final_dA = 0U;
    }
}

const Bms_Sop_DataType *Bms_Sop_GetData(void)
{
    return &g_BmsSopData;
}
