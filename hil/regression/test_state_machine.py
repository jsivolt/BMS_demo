"""REG-SM-001, REG-SM-002, REG-SM-003 and REG-CONTACTOR-001.

State changes go through the real CAN0 control interface (0x201). Nothing writes a
state variable directly, and no transition is verified with a fixed sleep: every step
polls with a timeout and records the transitions it actually saw.
"""

from __future__ import annotations

import time
from typing import Any

import regression_common as rc
from regression_result import Evidence, Observation, RegressionResult, Status
from regression_target import Session

STANDBY = 1
ACTIVE = 2
FAULT = 3

OFF = 0
NEG_ON = 1
PRECHARGE = 2
POS_ON = 3
RUN = 4
CONTACTOR_FAULT = 6

#: The sequence Bms_Contactor.c walks through with BMS_CONTACTOR_SIMULATION_MODE == 1.
EXPECTED_CONTACTOR_SEQUENCE = (OFF, NEG_ON, PRECHARGE, POS_ON, RUN)

SIMULATION_NOTICE = (
    "BMS_CONTACTOR_SIMULATION_MODE = 1 (defined in src/control/Bms_Contactor.c). "
    "Precharge therefore completes on a fixed 100 ms timer driven by "
    "BMS_CONTACTOR_POS_DELAY_MS, NOT on a bus-voltage judgement. "
    "This test does NOT validate real Vbus/Vpack precharge completion, and real Pack 1 "
    "relay GPIO (PTC23/24/25) is not measured externally by this runner."
)


def _send_control(session: Session, command: int) -> None:
    transport = session.transport
    assert transport is not None
    transport.send(
        "CAN0", rc.CAN0_RX_CONTROL_ID, bytes([command, 0, 0, 0, 0, 0, 0, 0])
    )


def _state_name(value: Any) -> str:
    try:
        return rc.BMS_STATE_NAMES.get(int(value), f"UNKNOWN({value})")
    except (TypeError, ValueError):
        return str(value)


def _contactor_name(value: Any) -> str:
    try:
        return rc.CONTACTOR_STATE_NAMES.get(int(value), f"UNKNOWN({value})")
    except (TypeError, ValueError):
        return str(value)


def _sequence_from(samples: list[tuple[float, dict[str, Any]]], key: str) -> list[tuple[float, int]]:
    """First-seen transition list for one integer field, ignoring repeats."""
    transitions: list[tuple[float, int]] = []
    previous: int | None = None
    for stamp, reading in samples:
        if key not in reading:
            continue
        value = int(reading[key])
        if value != previous:
            transitions.append((stamp, value))
            previous = value
    return transitions


# --------------------------------------------------------------------------------------------------
# REG-SM-001
# --------------------------------------------------------------------------------------------------

SM1_ID = "REG-SM-001"
SM1_NAME = "Nominal STANDBY"
SM1_CAPS = ("JLINK", "CAN1", "CAN2")


def sm_standby_regression(session: Session) -> RegressionResult:
    started = time.monotonic()
    expected = "the supervisor sits in STANDBY with no critical fault while nominal stimulus runs"
    blocked = session.blocked(SM1_ID, SM1_NAME, SM1_CAPS, expected, "State Machine")
    if blocked:
        return blocked
    dependency = session.prerequisite_problem(["REG-BOOT-001"])
    if dependency:
        return RegressionResult.skip(
            SM1_ID, SM1_NAME, dependency, area="State Machine", caps=SM1_CAPS, expected=expected
        )

    target = session.target
    assert target is not None
    try:
        samples = target.sample(1.0, 0.05)
    except Exception as exc:  # noqa: BLE001
        return RegressionResult.infra(
            SM1_ID, SM1_NAME, f"sampling failed: {exc}", area="State Machine",
            caps=SM1_CAPS, expected=expected,
        )

    states = [int(reading.get("state", -1)) for _, reading in samples]
    faults = [int(reading.get("sys_faults", 0)) for _, reading in samples]
    critical = [mask for mask in faults if mask & rc.FAULT_CRITICAL_MASK]
    snapshot = session.fault_snapshot()

    problems = []
    if not states or any(state != STANDBY for state in states):
        problems.append(
            "the supervisor is not continuously in STANDBY: "
            + ", ".join(_state_name(value) for value in sorted(set(states)))
        )
    if critical:
        problems.append(
            "a critical fault bit is set while nominal stimulus runs: "
            + ", ".join(rc.decode_fault_mask(critical[0]))
        )

    return RegressionResult(
        test_id=SM1_ID,
        name=SM1_NAME,
        status=Status.FAIL if problems else Status.PASS,
        duration_s=time.monotonic() - started,
        expected=expected,
        observed=(
            f"state {_state_name(states[-1]) if states else 'unknown'} across "
            f"{len(samples)} samples over 1.0 s"
        ),
        details="\n".join(problems + [rc.fault_snapshot_text(snapshot)]),
        area="State Machine",
        caps=SM1_CAPS,
        observations=(Observation.DEBUGGER_EXPORTED,),
        evidence=Evidence.HARDWARE,
        diagnostics={"states_seen": sorted(set(states)), "fault_snapshot": snapshot},
    )


# --------------------------------------------------------------------------------------------------
# REG-SM-002 + REG-CONTACTOR-001
# --------------------------------------------------------------------------------------------------

SM2_ID = "REG-SM-002"
SM2_NAME = "Enable to ACTIVE"
SM2_CAPS = ("CAN0", "JLINK", "CAN1", "CAN2")

CONTACTOR_ID = "REG-CONTACTOR-001"
CONTACTOR_NAME = "Pack 1 contactor sequence (simulation mode)"
CONTACTOR_CAPS = ("CAN0", "JLINK")


def sm_enable_regression(session: Session) -> RegressionResult:
    """Sends the real Enable command and polls for ACTIVE plus a closed Pack 1 path."""
    started = time.monotonic()
    expected = (
        "0x201 command 0x01 moves the supervisor STANDBY -> ACTIVE and Pack 1 reaches RUN "
        "with NEG and POS closed and PRE open"
    )
    blocked = session.blocked(SM2_ID, SM2_NAME, SM2_CAPS, expected, "State Machine")
    if blocked:
        return blocked
    dependency = session.prerequisite_problem(["REG-SM-001"])
    if dependency:
        return RegressionResult.skip(
            SM2_ID, SM2_NAME, dependency, area="State Machine", caps=SM2_CAPS, expected=expected
        )

    target = session.target
    assert target is not None
    timeout = session.config.timeout("state_s") + session.config.timeout("contactor_s")

    session.setup("Enable command (CAN0 0x201 = 0x01)", "sent")
    try:
        _send_control(session, rc.CMD_ENABLE)
    except Exception as exc:  # noqa: BLE001
        return RegressionResult.infra(
            SM2_ID, SM2_NAME, f"the Enable command could not be sent: {exc}",
            area="State Machine", caps=SM2_CAPS, expected=expected,
        )

    try:
        values, elapsed, samples = target.read_until(
            lambda reading: (
                int(reading.get("state", -1)) == ACTIVE
                and int(reading.get("cont_state", -1)) == RUN
            ),
            timeout,
            0.02,
        )
    except Exception as exc:  # noqa: BLE001
        return RegressionResult.infra(
            SM2_ID, SM2_NAME, f"polling after Enable failed: {exc}",
            area="State Machine", caps=SM2_CAPS, expected=expected,
        )

    state_transitions = _sequence_from(samples, "state")
    contactor_transitions = _sequence_from(samples, "cont_state")
    session.extra["contactor_samples"] = [
        (stamp, dict(reading)) for stamp, reading in samples
    ]

    state = int(values.get("state", -1))
    cont_state = int(values.get("cont_state", -1))
    negative = int(values.get("cont_neg", 0)) == 1
    positive = int(values.get("cont_pos", 0)) == 1
    precharge = int(values.get("cont_pre", 0)) == 1
    snapshot = session.fault_snapshot()

    problems = []
    if state != ACTIVE:
        problems.append(
            f"the supervisor is {_state_name(state)} after {elapsed:.2f} s, expected ACTIVE"
        )
    if cont_state != RUN:
        problems.append(
            f"Pack 1 contactor state is {_contactor_name(cont_state)}, expected RUN"
        )
    if not negative:
        problems.append("Pack 1 negative output is not asserted in RUN")
    if not positive:
        problems.append("Pack 1 positive output is not asserted in RUN")
    if precharge:
        problems.append("Pack 1 precharge output is still asserted in RUN")

    state_path = " -> ".join(
        f"{_state_name(value)}@{stamp * 1000.0:.0f}ms" for stamp, value in state_transitions
    )
    contactor_path = " -> ".join(
        f"{_contactor_name(value)}@{stamp * 1000.0:.0f}ms" for stamp, value in contactor_transitions
    )

    observations = [Observation.CAN, Observation.DEBUGGER_STATIC]
    can_note = ""
    if session.bus_available("CAN0"):
        payloads = _grab_can0_contactor(session, 1.0)
        if 0x302 in payloads:
            decoded = rc.decode_0x302(payloads[0x302])
            can_note = (
                f"CAN0 0x302 reports Pack 1 {decoded['state_names'][0]} "
                f"neg={int(decoded['pack1_negative'])} pos={int(decoded['pack1_positive'])} "
                f"pre={int(decoded['pack1_precharge'])}"
            )
            observations.append(Observation.CAN)
        else:
            can_note = "CAN0 0x302 cross-check not performed (no frame captured)"

    return RegressionResult(
        test_id=SM2_ID,
        name=SM2_NAME,
        status=Status.FAIL if problems else Status.PASS,
        duration_s=time.monotonic() - started,
        expected=expected,
        observed=(
            f"state {_state_name(state)} in {elapsed * 1000.0:.0f} ms; "
            f"Pack 1 {_contactor_name(cont_state)} "
            f"(neg={int(negative)} pos={int(positive)} pre={int(precharge)})"
        ),
        details="\n".join(
            problems
            + [
                f"supervisor path: {state_path or 'no change observed'}",
                f"Pack 1 contactor path: {contactor_path or 'no change observed'}",
                can_note,
                rc.fault_snapshot_text(snapshot),
            ]
        ),
        area="State Machine",
        caps=SM2_CAPS,
        observations=tuple(dict.fromkeys(observations)),
        evidence=Evidence.HARDWARE,
        diagnostics={
            "timeout_s": timeout,
            "elapsed_s": round(elapsed, 3),
            "state_transitions": [
                {"t_ms": round(stamp * 1000.0, 1), "state": value, "name": _state_name(value)}
                for stamp, value in state_transitions
            ],
            "contactor_transitions": [
                {
                    "t_ms": round(stamp * 1000.0, 1),
                    "state": value,
                    "name": _contactor_name(value),
                }
                for stamp, value in contactor_transitions
            ],
            "final_outputs": {"negative": negative, "positive": positive, "precharge": precharge},
            "fault_snapshot": snapshot,
        },
    )


def _grab_can0_contactor(session: Session, timeout_s: float) -> dict[int, bytes]:
    transport = session.transport
    if transport is None or not transport.is_open("CAN0"):
        return {}
    deadline = time.monotonic() + timeout_s
    payloads: dict[int, bytes] = {}
    while 0x302 not in payloads and time.monotonic() < deadline:
        frame = transport.recv("CAN0", 0.05)
        if frame is None:
            continue
        session.can0_frames.append(frame)
        if frame.can_id == 0x302:
            payloads[0x302] = frame.data
    return payloads


def contactor_regression(session: Session) -> RegressionResult:
    """Consumes the transition history REG-SM-002 recorded; it does not re-enable."""
    started = time.monotonic()
    expected = (
        "Pack 1 walks OFF -> NEG_ON -> PRECHARGE -> POS_ON -> RUN, with NEG asserted from "
        "NEG_ON, PRE only during PRECHARGE/POS_ON, and POS from POS_ON"
    )
    samples = session.extra.get("contactor_samples")
    if not samples:
        dependency = session.prerequisite_problem(["REG-SM-002"])
        return RegressionResult.skip(
            CONTACTOR_ID,
            CONTACTOR_NAME,
            dependency or "no contactor transition history was captured by REG-SM-002",
            area="Contactor",
            caps=CONTACTOR_CAPS,
            expected=expected,
        )

    transitions = _sequence_from(samples, "cont_state")
    observed_states = [value for _, value in transitions]

    # Keep only the first ordered appearance of each state so a stray repeat cannot
    # disguise a skipped phase.
    ordered: list[int] = []
    for value in observed_states:
        if not ordered or ordered[-1] != value:
            ordered.append(value)

    outputs: dict[int, dict[str, bool]] = {}
    for stamp, reading in samples:
        state = int(reading.get("cont_state", -1))
        if state not in outputs:
            outputs[state] = {
                "negative": int(reading.get("cont_neg", 0)) == 1,
                "positive": int(reading.get("cont_pos", 0)) == 1,
                "precharge": int(reading.get("cont_pre", 0)) == 1,
            }

    sampling_gaps = [
        round((samples[index + 1][0] - samples[index][0]) * 1000.0, 2)
        for index in range(len(samples) - 1)
    ]
    median_gap = sorted(sampling_gaps)[len(sampling_gaps) // 2] if sampling_gaps else 0.0

    problems = []
    missing = [value for value in EXPECTED_CONTACTOR_SEQUENCE if value not in ordered]
    if missing:
        problems.append(
            "the ordered sequence is incomplete; missing "
            + ", ".join(_contactor_name(value) for value in missing)
            + f" (median sampling interval {median_gap:.1f} ms)"
        )
    elif ordered[: len(EXPECTED_CONTACTOR_SEQUENCE)] != list(EXPECTED_CONTACTOR_SEQUENCE):
        problems.append(
            "the observed order does not match the expected sequence: "
            + " -> ".join(_contactor_name(value) for value in ordered)
        )

    phase_outputs = outputs.get(PRECHARGE, {})
    if phase_outputs and (not phase_outputs.get("negative") or phase_outputs.get("positive")):
        problems.append(
            "during PRECHARGE the outputs are not NEG+ / POS-: "
            f"neg={int(bool(phase_outputs.get('negative')))} "
            f"pos={int(bool(phase_outputs.get('positive')))}"
        )
    run_outputs = outputs.get(RUN, {})
    if run_outputs and run_outputs.get("precharge"):
        problems.append("precharge is still asserted in RUN")

    path = " -> ".join(
        f"{_contactor_name(value)}@{stamp * 1000.0:.0f}ms" for stamp, value in transitions
    )

    return RegressionResult(
        test_id=CONTACTOR_ID,
        name=CONTACTOR_NAME,
        status=Status.FAIL if problems else Status.PASS,
        duration_s=time.monotonic() - started,
        expected=expected,
        observed=" -> ".join(_contactor_name(value) for value in ordered) or "no transitions",
        details="\n".join(problems + [f"observed path: {path}", "", SIMULATION_NOTICE]),
        area="Contactor",
        caps=CONTACTOR_CAPS,
        observations=(Observation.DEBUGGER_STATIC,),
        evidence=Evidence.HARDWARE,
        diagnostics={
            "observed_sequence": [_contactor_name(value) for value in ordered],
            "expected_sequence": [_contactor_name(value) for value in EXPECTED_CONTACTOR_SEQUENCE],
            "outputs_per_state": {
                _contactor_name(state): flags for state, flags in sorted(outputs.items())
            },
            "median_sampling_interval_ms": median_gap,
            "samples": len(samples),
            "gpio_validation": "not covered: Pack 1 relay GPIO is not measured externally",
            "precharge_validation": "not covered: simulation mode, timer based",
        },
    )


# --------------------------------------------------------------------------------------------------
# REG-SM-003
# --------------------------------------------------------------------------------------------------

SM3_ID = "REG-SM-003"
SM3_NAME = "Disable to STANDBY"
SM3_CAPS = ("CAN0", "JLINK", "CAN1", "CAN2")


def sm_disable_regression(session: Session) -> RegressionResult:
    expected = (
        "0x201 command 0x02 moves the supervisor ACTIVE -> STANDBY and every Pack 1 output "
        "returns to 0"
    )
    started = time.monotonic()
    blocked = session.blocked(SM3_ID, SM3_NAME, SM3_CAPS, expected, "State Machine")
    if blocked:
        return blocked

    target = session.target
    assert target is not None

    # Establish the precondition through the real control interface only.
    current = int(target.read().get("state", -1))
    if current != STANDBY:
        return RegressionResult.skip(
            SM3_ID,
            SM3_NAME,
            f"the supervisor is {_state_name(current)} before the case, not STANDBY",
            area="State Machine",
            caps=SM3_CAPS,
            expected=expected,
        )

    try:
        _send_control(session, rc.CMD_ENABLE)
        enabled, enable_elapsed, _ = target.read_until(
            lambda reading: int(reading.get("state", -1)) == ACTIVE,
            session.config.timeout("state_s"),
            0.02,
        )
    except Exception as exc:  # noqa: BLE001
        return RegressionResult.infra(
            SM3_ID, SM3_NAME, f"the Enable precondition failed: {exc}",
            area="State Machine", caps=SM3_CAPS, expected=expected,
        )

    if int(enabled.get("state", -1)) != ACTIVE:
        return RegressionResult.skip(
            SM3_ID,
            SM3_NAME,
            f"the supervisor did not reach ACTIVE ({_state_name(enabled.get('state'))}), so "
            "Disable cannot be exercised",
            area="State Machine",
            caps=SM3_CAPS,
            expected=expected,
        )
    session.setup("Enable to reach ACTIVE before REG-SM-003", f"ACTIVE after {enable_elapsed:.2f} s")

    session.setup("Disable command (CAN0 0x201 = 0x02)", "sent")
    try:
        _send_control(session, rc.CMD_DISABLE)
        values, elapsed, samples = target.read_until(
            lambda reading: (
                int(reading.get("state", -1)) == STANDBY
                and int(reading.get("cont_state", -1)) == OFF
            ),
            session.config.timeout("state_s") + session.config.timeout("contactor_s"),
            0.02,
        )
    except Exception as exc:  # noqa: BLE001
        return RegressionResult.infra(
            SM3_ID, SM3_NAME, f"the Disable command failed: {exc}",
            area="State Machine", caps=SM3_CAPS, expected=expected,
        )

    state = int(values.get("state", -1))
    cont_state = int(values.get("cont_state", -1))
    negative = int(values.get("cont_neg", 0)) == 1
    positive = int(values.get("cont_pos", 0)) == 1
    precharge = int(values.get("cont_pre", 0)) == 1

    problems = []
    if state != STANDBY:
        problems.append(
            f"the supervisor is {_state_name(state)} after {elapsed:.2f} s, expected STANDBY"
        )
    if cont_state != OFF:
        problems.append(f"Pack 1 contactor state is {_contactor_name(cont_state)}, expected OFF")
    if negative or positive or precharge:
        problems.append(
            "an output is still asserted: "
            f"neg={int(negative)} pos={int(positive)} pre={int(precharge)}"
        )

    state_transitions = _sequence_from(samples, "state")
    path = " -> ".join(
        f"{_state_name(value)}@{stamp * 1000.0:.0f}ms" for stamp, value in state_transitions
    )

    return RegressionResult(
        test_id=SM3_ID,
        name=SM3_NAME,
        status=Status.FAIL if problems else Status.PASS,
        duration_s=time.monotonic() - started,
        expected=expected,
        observed=(
            f"state {_state_name(state)}, Pack 1 {_contactor_name(cont_state)}, "
            f"neg={int(negative)} pos={int(positive)} pre={int(precharge)} "
            f"after {elapsed * 1000.0:.0f} ms"
        ),
        details="\n".join(problems + [f"supervisor path: {path or 'no change observed'}"]),
        area="State Machine",
        caps=SM3_CAPS,
        observations=(Observation.CAN, Observation.DEBUGGER_STATIC),
        evidence=Evidence.HARDWARE,
        diagnostics={
            "state_transitions": [
                {"t_ms": round(stamp * 1000.0, 1), "state": value, "name": _state_name(value)}
                for stamp, value in state_transitions
            ],
            "final_outputs": {"negative": negative, "positive": positive, "precharge": precharge},
        },
    )
