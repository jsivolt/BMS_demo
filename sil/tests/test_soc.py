"""SOC — estimation, initialization and upstream signal chain.

All test cases whose subject is the SOC feature itself, covering
`src/battery/Bms_Soc.c` and the paths feeding it through `Battery_Monitor`.

Companion suites cover the two components SOC depends on but which stand on
their own:

  * `test_lib_interp.py`  — the generic table-lookup library
  * `test_persistence.py` — `Bms_Nvm` and the Data Flash record log

These are integration tests: stimulus is injected as real CAN frames through
the production vAFE / vPACK decoders and flows through `Battery_Monitor` into
`Bms_Soc`. A case named for a SOC behaviour will therefore also fail if an
upstream decoder regresses.

Case IDs match the validation matrix in SOC_DESIGN.md section 4.
Per-case results are in sil/reports/soc.md.
"""

from __future__ import annotations

import pytest

from bms_sil import (
    INITIAL_PCT_X10,
    INIT_SOURCE_DEFAULT,
    INIT_SOURCE_NVM,
    INIT_SOURCE_OCV,
    INIT_SOURCE_PENDING,
    MAX_PCT_X10,
    NOMINAL_CELL_MV,
    OCV_SLEEP_THRESHOLD_S,
    OCV_WAIT_TIMEOUT_MS,
    PACK_CAPACITY_MAH,
    charge_mAh,
    expected_blend,
)

# A cell voltage sitting exactly on an OCV table breakpoint, so the expected
# SOC after a tier 1 reset is unambiguous (3450 mV -> 50.0 %).
OCV_BREAKPOINT_MV = 3450

# Long enough that the cells count as relaxed.
RELAXED_SLEEP_S = OCV_SLEEP_THRESHOLD_S


# =========================================================================
# SOC estimation - Coulomb counting, clamping and the pack blend
# =========================================================================


def test_SA_03_blend_reference_degenerate(bms):
    """SA-03 (partial): the blend reduces to the common value when all three agree.

    In the current build Min == Max == Avg always (see SOC_DESIGN.md 5.1), so
    only the degenerate case is reachable end-to-end. The non-degenerate blend
    is covered by test_SA_blend_formula_reference below, against the same
    formula the C implements.
    """
    for target in (0, 123, 500, 999, MAX_PCT_X10):
        bms.set_soc(target)
        assert bms.soc_min == target
        assert bms.soc_max == target
        assert bms.soc_avg == target
        assert bms.soc == target


@pytest.mark.parametrize(
    "soc_min,soc_max,soc_avg,expected",
    [
        (300, 800, 0, 300),       # SA-01: Avg = 0   -> Min
        (300, 800, 1000, 800),    # SA-02: Avg = 100 % -> Max
        (300, 800, 550, 575),     # SA-03: worked example from SOC_DESIGN.md 3.5
        (400, 400, 550, 400),     # SA-04: Min == Max, no divide-by-zero
    ],
)
def test_SA_blend_formula_reference(soc_min, soc_max, soc_avg, expected):
    """SA-01/02/03/04: the documented blend formula, checked as arithmetic.

    NOTE: this exercises the Python mirror in bms_sil.expected_blend(), not the
    C. The C path cannot be driven to distinct Min/Max/Avg in this build; see
    test_blend_divergence_is_unreachable.
    """
    assert expected_blend(soc_min, soc_max, soc_avg) == expected


def test_blend_divergence_is_unreachable(bms):
    """Documents SOC_DESIGN.md 5.1 empirically rather than by assertion of prose.

    Nothing in the reachable API can make the three estimators differ: they
    integrate the same current against the same capacity, and the only seeding
    path that could separate them (the OCV reset) is unreachable. If this test
    ever fails, divergence has become possible and the blend needs real
    end-to-end coverage.
    """
    bms.set_soc(500)
    bms.run_normal(5000, current_mA=-40_000, cell_mV=NOMINAL_CELL_MV)
    assert bms.soc_min == bms.soc_max == bms.soc_avg
    bms.run_normal(5000, current_mA=+40_000, cell_mV=NOMINAL_CELL_MV)
    assert bms.soc_min == bms.soc_max == bms.soc_avg


def test_SA_05_clamps_at_empty(bms):
    """SA-05: sustained discharge saturates at 0 % and does not wrap."""
    bms.set_soc(20)  # 2.0 % = 2000 mAh; at 100 A that is 72 s of discharge
    bms.run_normal(120_000, current_mA=-100_000, cell_mV=3000)
    assert bms.soc == 0
    # Keep discharging: must stay at 0, not wrap to a large value.
    bms.run_normal(60_000, current_mA=-100_000, cell_mV=3000)
    assert bms.soc == 0
    assert bms.capacity_avg_mAh == pytest.approx(0.0, abs=1e-3)


def test_SA_06_clamps_at_full(bms):
    """SA-06: sustained charge saturates at 100 %."""
    bms.set_soc(980)
    bms.run_normal(120_000, current_mA=+100_000, cell_mV=3900)
    assert bms.soc == MAX_PCT_X10
    bms.run_normal(60_000, current_mA=+100_000, cell_mV=3900)
    assert bms.soc == MAX_PCT_X10
    assert bms.capacity_avg_mAh == pytest.approx(float(PACK_CAPACITY_MAH), rel=1e-6)


def test_SA_07_set_soc_clamps_above_full(bms):
    """SA-07: Bms_Soc_SetSoc_pct_x10 clamps an out-of-range request to 100 %."""
    bms.set_soc(1500)
    assert bms.soc == MAX_PCT_X10
    assert bms.soc_avg == MAX_PCT_X10


def test_SA_08_sign_convention(bms):
    """SA-08: positive current charges, negative discharges.

    This is the convention the over-current thresholds in Battery_Monitor.c
    imply (CHARGE_OC = +80000, DISCHARGE_OC = -100000).
    """
    bms.set_soc(500)
    bms.run_normal(600_000, current_mA=+50_000, cell_mV=NOMINAL_CELL_MV)
    charged = bms.soc
    assert charged > 500, "positive current must raise SOC"

    bms.set_soc(500)
    bms.run_normal(600_000, current_mA=-50_000, cell_mV=NOMINAL_CELL_MV)
    assert bms.soc < 500, "negative current must lower SOC"


def test_SA_09_one_c_discharge_scale(bms):
    """SA-09: a 1 C discharge for one hour takes a full pack from 100 % to ~0 %.

    Validates the dt_h scale factor end-to-end, not just its sign.
    """
    bms.set_soc(MAX_PCT_X10)
    # 1 C for this pack is 100 A. Run one hour of virtual time.
    bms.run_normal(3_600_000, current_mA=-PACK_CAPACITY_MAH, cell_mV=NOMINAL_CELL_MV)
    assert bms.soc == pytest.approx(0, abs=5)   # within 0.5 %


def test_coulomb_integration_matches_hand_calculation(bms):
    """The integrated charge matches mAh computed independently in Python."""
    bms.set_soc(500)
    start = bms.capacity_avg_mAh

    current_mA = -30_000
    duration_ms = 600_000          # 10 minutes
    bms.run_normal(duration_ms, current_mA=current_mA, cell_mV=NOMINAL_CELL_MV)

    expected = start + charge_mAh(current_mA, duration_ms)
    assert bms.capacity_avg_mAh == pytest.approx(expected, rel=1e-3)


# =========================================================================
# SOC initialization - the three-tier startup chain
# =========================================================================


def test_IT_02_erased_flash_gives_default_and_invalid(bms):
    """IT-02: no valid record -> tier 3, default SOC, estimates flagged invalid."""
    assert bms.soc == INITIAL_PCT_X10
    assert bms.soc_min == INITIAL_PCT_X10
    assert bms.soc_max == INITIAL_PCT_X10
    assert bms.soc_avg == INITIAL_PCT_X10
    assert bms.soc_valid is False
    assert bms.soc_legacy_valid is False
    assert bms.init_source == INIT_SOURCE_DEFAULT


def test_IT_01_restores_from_nvm(bms):
    """IT-01: a persisted record is restored into all three estimators, valid."""
    bms.set_soc(732)
    bms.run_normal(61_000, current_mA=0, cell_mV=NOMINAL_CELL_MV)
    assert bms.nvm_records >= 1, "expected a persisted record before power cycle"

    saved = bms.nvm_load()
    assert saved is not None

    bms.power_cycle()

    assert bms.soc_min == saved[0]
    assert bms.soc_max == saved[1]
    assert bms.soc_avg == saved[2]
    assert bms.soc_valid is True, "a real NVM reference must come up valid"
    assert bms.init_source == INIT_SOURCE_NVM


def test_IT_03_stale_soc1_records_fall_back_to_default(bms_dirty):
    """IT-03: old 16-byte "SOC1" records are rejected -> one boot at tier 3."""
    bms = bms_dirty

    # Lay down plausible-looking legacy records: magic "SOC1", 16-byte stride.
    legacy_magic = (0x31, 0x43, 0x4F, 0x53)     # 0x534F4331, little endian
    for slot in range(4):
        base = slot * 16
        for i, byte in enumerate(legacy_magic):
            bms.flash_write_byte(base + i, byte)
        for i in range(4, 16):
            bms.flash_write_byte(base + i, 0x00)

    bms.power_on()

    assert bms.soc == INITIAL_PCT_X10
    assert bms.soc_valid is False, "stale-format records must not be trusted"
    assert bms.init_source == INIT_SOURCE_DEFAULT


def test_IT_05_invalid_start_becomes_valid_once_integrating(bms):
    """IT-05: a tier-3 blind start flips to valid on the first good current tick.

    Documented behaviour, not an accident - see SOC_DESIGN.md 5.7.
    """
    assert bms.soc_valid is False
    bms.run_normal(1000, current_mA=-10_000, cell_mV=NOMINAL_CELL_MV)
    assert bms.soc_valid is True


def test_IT_06_invalid_start_is_never_persisted(bms):
    """IT-06: while the estimate is a blind guess, nothing is written to flash."""
    assert bms.soc_valid is False
    # Run well past the save period, but never supply a valid current.
    bms.run_ms(120_000)
    assert bms.flash_writes == 0
    assert bms.nvm_records == 0


def test_IT_04_ocv_reset_tier(bms_dirty):
    """IT-04: tier 1 seeds all three estimators from the OCV table.

    Reachable in SIL because the Bms_SleepTime double reports a relaxed pack.
    On target the provider is still hardcoded to 0 s (SOC_DESIGN.md 5.2), so
    this exercises the path the production stub currently disables - not
    something the target does today.
    """
    bms = bms_dirty
    bms.power_on(sleep_s=RELAXED_SLEEP_S, sleep_ready=True)

    # Cell voltages cannot exist yet, so init must be deferred rather than
    # silently falling through to a lower tier.
    assert bms.init_source == INIT_SOURCE_PENDING

    bms.run_ms(100, cells_mV=OCV_BREAKPOINT_MV)

    assert bms.init_source == INIT_SOURCE_OCV
    assert bms.soc == bms.ocv_to_soc(OCV_BREAKPOINT_MV)
    assert bms.soc_min == bms.soc_max == bms.soc_avg == bms.soc


def test_IT_09_deferred_init_holds_estimates_invalid(bms_dirty):
    """IT-09: while the OCV wait is pending, SOC is reported as unusable.

    A consumer must be able to tell "not seeded yet" from a real anchor, so the
    provenance reads PENDING and the estimates stay invalid rather than showing
    a plausible number that is about to be replaced.
    """
    bms = bms_dirty
    bms.power_on(sleep_s=RELAXED_SLEEP_S, sleep_ready=True)

    assert bms.init_source == INIT_SOURCE_PENDING
    assert bms.soc_valid is False

    # Starve the wait: no vAFE frames, so CellVoltageValid never goes TRUE.
    bms.run_ms(OCV_WAIT_TIMEOUT_MS - 100)

    assert bms.init_source == INIT_SOURCE_PENDING, "wait must not resolve early"


def test_IT_10_stage2_timeout_falls_back_to_nvm(bms):
    """IT-10: cell voltages never arriving times out into tier 2, not tier 1.

    The OCV branch must fail closed: without a cell-voltage reference it has
    nothing to seed from, so the persisted SOC has to win.
    """
    bms.set_soc(700)
    bms.run_normal(61_000, current_mA=0, cell_mV=NOMINAL_CELL_MV)

    # Reboot as a relaxed pack, but never deliver a vAFE cycle.
    bms.power_cycle(sleep_s=RELAXED_SLEEP_S, sleep_ready=True)
    assert bms.init_source == INIT_SOURCE_PENDING

    bms.run_ms(OCV_WAIT_TIMEOUT_MS)

    assert bms.init_source == INIT_SOURCE_NVM
    assert bms.soc == 700


def test_IT_11_stage1_timeout_when_sleep_time_never_ready(bms_dirty):
    """IT-11: an unready sleep-time source gates the wait even if cells are fine.

    Stage 1 is the gate: with no readable sleep time there is no way to know the
    cells are relaxed, so a cell-voltage set alone must not trigger an OCV reset.
    """
    bms = bms_dirty
    bms.power_on(sleep_s=RELAXED_SLEEP_S, sleep_ready=False)

    # Cells are healthy the whole time - only readiness is missing.
    bms.run_ms(OCV_WAIT_TIMEOUT_MS - 100, cells_mV=OCV_BREAKPOINT_MV)
    assert bms.init_source == INIT_SOURCE_PENDING

    bms.run_ms(100, cells_mV=OCV_BREAKPOINT_MV)

    assert bms.init_source != INIT_SOURCE_OCV, "unreadable sleep time must not seed OCV"
    assert bms.init_source == INIT_SOURCE_DEFAULT      # virgin flash, so tier 3
    assert bms.soc == INITIAL_PCT_X10


def test_IT_12_late_sleep_time_within_budget_still_reaches_ocv(bms_dirty):
    """IT-12: a timekeeping source that arrives late, but in time, still wins tier 1.

    This is the case the readiness flag exists for - the wait must survive
    stage 1 taking a few cycles rather than giving up on the first look.
    """
    bms = bms_dirty
    bms.power_on(sleep_s=RELAXED_SLEEP_S, sleep_ready=False)

    bms.run_ms(200, cells_mV=OCV_BREAKPOINT_MV)
    assert bms.init_source == INIT_SOURCE_PENDING

    # RTC finishes acquiring, still inside the 500 ms budget.
    bms.set_sleep_time(RELAXED_SLEEP_S, ready=True)
    bms.run_ms(100, cells_mV=OCV_BREAKPOINT_MV)

    assert bms.init_source == INIT_SOURCE_OCV
    assert bms.soc == bms.ocv_to_soc(OCV_BREAKPOINT_MV)


def test_IT_13_short_sleep_resolves_without_waiting_for_cells(bms_dirty):
    """IT-13: once the sleep time reads short, the wait ends immediately.

    A pack that was only briefly off can never OCV-reset, so there is nothing to
    wait for - blocking on cell voltages would delay initialization for no gain.
    """
    bms = bms_dirty
    bms.power_on(sleep_s=0, sleep_ready=False)
    assert bms.init_source == INIT_SOURCE_PENDING

    bms.set_sleep_time(0, ready=True)

    # One tick, no vAFE frames at all.
    bms.run_ms(100)

    assert bms.init_source == INIT_SOURCE_DEFAULT
    assert bms.elapsed_ms < OCV_WAIT_TIMEOUT_MS, "must not burn the full budget"


def test_IT_14_no_integration_while_init_is_pending(bms_dirty):
    """IT-14: current is not integrated onto an unseeded anchor.

    The estimators hold no reference during the wait, so accumulating charge
    into them would produce a SOC derived from an arbitrary zero.
    """
    bms = bms_dirty
    bms.power_on(sleep_s=RELAXED_SLEEP_S, sleep_ready=False)

    # A fully healthy chain apart from readiness, so the only reason not to
    # integrate is the pending init. Charge current specifically: the unseeded
    # anchor sits at 0 mAh and a discharge would clamp there, hiding the bug.
    stimulus = dict(
        current_mA=+50_000,
        pack_mV=NOMINAL_CELL_MV * 16,
        bus_mV=NOMINAL_CELL_MV * 16,
        cells_mV=NOMINAL_CELL_MV,
    )
    bms.run_ms(OCV_WAIT_TIMEOUT_MS - 100, **stimulus)

    assert bms.init_source == INIT_SOURCE_PENDING
    assert bms.pack_current_valid is True, "precondition: current must be flowing"
    assert bms.capacity_avg_mAh == 0.0, "no charge may accumulate before seeding"
    assert bms.soc_valid is False, "an unseeded estimate must never report valid"

    # On timeout the anchor is established from tier 3, unpolluted by the above.
    bms.run_ms(100, **stimulus)

    assert bms.init_source == INIT_SOURCE_DEFAULT
    assert bms.soc == INITIAL_PCT_X10


def test_IT_07_init_source_is_latched_not_recomputed(bms):
    """IT-07: the init source reports how SOC was *seeded*, not its current state.

    It is latched by Bms_Soc_InitPack() and must not be rewritten by runtime
    integration - a consumer reading CAN 0x308 Pack1SOCInitSource needs the
    provenance of the anchor, which does not change as the counter moves.
    """
    bms.set_soc(700)
    bms.run_normal(61_000, current_mA=0, cell_mV=NOMINAL_CELL_MV)
    bms.power_cycle()

    assert bms.init_source == INIT_SOURCE_NVM
    source_at_boot = bms.init_source

    # Integrate for a while; the SOC moves, the provenance must not.
    bms.run_normal(600_000, current_mA=-30_000, cell_mV=NOMINAL_CELL_MV)

    assert bms.soc != 700, "precondition: SOC should have moved"
    assert bms.init_source == source_at_boot


def test_IT_08_init_source_fits_the_can_signal_width(bms):
    """IT-08: every init-source value fits the 3-bit CAN field on 0x308.

    Pack1SOCInitSource is 17|3@1+, so any value above 7 would be silently
    truncated by Bms_Can_SendSocStatus().
    """
    assert 0 <= bms.init_source <= 7


# =========================================================================
# Signal chain - upstream coupling into SOC
# =========================================================================


def test_CH_01_current_dropout_invalidates_but_holds_soc(bms):
    """CH-01: losing pack current invalidates the estimate but holds the value.

    Exercises the vPACK timeout upstream of SOC: frames simply stop arriving.
    """
    bms.set_soc(640)
    bms.run_normal(2000, current_mA=-10_000, cell_mV=NOMINAL_CELL_MV)
    assert bms.soc_valid is True
    held = bms.soc

    # Stop injecting current frames; vPACK times out after 10 x 100 ms.
    bms.run_ms(2000)

    assert bms.pack_current_valid is False
    assert bms.soc_valid is False
    assert bms.soc == held, "SOC value must be held, not zeroed"


def test_CH_03_vafe_average_matches_mean_of_cells(bms):
    """CH-03: the vAFE average equals the mean of the 16 injected cells.

    Verifies the statistic added to Bms_Vafe_UpdateStatistics, propagated
    through Battery_Monitor's unit conversion.
    """
    cells = [3500 + (i * 10) for i in range(16)]     # 3500..3650 mV
    bms.run_ms(100, cells_mV=cells, current_mA=0, pack_mV=57_600)

    assert bms.cell_valid is True
    assert bms.cell_min_V == pytest.approx(min(cells) / 1000.0, abs=1e-6)
    assert bms.cell_max_V == pytest.approx(max(cells) / 1000.0, abs=1e-6)

    expected_avg_mV = sum(cells) // 16      # integer mean, as the C computes it
    assert bms.cell_avg_V == pytest.approx(expected_avg_mV / 1000.0, abs=1e-6)


def test_CH_single_alive_glitch_recovers(bms):
    """A one-off alive-counter glitch costs one cycle of data and then resyncs.

    Bms_Vpack re-bases the expected counter from whatever it last received, so
    an isolated jump invalidates exactly one cycle. That is intended behaviour.
    """
    bms.run_normal(2000, current_mA=-5_000, cell_mV=NOMINAL_CELL_MV)
    assert bms.soc_valid is True

    bms.skip_alive_once()
    bms.run_normal(500, current_mA=-5_000, cell_mV=NOMINAL_CELL_MV)

    assert bms.pack_current_valid is True, "chain should resync after one glitch"
    assert bms.soc_valid is True


@pytest.mark.xfail(
    strict=True,
    reason="DEFECT: Battery_Monitor.c:286 sets PackCurrentValid[0] from "
           "g_BmsVpackData.CurrentValid (frame arrival only), while the data copy "
           "is gated on g_BmsVpackData.Valid (which includes AliveValid). A stuck "
           "alive counter therefore freezes PackCurrent_mA at its last value while "
           "still reporting it valid, and SOC integrates the stale current forever.",
)
def test_CH_stuck_alive_counter_must_invalidate_current(bms):
    """A *sustained* alive-counter failure must invalidate pack current.

    Scenario: the pack is discharging at 100 A when the vPACK transmitter
    sticks. Frames keep arriving and now report 0 A, but the alive counter
    never advances. The stale -100 A must not keep driving the integrator, and
    the SOC must not be advertised as valid.
    """
    bms.run_normal(2000, current_mA=-100_000, cell_mV=NOMINAL_CELL_MV)
    assert bms.soc_valid is True
    soc_before = bms.soc

    bms.freeze_alive(True)
    bms.run_normal(600_000, current_mA=0, cell_mV=NOMINAL_CELL_MV)   # 10 min at rest

    assert bms.pack_current_valid is False, (
        f"stuck alive counter must invalidate pack current, but current reads "
        f"{bms.pack_current_mA} mA and is still flagged valid"
    )
    assert bms.soc_valid is False
    assert bms.soc == soc_before, "SOC must not drift on a frozen current value"
