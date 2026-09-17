"""REG-MEAS-001/002, REG-CAN-001/002 and REG-SCHED-001.

The published measurement surface: what the vAFE and vPACK decoders make of the bench
stimulus, what the firmware then puts on CAN0, and how regularly it does so.
"""

from __future__ import annotations

import time
from typing import Any

import regression_common as rc
from regression_result import Evidence, Observation, RegressionResult, Status
from regression_target import Session
from regression_can import FrameListener

# --------------------------------------------------------------------------------------------------
# REG-MEAS-001
# --------------------------------------------------------------------------------------------------

MEAS1_ID = "REG-MEAS-001"
MEAS1_NAME = "Nominal vAFE cell voltages"
MEAS1_AREA = "Measurement"
MEAS1_CAPS = ("JLINK", "CAN1")


def measurement_vafe_regression(session: Session) -> RegressionResult:
    started = time.monotonic()
    nominal = session.config.cell_voltage_mv
    expected = (
        f"CellVoltageValid == TRUE and MinCellVoltage == MaxCellVoltage == {nominal} mV, "
        f"Delta == 0"
    )
    blocked = session.blocked(
        MEAS1_ID,
        MEAS1_NAME,
        MEAS1_CAPS,
        expected,
        MEAS1_AREA,
        context="the vAFE stimulus is required",
    )
    if blocked:
        return blocked

    target = session.target
    assert target is not None
    timeout = session.config.timeout("measurement_s")

    for key in ("vafe_valid", "vafe_min", "vafe_max", "vafe_delta"):
        if not target.has(key):
            return RegressionResult.skip(
                MEAS1_ID,
                MEAS1_NAME,
                f"symbol {key} did not resolve against the ELF: "
                f"{target.unavailable.get(key, 'not declared')}",
                area=MEAS1_AREA,
                caps=MEAS1_CAPS,
                expected=expected,
            )

    try:
        values, elapsed, _ = target.read_until(
            lambda reading: (
                int(reading.get("vafe_valid", 0)) == 1
                and int(reading.get("vafe_min", -1)) == nominal
                and int(reading.get("vafe_max", -1)) == nominal
            ),
            timeout,
            0.02,
        )
    except Exception as exc:  # noqa: BLE001
        return RegressionResult.infra(
            MEAS1_ID,
            MEAS1_NAME,
            f"reading the vAFE decoder state failed: {exc}",
            area=MEAS1_AREA,
            caps=MEAS1_CAPS,
            expected=expected,
        )

    valid = int(values.get("vafe_valid", 0)) == 1
    minimum = int(values.get("vafe_min", -1))
    maximum = int(values.get("vafe_max", -1))
    delta = int(values.get("vafe_delta", -1))
    monitor_valid = values.get("bm_cell_valid")
    cell_vector = []
    for index in range(rc.CELL_COUNT):
        key = f"vafe_cell{index}"
        if target.has(key):
            cell_vector.append(int(values.get(key, -1)))

    diagnostics: dict[str, Any] = {
        "elapsed_s": round(elapsed, 3),
        "timeout_s": timeout,
        "vafe_data_valid": valid,
        "min_cell_mv": minimum,
        "max_cell_mv": maximum,
        "delta_cell_mv": delta,
        "measurement_counter": values.get("vafe_counter"),
        "header_valid": values.get("vafe_hdr_valid"),
        "expected_min_mv": nominal,
        "expected_max_mv": nominal,
    }
    if monitor_valid is not None:
        diagnostics["battery_monitor_cell_voltage_valid"] = int(monitor_valid) == 1

    problems = []
    if not valid:
        problems.append(f"vAFE DataValid stayed FALSE after {elapsed:.2f} s")
    if minimum != nominal:
        problems.append(f"MinCellVoltage is {minimum} mV, expected {nominal}")
    if maximum != nominal:
        problems.append(f"MaxCellVoltage is {maximum} mV, expected {nominal}")
    if delta != 0:
        problems.append(f"DeltaCellVoltage is {delta} mV, expected 0")

    can_note, can_diag = _cross_check_can0_cells(session, nominal)
    diagnostics.update(can_diag)

    observations = [Observation.DEBUGGER_STATIC]
    if can_diag:
        observations.append(Observation.CAN)

    return RegressionResult(
        test_id=MEAS1_ID,
        name=MEAS1_NAME,
        status=Status.FAIL if problems else Status.PASS,
        duration_s=time.monotonic() - started,
        expected=expected,
        observed=(
            f"DataValid={int(valid)} min={minimum} max={maximum} delta={delta} mV "
            f"after {elapsed:.2f} s"
        ),
        details="\n".join(problems + [can_note]),
        area=MEAS1_AREA,
        caps=MEAS1_CAPS,
        observations=tuple(observations),
        evidence=Evidence.HARDWARE,
        diagnostics=diagnostics,
    )


def _cross_check_can0_cells(session: Session, nominal: int) -> tuple[str, dict[str, Any]]:
    """Confirms the decoder result is also what the firmware published on CAN0."""
    if not session.bus_available("CAN0"):
        return "CAN0 cross-check not performed (CAN0 not selected for this run)", {}

    payloads = _grab_can0(session, (0x305, 0x310, 0x311, 0x312, 0x313), 1.5)
    if not payloads:
        return "CAN0 cross-check not performed (no 0x305/0x310-0x313 captured)", {}

    diagnostics: dict[str, Any] = {}
    notes = []
    if 0x305 in payloads:
        summary = rc.decode_0x305(payloads[0x305])
        diagnostics["can0_0x305"] = summary
        notes.append(
            f"CAN0 0x305 valid={int(summary['cell_voltage_valid'])} "
            f"min={summary['min_cell_mv']} max={summary['max_cell_mv']} "
            f"delta={summary['delta_cell_mv']}"
        )
    if all(can_id in payloads for can_id in (0x310, 0x311, 0x312, 0x313)):
        cells = rc.decode_cells_from_frames(payloads)
        diagnostics["can0_cells_mv"] = cells
        notes.append(f"CAN0 0x310-0x313 cells {cells[0]}..{cells[-1]} mV")
        if any(value != nominal for value in cells):
            notes.append("CAN0 cell payloads differ from the decoder result")
    return "; ".join(notes), diagnostics


# --------------------------------------------------------------------------------------------------
# REG-MEAS-002
# --------------------------------------------------------------------------------------------------

MEAS2_ID = "REG-MEAS-002"
MEAS2_NAME = "Nominal vPACK current and voltage"
MEAS2_AREA = "Measurement"
MEAS2_CAPS = ("JLINK", "CAN2")


def measurement_vpack_regression(session: Session) -> RegressionResult:
    started = time.monotonic()
    config = session.config
    expected = (
        "PackCurrentValid[0] == TRUE and the Pack 1 voltage valid flag == TRUE, with the "
        f"decoded values equal to the configured nominal stimulus "
        f"({config.pack_current_ma} mA, {config.pack_voltage_mv} mV pack, "
        f"{config.bus_voltage_mv} mV bus, status 0)"
    )
    blocked = session.blocked(
        MEAS2_ID,
        MEAS2_NAME,
        MEAS2_CAPS,
        expected,
        MEAS2_AREA,
        context="the vPACK stimulus is required",
    )
    if blocked:
        return blocked

    target = session.target
    assert target is not None
    timeout = session.config.timeout("measurement_s")

    for key in ("vp_cur_valid", "vp_volt_valid"):
        if not target.has(key):
            return RegressionResult.skip(
                MEAS2_ID,
                MEAS2_NAME,
                f"symbol {key} did not resolve against the ELF",
                area=MEAS2_AREA,
                caps=MEAS2_CAPS,
                expected=expected,
            )

    try:
        values, elapsed, _ = target.read_until(
            lambda reading: (
                int(reading.get("vp_cur_valid", 0)) == 1
                and int(reading.get("vp_volt_valid", 0)) == 1
            ),
            timeout,
            0.02,
        )
    except Exception as exc:  # noqa: BLE001
        return RegressionResult.infra(
            MEAS2_ID,
            MEAS2_NAME,
            f"reading the vPACK decoder state failed: {exc}",
            area=MEAS2_AREA,
            caps=MEAS2_CAPS,
            expected=expected,
        )

    cur_valid = int(values.get("vp_cur_valid", 0)) == 1
    volt_valid = int(values.get("vp_volt_valid", 0)) == 1
    alive_valid = int(values.get("vp_alive_valid", 0)) == 1
    overall = int(values.get("vp_valid", 0)) == 1
    current = int(values.get("vp_current", 0))
    pack_mv = int(values.get("vp_pack_mv", 0))
    bus_mv = int(values.get("vp_bus_mv", 0))
    status = int(values.get("vp_status", 0))

    diagnostics: dict[str, Any] = {
        "elapsed_s": round(elapsed, 3),
        "timeout_s": timeout,
        "vpack_current_valid": cur_valid,
        "vpack_voltage_valid": volt_valid,
        "vpack_alive_valid": alive_valid,
        "vpack_valid": overall,
        "pack_current_ma": current,
        "pack_voltage_mv": pack_mv,
        "bus_voltage_mv": bus_mv,
        "status_byte": status,
        "alive_counter": values.get("vp_alive"),
        "expected_pack_current_ma": config.pack_current_ma,
        "expected_pack_voltage_mv": config.pack_voltage_mv,
        "expected_bus_voltage_mv": config.bus_voltage_mv,
    }
    for key, label in (
        ("bm_cur_valid0", "battery_monitor_pack_current_valid[0]"),
        ("bm_vp_volt_valid", "battery_monitor_vpack_voltage_valid"),
        ("bm_valid", "battery_monitor_valid"),
        ("bm_packv1", "battery_monitor_pack1_v"),
    ):
        if target.has(key):
            diagnostics[label] = values.get(key)

    problems = []
    if not cur_valid:
        problems.append(f"PackCurrentValid[0] stayed FALSE after {elapsed:.2f} s")
    if not volt_valid:
        problems.append(f"vPACK VoltageValid stayed FALSE after {elapsed:.2f} s")
    if current != config.pack_current_ma:
        problems.append(f"decoded pack current is {current} mA, expected {config.pack_current_ma}")
    if pack_mv != config.pack_voltage_mv:
        problems.append(f"decoded pack voltage is {pack_mv} mV, expected {config.pack_voltage_mv}")
    if bus_mv != config.bus_voltage_mv:
        problems.append(f"decoded bus voltage is {bus_mv} mV, expected {config.bus_voltage_mv}")
    if status != 0:
        problems.append(
            f"device status byte is {status}; Battery_Monitor raises "
            "FAULT_VPACK_DEVICE_FAULT on any non-zero value"
        )

    can_note, can_diag = _cross_check_can0_vpack(session, config)
    diagnostics.update(can_diag)

    observations = [Observation.DEBUGGER_STATIC]
    if can_diag:
        observations.append(Observation.CAN)

    return RegressionResult(
        test_id=MEAS2_ID,
        name=MEAS2_NAME,
        status=Status.FAIL if problems else Status.PASS,
        duration_s=time.monotonic() - started,
        expected=expected,
        observed=(
            f"current_valid={int(cur_valid)} voltage_valid={int(volt_valid)} "
            f"current={current} mA pack={pack_mv} mV bus={bus_mv} mV status={status} "
            f"after {elapsed:.2f} s"
        ),
        details="\n".join(problems + [can_note]),
        area=MEAS2_AREA,
        caps=MEAS2_CAPS,
        observations=tuple(observations),
        evidence=Evidence.HARDWARE,
        diagnostics=diagnostics,
    )


def _cross_check_can0_vpack(session: Session, config: rc.NominalConfig) -> tuple[str, dict[str, Any]]:
    if not session.bus_available("CAN0"):
        return "CAN0 cross-check not performed (CAN0 not selected for this run)", {}
    payloads = _grab_can0(session, (0x301, 0x306), 1.2)
    if not payloads:
        return "CAN0 cross-check not performed (no 0x301/0x306 captured)", {}

    diagnostics: dict[str, Any] = {}
    notes = []
    if 0x306 in payloads:
        current = rc.decode_0x306(payloads[0x306])
        diagnostics["can0_0x306"] = current
        notes.append(
            f"CAN0 0x306 pack1 current valid={int(current['pack_current_valid'][0])} "
            f"value={current['pack_current_da'][0]} dA"
        )
    if 0x301 in payloads:
        pack = rc.decode_0x301(payloads[0x301])
        diagnostics["can0_0x301"] = pack
        notes.append(
            f"CAN0 0x301 pack1={pack['pack_voltage_dv'][0]} dV "
            f"valid={int(pack['pack_voltage_valid'])}"
        )
    return "; ".join(notes), diagnostics


# --------------------------------------------------------------------------------------------------
# REG-CAN-001 / REG-CAN-002
# --------------------------------------------------------------------------------------------------

CAN1_ID = "REG-CAN-001"
CAN1_NAME = "CAN0 periodic frame set"
CAN1_AREA = "CAN"
CAN1_CAPS = ("CAN0",)

CAN2_ID = "REG-CAN-002"
CAN2_NAME = "CAN0 periodic timing"
CAN2_AREA = "CAN"


def _grab_can0(session: Session, wanted: set[int] | tuple[int, ...], timeout_s: float) -> dict[int, bytes]:
    """Last payload per wanted CAN0 id, within the timeout."""
    transport = session.transport
    if transport is None or not transport.is_open("CAN0"):
        return {}
    wanted_set = set(wanted)
    payloads: dict[int, bytes] = {}
    deadline = time.monotonic() + timeout_s
    while set(payloads) != wanted_set and time.monotonic() < deadline:
        frame = transport.recv("CAN0", 0.05)
        if frame is None:
            continue
        session.can0_frames.append(frame)
        if frame.can_id in wanted_set:
            payloads[frame.can_id] = frame.data
    return payloads


def can_output_regression(session: Session) -> list[RegressionResult]:
    """Collects CAN0 once and produces both the frame-set and the timing verdict."""
    started = time.monotonic()
    expected_set = "all 17 periodic CAN0 ids present"
    expected_timing = "every periodic CAN0 id arrives at 100 ms within tolerance"
    blocked = session.blocked(
        CAN1_ID, CAN1_NAME, CAN1_CAPS, expected_set, CAN1_AREA,
        context="CAN0 observation is required",
    )
    if blocked:
        return [
            blocked,
            RegressionResult.skip(
                CAN2_ID,
                CAN2_NAME,
                blocked.skip_reason or blocked.infra_error,
                area=CAN2_AREA,
                caps=CAN1_CAPS,
                expected=expected_timing,
            ),
        ]

    duration = session.config.can_collect_s
    transport = session.transport
    assert transport is not None
    listener = FrameListener(transport, "CAN0", name="can0-periodic").start()
    deadline = time.monotonic() + duration
    while time.monotonic() < deadline:
        time.sleep(0.05)
    listener.stop()

    frames = listener.frames()
    session.can0_frames.extend(frames)

    if not frames:
        # A completely silent bus is a bench prerequisite, not a DUT verdict: the board
        # may be unpowered, or the adapter may be plugged into a different bus.  Reporting
        # 17 "missing id" failures here would be a fabricated result.
        channel = "?"
        state = transport.states().get("CAN0")
        if state is not None:
            channel = state.spec.channel
        reason = (
            f"CAN0 ({channel}) carried no frames at all during {duration:.1f} s. Check that "
            "the board is powered, running the firmware, and that CAN0 is wired to this "
            "channel before treating any periodic-traffic result as a DUT verdict"
        )
        return [
            RegressionResult.skip(
                CAN1_ID, CAN1_NAME, reason, area=CAN1_AREA, caps=CAN1_CAPS, expected=expected_set
            ),
            RegressionResult.skip(
                CAN2_ID, CAN2_NAME, reason, area=CAN2_AREA, caps=CAN1_CAPS, expected=expected_timing
            ),
        ]

    by_id: dict[int, list[float]] = {}
    for frame in frames:
        by_id.setdefault(frame.can_id, []).append(frame.t)

    present = sorted(can_id for can_id in rc.CAN0_TX_IDS if can_id in by_id)
    absent = [can_id for can_id in rc.CAN0_TX_IDS if can_id not in by_id]
    unexpected = sorted(can_id for can_id in by_id if can_id not in rc.CAN0_TX_IDS)

    observed = (
        f"{len(present)}/{len(rc.CAN0_TX_IDS)} expected ids seen in {duration:.1f} s, "
        f"{len(frames)} frames total"
    )
    if absent:
        observed += "; missing " + ", ".join(f"0x{can_id:03X}" for can_id in absent)

    frame_set = RegressionResult(
        test_id=CAN1_ID,
        name=CAN1_NAME,
        status=Status.FAIL if absent else Status.PASS,
        duration_s=time.monotonic() - started,
        expected=(
            "every CAN0 id the firmware sends every 100 ms is observed: "
            + ", ".join(f"0x{can_id:03X}" for can_id in rc.CAN0_TX_IDS)
        ),
        observed=observed,
        details=(
            ("missing: " + ", ".join(f"0x{c:03X}" for c in absent) + "\n") if absent else ""
        )
        + (
            "unexpected ids on CAN0 (0x400 belongs to CAN1, not CAN0): "
            + ", ".join(f"0x{can_id:03X}" for can_id in unexpected)
            if unexpected
            else ""
        ),
        area=CAN1_AREA,
        caps=CAN1_CAPS,
        observations=(Observation.CAN,),
        evidence=Evidence.HARDWARE,
        diagnostics={
            "collect_s": duration,
            "frames_total": len(frames),
            "ids_present": [f"0x{can_id:03X}" for can_id in present],
            "ids_missing": [f"0x{can_id:03X}" for can_id in absent],
            "ids_unexpected": [f"0x{can_id:03X}" for can_id in unexpected],
            "frames_per_id": {f"0x{can_id:03X}": len(stamps) for can_id, stamps in sorted(by_id.items())},
        },
    )

    expected_period = session.config.can0_expected_period_ms
    tolerance = session.config.can0_period_tolerance_ms
    burst_threshold = session.config.can0_burst_threshold_ms
    coverage_min = session.config.can0_min_coverage_ratio

    per_id: dict[str, Any] = {}
    problems: list[str] = []
    for can_id in rc.CAN0_TX_IDS:
        stamps = sorted(by_id.get(can_id, []))
        stats = rc.inter_arrival_stats(
            stamps, expected_period, tolerance, burst_threshold, coverage_min
        )
        per_id[f"0x{can_id:03X}"] = stats
        if not stamps:
            continue
        for problem in rc.timing_verdict(stats, coverage_min):
            problems.append(f"0x{can_id:03X} ({rc.CAN0_TX_NAMES.get(can_id, '?')}): {problem}")

    worst_burst = [
        (hex(int(key, 16)), entry["short_interval_count"], entry["min_period_ms"])
        for key, entry in per_id.items()
        if entry.get("burst_detected")
    ]

    timing = RegressionResult(
        test_id=CAN2_ID,
        name=CAN2_NAME,
        status=Status.FAIL if problems else Status.PASS,
        duration_s=time.monotonic() - started,
        expected=(
            f"every periodic CAN0 id arrives on average at {expected_period:.0f} ms with all "
            f"inter-arrival gaps within +/-{tolerance:.0f} ms, and no interval below the "
            f"{burst_threshold:.0f} ms burst threshold"
        ),
        observed=(
            "no timing problems across "
            f"{len([c for c in rc.CAN0_TX_IDS if by_id.get(c)])} ids"
            if not problems
            else f"{len(problems)} timing problem(s); " + "; ".join(problems[:4])
        ),
        details="\n".join(problems),
        area=CAN2_AREA,
        caps=CAN1_CAPS,
        observations=(Observation.CAN,),
        evidence=Evidence.HARDWARE,
        diagnostics={
            "collect_s": duration,
            "expected_period_ms": expected_period,
            "tolerance_ms": tolerance,
            "burst_threshold_ms": burst_threshold,
            "per_id": per_id,
            "bursts": worst_burst,
            "scheduler": session.extra.get("scheduler"),
        },
    )
    return [frame_set, timing]


# --------------------------------------------------------------------------------------------------
# REG-SCHED-001
# --------------------------------------------------------------------------------------------------

SCHED_ID = "REG-SCHED-001"
SCHED_NAME = "Scheduler tick accounting"
SCHED_AREA = "Scheduler"
SCHED_CAPS = ("JLINK",)


def scheduler_regression(session: Session) -> RegressionResult:
    """The scheduler must never bank missed ticks.

    Bms_Scheduler_MainFunction executes each due task at most once per call and reduces
    the counter modulo the period, which is what stopped the historic CAN cycle-time
    burst.  g_BmsSchedulerMissedTickCount counts ticks beyond the first per call, so a
    non-zero value over a quiet window means the main loop fell behind.
    """
    started = time.monotonic()
    expected = (
        "g_BmsSchedulerMissedTickCount stays 0 and the pending-tick peak stays small "
        "while the firmware runs under nominal stimulus"
    )
    blocked = session.blocked(SCHED_ID, SCHED_NAME, SCHED_CAPS, expected, SCHED_AREA)
    if blocked:
        return blocked

    target = session.target
    assert target is not None
    for key in ("sched_pending", "sched_pending_max", "sched_missed"):
        if not target.has(key):
            return RegressionResult.skip(
                SCHED_ID,
                SCHED_NAME,
                f"symbol {key} did not resolve against the ELF",
                area=SCHED_AREA,
                caps=SCHED_CAPS,
                expected=expected,
            )

    window = max(1.0, session.config.can_collect_s * 0.5)
    try:
        samples = target.sample(window, 0.05)
    except Exception as exc:  # noqa: BLE001
        return RegressionResult.infra(
            SCHED_ID,
            SCHED_NAME,
            f"sampling the scheduler counters failed: {exc}",
            area=SCHED_AREA,
            caps=SCHED_CAPS,
            expected=expected,
        )

    pending = [int(reading.get("sched_pending", 0)) for _, reading in samples]
    peak = max((int(reading.get("sched_pending_max", 0)) for _, reading in samples), default=0)
    missed = max((int(reading.get("sched_missed", 0)) for _, reading in samples), default=0)
    bound = 5

    problems = []
    if missed != 0:
        problems.append(
            f"g_BmsSchedulerMissedTickCount reached {missed}: the main loop fell behind "
            "and the scheduler had to drop catch-up work"
        )
    if peak > bound:
        problems.append(
            f"g_BmsSchedulerPendingTicksMax reached {peak}, above the tolerated peak of {bound}"
        )

    session.extra["scheduler"] = {
        "missed": missed,
        "pending_max": peak,
        "pending_observed": sorted(set(pending)),
    }

    return RegressionResult(
        test_id=SCHED_ID,
        name=SCHED_NAME,
        status=Status.FAIL if problems else Status.PASS,
        duration_s=time.monotonic() - started,
        expected=expected,
        observed=(
            f"missed_ticks={missed}, pending peak={peak}, "
            f"pending values seen {sorted(set(pending))} over {window:.1f} s"
        ),
        details="\n".join(problems),
        area=SCHED_AREA,
        caps=SCHED_CAPS,
        observations=(Observation.DEBUGGER_EXPORTED,),
        evidence=Evidence.HARDWARE,
        diagnostics={
            "window_s": round(window, 2),
            "samples": len(samples),
            "missed_tick_count": missed,
            "pending_ticks_max": peak,
            "pending_values": sorted(set(pending)),
            "tolerated_pending_peak": bound,
        },
    )
