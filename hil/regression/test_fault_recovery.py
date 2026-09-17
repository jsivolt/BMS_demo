"""REG-FAULT-001, REG-FAULT-002 and REG-FAULT-003.

The fault is provoked through the real measurement path - one vAFE cell is driven above
the over-voltage trip - never by calling FaultManager_SetSystem from the test.

Three distinct behaviours are exercised and must not be conflated:

    clearing the live fault condition   (the cell goes back under the clear threshold)
    clearing the supervisor FAULT state (0x201 command 0x03)
    clearing the latched history        (0x201 command 0x04)
"""

from __future__ import annotations

import time
from typing import Any

import regression_common as rc
from regression_result import Evidence, Observation, RegressionResult, Status
from regression_target import Session

FAULT1_ID = "REG-FAULT-001"
FAULT1_NAME = "Cell over-voltage fault injection"
FAULT1_CAPS = ("JLINK", "CAN1", "CAN0")

FAULT2_ID = "REG-FAULT-002"
FAULT2_NAME = "Over-voltage hysteresis and clear"
FAULT2_CAPS = ("JLINK", "CAN1", "CAN0")

FAULT3_ID = "REG-FAULT-003"
FAULT3_NAME = "ClearFaultHistory"
FAULT3_CAPS = ("JLINK", "CAN1", "CAN0")

STANDBY = 1
ACTIVE = 2
FAULT = 3
OFF = 0
CONTACTOR_FAULT = 6

IMMBALANCE_NOTE = (
    "FAULT_CELL_IMBALANCE (bit 15) is expected to be set as well: driving a single cell "
    "to 4260 mV against fifteen at 3700 mV gives a 560 mV delta, above the 300 mV set "
    "threshold. It is not part of FAULT_CRITICAL_MASK, so it must never be the reason the "
    "supervisor leaves STANDBY or ACTIVE."
)


def _cell_state(session: Session) -> dict[str, Any]:
    values = session.target.read() if session.target else {}
    return {
        "state": int(values.get("state", -1)),
        "sys_faults": int(values.get("sys_faults", 0)),
        "sys_last": int(values.get("sys_last", 0)),
        "pack1_faults": int(values.get("pack1_faults", 0)),
        "pack1_last": int(values.get("pack1_last", 0)),
        "cont_state": int(values.get("cont_state", -1)),
        "cont_neg": int(values.get("cont_neg", 0)) == 1,
        "cont_pos": int(values.get("cont_pos", 0)) == 1,
        "cont_pre": int(values.get("cont_pre", 0)) == 1,
    }


def _settle(session: Session, seconds: float) -> None:
    """Lets the 10 ms and 100 ms tasks run a few cycles before the next verdict."""
    time.sleep(seconds)


def _cell_ov_set(mask: int) -> bool:
    return rc.mask_bit(mask, rc.FAULT_CELL_OV_BIT)


# --------------------------------------------------------------------------------------------------
# REG-FAULT-001
# --------------------------------------------------------------------------------------------------


def fault_injection_regression(session: Session) -> RegressionResult:
    started = time.monotonic()
    config = session.config
    index = config.fault_cell_index
    expected = (
        f"driving vAFE cell {index + 1} to {config.cell_ov_mv} mV while the supervisor is "
        "ACTIVE sets FAULT_CELL_OV in the live system mask, latches it into the history, "
        f"and moves the supervisor to FAULT with every Pack 1 output at 0"
    )
    blocked = session.blocked(
        FAULT1_ID, FAULT1_NAME, FAULT1_CAPS, expected, "Fault",
        context="a measurement-path fault needs the vAFE stimulus and a way to observe the result",
    )
    if blocked:
        return blocked
    dependency = session.prerequisite_problem(["REG-SM-002"])
    if dependency:
        return RegressionResult.skip(
            FAULT1_ID, FAULT1_NAME, dependency, area="Fault", caps=FAULT1_CAPS, expected=expected
        )

    target = session.target
    assert target is not None
    if session.stimulus is None:
        return RegressionResult.skip(
            FAULT1_ID,
            FAULT1_NAME,
            "no vAFE stimulus runner is active, so a measurement-path fault cannot be injected",
            area="Fault",
            caps=FAULT1_CAPS,
            expected=expected,
        )

    before = _cell_state(session)
    if before["state"] != ACTIVE:
        return RegressionResult.skip(
            FAULT1_ID,
            FAULT1_NAME,
            f"the supervisor is state {before['state']} before injection, expected ACTIVE",
            area="Fault",
            caps=FAULT1_CAPS,
            expected=expected,
        )

    session.setup(
        f"inject {config.cell_ov_mv} mV on vAFE cell {index + 1}",
        "stimulus updated on CAN1",
    )
    session.stimulus.vafe.set_cell(index, config.cell_ov_mv)

    timeout = session.config.timeout("fault_s")
    try:
        values, elapsed, samples = target.read_until(
            lambda reading: (
                rc.mask_bit(int(reading.get("sys_faults", 0)), rc.FAULT_CELL_OV_BIT)
                and int(reading.get("state", -1)) == FAULT
            ),
            timeout,
            0.02,
        )
    except Exception as exc:  # noqa: BLE001
        return RegressionResult.infra(
            FAULT1_ID, FAULT1_NAME, f"polling after injection failed: {exc}",
            area="Fault", caps=FAULT1_CAPS, expected=expected,
        )

    live = int(values.get("sys_faults", 0))
    last = int(values.get("sys_last", 0))
    state = int(values.get("state", -1))
    contactor = int(values.get("cont_state", -1))
    neg = int(values.get("cont_neg", 0)) == 1
    pos = int(values.get("cont_pos", 0)) == 1
    pre = int(values.get("cont_pre", 0)) == 1

    # Give the contactor machine its own window to react to the critical fault.
    _settle(session, 0.4)
    after = _cell_state(session)
    snapshot = session.fault_snapshot()

    problems = []
    if not _cell_ov_set(live):
        problems.append(
            f"FAULT_CELL_OV was not set within {elapsed:.2f} s; system mask 0x{live:08X}"
        )
    if state != FAULT:
        problems.append(f"the supervisor is state {state}, expected FAULT (3)")
    if not _cell_ov_set(last):
        problems.append(
            f"the latched history does not contain FAULT_CELL_OV (0x{last:08X})"
        )
    if after["cont_neg"] or after["cont_pos"] or after["cont_pre"]:
        problems.append(
            "Pack 1 outputs are not all 0: "
            f"neg={int(after['cont_neg'])} pos={int(after['cont_pos'])} pre={int(after['cont_pre'])}"
        )
    if after["cont_state"] not in (CONTACTOR_FAULT, OFF):
        problems.append(
            f"Pack 1 contactor state is {after['cont_state']}, expected FAULT (6) or OFF (0)"
        )

    can_note, can_diag = _latched_history_from_can(session)
    imbalance_set = rc.mask_bit(live, rc.FAULT_CELL_IMBALANCE_BIT)

    return RegressionResult(
        test_id=FAULT1_ID,
        name=FAULT1_NAME,
        status=Status.FAIL if problems else Status.PASS,
        duration_s=time.monotonic() - started,
        expected=expected,
        observed=(
            f"system mask 0x{live:08X} ({', '.join(rc.decode_fault_mask(live)) or 'none'}) "
            f"in {elapsed * 1000.0:.0f} ms; supervisor state {state}; "
            f"Pack 1 outputs neg={int(after['cont_neg'])} pos={int(after['cont_pos'])} "
            f"pre={int(after['cont_pre'])}"
        ),
        details="\n".join(
            problems
            + [
                rc.fault_snapshot_text(snapshot),
                can_note,
                "",
                IMMBALANCE_NOTE,
                f"FAULT_CELL_IMBALANCE is {'set' if imbalance_set else 'clear'} in this sample.",
                "",
                "The fault was provoked through the measurement path (CAN1 vAFE), not by "
                "calling FaultManager_SetSystem.",
            ]
        ),
        area="Fault",
        caps=FAULT1_CAPS,
        observations=(Observation.CAN, Observation.DEBUGGER_STATIC),
        evidence=Evidence.HARDWARE,
        diagnostics={
            "injected_cell_index": index,
            "injected_cell_mv": config.cell_ov_mv,
            "detection_latency_ms": round(elapsed * 1000.0, 1),
            "live_system_mask": live,
            "live_system_bits": rc.decode_fault_mask(live),
            "latched_system_mask": last,
            "latched_system_bits": rc.decode_fault_mask(last),
            "cell_imbalance_also_set": imbalance_set,
            "final_outputs": {
                "negative": after["cont_neg"],
                "positive": after["cont_pos"],
                "precharge": after["cont_pre"],
            },
            "fault_snapshot": snapshot,
            **can_diag,
        },
    )


def _latched_history_from_can(session: Session) -> tuple[str, dict[str, Any]]:
    """Cross-checks the latched history against the CAN0 0x30A frame."""
    if not session.bus_available("CAN0"):
        return "CAN0 0x30A cross-check not performed (CAN0 not selected for this run)", {}
    transport = session.transport
    assert transport is not None
    deadline = time.monotonic() + 1.5
    while time.monotonic() < deadline:
        frame = transport.recv("CAN0", 0.05)
        if frame is None:
            continue
        session.can0_frames.append(frame)
        if frame.can_id == 0x30A:
            decoded = rc.decode_0x30A(frame.data)
            return (
                f"CAN0 0x30A latched system history 0x{decoded['last_system']['mask']:08X}"
                f" ({', '.join(decoded['last_system']['bits']) or 'none'})",
                {"can0_0x30A_latched_system": decoded["last_system"]},
            )
    return "CAN0 0x30A cross-check not performed (no frame captured)", {}


# --------------------------------------------------------------------------------------------------
# REG-FAULT-002
# --------------------------------------------------------------------------------------------------


def _wait_for_cell_ov(session: Session, wanted: bool, timeout_s: float) -> tuple[bool, float, int]:
    """Polls until FAULT_CELL_OV matches ``wanted`` in the live mask."""
    target = session.target
    assert target is not None

    def predicate(reading: dict[str, Any]) -> bool:
        return rc.mask_bit(int(reading.get("sys_faults", 0)), rc.FAULT_CELL_OV_BIT) == wanted

    values, elapsed, _ = target.read_until(predicate, timeout_s, 0.02)
    mask = int(values.get("sys_faults", 0))
    return rc.mask_bit(mask, rc.FAULT_CELL_OV_BIT) == wanted, elapsed, mask


def fault_hysteresis_regression(session: Session) -> RegressionResult:
    started = time.monotonic()
    config = session.config
    index = config.fault_cell_index
    expected = (
        f"{config.cell_ov_hold_mv} mV keeps FAULT_CELL_OV set (clear threshold is 4150 mV); "
        f"{config.cell_ov_clear_mv} mV clears the live fault; the supervisor stays in FAULT "
        "until ClearFault is sent, and the latched history still holds FAULT_CELL_OV"
    )
    blocked = session.blocked(FAULT2_ID, FAULT2_NAME, FAULT2_CAPS, expected, "Fault")
    if blocked:
        return blocked
    dependency = session.prerequisite_problem([FAULT1_ID])
    if dependency:
        return RegressionResult.skip(
            FAULT2_ID, FAULT2_NAME, dependency, area="Fault", caps=FAULT2_CAPS, expected=expected
        )

    target = session.target
    assert target is not None
    assert session.stimulus is not None

    notes: list[str] = []
    problems: list[str] = []
    diagnostics: dict[str, Any] = {
        "hold_mv": config.cell_ov_hold_mv,
        "clear_mv": config.cell_ov_clear_mv,
    }

    # 1. Above the clear threshold: the fault must stay.
    session.stimulus.vafe.set_cell(index, config.cell_ov_hold_mv)
    _settle(session, 0.6)
    held = _cell_state(session)
    diagnostics["hold_mask"] = held["sys_faults"]
    diagnostics["hold_state"] = held["state"]
    if not _cell_ov_set(held["sys_faults"]):
        problems.append(
            f"FAULT_CELL_OV cleared at {config.cell_ov_hold_mv} mV, but the clear threshold "
            f"is 4150 mV"
        )
    notes.append(
        f"{config.cell_ov_hold_mv} mV -> FAULT_CELL_OV "
        f"{'still set' if _cell_ov_set(held['sys_faults']) else 'CLEARED'}"
    )

    # 2. Below the clear threshold: the live fault goes, the supervisor does not.
    session.stimulus.vafe.set_cell(index, config.cell_ov_clear_mv)
    cleared, clear_elapsed, clear_mask = _wait_for_cell_ov(
        session, False, session.config.timeout("fault_s")
    )
    _settle(session, 0.4)
    after_clear = _cell_state(session)
    diagnostics["clear_mask"] = after_clear["sys_faults"]
    diagnostics["clear_state"] = after_clear["state"]
    diagnostics["clear_latency_ms"] = round(clear_elapsed * 1000.0, 1)
    diagnostics["latched_after_clear"] = after_clear["sys_last"]

    if not cleared:
        problems.append(
            f"FAULT_CELL_OV did not clear at {config.cell_ov_clear_mv} mV "
            f"(threshold 4150 mV); mask 0x{clear_mask:08X}"
        )
    if after_clear["state"] != FAULT:
        problems.append(
            f"the supervisor left FAULT on its own (state {after_clear['state']}); FAULT is "
            "expected to stay latched until ClearFault is received"
        )
    if not _cell_ov_set(after_clear["sys_last"]):
        problems.append(
            "the latched history lost FAULT_CELL_OV when the live condition cleared"
        )
    notes.append(
        f"{config.cell_ov_clear_mv} mV -> live FAULT_CELL_OV cleared in "
        f"{clear_elapsed * 1000.0:.0f} ms, supervisor still state {after_clear['state']}"
    )

    # 3. ClearFault is honoured only in FAULT.
    session.setup("ClearFault command (CAN0 0x201 = 0x03)", "sent")
    try:
        _send_clear_fault(session)
        values, elapsed, _ = target.read_until(
            lambda reading: int(reading.get("state", -1)) == STANDBY,
            session.config.timeout("state_s"),
            0.02,
        )
    except Exception as exc:  # noqa: BLE001
        return RegressionResult.infra(
            FAULT2_ID, FAULT2_NAME, f"the ClearFault command failed: {exc}",
            area="Fault", caps=FAULT2_CAPS, expected=expected,
        )

    final = _cell_state(session)
    diagnostics["state_after_clear_fault"] = final["state"]
    diagnostics["latched_after_clear_fault"] = final["sys_last"]
    if int(values.get("state", -1)) != STANDBY:
        problems.append(
            f"ClearFault did not return the supervisor to STANDBY (state {final['state']})"
        )
    if not _cell_ov_set(final["sys_last"]):
        problems.append("ClearFault also removed FAULT_CELL_OV from the latched history")
    notes.append(
        f"ClearFault -> state {final['state']} in {elapsed * 1000.0:.0f} ms; history still "
        f"0x{final['sys_last']:08X}"
    )

    snapshot = session.fault_snapshot()
    return RegressionResult(
        test_id=FAULT2_ID,
        name=FAULT2_NAME,
        status=Status.FAIL if problems else Status.PASS,
        duration_s=time.monotonic() - started,
        expected=expected,
        observed="; ".join(notes),
        details="\n".join(problems + [rc.fault_snapshot_text(snapshot)]),
        area="Fault",
        caps=FAULT2_CAPS,
        observations=(Observation.CAN, Observation.DEBUGGER_STATIC),
        evidence=Evidence.HARDWARE,
        diagnostics={**diagnostics, "fault_snapshot": snapshot},
    )


def _send_clear_fault(session: Session) -> None:
    transport = session.transport
    assert transport is not None
    transport.send("CAN0", rc.CAN0_RX_CONTROL_ID, bytes([rc.CMD_CLEAR_FAULT, 0, 0, 0, 0, 0, 0, 0]))


# --------------------------------------------------------------------------------------------------
# REG-FAULT-003
# --------------------------------------------------------------------------------------------------


def fault_history_regression(session: Session) -> RegressionResult:
    started = time.monotonic()
    expected = (
        "0x201 command 0x04 clears the latched history masks without touching the live masks "
        "or the supervisor state"
    )
    blocked = session.blocked(FAULT3_ID, FAULT3_NAME, FAULT3_CAPS, expected, "Fault")
    if blocked:
        return blocked
    dependency = session.prerequisite_problem([FAULT2_ID])
    if dependency:
        return RegressionResult.skip(
            FAULT3_ID, FAULT3_NAME, dependency, area="Fault", caps=FAULT3_CAPS, expected=expected
        )

    target = session.target
    assert target is not None

    before = _cell_state(session)
    if not _cell_ov_set(before["sys_last"]):
        return RegressionResult.skip(
            FAULT3_ID,
            FAULT3_NAME,
            "the latched history does not contain FAULT_CELL_OV before the clear, so there is "
            "nothing to clear",
            area="Fault",
            caps=FAULT3_CAPS,
            expected=expected,
        )

    session.setup("ClearFaultHistory command (CAN0 0x201 = 0x04)", "sent")
    try:
        session.transport.send(
            "CAN0",
            rc.CAN0_RX_CONTROL_ID,
            bytes([rc.CMD_CLEAR_FAULT_HISTORY, 0, 0, 0, 0, 0, 0, 0]),
        )
        values, elapsed, _ = target.read_until(
            lambda reading: (
                int(reading.get("sys_last", 1)) == 0
                and int(reading.get("pack1_last", 1)) == 0
            ),
            session.config.timeout("state_s"),
            0.02,
        )
    except Exception as exc:  # noqa: BLE001
        return RegressionResult.infra(
            FAULT3_ID, FAULT3_NAME, f"the ClearFaultHistory command failed: {exc}",
            area="Fault", caps=FAULT3_CAPS, expected=expected,
        )

    after = _cell_state(session)
    diagnostics: dict[str, Any] = {
        "latched_before": {
            "system": before["sys_last"],
            "pack1": before["pack1_last"],
        },
        "latched_after": {
            "system": after["sys_last"],
            "pack1": after["pack1_last"],
        },
        "live_after": {
            "system": after["sys_faults"],
            "pack1": after["pack1_faults"],
        },
        "state_before": before["state"],
        "state_after": after["state"],
        "latency_ms": round(elapsed * 1000.0, 1),
    }

    problems = []
    if after["sys_last"] != 0:
        problems.append(
            f"the latched system history is still 0x{after['sys_last']:08X} "
            f"({', '.join(rc.decode_fault_mask(after['sys_last']))})"
        )
    if after["pack1_last"] != 0:
        problems.append(f"the latched Pack 1 history is still 0x{after['pack1_last']:08X}")
    if after["state"] != before["state"]:
        problems.append(
            f"the supervisor state changed from {before['state']} to {after['state']}; "
            "ClearFaultHistory must not touch it"
        )

    can_note, can_diag = _latched_history_from_can(session)
    diagnostics.update(can_diag)

    # Leave the bench in the nominal condition the remaining cases expect.
    if session.stimulus is not None:
        session.stimulus.vafe.restore_nominal()
        session.setup("restore nominal cell voltages", "all cells back to the configured nominal")

    return RegressionResult(
        test_id=FAULT3_ID,
        name=FAULT3_NAME,
        status=Status.FAIL if problems else Status.PASS,
        duration_s=time.monotonic() - started,
        expected=expected,
        observed=(
            f"latched system 0x{before['sys_last']:08X} -> 0x{after['sys_last']:08X}, "
            f"latched Pack 1 0x{before['pack1_last']:08X} -> 0x{after['pack1_last']:08X}, "
            f"live system mask unchanged at 0x{after['sys_faults']:08X}, "
            f"state {after['state']}"
        ),
        details="\n".join(
            problems
            + [
                f"live system mask after the clear: 0x{after['sys_faults']:08X} "
                f"({', '.join(rc.decode_fault_mask(after['sys_faults'])) or 'none'})",
                can_note,
                "",
                "Clearing the live fault, clearing the supervisor FAULT state and clearing the "
                "history are three different operations; this case only covers the third.",
            ]
        ),
        area="Fault",
        caps=FAULT3_CAPS,
        observations=(Observation.CAN, Observation.DEBUGGER_STATIC),
        evidence=Evidence.HARDWARE,
        diagnostics=diagnostics,
    )
