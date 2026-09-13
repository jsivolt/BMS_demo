"""Bms_Sop — state of power, the published current limits.

Covers the validation plan in SOP_DESIGN.md section 4: the static limit maps
(SP-01, SP-02), the feedback derate (SP-03, SP-04), the mode gating (SP-05)
and the static configuration check (SP-12).

The limit maps under test are PLACEHOLDER calibration, not datasheet ratings,
so these cases assert the arithmetic and the shape of the maps, never that a
particular current is safe for a real cell.

Per-case results are in sil/reports/.
"""

from __future__ import annotations

import pytest

from bms_sil import (
    SOP_DERATE_NONE,
    SOP_LIMIT_CHARGE,
    SOP_LIMIT_DISCHARGE,
    SOP_LIMIT_REGEN,
    SOP_MODE_CHARGE,
    SOP_MODE_DISCHARGE,
    SOP_SOC_AXIS,
    SOP_TEMP_AXIS,
)

# Room temperature in 0.1 degC — a breakpoint on the temperature axis, so a
# lookup there needs no interpolation along that axis.
T_ROOM = 250


def settle(bms, *, soc_pct_x10=500, cell_mV=3600, temp_dC=T_ROOM):
    """Bring the pipeline to a known steady state, then run one more cycle.

    Initialisation is deferred, so the first few cycles report a pending SOC.
    Running past the wait budget first keeps these cases about Bms_Sop.
    """
    bms.run_normal(600, cell_mV=cell_mV)
    bms.set_ntc(temp_dC, temp_dC, temp_dC)
    bms.set_soc(soc_pct_x10)
    bms.run_normal(100, cell_mV=cell_mV)


# ---------------------------------------------------------------------------
# SP-01 / SP-02 — the static limit maps
# ---------------------------------------------------------------------------


def test_SP_01_map_returns_stored_value_at_a_breakpoint(bms):
    """SP-01: a lookup on both axes at breakpoints returns the stored value."""
    # Discharge, SOC 10 %, 25.0 degC — row 4, column 1 of the placeholder map.
    assert bms.sop_static_limit(SOP_LIMIT_DISCHARGE, 100, T_ROOM) == 300
    # Discharge, SOC 100 %, 25.0 degC — the map peak.
    assert bms.sop_static_limit(SOP_LIMIT_DISCHARGE, 1000, T_ROOM) == 900


def test_SP_01_map_interpolates_between_breakpoints(bms):
    """SP-01: a SOC between two breakpoints interpolates linearly."""
    low = bms.sop_static_limit(SOP_LIMIT_DISCHARGE, 100, T_ROOM)
    high = bms.sop_static_limit(SOP_LIMIT_DISCHARGE, 200, T_ROOM)
    mid = bms.sop_static_limit(SOP_LIMIT_DISCHARGE, 150, T_ROOM)

    assert low < mid < high
    assert mid == (low + high) // 2


def test_SP_01_map_clamps_at_all_four_edges(bms):
    """SP-01: SOC and temperature past an edge clamp, never extrapolate."""
    soc_hi, soc_lo = SOP_SOC_AXIS[-1], SOP_SOC_AXIS[0]
    t_hi, t_lo = SOP_TEMP_AXIS[-1], SOP_TEMP_AXIS[0]

    # Past the SOC edges, on a temperature breakpoint.
    assert bms.sop_static_limit(SOP_LIMIT_DISCHARGE, 60000, T_ROOM) == bms.sop_static_limit(
        SOP_LIMIT_DISCHARGE, soc_hi, T_ROOM
    )
    assert bms.sop_static_limit(SOP_LIMIT_DISCHARGE, 0, T_ROOM) == bms.sop_static_limit(
        SOP_LIMIT_DISCHARGE, soc_lo, T_ROOM
    )

    # Past the temperature edges, on a SOC breakpoint.
    assert bms.sop_static_limit(SOP_LIMIT_DISCHARGE, 600, 30000) == bms.sop_static_limit(
        SOP_LIMIT_DISCHARGE, 600, t_hi
    )
    assert bms.sop_static_limit(SOP_LIMIT_DISCHARGE, 600, -30000) == bms.sop_static_limit(
        SOP_LIMIT_DISCHARGE, 600, t_lo
    )


def test_SP_01_unknown_limit_id_returns_zero(bms):
    """SP-01: an id outside the enum returns 0 rather than reading past a map."""
    assert bms.sop_static_limit(99, 500, T_ROOM) == 0


def test_SP_02_the_three_maps_are_distinct(bms):
    """SP-02: discharge, regen and charge are three separate calibrations."""
    soc, temp = 400, T_ROOM
    d = bms.sop_static_limit(SOP_LIMIT_DISCHARGE, soc, temp)
    r = bms.sop_static_limit(SOP_LIMIT_REGEN, soc, temp)
    c = bms.sop_static_limit(SOP_LIMIT_CHARGE, soc, temp)

    assert d != r
    assert r != c


def test_SP_02_charge_and_regen_reach_zero_at_full(bms):
    """SP-02: a full pack accepts no charge and no regen, at any temperature."""
    for temp in SOP_TEMP_AXIS:
        assert bms.sop_static_limit(SOP_LIMIT_CHARGE, 1000, temp) == 0
        assert bms.sop_static_limit(SOP_LIMIT_REGEN, 1000, temp) == 0


def test_SP_02_discharge_reaches_zero_at_empty(bms):
    """SP-02: an empty pack delivers no discharge current, at any temperature."""
    for temp in SOP_TEMP_AXIS:
        assert bms.sop_static_limit(SOP_LIMIT_DISCHARGE, 0, temp) == 0


def test_SP_02_cold_blocks_charge_and_regen(bms):
    """SP-02: at -20 degC the maps allow no charge current at any SOC.

    This is the cold protection SOP_DESIGN.md section 3.6.2 puts in the map
    rather than in a separate derate factor: charging a cold lithium cell
    plates metal on the anode, and that does not reverse.
    """
    for soc in SOP_SOC_AXIS:
        assert bms.sop_static_limit(SOP_LIMIT_CHARGE, soc, -200) == 0
        assert bms.sop_static_limit(SOP_LIMIT_REGEN, soc, -200) == 0

    # Discharging cold is allowed, just heavily reduced.
    assert bms.sop_static_limit(SOP_LIMIT_DISCHARGE, 1000, -200) > 0
    assert bms.sop_static_limit(SOP_LIMIT_DISCHARGE, 1000, -200) < bms.sop_static_limit(
        SOP_LIMIT_DISCHARGE, 1000, T_ROOM
    )


def test_SP_02_placeholder_maps_stay_inside_the_over_current_trips(bms):
    """SP-02: no map value would itself trip an over-current fault.

    The discharge trip is 100.0 A and the charge trip 80.0 A, so a published
    limit at or above either would invite the consumer straight into a fault.
    """
    for soc in SOP_SOC_AXIS:
        for temp in SOP_TEMP_AXIS:
            assert bms.sop_static_limit(SOP_LIMIT_DISCHARGE, soc, temp) < 1000
            assert bms.sop_static_limit(SOP_LIMIT_CHARGE, soc, temp) < 800
            assert bms.sop_static_limit(SOP_LIMIT_REGEN, soc, temp) < 800


# ---------------------------------------------------------------------------
# SP-03 / SP-04 — the feedback derate
# ---------------------------------------------------------------------------


def test_SP_03_no_derate_in_the_safe_region(bms):
    """SP-03: a healthy pack derates nothing and flags nothing."""
    settle(bms, cell_mV=3600, temp_dC=T_ROOM)
    sop = bms.sop()

    assert sop.DischargeDerate == SOP_DERATE_NONE
    assert sop.ChargeDerate == SOP_DERATE_NONE
    assert sop.RegenDerate == SOP_DERATE_NONE
    assert not sop.DerateActiveVLow
    assert not sop.DerateActiveVHigh
    assert not sop.DerateActiveTHigh


def test_SP_03_low_cell_voltage_ramps_the_discharge_derate(bms):
    """SP-03: the V-low factor is linear between Start and End."""
    limits = bms.cell_limits()
    midpoint = (limits.DerateVLowStart_mV + limits.DerateVLowEnd_mV) // 2
    expected = (SOP_DERATE_NONE + limits.DerateFloor) // 2

    settle(bms, cell_mV=midpoint)
    sop = bms.sop()

    assert abs(sop.DischargeDerate - expected) <= 2
    assert sop.DerateActiveVLow


def test_SP_03_derate_reaches_the_floor_past_the_end_point(bms):
    """SP-03: below the End breakpoint the factor sits at DerateFloor."""
    limits = bms.cell_limits()

    settle(bms, cell_mV=limits.DerateVLowEnd_mV - 50)
    sop = bms.sop()

    assert sop.DischargeDerate == limits.DerateFloor
    assert sop.DerateActiveVLow


def test_SP_03_low_voltage_does_not_derate_the_charge_direction(bms):
    """SP-03: V-low governs discharge only — a low cell may still be charged."""
    limits = bms.cell_limits()

    settle(bms, cell_mV=limits.DerateVLowEnd_mV - 50)
    sop = bms.sop()

    assert sop.DischargeDerate == limits.DerateFloor
    assert sop.ChargeDerate == SOP_DERATE_NONE
    assert sop.RegenDerate == SOP_DERATE_NONE


def test_SP_03_high_cell_voltage_derates_charge_and_regen_only(bms):
    """SP-03: V-high governs the charge direction, not discharge."""
    limits = bms.cell_limits()

    settle(bms, cell_mV=limits.DerateVHighEnd_mV + 20)
    sop = bms.sop()

    assert sop.ChargeDerate == limits.DerateFloor
    assert sop.RegenDerate == limits.DerateFloor
    assert sop.DischargeDerate == SOP_DERATE_NONE
    assert sop.DerateActiveVHigh
    assert not sop.DerateActiveVLow


def test_SP_03_high_temperature_derates_every_direction(bms):
    """SP-03: T-high applies to all three limits."""
    limits = bms.cell_limits()

    settle(bms, cell_mV=3600, temp_dC=limits.DerateTHighEnd_dC + 50)
    sop = bms.sop()

    assert sop.DischargeDerate == limits.DerateFloor
    assert sop.ChargeDerate == limits.DerateFloor
    assert sop.RegenDerate == limits.DerateFloor
    assert sop.DerateActiveTHigh


def test_SP_03_cold_does_not_trigger_the_high_temperature_ramp(bms):
    """SP-03: a negative temperature is in the safe region of the T-high ramp.

    The ramp runs in unsigned arithmetic, so a signed temperature reaching it
    unguarded would wrap to a large positive value and derate to the floor.
    """
    settle(bms, cell_mV=3600, temp_dC=-150)
    sop = bms.sop()

    assert not sop.DerateActiveTHigh
    assert sop.DischargeDerate == SOP_DERATE_NONE


def test_SP_04_combined_derate_is_the_minimum_never_the_product(bms):
    """SP-04: two active factors do not multiply.

    Both factors are driven to the same midpoint value. The minimum of the two
    is that value; the product would be roughly a third of it.
    """
    limits = bms.cell_limits()
    cell_mid = (limits.DerateVLowStart_mV + limits.DerateVLowEnd_mV) // 2
    temp_mid = (limits.DerateTHighStart_dC + limits.DerateTHighEnd_dC) // 2
    expected = (SOP_DERATE_NONE + limits.DerateFloor) // 2

    settle(bms, cell_mV=cell_mid, temp_dC=temp_mid)
    sop = bms.sop()

    assert sop.DerateActiveVLow
    assert sop.DerateActiveTHigh
    assert abs(sop.DischargeDerate - expected) <= 2

    product = (expected * expected) // SOP_DERATE_NONE
    assert sop.DischargeDerate > product


def test_SP_04_the_tighter_of_two_factors_governs(bms):
    """SP-04: whichever constraint is tightest sets the factor."""
    limits = bms.cell_limits()
    cell_mid = (limits.DerateVLowStart_mV + limits.DerateVLowEnd_mV) // 2

    # V-low mid-ramp, temperature past its End: T-high is the tighter one.
    settle(bms, cell_mV=cell_mid, temp_dC=limits.DerateTHighEnd_dC + 50)
    sop = bms.sop()

    assert sop.DischargeDerate == limits.DerateFloor


def test_SP_04_final_is_the_table_value_scaled_by_the_factor(bms):
    """SP-04: the published value is table x factor / 1000."""
    limits = bms.cell_limits()

    settle(bms, cell_mV=(limits.DerateVLowStart_mV + limits.DerateVLowEnd_mV) // 2)
    sop = bms.sop()

    expected = (sop.DischargeTable_dA * sop.DischargeDerate) // 1000
    assert sop.DischargeFinal_dA == expected


# ---------------------------------------------------------------------------
# SP-05 — mode gating and the calibratable mode
# ---------------------------------------------------------------------------


def test_SP_05_discharge_mode_zeroes_the_charge_limit(bms):
    """SP-05: in Discharge mode only discharge and regen are published."""
    settle(bms)
    sop = bms.sop()

    assert sop.Mode == SOP_MODE_DISCHARGE
    assert sop.ChargeFinal_dA == 0
    assert sop.DischargeFinal_dA > 0
    assert sop.RegenFinal_dA > 0


def test_SP_05_charge_mode_zeroes_discharge_and_regen(bms):
    """SP-05: in Charge mode only the charge limit is published."""
    bms.set_sop_mode(SOP_MODE_CHARGE)
    settle(bms)
    sop = bms.sop()

    assert sop.Mode == SOP_MODE_CHARGE
    assert sop.DischargeFinal_dA == 0
    assert sop.RegenFinal_dA == 0
    assert sop.ChargeFinal_dA > 0


def test_SP_05_inactive_limit_keeps_its_calibration_fields(bms):
    """SP-05: only the published value is zeroed, not the working values.

    The raw table result and the derate factor stay live for the inactive
    direction, which is what makes the calibration readable over XCP.
    """
    settle(bms)
    sop = bms.sop()

    assert sop.ChargeFinal_dA == 0
    assert sop.ChargeTable_dA > 0
    assert sop.ChargeDerate == SOP_DERATE_NONE


def test_SP_05_mode_is_writable_at_runtime(bms):
    """SP-05: the mode can be overwritten live, as an XCP master would.

    Until a mode-provider component exists the mode is a calibratable
    variable, so a tool can flip it between cycles.
    """
    settle(bms)
    assert bms.sop().Mode == SOP_MODE_DISCHARGE

    bms.set_sop_mode(SOP_MODE_CHARGE)
    bms.run_normal(100, cell_mV=3600)
    assert bms.sop().Mode == SOP_MODE_CHARGE

    bms.set_sop_mode(SOP_MODE_DISCHARGE)
    bms.run_normal(100, cell_mV=3600)
    assert bms.sop().Mode == SOP_MODE_DISCHARGE


def test_SP_05_an_invalid_mode_reads_as_discharge(bms):
    """SP-05: a bad value falls to Discharge, which publishes no charge limit."""
    bms.set_sop_mode(200)
    settle(bms)
    sop = bms.sop()

    assert sop.Mode == SOP_MODE_DISCHARGE
    assert sop.ChargeFinal_dA == 0


def test_SP_05_limits_start_at_zero_before_the_first_update(bms):
    """SP-05: Init publishes zero rather than a plausible-looking table value."""
    bms.power_on()
    sop = bms.sop()

    assert sop.DischargeFinal_dA == 0
    assert sop.RegenFinal_dA == 0
    assert sop.ChargeFinal_dA == 0
    assert sop.Mode == SOP_MODE_DISCHARGE


# ---------------------------------------------------------------------------
# SP-12 — static configuration check
# ---------------------------------------------------------------------------


def test_SP_12_every_derate_end_sits_inside_its_fault_threshold(bms):
    """SP-12: derating must finish before protection starts (SOP-FR-06).

    A static check over the Bms_BattCfg constants, not a runtime test.
    """
    c = bms.cell_limits()

    assert c.DerateVHighEnd_mV < c.CellVoltageMax_mV
    assert c.DerateVLowEnd_mV > c.CellVoltageMin_mV
    assert c.DerateTHighEnd_dC < c.TemperatureMax_dC


def test_SP_12_derate_ramps_run_the_right_way(bms):
    """SP-12: each ramp starts on the safe side and ends nearer the trip."""
    c = bms.cell_limits()

    assert c.DerateVHighStart_mV < c.DerateVHighEnd_mV
    assert c.DerateVLowStart_mV > c.DerateVLowEnd_mV
    assert c.DerateTHighStart_dC < c.DerateTHighEnd_dC


def test_SP_12_derate_floor_is_a_valid_factor(bms):
    """SP-12: the floor is a factor, so it cannot exceed 'no derate'."""
    c = bms.cell_limits()

    assert 0 <= c.DerateFloor <= SOP_DERATE_NONE


@pytest.mark.parametrize(
    "clear,setpoint,name",
    [
        ("CellVoltageMaxClear_mV", "CellVoltageMax_mV", "cell over-voltage"),
        ("TemperatureMaxClear_dC", "TemperatureMax_dC", "over-temperature"),
        ("CellImbalanceMaxClear_mV", "CellImbalanceMax_mV", "cell imbalance"),
        ("TemperatureDeltaMaxClear_dC", "TemperatureDeltaMax_dC", "pack-to-pack delta"),
    ],
)
def test_SP_12_clear_sits_on_the_safe_side_of_set(bms, clear, setpoint, name):
    """SP-12: hysteresis only works if the clear value is the safer one.

    Guards the envelope move: these constants left Battery_Monitor.c and no
    runtime case exercises them.
    """
    c = bms.cell_limits()
    assert getattr(c, clear) < getattr(c, setpoint), name


def test_SP_12_under_voltage_and_under_temperature_clear_upward(bms):
    """SP-12: the two low-side thresholds clear in the opposite direction."""
    c = bms.cell_limits()

    assert c.CellVoltageMinClear_mV > c.CellVoltageMin_mV
    assert c.TemperatureMinClear_dC > c.TemperatureMin_dC
