"""REG-FLASH-001 and REG-BOOT-001.

Flash the built image, prove the target runs it, then observe the real INIT -> STANDBY
path through g_DebugBmsState.
"""

from __future__ import annotations

import time
from typing import Any

import regression_common as rc
from regression_result import Evidence, Observation, RegressionResult, Status
from regression_target import Session

# --------------------------------------------------------------------------------------------------
# REG-FLASH-001
# --------------------------------------------------------------------------------------------------

FLASH_ID = "REG-FLASH-001"
FLASH_NAME = "Flash Debug_FLASH image and verify firmware identity"
FLASH_AREA = "Flash"
FLASH_CAPS = ("JLINK",)
FLASH_EXPECTED = (
    "flash.bat returns 0; the target can be connected afterwards; the .pflash section on "
    "the MCU matches the ELF; the firmware is running"
)


def flash_regression(session: Session) -> RegressionResult:
    started = time.monotonic()
    target = session.target
    diagnostics: dict[str, Any] = {}

    if getattr(session.args, "no_flash", False):
        return RegressionResult.skip(
            FLASH_ID,
            FLASH_NAME,
            "--no-flash was given; the image on the target was only verified, not rewritten",
            area=FLASH_AREA,
            caps=FLASH_CAPS,
            expected=FLASH_EXPECTED,
        )

    blocked = session.blocked(FLASH_ID, FLASH_NAME, FLASH_CAPS, FLASH_EXPECTED, FLASH_AREA)
    if blocked:
        return blocked

    repo = rc.REPO
    script = repo / "flash.bat"
    if not script.is_file():
        return RegressionResult.infra(
            FLASH_ID,
            FLASH_NAME,
            f"flash script not found: {script}",
            area=FLASH_AREA,
            caps=FLASH_CAPS,
            expected=FLASH_EXPECTED,
        )

    # The probe is exclusive: hil_common.check_host already refuses to start while a
    # J-Link GDB server holds it, and flash.bat starts its own server.
    outcome = rc.run_command(["cmd", "/c", "flash.bat"], repo, timeout_s=300.0)
    diagnostics["flash_command"] = outcome.command
    diagnostics["flash_returncode"] = outcome.returncode
    diagnostics["flash_output"] = rc.tail(outcome.stdout + outcome.stderr, 25)

    if outcome.returncode != 0:
        return RegressionResult(
            test_id=FLASH_ID,
            name=FLASH_NAME,
            status=Status.FAIL,
            duration_s=time.monotonic() - started,
            expected=FLASH_EXPECTED,
            observed=f"flash.bat exited {outcome.returncode}",
            details=rc.tail(outcome.stdout + outcome.stderr, 25),
            area=FLASH_AREA,
            caps=FLASH_CAPS,
            evidence=Evidence.HARDWARE,
            diagnostics=diagnostics,
        )

    # flash.bat leaves the target running; a fresh connect fills RAM with 0xDEADBEEF, so
    # the reset below is required before the firmware can be judged alive.
    try:
        target.reset_and_run()
    except Exception as exc:  # noqa: BLE001 - a probe failure here is infrastructure
        return RegressionResult.infra(
            FLASH_ID,
            FLASH_NAME,
            f"the target could not be reset after flashing: {exc}",
            area=FLASH_AREA,
            caps=FLASH_CAPS,
            expected=FLASH_EXPECTED,
            observed="reset after flash failed",
        )

    verification = target.verify_flash()
    diagnostics["target_flash_verification"] = verification

    readings = [target.read().get("led") for _ in range(4)]
    for _ in range(3):
        time.sleep(0.25)
        readings.append(target.read().get("led"))
    diagnostics["led_samples"] = readings
    running = len(set(readings)) > 1

    problems = []
    if verification != "matched":
        problems.append(f"target .pflash does not match the ELF: {verification}")
    if not running:
        problems.append(f"g_LedCounter did not change ({readings}) - the firmware is not running")

    observed = (
        f"flash.bat exited 0; .pflash {verification}; "
        f"g_LedCounter {'changed' if running else 'frozen'} {readings}"
    )

    return RegressionResult(
        test_id=FLASH_ID,
        name=FLASH_NAME,
        status=Status.FAIL if problems else Status.PASS,
        duration_s=time.monotonic() - started,
        expected=FLASH_EXPECTED,
        observed=observed,
        details="\n".join(problems),
        area=FLASH_AREA,
        caps=FLASH_CAPS,
        observations=(Observation.DEBUGGER_EXPORTED,),
        evidence=Evidence.HARDWARE,
        diagnostics=diagnostics,
    )


# --------------------------------------------------------------------------------------------------
# REG-BOOT-001
# --------------------------------------------------------------------------------------------------

BOOT_ID = "REG-BOOT-001"
BOOT_NAME = "Boot to STANDBY"
BOOT_AREA = "Boot"
BOOT_CAPS = ("JLINK", "CAN1", "CAN2")
BOOT_EXPECTED = (
    "after reset the supervisor runs INIT -> STANDBY within the bounded timeout and "
    "never enters FAULT under nominal vAFE/vPACK stimulus"
)
STANDBY = 1
FAULT = 3


def _state_history(
    session: Session,
    predicate,
    timeout_s: float,
    period_s: float = 0.02,
) -> tuple[list[tuple[float, int]], dict[str, Any], float]:
    """Polls g_DebugBmsState and returns every transition it observed."""
    assert session.target is not None
    values, elapsed, samples = session.target.read_until(predicate, timeout_s, period_s)
    transitions: list[tuple[float, int]] = []
    previous: int | None = None
    for stamp, reading in samples:
        state = int(reading.get("state", -1))
        if state != previous:
            transitions.append((stamp, state))
            previous = state
    return transitions, values, elapsed


def boot_regression(session: Session) -> RegressionResult:
    started = time.monotonic()
    blocked = session.blocked(
        BOOT_ID,
        BOOT_NAME,
        BOOT_CAPS,
        BOOT_EXPECTED,
        BOOT_AREA,
        context="nominal boot needs the vAFE and vPACK stimulus",
    )
    if blocked:
        return blocked

    target = session.target
    assert target is not None

    session.setup("reset before REG-BOOT-001", "target reset and released")
    target.reset_and_run()

    timeout = session.config.timeout("boot_s")
    try:
        transitions, values, elapsed = _state_history(
            session, lambda reading: int(reading.get("state", -1)) == STANDBY, timeout
        )
    except Exception as exc:  # noqa: BLE001
        return RegressionResult.infra(
            BOOT_ID,
            BOOT_NAME,
            f"polling g_DebugBmsState failed: {exc}",
            area=BOOT_AREA,
            caps=BOOT_CAPS,
            expected=BOOT_EXPECTED,
        )

    final_state = int(values.get("state", -1))
    saw_fault = any(state == FAULT for _, state in transitions)
    reached_standby = final_state == STANDBY

    snapshot = session.fault_snapshot()
    diagnostics: dict[str, Any] = {
        "transitions": [
            {"t_ms": round(stamp * 1000.0, 1), "state": state, "name": rc.BMS_STATE_NAMES.get(state, "?")}
            for stamp, state in transitions
        ],
        "final_state": final_state,
        "final_state_name": rc.BMS_STATE_NAMES.get(final_state, "?"),
        "elapsed_s": round(elapsed, 3),
        "timeout_s": timeout,
        "fault_snapshot": snapshot,
    }
    if "sched_missed" in values:
        diagnostics["scheduler"] = {
            "pending": values.get("sched_pending"),
            "pending_max": values.get("sched_pending_max"),
            "missed": values.get("sched_missed"),
        }

    path = " -> ".join(
        f"{rc.BMS_STATE_NAMES.get(state, '?')}@{stamp * 1000.0:.0f}ms" for stamp, state in transitions
    )

    can0_note = "CAN0 cross-check not performed (CAN0 not selected for this run)"
    if session.bus_available("CAN0"):
        frame = _wait_for_can0_state(session, STANDBY, 1.0)
        if frame is None:
            can0_note = "CAN0 cross-check not performed (no 0x300 captured in the window)"
        else:
            diagnostics["can0_0x300_state"] = frame[0]
            can0_note = f"CAN0 0x300 reports state {frame[0]}"

    if not reached_standby:
        status = Status.FAIL
        details = (
            f"last state {rc.BMS_STATE_NAMES.get(final_state, '?')} after {elapsed:.2f} s "
            f"(timeout {timeout:.0f} s)\n{rc.fault_snapshot_text(snapshot)}"
        )
    elif saw_fault:
        status = Status.FAIL
        details = (
            "the supervisor entered FAULT before reaching STANDBY under nominal stimulus\n"
            f"path: {path}\n{rc.fault_snapshot_text(snapshot)}"
        )
    else:
        status = Status.PASS
        details = f"path: {path}\n{can0_note}\n{rc.fault_snapshot_text(snapshot)}"

    observations = [Observation.DEBUGGER_EXPORTED]
    if session.bus_available("CAN0"):
        observations.append(Observation.CAN)

    return RegressionResult(
        test_id=BOOT_ID,
        name=BOOT_NAME,
        status=status,
        duration_s=time.monotonic() - started,
        expected=BOOT_EXPECTED,
        observed=(
            f"{path or 'no state change observed'}; reached STANDBY in "
            f"{elapsed * 1000.0:.0f} ms"
            if reached_standby
            else f"never reached STANDBY; last state {rc.BMS_STATE_NAMES.get(final_state, '?')}"
        ),
        details=details,
        area=BOOT_AREA,
        caps=BOOT_CAPS,
        observations=tuple(observations),
        evidence=Evidence.HARDWARE,
        diagnostics=diagnostics,
    )


def _wait_for_can0_state(
    session: Session, wanted: int, timeout_s: float
) -> tuple[int, bytes] | None:
    """Collects CAN0 0x300 briefly and returns the first payload carrying the wanted state."""
    transport = session.transport
    if transport is None or not transport.is_open("CAN0"):
        return None
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        frame = transport.recv("CAN0", 0.05)
        if frame is None or frame.can_id != 0x300 or len(frame.data) < 8:
            continue
        session.can0_frames.append(frame)
        if frame.data[0] == wanted:
            return frame.data[0], frame.data
    return None
