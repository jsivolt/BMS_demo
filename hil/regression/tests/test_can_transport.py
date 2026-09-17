"""CAN transport configuration, availability reporting and the no-hardware path.

No CAN adapter is required: the tests either use a fake transport or check that an
unopenable channel is reported as an infrastructure problem instead of raising.
"""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

import pytest

import regression_can as rc

REG_DIR = Path(__file__).resolve().parents[1]


def write_config(tmp_path: Path, buses: dict) -> Path:
    path = tmp_path / "can_config.json"
    path.write_text(json.dumps({"buses": buses}), encoding="utf-8")
    return path


# --------------------------------------------------------------------------------------------------
# Configuration loading
# --------------------------------------------------------------------------------------------------


def test_the_committed_config_maps_all_four_logical_buses():
    specs = rc.load_bus_specs(REG_DIR / "can_config.json")
    assert set(specs) == {"CAN0", "CAN1", "CAN2", "CAN5"}
    assert specs["CAN0"].interface == "pcan"
    assert specs["CAN0"].bitrate == 500000
    assert specs["CAN1"].bitrate == 1000000
    assert specs["CAN2"].bitrate == 1000000
    assert specs["CAN5"].bitrate == 1000000


def test_a_spec_is_read_from_the_file(tmp_path):
    path = write_config(
        tmp_path,
        {"CAN0": {"interface": "pcan", "channel": "PCAN_USBBUS1", "bitrate": 500000}},
    )
    specs = rc.load_bus_specs(path)
    assert specs["CAN0"].channel == "PCAN_USBBUS1"
    assert specs["CAN0"].logical == "CAN0"


def test_an_override_beats_the_file(tmp_path):
    path = write_config(
        tmp_path,
        {"CAN0": {"interface": "pcan", "channel": "PCAN_USBBUS1", "bitrate": 500000}},
    )
    specs = rc.load_bus_specs(path, {"CAN0": "pcan:PCAN_USBBUS4:1000000"})
    assert specs["CAN0"].channel == "PCAN_USBBUS4"
    assert specs["CAN0"].bitrate == 1000000


def test_overrides_alone_are_enough_without_a_file():
    specs = rc.load_bus_specs(None, {"CAN5": "socketcan:can0:1000000"})
    assert specs["CAN5"].interface == "socketcan"
    assert specs["CAN5"].channel == "can0"


def test_a_missing_file_with_no_override_is_an_error(tmp_path):
    with pytest.raises(rc.CanConfigError):
        rc.load_bus_specs(tmp_path / "absent.json")


def test_malformed_json_is_an_infrastructure_error(tmp_path):
    path = tmp_path / "can_config.json"
    path.write_text("{oops", encoding="utf-8")
    with pytest.raises(rc.CanConfigError):
        rc.load_bus_specs(path)


def test_a_bus_without_a_channel_is_rejected(tmp_path):
    path = write_config(tmp_path, {"CAN0": {"interface": "pcan", "bitrate": 500000}})
    with pytest.raises(rc.CanConfigError) as excinfo:
        rc.load_bus_specs(path)
    assert "channel" in str(excinfo.value)


def test_a_non_integer_bitrate_is_rejected(tmp_path):
    path = write_config(
        tmp_path, {"CAN0": {"interface": "pcan", "channel": "PCAN_USBBUS1", "bitrate": "fast"}}
    )
    with pytest.raises(rc.CanConfigError):
        rc.load_bus_specs(path)


def test_an_empty_bus_map_is_rejected(tmp_path):
    path = write_config(tmp_path, {})
    with pytest.raises(rc.CanConfigError):
        rc.load_bus_specs(path)


def test_override_syntax_is_validated():
    spec = rc.parse_bus_override("pcan:PCAN_USBBUS2:1000000", "CAN1")
    assert (spec.interface, spec.channel, spec.bitrate) == ("pcan", "PCAN_USBBUS2", 1000000)

    with pytest.raises(rc.CanConfigError):
        rc.parse_bus_override("pcan:PCAN_USBBUS2", "CAN1")
    with pytest.raises(rc.CanConfigError):
        rc.parse_bus_override("pcan:PCAN_USBBUS2:notanumber", "CAN1")
    with pytest.raises(rc.CanConfigError):
        rc.parse_bus_override("pcan:PCAN_USBBUS2:-1", "CAN1")


# --------------------------------------------------------------------------------------------------
# Availability reporting
# --------------------------------------------------------------------------------------------------


def test_a_bus_that_is_not_selected_is_reported_as_such_not_as_a_failure():
    specs = {
        "CAN0": rc.BusSpec("CAN0", "pcan", "PCAN_USBBUS1", 500000),
        "CAN5": rc.BusSpec("CAN5", "pcan", "PCAN_USBBUS4", 1000000),
    }
    transport = rc.CanTransport(specs, selected=["CAN0"])
    transport.open()
    try:
        states = transport.states()
        assert states["CAN0"].selected is True
        assert states["CAN0"].spec.channel == "PCAN_USBBUS1"
        assert states["CAN5"].selected is False
        assert states["CAN5"].available is False
        assert states["CAN5"].note == "not selected for this run"
        assert states["CAN5"].error == ""
    finally:
        transport.close()


def test_a_channel_that_is_not_on_this_machine_is_classified_as_absent():
    """A bus that is simply not plugged in must SKIP, not report an infrastructure error."""
    specs = {"CAN0": rc.BusSpec("CAN0", "no-such-backend", "NOT_A_CHANNEL", 500000)}
    transport = rc.CanTransport(specs, selected=["CAN0"])
    transport.open()  # must not raise: one bad channel must not kill the run
    try:
        state = transport.states()["CAN0"]
        assert state.selected is True
        assert state.available is False
        assert state.presence == "absent"
        assert state.error == ""
        assert "not present" in state.note
        assert transport.is_open("CAN0") is False
    finally:
        transport.close()


def test_a_channel_that_exists_but_will_not_open_is_an_error(monkeypatch):
    """The channel is enumerated, so the failure is ours, not a missing prerequisite."""
    monkeypatch.setattr(rc, "enumerated_channels", lambda interface: {"PCAN_USBBUS3"})
    specs = {"CAN0": rc.BusSpec("CAN0", "no-such-backend", "PCAN_USBBUS3", 500000)}
    transport = rc.CanTransport(specs, selected=["CAN0"])
    transport.open()
    try:
        state = transport.states()["CAN0"]
        assert state.available is False
        assert state.presence == "occupied"
        assert state.error, "the failure reason has to be captured for the report"
        assert "could not be opened" in state.note
    finally:
        transport.close()


def test_presence_is_carried_into_the_report_mapping():
    specs = {"CAN0": rc.BusSpec("CAN0", "no-such-backend", "NOT_A_CHANNEL", 500000)}
    transport = rc.CanTransport(specs, selected=["CAN0"])
    transport.open()
    try:
        assert transport.describe()["CAN0"]["presence"] == "absent"
    finally:
        transport.close()


def test_describe_returns_the_mapping_the_report_needs():
    specs = {"CAN0": rc.BusSpec("CAN0", "no-such-backend", "PCAN_USBBUS1", 500000, "host bus")}
    transport = rc.CanTransport(specs, selected=["CAN0"])
    transport.open()
    try:
        described = transport.describe()["CAN0"]
        assert described["interface"] == "no-such-backend"
        assert described["channel"] == "PCAN_USBBUS1"
        assert described["bitrate"] == 500000
        assert described["label"] == "host bus"
        assert described["selected"] is True
    finally:
        transport.close()


def test_using_an_unopened_bus_raises_a_typed_error():
    specs = {"CAN5": rc.BusSpec("CAN5", "pcan", "PCAN_USBBUS4", 1000000)}
    transport = rc.CanTransport(specs, selected=["CAN0"])
    transport.open()
    try:
        with pytest.raises(rc.CanBusUnavailable):
            transport.send("CAN5", 0x600, b"\xff\x00")
        with pytest.raises(rc.CanBusUnavailable):
            transport.recv("CAN5", 0.0)
    finally:
        transport.close()


def test_an_unknown_logical_bus_is_rejected():
    transport = rc.CanTransport({"CAN0": rc.BusSpec("CAN0", "pcan", "X", 500000)}, selected=["CAN0"])
    with pytest.raises(rc.CanBusUnavailable):
        transport.send("CAN9", 0x300, b"\x00")


# --------------------------------------------------------------------------------------------------
# Null transport - the no-hardware path
# --------------------------------------------------------------------------------------------------


def test_null_transport_never_pretends_to_have_a_bus():
    transport = rc.NullTransport(reason="no CAN transport available")
    assert transport.states() == {}
    assert transport.is_open("CAN0") is False
    with pytest.raises(rc.CanBusUnavailable):
        transport.send("CAN0", 0x201, b"\x01")
    with pytest.raises(rc.CanBusUnavailable):
        transport.recv("CAN0", 0.1)


def test_null_transport_reports_the_configured_buses_as_unavailable():
    specs = {"CAN0": rc.BusSpec("CAN0", "pcan", "PCAN_USBBUS1", 500000)}
    transport = rc.NullTransport(specs, reason="python-can is not installed")
    state = transport.states()["CAN0"]
    assert state.available is False
    assert state.note == "python-can is not installed"


# --------------------------------------------------------------------------------------------------
# Frames, listeners and occupancy
# --------------------------------------------------------------------------------------------------


def test_frame_serialises_the_id_as_hex():
    frame = rc.Frame(bus="CAN0", can_id=0x300, data=b"\x01\x02", dlc=2, t=1.5)
    raw = frame.to_dict()
    assert raw["can_id"] == "0x300"
    assert raw["data"] == "01 02"
    assert raw["bus"] == "CAN0"


def test_summarise_occupancy_groups_by_id():
    frames = [
        rc.Frame("CAN1", 0x401, b"\x74\x0e" * 4, 8, 0.0),
        rc.Frame("CAN1", 0x405, b"\x01\x00", 2, 0.001),
        rc.Frame("CAN1", 0x401, b"\x74\x0e" * 4, 8, 0.002),
    ]
    summary = rc.summarise_occupancy(frames)
    assert summary["frames"] == 3
    assert summary["distinct_ids"] == 2
    assert summary["ids"][0]["can_id"] == "0x401"
    assert summary["ids"][0]["count"] == 2
    assert summary["ids"][1]["can_id"] == "0x405"


def test_summarise_occupancy_of_an_idle_bus_is_empty():
    summary = rc.summarise_occupancy([])
    assert summary == {"frames": 0, "distinct_ids": 0, "ids": []}


class FakeTransport(rc.Transport):
    """Feeds a fixed frame list, then nothing."""

    name = "fake"

    def __init__(self, frames):
        self._frames = list(frames)

    def states(self):
        return {}

    def is_open(self, bus):
        return True

    def send(self, bus, can_id, data):
        return None

    def recv(self, bus, timeout_s=0.0):
        return self._frames.pop(0) if self._frames else None


def test_frame_listener_collects_in_the_background():
    frames = [rc.Frame("CAN0", 0x300 + index, b"\x00", 1, float(index)) for index in range(25)]
    listener = rc.FrameListener(FakeTransport(frames), "CAN0").start()
    listener.join(timeout=3.0)
    listener.stop()
    collected = listener.frames()
    assert len(collected) == 25
    assert [frame.can_id for frame in collected] == [0x300 + index for index in range(25)]


def test_frame_listener_records_a_transport_error_instead_of_dying_silently():
    class Broken(FakeTransport):
        def recv(self, bus, timeout_s=0.0):
            raise rc.CanBusUnavailable("bus went away")

    listener = rc.FrameListener(Broken([]), "CAN0").start()
    listener.join(timeout=3.0)
    listener.stop()
    assert "bus went away" in listener.error


def test_frame_listener_stop_is_idempotent():
    listener = rc.FrameListener(FakeTransport([]), "CAN0").start()
    listener.stop()
    listener.stop()
    assert listener.frames() == []


def test_collect_stops_at_the_deadline():
    import time

    class Counting(rc.Transport):
        def __init__(self):
            self.count = 0

        def states(self):
            return {}

        def is_open(self, bus):
            return True

        def send(self, bus, can_id, data):
            return None

        def recv(self, bus, timeout_s=0.0):
            self.count += 1
            time.sleep(0.005)
            return rc.Frame(bus, 0x300, b"\x00", 1, time.perf_counter())

    transport = Counting()
    frames = transport.collect("CAN0", 0.05)
    assert frames
    assert all(frame.can_id == 0x300 for frame in frames)


def test_collect_applies_an_id_filter():
    frames = [
        rc.Frame("CAN0", 0x300, b"\x00", 1, 0.0),
        rc.Frame("CAN0", 0x301, b"\x00", 1, 0.0),
        rc.Frame("CAN0", 0x300, b"\x00", 1, 0.0),
    ]
    filtered = FakeTransport(frames).collect("CAN0", 0.01, predicate=lambda f: f.can_id == 0x300)
    assert [frame.can_id for frame in filtered] == [0x300, 0x300]


# --------------------------------------------------------------------------------------------------
# Import safety
# --------------------------------------------------------------------------------------------------


def test_the_pure_modules_import_without_python_can_or_pylink():
    """The host test suite must not need a probe or a CAN adapter to collect."""
    script = (
        "import sys\n"
        "sys.modules['can'] = None\n"
        "sys.modules['pylink'] = None\n"
        "import regression_common, regression_result, regression_can\n"
        "assert regression_can._can is None\n"
        "print('IMPORT_OK')\n"
    )
    completed = subprocess.run(
        [sys.executable, "-c", script],
        cwd=str(REG_DIR),
        capture_output=True,
        text=True,
        timeout=60,
    )
    assert completed.returncode == 0, completed.stderr
    assert "IMPORT_OK" in completed.stdout


def test_a_missing_backend_is_reported_as_a_typed_error():
    if rc._can is None:
        with pytest.raises(rc.CanBackendMissing):
            rc.CanTransport({}, selected=[]).open()
        return
    transport = rc.CanTransport({"CAN0": rc.BusSpec("CAN0", "socketcan", "can0", 500000)}, ["CAN0"])
    transport.open()
    try:
        # Whatever the platform does, opening must not raise and must be reported.
        assert "CAN0" in transport.states()
    finally:
        transport.close()
