#!/usr/bin/env python3
"""BMS_demo Phase 1 smoke regression runner.

Purpose
    Run the project's own build, the existing SIL suite, and one orchestrated HIL smoke
    scenario on the S32K344 bench, then write a Markdown and a JSON report.

Outputs
    hil/regression/reports/regression_latest.md
    hil/regression/reports/regression_latest.json
    (runtime artifacts - gitignored)

Prerequisites
    Debug_FLASH/BMS_demo.elf built; a J-Link probe for anything that observes the target;
    a PCAN channel attached to whichever bus is named with --bus.

Usage
    python hil/regression/run_regression.py --smoke
    python hil/regression/run_regression.py --bus CAN0
    python hil/regression/run_regression.py --bus CAN1 --bus CAN2
    python hil/regression/run_regression.py --test REG-SM-002

Exit status
    0  every executed requested test passed (XFAIL is allowed)
    1  at least one DUT failure
    2  incomplete run: a skip or an infrastructure error
"""

from __future__ import annotations

import argparse
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

REG_DIR = Path(__file__).resolve().parent
for _path in (str(REG_DIR), str(REG_DIR.parent)):
    if _path not in sys.path:
        sys.path.insert(0, _path)

import regression_can as rcan  # noqa: E402
import regression_common as rc  # noqa: E402
import regression_target as rt  # noqa: E402
import test_boot  # noqa: E402
import test_fault_recovery  # noqa: E402
import test_measurements  # noqa: E402
import test_state_machine  # noqa: E402
import test_xcp  # noqa: E402
from regression_result import (  # noqa: E402
    ArtifactIdentity,
    RegressionResult,
    Status,
    Suite,
    console_summary,
)

DEFAULT_REPORT = REG_DIR / "reports" / "regression_latest.md"
DEFAULT_JSON = REG_DIR / "reports" / "regression_latest.json"

ALL_BUSES = ("CAN0", "CAN1", "CAN2", "CAN5")

#: Every test id this framework can produce, in execution order.
TEST_IDS = (
    "REG-BUILD-001",
    "REG-SIL-001",
    "REG-FLASH-001",
    "REG-BOOT-001",
    "REG-SCHED-001",
    "REG-MEAS-001",
    "REG-MEAS-002",
    "REG-CAN-001",
    "REG-CAN-002",
    "REG-SM-001",
    "REG-SM-002",
    "REG-CONTACTOR-001",
    "REG-FAULT-001",
    "REG-FAULT-002",
    "REG-FAULT-003",
    "REG-SM-003",
    "REG-XCP-001",
    "REG-XCP-002",
)

#: (stage name, callable, state_changing).  Everything runs; only requested ids are reported,
#: because a case may need the state an earlier case established.
SCENARIO = (
    ("flash", test_boot.flash_regression, False),
    ("boot", test_boot.boot_regression, False),
    ("scheduler", test_measurements.scheduler_regression, False),
    ("vafe measurement", test_measurements.measurement_vafe_regression, False),
    ("vpack measurement", test_measurements.measurement_vpack_regression, False),
    ("can0 output", test_measurements.can_output_regression, False),
    ("standby", test_state_machine.sm_standby_regression, False),
    ("enable", test_state_machine.sm_enable_regression, True),
    ("contactor sequence", test_state_machine.contactor_regression, False),
    ("fault injection", test_fault_recovery.fault_injection_regression, True),
    ("fault hysteresis", test_fault_recovery.fault_hysteresis_regression, True),
    ("fault history", test_fault_recovery.fault_history_regression, True),
    ("disable", test_state_machine.sm_disable_regression, True),
    ("xcp connect", test_xcp.xcp_connect_regression, False),
    ("xcp short upload", test_xcp.xcp_short_upload_regression, False),
)


# --------------------------------------------------------------------------------------------------
# CLI
# --------------------------------------------------------------------------------------------------


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="BMS_demo Phase 1 smoke regression",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--smoke",
        action="store_true",
        help="the Phase 1 smoke set (identical to the default set in this version)",
    )
    parser.add_argument("--no-build", action="store_true", help="skip REG-BUILD-001")
    parser.add_argument("--no-sil", action="store_true", help="skip REG-SIL-001")
    parser.add_argument("--no-flash", action="store_true", help="do not rewrite the target image")
    parser.add_argument(
        "--test",
        action="append",
        default=[],
        metavar="ID",
        help="report only these test ids (repeatable, comma separated). Prerequisite steps "
        "still run so the state they establish is real.",
    )
    parser.add_argument(
        "--bus",
        action="append",
        default=[],
        metavar="BUS",
        help=f"restrict this run to physically connected buses: {', '.join(ALL_BUSES)}. "
        "Buses not named are reported as 'not selected' and their tests SKIP.",
    )
    parser.add_argument("--can-config", type=Path, default=rc.DEFAULT_CAN_CONFIG)
    parser.add_argument("--nominal-config", type=Path, default=rc.DEFAULT_NOMINAL_CONFIG)
    for bus in ALL_BUSES:
        parser.add_argument(
            f"--{bus.lower()}",
            default=None,
            metavar="IFACE:CHANNEL:BITRATE",
            help=f"override the {bus} mapping for this run, e.g. pcan:PCAN_USBBUS2:1000000",
        )
    parser.add_argument("--build-config", default="Debug_FLASH")
    parser.add_argument("--report", type=Path, default=DEFAULT_REPORT)
    parser.add_argument("--json", type=Path, default=DEFAULT_JSON)
    parser.add_argument("--list-tests", action="store_true", help="print the test ids and exit")

    import hil_common

    hil_common.add_target_args(parser)
    return parser.parse_args(argv)


def requested_ids(args: argparse.Namespace) -> list[str] | None:
    raw: list[str] = []
    for chunk in args.test:
        raw.extend(part.strip() for part in chunk.split(",") if part.strip())
    if not raw:
        return None
    return [value.upper() for value in raw]


# --------------------------------------------------------------------------------------------------
# Host stages
# --------------------------------------------------------------------------------------------------


def host_stage(session: rt.Session, suite: Suite) -> None:
    result, _ = rc.build_regression(
        repo=rc.REPO,
        build_config=session.args.build_config,
        enabled=not session.args.no_build,
    )
    _publish(session, suite, result)

    sil, _ = rc.sil_regression(repo=rc.REPO, enabled=not session.args.no_sil)
    _publish(session, suite, sil)


# --------------------------------------------------------------------------------------------------
# Wiring
# --------------------------------------------------------------------------------------------------


def _publish(session: rt.Session, suite: Suite, result: RegressionResult | None) -> None:
    """Records every result for dependency lookups, but reports only the requested ones."""
    if result is None:
        return
    session.executed[result.test_id] = result
    if session.extra.get("requested") is None or result.test_id in session.extra["requested"]:
        suite.add(result)


def _publish_all(session: rt.Session, suite: Suite, results) -> None:
    if results is None:
        return
    if isinstance(results, RegressionResult):
        _publish(session, suite, results)
        return
    for result in results:
        _publish(session, suite, result)


def open_target(session: rt.Session, suite: Suite) -> None:
    import hil_common

    args = session.args
    available, note = rt.probe_available(args.jlink_dll, args.usb_sn)
    if not available:
        session.extra["jlink_reason"] = note
        session.extra["jlink_infra"] = False
        session.setup("J-Link probe", f"not used: {note}", ok=False)
        return

    session.setup("J-Link probe", note, ok=True)
    try:
        hil_common.check_host(args)
    except hil_common.SetupError as exc:
        session.extra["jlink_reason"] = str(exc)
        session.extra["jlink_infra"] = True
        session.setup("J-Link host check", f"INFRA ERROR: {exc}", ok=False)
        return

    target = rt.Target(args)
    try:
        target.open()
        target.resolve()
    except Exception as exc:  # noqa: BLE001 - any failure here is infrastructure
        target.close()
        session.extra["jlink_reason"] = f"{type(exc).__name__}: {exc}"
        session.extra["jlink_infra"] = True
        session.setup("J-Link session", f"INFRA ERROR: {exc}", ok=False)
        return

    session.target = target
    session.setup(
        "J-Link session",
        f"{target.description}; {len(target.layout.fields)} symbols resolved "
        f"({len(target.addresses)} addresses, {len(target.layout.blocks)} memory blocks)",
        ok=True,
    )
    if target.unavailable:
        session.setup(
            "Symbols not resolved",
            ", ".join(sorted(target.unavailable)),
            ok=False,
        )


def open_buses(session: rt.Session, suite: Suite) -> None:
    args = session.args
    overrides = {
        bus: getattr(args, bus.lower())
        for bus in ALL_BUSES
        if getattr(args, bus.lower(), None)
    }
    try:
        specs = rcan.load_bus_specs(args.can_config, overrides)
    except rcan.CanConfigError as exc:
        suite.infra_failure = str(exc)
        return
    session.can_specs = specs

    # No --bus means "the whole configured topology"; naming buses restricts the run to
    # the ones physically attached right now, and the rest are reported as not selected.
    selected = [bus.upper() for bus in args.bus] if args.bus else sorted(specs)
    unknown = [bus for bus in selected if bus not in specs]
    if unknown:
        suite.infra_failure = (
            f"unknown logical bus name(s): {', '.join(unknown)}; known: "
            f"{', '.join(sorted(specs))}"
        )
        return

    transport = rcan.CanTransport(specs, selected=selected)
    try:
        transport.open()
    except rcan.CanBackendMissing as exc:
        session.transport = rcan.NullTransport(specs, reason=str(exc))
        session.setup("CAN transport", f"unavailable: {exc}", ok=False)
        return
    session.transport = transport

    opened = transport.open_buses()
    session.setup(
        "CAN buses",
        f"selected {', '.join(selected)}; opened {', '.join(opened) if opened else 'none'}",
        ok=bool(opened),
    )
    for bus in selected:
        state = transport.states().get(bus)
        if state is None:
            continue
        detail = state.note
        if state.error:
            detail += f" ({state.error})"
        session.setup(f"CAN bus {bus} -> {state.spec.channel}", detail, ok=state.available)


def wait_for_can_after_reflash(session: rt.Session) -> None:
    """Drops anything queued while the target was being flashed or reset."""
    transport = session.transport
    if transport is None:
        return
    for bus in ALL_BUSES:
        if transport.is_open(bus):
            try:
                transport.flush(bus, 0.05)
            except rcan.CanError:
                pass


def sniff_stimulus_buses(session: rt.Session) -> None:
    transport = session.transport
    if transport is None:
        return
    buses = [bus for bus in ("CAN1", "CAN2", "CAN5", "CAN0") if transport.is_open(bus)]
    if not buses:
        return
    duration = session.config.timeout("quiet_bus_s")
    session.sniff = rt.sniff_buses(transport, buses, duration)
    for bus in buses:
        entry = session.sniff["buses"].get(bus, {})
        occupancy = entry.get("occupancy") or {}
        session.setup(
            f"bus occupancy {bus}",
            f"{entry.get('note', 'unknown')} after {duration:.1f} s; "
            f"{occupancy.get('frames', 0)} frame(s), "
            f"{occupancy.get('distinct_ids', 0)} distinct id(s)",
            ok=not session.sniff["conflicts"].get(bus),
        )
    for bus, conflict in session.sniff["conflicts"].items():
        session.setup(f"bus conflict on {bus}", conflict["reason"], ok=False)


# --------------------------------------------------------------------------------------------------
# Safety
# --------------------------------------------------------------------------------------------------


def safety_check(session: rt.Session, expected_critical: int) -> str:
    """Returns a reason to stop issuing state-changing commands, or an empty string."""
    if session.target is None or session.target.layout is None:
        return ""
    try:
        values = session.target.read()
    except Exception as exc:  # noqa: BLE001
        return f"lost the debugger connection: {exc}"

    mask = int(values.get("sys_faults", 0))
    unexpected = mask & rc.FAULT_CRITICAL_MASK & ~expected_critical
    if unexpected:
        return (
            "unexpected critical fault "
            f"0x{unexpected:08X} ({', '.join(rc.decode_fault_mask(unexpected))})"
        )

    state = int(values.get("state", -1))
    cont_state = int(values.get("cont_state", -1))
    neg = int(values.get("cont_neg", 0)) == 1
    pos = int(values.get("cont_pos", 0)) == 1
    if state == 2 and cont_state == 4 and not (neg and pos):
        return (
            "contactor mismatch: the supervisor is ACTIVE and Pack 1 reports RUN, but its "
            f"outputs are neg={int(neg)} pos={int(pos)}"
        )
    return ""


def teardown(session: rt.Session) -> None:
    """Best-effort return to a safe bench state: STANDBY with the contactors open."""
    transport = session.transport
    if transport is None or not transport.is_open("CAN0"):
        if session.extra.get("safety_abort"):
            session.setup("safe teardown", "not attempted: " + session.extra["safety_abort"], ok=False)
        return
    if session.extra.get("safety_abort"):
        session.setup(
            "safe teardown",
            "not attempted after an abort: " + session.extra["safety_abort"],
            ok=False,
        )
        return
    if session.target is None or session.target.layout is None:
        session.setup("safe teardown", "not attempted: no debugger session", ok=False)
        return

    try:
        values = session.target.read()
        if int(values.get("state", -1)) == 0:
            session.setup("safe teardown", "target is still in INIT; nothing to unwind", ok=True)
            return
        if int(values.get("state", -1)) == 1:
            session.setup(
                "safe teardown",
                "already STANDBY; Pack 1 contactor state "
                f"{int(values.get('cont_state', -1))}",
                ok=True,
            )
            return
        transport.send(
            "CAN0", rc.CAN0_RX_CONTROL_ID, bytes([rc.CMD_DISABLE, 0, 0, 0, 0, 0, 0, 0])
        )
        session.setup("safe teardown", "Disable command sent", ok=True)
        target = session.target
        final, elapsed, _ = target.read_until(
            lambda reading: int(reading.get("state", -1)) == 1
            and int(reading.get("cont_state", -1)) == 0,
            3.0,
            0.02,
        )
        ok = int(final.get("state", -1)) == 1 and int(final.get("cont_state", -1)) == 0
        session.setup(
            "safe teardown result",
            f"state {final.get('state')}, Pack 1 contactor {final.get('cont_state')} "
            f"after {elapsed:.2f} s",
            ok=ok,
        )
    except Exception as exc:  # noqa: BLE001 - teardown must never mask the real result
        session.setup("safe teardown", f"failed: {exc}", ok=False)


# --------------------------------------------------------------------------------------------------
# Main
# --------------------------------------------------------------------------------------------------


def build_session(args: argparse.Namespace) -> rt.Session:
    config = rc.NominalConfig.load(args.nominal_config)
    return rt.Session(args=args, config=config)


def run(args: argparse.Namespace, suite: Suite) -> rt.Session:
    session = build_session(args)
    session.suite = suite
    session.extra["requested"] = requested_ids(args)

    started = datetime.now(timezone.utc)
    suite.started_utc = started.strftime("%Y-%m-%d %H:%M:%S")

    # Identity of the host artifact, recorded before anything touches the target.
    elf_identity = ArtifactIdentity.from_path(args.elf)
    git = rc.git_info(rc.REPO)
    suite.firmware = {
        "elf": elf_identity.to_dict(),
        "git": git,
        "target_flash": {"section": ".pflash", "verification": "not performed"},
        "build_config": args.build_config,
    }
    suite.environment = {
        "Python": sys.version.split()[0],
        "Interpreter": sys.executable,
        "Host": f"{sys.platform}",
        "Plain Python executable": str(rc.REPO / ".venv" / "Scripts" / "python.exe"),
    }
    try:
        import can as _can

        suite.environment["python-can"] = getattr(_can, "__version__", "unknown")
    except Exception:  # noqa: BLE001
        suite.environment["python-can"] = "not installed"
    try:
        import pylink as _pylink

        suite.environment["pylink-square"] = getattr(_pylink, "__version__", "unknown")
    except Exception:  # noqa: BLE001
        suite.environment["pylink-square"] = "not installed"

    if not elf_identity.exists:
        suite.infra_failure = f"ELF not found: {args.elf}. Build it first."
        return session

    # Host stages -----------------------------------------------------------------------------
    host_stage(session, suite)

    # Bus wiring ------------------------------------------------------------------------------
    open_buses(session, suite)

    # Target ----------------------------------------------------------------------------------
    open_target(session, suite)

    suite.hardware = {
        "mcu": "S32K344",
        "probe": session.target.description if session.target else "not connected",
        "jlink": session.target.jlink_version if session.target else "n/a",
        "can": session.transport.describe() if session.transport else {},
    }
    if session.target is not None:
        verification = session.target.verify_flash()
        suite.firmware["target_flash"] = {
            "section": ".pflash",
            "verification": verification,
            "detail": session.target.flash_detail,
        }
        session.setup(
            "target .pflash vs ELF",
            verification,
            ok=verification == "matched",
        )
        if verification != "matched":
            suite.infra_failure = (
                "the image on the target does not match the ELF "
                f"({verification}); HIL results would describe the wrong firmware"
            )
            session.extra["jlink_reason"] = (
                "the debugger session was closed because the target image does not match the ELF"
            )
            session.extra["jlink_infra"] = True
            session.target.close()
            session.target = None
    else:
        session.setup(
            "target .pflash vs ELF",
            "not performed: no J-Link session",
            ok=False,
        )

    sniff_stimulus_buses(session)

    # Deterministic stimulus ------------------------------------------------------------------
    # Never take over a bus that someone else is already driving with the same ids: two
    # transmitters on one identifier produce bus errors and an ambiguous verdict.
    stimulus_buses = []
    for bus in ("CAN1", "CAN2"):
        if not session.bus_available(bus):
            continue
        if (session.sniff.get("conflicts") or {}).get(bus):
            session.setup(
                f"stimulus on {bus}",
                "not started: " + session.sniff["conflicts"][bus]["reason"],
                ok=False,
            )
            continue
        stimulus_buses.append(bus)

    stimulus = None
    if session.transport is not None and stimulus_buses:
        stimulus = rt.StimulusRunner(
            session.transport,
            rt.VafeSimulator(session.config),
            rt.VpackSimulator(session.config),
            period_s=session.config.stimulus_period_s,
            buses=tuple(stimulus_buses),
        ).start()
        session.stimulus = stimulus
        session.setup(
            "deterministic stimulus",
            f"{' and '.join(stimulus_buses)} receiving vAFE/vPACK bursts every "
            f"{session.config.stimulus_period_s * 1000:.0f} ms; "
            f"pack {session.config.pack_voltage_mv} mV, "
            f"current {session.config.pack_current_ma} mA, "
            f"cells {session.config.cell_voltage_mv} mV",
            ok=True,
        )
    else:
        session.setup(
            "deterministic stimulus",
            "not started: neither CAN1 nor CAN2 is available and conflict-free",
            ok=False,
        )

    wait_for_can_after_reflash(session)

    # Scenario -------------------------------------------------------------------------------
    try:
        for name, step, state_changing in SCENARIO:
            if state_changing and session.extra.get("safety_abort"):
                _publish_all(session, suite, _blocked_by_safety(session, name))
                continue
            try:
                _publish_all(session, suite, step(session))
            except rt.PrerequisiteSkip:
                continue
            except Exception as exc:  # noqa: BLE001 - one broken step must not kill the run
                _publish_all(session, suite, _step_crashed(session, name, exc))
                continue

            if state_changing and not session.extra.get("safety_abort"):
                reason = safety_check(session, _expected_critical_bits(name))
                if reason:
                    session.extra["safety_abort"] = reason
                    session.setup(f"safety stop after '{name}'", reason, ok=False)
    finally:
        if stimulus is not None:
            stimulus.stop()
            session.setup(
                "stimulus stopped",
                f"vAFE bursts {stimulus.vafe_bursts}, vPACK bursts {stimulus.vpack_bursts}"
                + (f", errors {stimulus.errors[:2]}" if stimulus.errors else ""),
                ok=not stimulus.errors,
            )
        teardown(session)
        if session.target is not None:
            session.target.close()
        if session.transport is not None:
            session.transport.close()

    suite.duration_s = (datetime.now(timezone.utc) - started).total_seconds()
    suite.requested = session.extra["requested"] or []
    suite.selection_active = bool(session.extra["requested"])

    suite.configuration = {
        "build_config": args.build_config,
        "elf": str(args.elf),
        "selected_buses": sorted(
            bus for bus, state in (session.transport.states().items() if session.transport else [])
            if state.selected
        ),
        "can_config_file": str(args.can_config),
        "nominal_config_file": str(args.nominal_config),
        "bus_map": {
            bus: {"interface": s.interface, "channel": s.channel, "bitrate": s.bitrate}
            for bus, s in sorted(session.can_specs.items())
        },
        "nominal": session.config.raw.get("nominal", {}),
        "timing": session.config.raw.get("timing", {}),
        "fault_injection": session.config.raw.get("fault_injection", {}),
        "flags": {
            "smoke": args.smoke,
            "no_build": args.no_build,
            "no_sil": args.no_sil,
            "no_flash": args.no_flash,
        },
    }
    return session


def _expected_critical_bits(name: str) -> int:
    """FAULT_CELL_OV is deliberately present once the scenario injects it."""
    if name.startswith("fault") or name == "disable":
        return 1 << rc.FAULT_CELL_OV_BIT
    return 0


def _blocked_by_safety(session: rt.Session, name: str) -> list[RegressionResult]:
    reason = session.extra.get("safety_abort", "safety stop")
    results = []
    for test_id in _ids_produced_by_name(name):
        if test_id in session.executed:
            continue
        results.append(
            RegressionResult.skip(
                test_id,
                test_id,
                f"state-changing scenario aborted: {reason}",
            )
        )
    return results


def _step_crashed(session: rt.Session, name: str, exc: Exception) -> list[RegressionResult]:
    results = []
    for test_id in _ids_produced_by_name(name):
        if test_id in session.executed:
            continue
        results.append(
            RegressionResult.infra(
                test_id,
                test_id,
                f"the '{name}' step raised {type(exc).__name__}: {exc}",
                area="Uncategorised",
            )
        )
    return results


_STEP_IDS: dict[str, tuple[str, ...]] = {
    "flash": (test_boot.FLASH_ID,),
    "boot": (test_boot.BOOT_ID,),
    "scheduler": (test_measurements.SCHED_ID,),
    "vafe measurement": (test_measurements.MEAS1_ID,),
    "vpack measurement": (test_measurements.MEAS2_ID,),
    "can0 output": (test_measurements.CAN1_ID, test_measurements.CAN2_ID),
    "standby": ("REG-SM-001",),
    "enable": ("REG-SM-002",),
    "contactor sequence": (test_state_machine.CONTACTOR_ID,),
    "fault injection": (test_fault_recovery.FAULT1_ID,),
    "fault hysteresis": (test_fault_recovery.FAULT2_ID,),
    "fault history": (test_fault_recovery.FAULT3_ID,),
    "disable": ("REG-SM-003",),
    "xcp connect": (test_xcp.XCP1_ID,),
    "xcp short upload": (test_xcp.XCP2_ID,),
}


def _ids_produced_by_name(name: str) -> tuple[str, ...]:
    return _STEP_IDS.get(name, ())


def main(argv: list[str] | None = None) -> int:
    # python-can logs driver chatter (PCAN uptime warnings) at WARNING.  Keeping it off
    # stderr keeps the output readable and stops PowerShell turning it into an error exit.
    import logging

    logging.getLogger("can").setLevel(logging.ERROR)

    args = parse_args(argv)
    if args.list_tests:
        for test_id in TEST_IDS:
            print(test_id)
        return 0

    suite = Suite()
    try:
        run(args, suite)
    except Exception as exc:  # noqa: BLE001 - always produce a report
        suite.infra_failure = suite.infra_failure or f"{type(exc).__name__}: {exc}"

    requested = requested_ids(args)
    if requested is not None:
        missing = [test_id for test_id in requested if suite.get(test_id) is None]
        for test_id in missing:
            suite.add(
                RegressionResult.skip(
                    test_id,
                    test_id,
                    "no result was produced for this id in this configuration",
                )
            )

    try:
        suite.write_markdown(args.report)
        suite.write_json(args.json)
    except OSError as exc:
        print(f"could not write the report: {exc}", file=sys.stderr)

    print(console_summary(suite, report_path=args.report))
    return suite.exit_code()


if __name__ == "__main__":
    sys.exit(main())
