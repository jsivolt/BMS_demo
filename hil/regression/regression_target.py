"""Target access for the regression framework.

Three concerns are kept apart:

* ``Target``            - J-Link observation of the running firmware through ELF symbols
* ``VafeSimulator`` / ``VpackSimulator`` - what the bench feeds CAN1 and CAN2
* ``sniff_buses``       - what is *already* on those buses before we take them over

``hil_common`` (and therefore pylink) is imported lazily so that this module can still
be imported, and its pure parts unit-tested, on a machine with no probe.
"""

from __future__ import annotations

import sys
import threading
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Callable, Iterable, Sequence

REG_DIR = Path(__file__).resolve().parent
HIL_DIR = REG_DIR.parent
for _path in (str(REG_DIR), str(HIL_DIR)):
    if _path not in sys.path:
        sys.path.insert(0, _path)

import regression_common as rc  # noqa: E402 - needs the sys.path setup above
from regression_can import (  # noqa: E402
    CanError,
    FrameListener,
    Transport,
    summarise_occupancy,
)


# --------------------------------------------------------------------------------------------------
# Symbol table
# --------------------------------------------------------------------------------------------------

#: (key, C expression, struct code).  Every entry was verified resolvable against
#: Debug_FLASH/BMS_demo.elf on 2026-09-16 - file-static symbols resolve by plain name,
#: which is the same mechanism fault_decode.gdb and debug_live.bat already rely on.
CORE_FIELDS: tuple[tuple[str, str, str], ...] = (
    # firmware identity / liveness
    ("state", "g_DebugBmsState", "B"),
    ("state_raw", "'Bms_StateMachine.c'::g_BmsState", "i"),
    ("led", "g_LedCounter", "I"),
    ("hf_hfsr", "g_HardFault_HFSR", "I"),
    ("hf_cfsr", "g_HardFault_CFSR", "I"),
    # scheduler diagnostics
    ("sched_pending", "g_BmsSchedulerPendingTicks", "I"),
    ("sched_pending_max", "g_BmsSchedulerPendingTicksMax", "I"),
    ("sched_missed", "g_BmsSchedulerMissedTickCount", "I"),
    # fault manager: live and latched, system and all three packs
    ("sys_faults", "g_SystemFaults", "I"),
    ("sys_last", "g_LastSystemFaults", "I"),
)

OPTIONAL_FIELDS: tuple[tuple[str, str, str], ...] = (
    # contactor context (file-static in Bms_Contactor.c)
    ("cont_state", "g_Contactor[0].state", "i"),
    ("cont_neg", "g_Contactor[0].output.negative", "B"),
    ("cont_pos", "g_Contactor[0].output.positive", "B"),
    ("cont_pre", "g_Contactor[0].output.precharge", "B"),
    ("cont_timer", "g_Contactor[0].stateTimerMs", "I"),
    # per-pack fault masks
    ("pack1_faults", "g_PackFaults[0]", "I"),
    ("pack2_faults", "g_PackFaults[1]", "I"),
    ("pack3_faults", "g_PackFaults[2]", "I"),
    ("pack1_last", "g_LastPackFaults[0]", "I"),
    ("pack2_last", "g_LastPackFaults[1]", "I"),
    ("pack3_last", "g_LastPackFaults[2]", "I"),
    # vAFE decoder state
    ("vafe_min", "g_BmsVafeData.MinCellVoltage_mV", "H"),
    ("vafe_max", "g_BmsVafeData.MaxCellVoltage_mV", "H"),
    ("vafe_delta", "g_BmsVafeData.DeltaCellVoltage_mV", "H"),
    ("vafe_counter", "g_BmsVafeData.MeasurementCounter", "B"),
    ("vafe_hdr_valid", "g_BmsVafeData.HeaderValid", "B"),
    ("vafe_valid", "g_BmsVafeData.DataValid", "B"),
    # vPACK decoder state
    ("vp_current", "g_BmsVpackData.PackCurrent_mA", "i"),
    ("vp_pack_mv", "g_BmsVpackData.PackVoltage_mV", "I"),
    ("vp_bus_mv", "g_BmsVpackData.BusVoltage_mV", "I"),
    ("vp_alive", "g_BmsVpackData.AliveCounter", "B"),
    ("vp_status", "g_BmsVpackData.Status", "B"),
    ("vp_cur_valid", "g_BmsVpackData.CurrentValid", "B"),
    ("vp_volt_valid", "g_BmsVpackData.VoltageValid", "B"),
    ("vp_alive_valid", "g_BmsVpackData.AliveValid", "B"),
    ("vp_valid", "g_BmsVpackData.Valid", "B"),
    # Battery_Monitor aggregate (file-static)
    ("bm_cell_valid", "g_BatteryData.CellVoltageValid", "B"),
    ("bm_cur_valid0", "g_BatteryData.PackCurrentValid[0]", "B"),
    ("bm_vp_cur_valid", "g_BatteryData.VpackCurrentValid", "B"),
    ("bm_vp_volt_valid", "g_BatteryData.VpackVoltageValid", "B"),
    ("bm_vp_alive_valid", "g_BatteryData.VpackAliveValid", "B"),
    ("bm_vp_valid", "g_BatteryData.VpackValid", "B"),
    ("bm_valid", "g_BatteryData.Valid", "B"),
    ("bm_packv1", "g_BatteryData.PackV1", "f"),
    ("bm_current0", "g_BatteryData.PackCurrent_mA[0]", "i"),
    ("bm_temp_valid0", "g_BatteryData.PackTemperatureValid[0]", "B"),
    ("bm_temp_summary_valid", "g_BatteryData.TemperatureSummaryValid", "B"),
    # XCP debug surface
    ("xcp_connected", "g_BmsXcpConnected", "B"),
    ("xcp_connect_count", "g_BmsXcpConnectCount", "I"),
    ("xcp_mta", "g_BmsXcpMta", "I"),
    ("xcp_test_cal", "g_BmsXcpTestCalibration", "I"),
    ("can5_rx_count", "g_BmsCan5RxCount", "I"),
)

ALL_FIELDS: tuple[tuple[str, str, str], ...] = CORE_FIELDS + OPTIONAL_FIELDS

#: Keys the scenarios cannot work without.
REQUIRED_KEYS: frozenset[str] = frozenset(key for key, _, _ in CORE_FIELDS)

MARKER = __import__("re").compile(r"@@([\w.]+)=(-?\d+)")


# --------------------------------------------------------------------------------------------------
# Tolerant symbol probing
# --------------------------------------------------------------------------------------------------


def probe_symbols(
    gdb: Path,
    elf: Path,
    fields: Sequence[tuple[str, str, str]],
) -> tuple[dict[str, int], list[str], str]:
    """Address/size probe that reports what is missing instead of failing on the first gap."""
    import hil_common

    exprs: dict[str, str] = {}
    for key, expression, _ in fields:
        exprs[f"{key}.addr"] = f"&({expression})"
        exprs[f"{key}.size"] = f"sizeof({expression})"
    out = hil_common.gdb_offline(
        gdb,
        elf,
        [f'printf "@@{name}=%lu\\n", (unsigned long)({expr})' for name, expr in exprs.items()],
    )
    values = {match.group(1): int(match.group(2)) for match in MARKER.finditer(out)}
    missing = sorted(name for name in exprs if name not in values)
    return values, missing, out


# --------------------------------------------------------------------------------------------------
# Target
# --------------------------------------------------------------------------------------------------


class TargetError(RuntimeError):
    """The target could not be prepared or interrogated."""


@dataclass
class Target:
    """A J-Link session plus the resolved ELF symbol layout."""

    args: Any
    fields: Sequence[tuple[str, str, str]] = ALL_FIELDS

    bench: Any = None
    layout: Any = None
    unavailable: dict[str, str] = field(default_factory=dict)
    addresses: dict[str, int] = field(default_factory=dict)
    flash_verification: str = "not performed"
    flash_detail: str = ""

    # -- lifecycle ---------------------------------------------------------------------------------

    def open(self) -> "Target":
        import hil_common

        if self.bench is None:
            hil_common.check_host(self.args)
            self.bench = hil_common.Bench(self.args)
            self.bench.__enter__()
        return self

    def close(self) -> None:
        if self.bench is not None:
            try:
                self.bench.close()
            finally:
                self.bench = None

    def __enter__(self) -> "Target":
        return self.open()

    def __exit__(self, *exc: object) -> None:
        self.close()

    @property
    def description(self) -> str:
        return self.bench.description if self.bench is not None else "not connected"

    @property
    def jlink_version(self) -> str:
        if self.bench is None:
            return "n/a"
        try:
            return f"J-Link DLL V{self.bench.jl.version}"
        except Exception:  # noqa: BLE001 - reporting only
            return "unknown"

    # -- symbols -----------------------------------------------------------------------------------

    def resolve(self) -> None:
        """Builds the readable layout; optional symbols that will not resolve are recorded."""
        import hil_common

        if self.bench is None:
            raise TargetError("resolve() needs an open J-Link session")

        values, missing, raw = probe_symbols(self.args.gdb, self.args.elf, self.fields)
        missing_keys = {name.split(".", 1)[0] for name in missing}
        absent_required = sorted(missing_keys & REQUIRED_KEYS)
        if absent_required:
            raise TargetError(
                "required ELF symbols are missing: " + ", ".join(absent_required) + f"\n{raw.strip()}"
            )

        resolved = [entry for entry in self.fields if entry[0] not in missing_keys]
        for key in missing_keys:
            expression = next(expr for name, expr, _ in self.fields if name == key)
            self.unavailable[key] = f"{expression} did not resolve against the ELF"

        try:
            self.layout = hil_common.build_layout(self.args.gdb, self.args.elf, list(resolved))
        except hil_common.SetupError as exc:
            raise TargetError(f"symbol layout failed: {exc}") from exc

        for key, (address, _, _) in self.layout.fields.items():
            self.addresses[key] = address

    def has(self, key: str) -> bool:
        return self.layout is not None and key in self.layout.fields

    def read(self) -> dict[str, Any]:
        if self.layout is None:
            raise TargetError("symbol layout has not been resolved yet")
        return dict(self.bench.read_layout(self.layout))

    def read_until(
        self,
        predicate: Callable[[dict[str, Any]], bool],
        timeout_s: float,
        period_s: float = 0.02,
    ) -> tuple[dict[str, Any], float, list[tuple[float, dict[str, Any]]]]:
        values, elapsed, samples = self.bench.read_until(
            self.layout, predicate, timeout_s, period_s
        )
        return dict(values), elapsed, [(stamp, dict(reading)) for stamp, reading in samples]

    def sample(self, duration_s: float, period_s: float = 0.01) -> list[tuple[float, dict[str, Any]]]:
        return [
            (stamp, dict(reading))
            for stamp, reading in self.bench.sample(self.layout, duration_s, period_s)
        ]

    def address_of(self, key: str) -> int:
        if key not in self.addresses:
            raise TargetError(f"{key} has no resolved address (unavailable or not requested)")
        return self.addresses[key]

    def reset_and_run(self) -> None:
        """Mandatory after every connect: device S32K344 fills RAM with 0xDEADBEEF."""
        self.bench.reset_and_run()

    def verify_flash(self, section: str = ".pflash") -> str:
        self.flash_verification = self.bench.verify_flash(self.args.gdb, self.args.elf, section)
        if self.flash_verification != "matched":
            self.flash_detail = self.flash_verification
        return self.flash_verification

    # -- evidence ----------------------------------------------------------------------------------

    def fault_snapshot(self) -> dict[str, Any]:
        """The four masks, decoded.  Missing optional symbols simply read as zero."""
        values = self.read()
        return rc.fault_snapshot(values)

    def observation_methods(self, keys: Iterable[str]) -> tuple[Any, ...]:
        """Which interface the given keys were read through, for the report."""
        from regression_result import Observation

        methods = set()
        for key in keys:
            if key.startswith("can") or key.startswith("xcp_"):
                methods.add(Observation.DEBUGGER_EXPORTED)
            elif key in self.unavailable:
                methods.add(Observation.DEBUGGER_STATIC)
            else:
                methods.add(Observation.DEBUGGER_STATIC)
        return tuple(sorted(methods, key=lambda item: item.value))


# --------------------------------------------------------------------------------------------------
# Deterministic bench stimulus
# --------------------------------------------------------------------------------------------------


@dataclass
class VafeSimulator:
    """Builds the CAN1 virtual-AFE frames from the configured nominal values."""

    config: rc.NominalConfig
    cells_mv: list[int] = field(default_factory=list)
    counter: int = 0

    def __post_init__(self) -> None:
        if not self.cells_mv:
            self.cells_mv = self.config.nominal_cells()

    def set_cell(self, index: int, value_mv: int) -> None:
        if not 0 <= index < rc.CELL_COUNT:
            raise ValueError(f"cell index {index} outside 0..{rc.CELL_COUNT - 1}")
        self.cells_mv[index] = int(value_mv)

    def set_all(self, cells_mv: Sequence[int]) -> None:
        if len(cells_mv) != rc.CELL_COUNT:
            raise ValueError(f"expected {rc.CELL_COUNT} cells")
        self.cells_mv = [int(value) for value in cells_mv]

    def restore_nominal(self) -> None:
        self.cells_mv = self.config.nominal_cells()

    def burst(self) -> list[tuple[int, bytes]]:
        frames = rc.vafe_burst(self.cells_mv, self.counter)
        self.counter = (self.counter + 1) & 0xFF
        return frames


@dataclass
class VpackSimulator:
    """Builds the CAN2 virtual pack-monitor frames, alive counter included."""

    config: rc.NominalConfig
    current_ma: int = 0
    shunt_uv: int = 0
    pack_mv: int = 0
    bus_mv: int = 0
    status: int = 0
    alive: int = 0

    def __post_init__(self) -> None:
        if not self.current_ma:
            self.current_ma = self.config.pack_current_ma
        if not self.pack_mv:
            self.pack_mv = self.config.pack_voltage_mv
        if not self.bus_mv:
            self.bus_mv = self.config.bus_voltage_mv
        if not self.shunt_uv:
            self.shunt_uv = self.config.shunt_voltage_uv
        self.status = self.config.vpack_status

    def burst(self) -> list[tuple[int, bytes]]:
        self.alive = rc.next_vpack_alive(self.alive)
        return rc.vpack_burst(
            self.current_ma,
            self.pack_mv,
            self.bus_mv,
            self.alive,
            self.shunt_uv,
            self.status,
        )


class StimulusRunner:
    """Feeds the configured CAN1 / CAN2 stimulus until stopped.

    Both buses are driven from one thread.  A bus that is not open is simply skipped, and
    the reason is kept so the report can state exactly which stimulus was active.
    """

    def __init__(
        self,
        transport: Transport,
        vafe: VafeSimulator,
        vpack: VpackSimulator,
        period_s: float,
        buses: Sequence[str] = ("CAN1", "CAN2"),
    ) -> None:
        self.transport = transport
        self.vafe = vafe
        self.vpack = vpack
        self.period_s = max(0.005, period_s)
        self.buses = tuple(bus.upper() for bus in buses)
        self.vafe_bursts = 0
        self.vpack_bursts = 0
        self.errors: list[str] = []
        self.skipped: dict[str, str] = {}
        self._stop = threading.Event()
        self._thread: threading.Thread | None = None

    def start(self) -> "StimulusRunner":
        for bus in self.buses:
            if not self.transport.is_open(bus):
                state = self.transport.states().get(bus)
                self.skipped[bus] = state.note if state else "bus is not open"
        self._thread = threading.Thread(target=self._run, name="bms-stimulus", daemon=True)
        self._thread.start()
        return self

    def _send(self, bus: str, frames: Sequence[tuple[int, bytes]]) -> None:
        for can_id, payload in frames:
            try:
                self.transport.send(bus, can_id, payload)
            except CanError as exc:
                self.errors.append(str(exc))
                return

    def _run(self) -> None:
        while not self._stop.is_set():
            started = time.monotonic()
            if "CAN1" in self.buses and "CAN1" not in self.skipped:
                self._send("CAN1", self.vafe.burst())
                self.vafe_bursts += 1
            if "CAN2" in self.buses and "CAN2" not in self.skipped:
                self._send("CAN2", self.vpack.burst())
                self.vpack_bursts += 1
            sleep_for = self.period_s - (time.monotonic() - started)
            self._stop.wait(max(0.0, sleep_for))

    def stop(self) -> None:
        self._stop.set()
        if self._thread is not None:
            self._thread.join(timeout=2.0)
            self._thread = None

    def summary(self) -> dict[str, Any]:
        return {
            "vafe_bursts_sent": self.vafe_bursts,
            "vpack_bursts_sent": self.vpack_bursts,
            "period_ms": round(self.period_s * 1000.0, 1),
            "cells_mv": list(self.vafe.cells_mv),
            "pack_voltage_mv": self.vpack.pack_mv,
            "bus_voltage_mv": self.vpack.bus_mv,
            "pack_current_ma": self.vpack.current_ma,
            "skipped_buses": self.skipped,
            "errors": self.errors[:5],
        }


# --------------------------------------------------------------------------------------------------
# Passive bus occupancy check
# --------------------------------------------------------------------------------------------------

#: CAN ids the framework must own exclusively to produce deterministic results.
OWNED_IDS: dict[str, tuple[int, ...]] = {
    "CAN1": (0x401, 0x402, 0x403, 0x404, 0x405),
    "CAN2": (0x410, 0x411),
    "CAN5": (0x600,),
}


def sniff_buses(
    transport: Transport,
    buses: Sequence[str],
    duration_s: float,
) -> dict[str, Any]:
    """Passively reports what is already transmitting on the given buses.

    The framework must be the only transmitter of the ids it owns, otherwise two nodes
    put different payloads under the same identifier and the results become ambiguous.
    """
    report: dict[str, Any] = {"duration_s": duration_s, "buses": {}, "conflicts": {}}
    for bus in buses:
        name = bus.upper()
        if not transport.is_open(name):
            state = transport.states().get(name)
            report["buses"][name] = {
                "available": False,
                "note": state.note if state else "bus is not open",
            }
            continue
        listener = FrameListener(transport, name, name=f"sniff-{name}").start()
        time.sleep(duration_s)
        listener.stop()
        frames = listener.frames()
        occupancy = summarise_occupancy(frames)
        report["buses"][name] = {
            "available": True,
            "note": "idle" if not frames else "traffic present",
            "error": listener.error,
            "occupancy": occupancy,
        }
        owned = OWNED_IDS.get(name, ())
        seen = {int(entry["can_id"], 16) for entry in occupancy["ids"]}
        conflicting = sorted(can_id for can_id in owned if can_id in seen)
        if conflicting:
            report["conflicts"][name] = {
                "ids": [f"0x{can_id:03X}" for can_id in conflicting],
                "reason": (
                    f"{name} already carries "
                    + ", ".join(f"0x{can_id:03X}" for can_id in conflicting)
                    + " from another transmitter; the regression must own these ids "
                    "to produce deterministic stimulus"
                ),
            }
    return report


# --------------------------------------------------------------------------------------------------
# Session
# --------------------------------------------------------------------------------------------------


class PrerequisiteSkip(Exception):
    """Raised by a scenario step when a prerequisite means the case cannot execute."""

    def __init__(self, reason: str) -> None:
        super().__init__(reason)
        self.reason = reason


@dataclass
class CapabilityProblem:
    """Why a case cannot run, and whether that is a missing prerequisite or a real fault.

    ``infra`` False -> SKIP.  The capability was not selected, or the hardware is simply
    not on the bench today.
    ``infra`` True  -> INFRA ERROR.  The capability was asked for and exists, but will not
    open.  That must never be reported as a DUT failure.
    """

    reason: str
    infra: bool = False


def probe_available(dll: Path, usb_sn: int | None = None) -> tuple[bool, str]:
    """Reports whether a J-Link probe is plugged in, without connecting to a target."""
    try:
        import pylink
    except Exception as exc:  # noqa: BLE001
        return False, f"pylink-square is not installed ({exc})"
    if not Path(dll).exists():
        return False, f"J-Link DLL not found: {dll}"
    try:
        library = pylink.Library(str(dll))
        link = pylink.JLink(lib=library)
        emulators = link.connected_emulators()
    except Exception as exc:  # noqa: BLE001
        return False, f"J-Link enumeration failed: {type(exc).__name__}: {exc}"
    if not emulators:
        return False, "no J-Link probe is connected"
    if usb_sn is not None and all(int(getattr(e, "SerialNumber", -1)) != usb_sn for e in emulators):
        return False, f"no J-Link probe with serial number {usb_sn} is connected"
    serials = ", ".join(str(getattr(e, "SerialNumber", "?")) for e in emulators)
    return True, f"probe(s) present: {serials}"


@dataclass
class Session:
    """Everything one regression run shares between scenario steps."""

    args: Any
    config: rc.NominalConfig
    can_specs: dict[str, Any] = field(default_factory=dict)

    target: Target | None = None
    transport: Transport | None = None
    stimulus: StimulusRunner | None = None

    suite: Any = None
    #: Every result this run produced, whether or not it was requested for the report.
    executed: dict[str, Any] = field(default_factory=dict)
    sniff: dict[str, Any] = field(default_factory=dict)
    can0_frames: list = field(default_factory=list)
    env: dict[str, Any] = field(default_factory=dict)
    config_snapshot: dict[str, Any] = field(default_factory=dict)
    extra: dict[str, Any] = field(default_factory=dict)

    # -- capability handling -----------------------------------------------------------------------

    def bus_available(self, bus: str) -> bool:
        if self.transport is None:
            return False
        return self.transport.is_open(bus.upper())

    def jlink_available(self) -> bool:
        return self.target is not None and self.target.layout is not None

    def capability_problem(self, caps: Iterable[str]) -> CapabilityProblem | None:
        """Returns why the required capabilities are not all present, or None."""
        for cap in caps:
            name = cap.upper()
            if name == "JLINK":
                if self.target is None:
                    return CapabilityProblem(
                        self.extra.get("jlink_reason", "no J-Link session"),
                        infra=bool(self.extra.get("jlink_infra", False)),
                    )
                if self.target.layout is None:
                    return CapabilityProblem(
                        "J-Link session has no resolved symbol layout",
                        infra=bool(self.extra.get("jlink_infra", False)),
                    )
                continue
            if self.bus_available(name):
                continue
            conflict = (self.sniff.get("conflicts") or {}).get(name)
            if conflict:
                # Someone else owns ids we must own exclusively: not a DUT problem, and
                # not a broken channel either - we simply cannot produce a clean result.
                return CapabilityProblem(conflict["reason"], infra=False)
            if self.transport is None:
                return CapabilityProblem(f"{name}: no CAN transport available", infra=False)
            state = self.transport.states().get(name)
            if state is None:
                return CapabilityProblem(f"{name}: not configured", infra=False)
            if state.error:
                return CapabilityProblem(
                    f"{name} ({state.spec.channel}): {state.error}", infra=True
                )
            return CapabilityProblem(f"{name}: {state.note}", infra=False)
        return None

    def blocked(
        self,
        test_id: str,
        name: str,
        caps: Sequence[str],
        expected: str,
        area: str,
        context: str = "",
    ) -> Any:
        """A ready-made SKIP or INFRA ERROR result when the capabilities are missing."""
        from regression_result import RegressionResult

        problem = self.capability_problem(caps)
        if problem is None:
            return None
        reason = f"{context}: {problem.reason}" if context else problem.reason
        if problem.infra:
            return RegressionResult.infra(
                test_id, name, reason, area=area, caps=caps, expected=expected
            )
        return RegressionResult.skip(
            test_id, name, reason, area=area, caps=caps, expected=expected
        )

    # -- prerequisite bookkeeping ------------------------------------------------------------------

    def status_of(self, test_id: str) -> Any:
        """Status of an earlier case, requested for the report or not."""
        result = self.executed.get(test_id)
        if result is not None:
            return result.status
        if self.suite is not None:
            reported = self.suite.get(test_id)
            if reported is not None:
                return reported.status
        return None

    def prerequisite_problem(self, test_ids: Iterable[str]) -> str | None:
        """Blocks dependants when an earlier case did not pass.

        A SKIP or INFRA ERROR upstream propagates as a SKIP here, so one broken
        prerequisite cannot turn into a cascade of meaningless failures.
        """
        from regression_result import Status

        for test_id in test_ids:
            status = self.status_of(test_id)
            if status is None:
                continue
            if status is Status.PASS or status is Status.XFAIL:
                continue
            if status is Status.FAIL:
                return f"Prerequisite {test_id} failed"
            if status is Status.ERROR:
                return f"Prerequisite {test_id} hit an infrastructure error"
            return f"Prerequisite {test_id} was skipped"
        return None

    def setup(self, step: str, outcome: str, ok: bool | None = None) -> None:
        if self.suite is not None:
            self.suite.record_setup(step, outcome, ok=ok)

    def fault_snapshot(self) -> dict[str, Any]:
        if self.target is None or self.target.layout is None:
            return {}
        try:
            return self.target.fault_snapshot()
        except Exception:  # noqa: BLE001 - evidence gathering must not break a step
            return {}
