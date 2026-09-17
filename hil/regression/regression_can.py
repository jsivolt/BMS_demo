"""CAN transport abstraction for the BMS_demo regression framework.

Test modules never talk to a vendor API.  They use the small surface defined here:

    send(bus, can_id, data)
    recv(bus, timeout_s)
    collect(bus, duration_s)

``python-can`` is imported defensively so this module - and the whole host-side test
suite - still import on a machine without it.  Nothing in this module knows anything
about BMS frame layouts; the payload codecs live in ``regression_common``.

Bus availability is reported, never assumed:

* not selected by ``--bus``           -> SKIP  (the user did not configure it)
* selected but the channel will not open -> INFRA ERROR (expected, yet broken)
"""

from __future__ import annotations

import threading
import time
from collections import deque
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, Iterable, Sequence

try:  # pragma: no cover - exercised by the guarded-import host test
    import can as _can
except Exception:  # noqa: BLE001 - deliberately broad: the module must still import
    _can = None


class CanError(RuntimeError):
    """Base class for CAN problems."""


class CanConfigError(CanError):
    """The configuration file or a --canN override is malformed."""


class CanBackendMissing(CanError):
    """python-can is not installed."""


class CanBusUnavailable(CanError):
    """The logical bus is not open in this run."""


class CanOperationFailed(CanError):
    """An open bus failed while sending or receiving."""


@dataclass(frozen=True)
class BusSpec:
    """One logical BMS bus and the physical channel behind it."""

    logical: str
    interface: str
    channel: str
    bitrate: int
    label: str = ""

    def to_dict(self) -> dict[str, object]:
        return {
            "interface": self.interface,
            "channel": self.channel,
            "bitrate": self.bitrate,
            "label": self.label,
        }


@dataclass
class BusState:
    """Runtime state of one logical bus.

    ``presence`` separates the two failure modes the report must never confuse:

    * ``absent``   - the channel is not on this machine, so a test that needs it SKIPs
    * ``occupied`` - the channel exists but will not open, which is an infrastructure error
    """

    spec: BusSpec
    selected: bool = False
    available: bool = False
    note: str = ""
    error: str = ""
    presence: str = "unselected"

    def to_dict(self) -> dict[str, object]:
        return {
            "interface": self.spec.interface,
            "channel": self.spec.channel,
            "bitrate": self.spec.bitrate,
            "label": self.spec.label,
            "selected": self.selected,
            "available": self.available,
            "presence": self.presence,
            "note": self.note,
            "error": self.error,
        }


@dataclass
class Frame:
    """One received classical CAN frame."""

    bus: str
    can_id: int
    data: bytes
    dlc: int
    t: float
    host_t: float = 0.0
    is_extended: bool = False

    def to_dict(self) -> dict[str, object]:
        return {
            "bus": self.bus,
            "can_id": f"0x{self.can_id:03X}",
            "dlc": self.dlc,
            "data": self.data.hex(" ").upper(),
            "t": round(self.t, 6),
        }


# --------------------------------------------------------------------------------------------------
# Configuration
# --------------------------------------------------------------------------------------------------


def parse_bus_override(text: str, logical: str) -> BusSpec:
    """Parses ``interface:channel:bitrate`` from a --canN option."""
    parts = text.split(":")
    if len(parts) != 3:
        raise CanConfigError(
            f"--{logical.lower()} expects 'interface:channel:bitrate', got {text!r}"
        )
    interface, channel, bitrate_text = (part.strip() for part in parts)
    if not interface or not channel:
        raise CanConfigError(f"--{logical.lower()} has an empty interface or channel: {text!r}")
    try:
        bitrate = int(bitrate_text)
    except ValueError as exc:
        raise CanConfigError(f"--{logical.lower()} bitrate is not an integer: {text!r}") from exc
    if bitrate <= 0:
        raise CanConfigError(f"--{logical.lower()} bitrate must be positive: {text!r}")
    return BusSpec(logical=logical, interface=interface, channel=channel, bitrate=bitrate)


def load_bus_specs(
    config_path: Path | None,
    overrides: dict[str, str] | None = None,
) -> "dict[str, BusSpec]":
    """Loads the logical-bus -> physical-channel map.

    ``overrides`` maps a logical bus name to an ``interface:channel:bitrate`` string and
    wins over the file.  A missing file is only an error when an override does not cover
    every requested bus, so ``--bus CAN0 --can0 ...`` works with no file at all.
    """
    import json

    specs: dict[str, BusSpec] = {}
    if config_path is not None and config_path.is_file():
        try:
            raw = json.loads(config_path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as exc:
            raise CanConfigError(f"{config_path} is not valid JSON: {exc}") from exc
        buses = raw.get("buses")
        if not isinstance(buses, dict) or not buses:
            raise CanConfigError(f"{config_path} has no non-empty 'buses' object")
        for logical, entry in buses.items():
            if not isinstance(entry, dict):
                raise CanConfigError(f"{config_path}: bus {logical} is not an object")
            missing = [k for k in ("interface", "channel", "bitrate") if k not in entry]
            if missing:
                raise CanConfigError(
                    f"{config_path}: bus {logical} is missing {', '.join(missing)}"
                )
            try:
                bitrate = int(entry["bitrate"])
            except (TypeError, ValueError) as exc:
                raise CanConfigError(
                    f"{config_path}: bus {logical} bitrate is not an integer"
                ) from exc
            specs[logical.upper()] = BusSpec(
                logical=logical.upper(),
                interface=str(entry["interface"]),
                channel=str(entry["channel"]),
                bitrate=bitrate,
                label=str(entry.get("label", "")),
            )
    elif config_path is not None and overrides is None:
        raise CanConfigError(f"CAN configuration file not found: {config_path}")

    for logical, text in (overrides or {}).items():
        specs[logical.upper()] = parse_bus_override(text, logical.upper())

    if not specs:
        raise CanConfigError("no CAN buses are configured (no file and no --canN override)")
    return specs


def detect_available_channels(interface: str = "pcan") -> list[dict]:
    """Best-effort enumeration of the channels the driver currently exposes.

    Used to tell 'the channel is not plugged in' (a missing prerequisite) apart from
    'the channel exists but will not open' (an infrastructure error).  A backend that
    cannot enumerate returns an empty list rather than raising.
    """
    if _can is None:
        raise CanBackendMissing("python-can is not installed")
    try:
        return list(_can.detect_available_configs(interfaces=[interface]))
    except Exception:  # noqa: BLE001 - enumeration is advisory only
        return []


_CHANNEL_CACHE: dict[str, "set[str] | None"] = {}


def enumerated_channels(interface: str) -> "set[str] | None":
    """Channel names the driver reports, or None when enumeration is unsupported."""
    if interface in _CHANNEL_CACHE:
        return _CHANNEL_CACHE[interface]
    entries = detect_available_channels(interface)
    if not entries:
        # An empty result is ambiguous: it can mean "nothing plugged in" or "this backend
        # cannot enumerate".  Either way the channel cannot be shown to be present.
        _CHANNEL_CACHE[interface] = set()
        return _CHANNEL_CACHE[interface]
    names = {str(entry.get("channel")) for entry in entries if entry.get("channel")}
    _CHANNEL_CACHE[interface] = names
    return names


# --------------------------------------------------------------------------------------------------
# Transports
# --------------------------------------------------------------------------------------------------


class Transport:
    """Interface the scenario modules program against."""

    name = "abstract"

    def states(self) -> dict[str, BusState]:
        raise NotImplementedError

    def is_open(self, bus: str) -> bool:
        raise NotImplementedError

    def send(self, bus: str, can_id: int, data: bytes) -> None:
        raise NotImplementedError

    def recv(self, bus: str, timeout_s: float = 0.0) -> Frame | None:
        raise NotImplementedError

    def collect(
        self,
        bus: str,
        duration_s: float,
        predicate: Callable[[Frame], bool] | None = None,
    ) -> list[Frame]:
        deadline = time.monotonic() + max(0.0, duration_s)
        out: list[Frame] = []
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return out
            frame = self.recv(bus, min(0.05, remaining))
            if frame is None:
                continue
            if predicate is None or predicate(frame):
                out.append(frame)
            if predicate is not None and len(out) and time.monotonic() >= deadline:
                return out

    def close(self) -> None:
        raise NotImplementedError

    def describe(self) -> dict[str, dict]:
        return {bus: state.to_dict() for bus, state in self.states().items()}

    def __enter__(self) -> "Transport":
        return self

    def __exit__(self, *exc: object) -> None:
        self.close()


class NullTransport(Transport):
    """Used when no CAN hardware is available at all.

    Every operation raises ``CanBusUnavailable`` so a scenario step cannot silently
    'succeed' without a bus.
    """

    name = "none"

    def __init__(self, specs: "dict[str, BusSpec] | None" = None, reason: str = "") -> None:
        self.reason = reason or "no CAN transport available"
        self._states = {
            bus: BusState(spec=spec, selected=False, available=False, note=self.reason)
            for bus, spec in (specs or {}).items()
        }

    def states(self) -> dict[str, BusState]:
        return self._states

    def is_open(self, bus: str) -> bool:
        return False

    def _fail(self, bus: str) -> None:
        raise CanBusUnavailable(f"{bus}: {self.reason}")

    def send(self, bus: str, can_id: int, data: bytes) -> None:
        self._fail(bus)

    def recv(self, bus: str, timeout_s: float = 0.0) -> Frame | None:
        self._fail(bus)

    def close(self) -> None:
        return None


class CanTransport(Transport):
    """``python-can`` transport with one channel open per selected logical bus."""

    name = "python-can"

    def __init__(
        self,
        specs: "dict[str, BusSpec]",
        selected: Iterable[str] | None = None,
        receive_own_messages: bool = False,
    ) -> None:
        self._specs = dict(specs)
        self._selected = {bus.upper() for bus in selected} if selected else set()
        self._buses: dict[str, "object"] = {}
        self._states: dict[str, BusState] = {}
        self._lock = threading.Lock()
        self._receive_own = receive_own_messages

    # -- lifecycle ---------------------------------------------------------------------------------

    def open(self) -> None:
        """Tries to open every selected bus.  Never raises for one bad channel."""
        if _can is None:
            raise CanBackendMissing(
                "python-can is not installed. Run: "
                "<repo>\\.venv\\Scripts\\python.exe -m pip install -r "
                "hil/regression/requirements.txt"
            )
        for bus, spec in self._specs.items():
            selected = (not self._selected) or (bus in self._selected)
            state = BusState(spec=spec, selected=selected)
            if not selected:
                state.note = "not selected for this run"
                state.presence = "unselected"
                self._states[bus] = state
                continue
            try:
                handle = _can.Bus(
                    interface=spec.interface,
                    channel=spec.channel,
                    bitrate=spec.bitrate,
                    receive_own_messages=self._receive_own,
                )
            except Exception as exc:  # noqa: BLE001 - backend raises many types
                self._classify_failure(state, exc)
                self._states[bus] = state
                continue
            self._buses[bus] = handle
            state.available = True
            state.presence = "available"
            state.note = "open"
            self._states[bus] = state

    @staticmethod
    def _classify_failure(state: BusState, exc: Exception) -> None:
        """A channel that is simply not plugged in must SKIP, not report an infra error."""
        state.available = False
        try:
            known = enumerated_channels(state.spec.interface)
        except CanBackendMissing:
            known = None
        if known is not None and state.spec.channel not in known:
            state.presence = "absent"
            state.note = (
                f"channel {state.spec.channel} is not present on this machine "
                f"(driver reports: {', '.join(sorted(known)) if known else 'none'})"
            )
            state.error = ""
        else:
            state.presence = "occupied"
            state.error = f"{type(exc).__name__}: {exc}"
            state.note = "channel exists but could not be opened"

    def close(self) -> None:
        for handle in self._buses.values():
            try:
                handle.shutdown()
            except Exception:  # noqa: BLE001 - best effort teardown
                pass
        self._buses.clear()

    # -- introspection -----------------------------------------------------------------------------

    def states(self) -> dict[str, BusState]:
        return self._states

    def is_open(self, bus: str) -> bool:
        return bus.upper() in self._buses

    def open_buses(self) -> list[str]:
        return sorted(self._buses)

    def describe(self) -> dict[str, dict]:
        return {bus: state.to_dict() for bus, state in self._states.items()}

    def _handle(self, bus: str):
        handle = self._buses.get(bus.upper())
        if handle is None:
            state = self._states.get(bus.upper())
            why = state.note if state else "unknown logical bus"
            raise CanBusUnavailable(f"{bus}: {why}")
        return handle

    # -- traffic -----------------------------------------------------------------------------------

    def send(self, bus: str, can_id: int, data: bytes) -> None:
        handle = self._handle(bus)
        if len(data) > 8:
            raise CanOperationFailed(f"{bus}: classical CAN payload is at most 8 bytes")
        message = _can.Message(
            arbitration_id=can_id,
            data=bytes(data),
            is_extended_id=False,
            dlc=len(data),
        )
        try:
            handle.send(message)
        except Exception as exc:  # noqa: BLE001
            raise CanOperationFailed(f"{bus}: send 0x{can_id:03X} failed: {exc}") from exc

    def recv(self, bus: str, timeout_s: float = 0.0) -> Frame | None:
        handle = self._handle(bus)
        try:
            message = handle.recv(timeout=timeout_s)
        except Exception as exc:  # noqa: BLE001
            raise CanOperationFailed(f"{bus}: receive failed: {exc}") from exc
        if message is None:
            return None
        stamp = message.timestamp
        if stamp is None:
            stamp = time.perf_counter()
        payload = bytes(message.data or b"")
        return Frame(
            bus=bus.upper(),
            can_id=message.arbitration_id,
            data=payload,
            dlc=message.dlc if message.dlc is not None else len(payload),
            t=float(stamp),
            host_t=time.perf_counter(),
            is_extended=bool(message.is_extended_id),
        )

    def flush(self, bus: str, duration_s: float = 0.1) -> int:
        """Drains anything already queued so a later collect starts from a clean point."""
        dropped = 0
        deadline = time.monotonic() + duration_s
        while time.monotonic() < deadline:
            if self.recv(bus, 0.01) is not None:
                dropped += 1
            else:
                break
        return dropped


class FrameListener:
    """Background collector for one bus.

    Used for the passive bus sniffer and for gathering CAN0 evidence while the scenario
    keeps driving the target.
    """

    def __init__(self, transport: Transport, bus: str, name: str = "") -> None:
        self.transport = transport
        self.bus = bus.upper()
        self.name = name or f"{self.bus} listener"
        self._frames: deque[Frame] = deque(maxlen=200_000)
        self._thread: threading.Thread | None = None
        self._stop = threading.Event()
        self.error = ""

    def start(self) -> "FrameListener":
        self._thread = threading.Thread(target=self._run, name=self.name, daemon=True)
        self._thread.start()
        return self

    def _run(self) -> None:
        while not self._stop.is_set():
            try:
                frame = self.transport.recv(self.bus, 0.05)
            except CanError as exc:
                self.error = str(exc)
                return
            if frame is not None:
                # deque.append is atomic, so the reader never needs a lock.
                self._frames.append(frame)

    def frames(self) -> list[Frame]:
        return list(self._frames)

    def clear(self) -> None:
        self._frames.clear()

    def join(self, timeout: float = 2.0) -> None:
        """Waits for the collector thread to finish on its own (used by tests and teardown)."""
        thread = self._thread
        if thread is not None:
            thread.join(timeout=timeout)

    def stop(self) -> None:
        self._stop.set()
        if self._thread is not None:
            self._thread.join(timeout=2.0)
            self._thread = None


# --------------------------------------------------------------------------------------------------
# Helpers used by the scenario layer
# --------------------------------------------------------------------------------------------------


def summarise_occupancy(frames: Sequence[Frame]) -> dict[str, object]:
    """Groups frames by CAN id with counts and a short data summary.

    Used by the passive prerequisite that reports what is already on CAN1/CAN2 before the
    framework starts driving those buses.
    """
    per_id: dict[int, dict[str, object]] = {}
    for frame in frames:
        entry = per_id.setdefault(frame.can_id, {"can_id": frame.can_id, "count": 0, "last": ""})
        entry["count"] = int(entry["count"]) + 1
        entry["last"] = frame.data.hex(" ").upper()
    ordered = sorted(per_id.values(), key=lambda item: int(item["can_id"]))
    for entry in ordered:
        entry["can_id"] = f"0x{int(entry['can_id']):03X}"
    return {
        "frames": len(frames),
        "distinct_ids": len(ordered),
        "ids": ordered,
    }
