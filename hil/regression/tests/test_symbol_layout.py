"""The ELF symbol table the scenarios depend on.

Runs offline against Debug_FLASH/BMS_demo.elf through the S32DS gdb. No probe is
touched. Skipped when the toolchain or the ELF is not present, so the host suite still
collects on a bare checkout.
"""

from __future__ import annotations

import pytest

import regression_common as rc
import regression_can
import regression_target as rt

try:
    import hil_common
except Exception:  # noqa: BLE001 - probe the availability, do not fail collection
    hil_common = None


def _require_toolchain():
    if hil_common is None:
        pytest.skip("hil_common is not importable (pylink-square missing)")
    gdb = hil_common.DEFAULT_GDB
    elf = hil_common.DEFAULT_ELF
    if not gdb.exists():
        pytest.skip(f"S32DS gdb not found: {gdb}")
    if not elf.is_file():
        pytest.skip(f"ELF not built: {elf}")
    return gdb, elf


@pytest.fixture(scope="module")
def probed():
    gdb, elf = _require_toolchain()
    values, missing, raw = rt.probe_symbols(gdb, elf, rt.ALL_FIELDS)
    missing_keys = sorted({name.split(".", 1)[0] for name in missing})
    return gdb, elf, values, missing_keys, raw


@pytest.fixture(scope="module")
def layout(probed):
    gdb, elf, _, missing_keys, _ = probed
    resolved = [entry for entry in rt.ALL_FIELDS if entry[0] not in set(missing_keys)]
    return hil_common.build_layout(gdb, elf, resolved)


def test_every_declared_symbol_resolves_on_the_current_elf(probed):
    _, _, _, missing_keys, raw = probed
    assert missing_keys == [], f"these symbols no longer resolve: {missing_keys}\n{raw.strip()}"


def test_all_required_keys_are_present():
    assert rt.REQUIRED_KEYS, "the required set must not be empty"
    for key, _, _ in rt.CORE_FIELDS:
        assert key in rt.REQUIRED_KEYS


def test_every_struct_code_matches_the_real_object_size(layout):
    """build_layout already asserts this; the assertion here documents why it matters."""
    for key, (_, size, code) in layout.fields.items():
        assert size > 0
        assert code


def test_layout_reads_into_one_buffer_without_overlap(layout):
    """A shared sample cannot work if two fields land on the same offset."""
    offsets = list(layout.offsets.values())
    assert len(offsets) == len(set(offsets)), "two fields share a sample offset"
    assert layout.sample_size > 0
    assert len(layout.blocks) >= 1
    assert layout.sample_size < 4096, "the layout is too large for a fast poll"


def test_all_symbols_live_in_sram(probed):
    _, _, values, _, _ = probed
    for key, _, _ in rt.ALL_FIELDS:
        address = values[f"{key}.addr"]
        assert 0x20400000 <= address <= 0x2047FFFF, f"{key} is outside the S32K344 SRAM window"


def test_every_symbol_lies_inside_the_xcp_ram_whitelist(probed):
    """0x20400000-0x2047FFFF is what Xcp_IsValidRamRange accepts for reads."""
    _, _, values, _, _ = probed
    for key, _, _ in rt.ALL_FIELDS:
        assert 0x20400000 <= values[f"{key}.addr"] <= 0x2047FFFF


def test_the_keys_the_scenarios_rely_on_are_declared():
    keys = {key for key, _, _ in rt.ALL_FIELDS}
    for required in (
        "state",
        "led",
        "sys_faults",
        "sys_last",
        "pack1_faults",
        "pack1_last",
        "cont_state",
        "cont_neg",
        "cont_pos",
        "cont_pre",
        "vafe_min",
        "vafe_max",
        "vafe_delta",
        "vafe_valid",
        "vp_current",
        "vp_pack_mv",
        "vp_cur_valid",
        "vp_volt_valid",
        "bm_cell_valid",
        "bm_cur_valid0",
        "bm_valid",
        "sched_pending",
        "sched_pending_max",
        "sched_missed",
        "xcp_connected",
        "xcp_connect_count",
    ):
        assert required in keys, f"missing observation key: {required}"


def test_xcp_test_calibration_is_inside_the_downloadable_range(probed):
    """Xcp_IsWritableRange compares exact addresses; the address must at least be in SRAM."""
    _, _, values, _, _ = probed
    address = values["xcp_test_cal.addr"]
    assert 0x20400000 <= address <= 0x2047FFFF


def test_file_static_symbols_resolve_by_plain_name(probed):
    """The same mechanism fault_decode.gdb and debug_live.bat already depend on."""
    _, _, values, _, _ = probed
    assert values["cont_state.addr"] > 0
    assert values["bm_cell_valid.addr"] > 0
    assert values["sys_faults.addr"] > 0
    assert values["state_raw.addr"] > 0


def test_probe_symbols_reports_a_missing_symbol_instead_of_raising(probed):
    gdb, elf, _, _, _ = probed
    _, missing, _ = rt.probe_symbols(gdb, elf, [("nope", "g_ThisDoesNotExist", "I")])
    assert missing == ["nope.addr", "nope.size"]


def test_fault_snapshot_decodes_all_four_masks():
    snapshot = rc.fault_snapshot(
        {
            "sys_faults": 1 << rc.FAULT_CELL_OV_BIT,
            "sys_last": 1 << rc.FAULT_CELL_OV_BIT,
            "pack1_faults": 1 << 19,
            "pack1_last": 0,
            "pack2_faults": 0,
            "pack2_last": 0,
            "pack3_faults": 0,
            "pack3_last": 0,
        }
    )
    assert snapshot["current_system_faults"]["bits"] == ["FAULT_CELL_OV"]
    assert snapshot["last_system_faults"]["bits"] == ["FAULT_CELL_OV"]
    assert snapshot["current_pack_faults"][0]["bits"] == ["FAULT_PACK1_VOLTAGE_TIMEOUT"]
    assert snapshot["current_pack_faults"][1]["bits"] == []
    assert len(snapshot["last_pack_faults"]) == 3


def test_fault_snapshot_text_decodes_every_mask_symbolically():
    text = rc.fault_snapshot_text(rc.fault_snapshot({"sys_faults": 1 << 2, "sys_last": 0}))
    assert "current_system_faults : 0x00000004 FAULT_CELL_OV" in text
    assert "last_system_faults    : 0x00000000 (none)" in text
    assert "current_pack_faults[0]" in text
    assert "last_pack_faults[2]" in text


# --------------------------------------------------------------------------------------------------
# Simulators (pure - no bus involved)
# --------------------------------------------------------------------------------------------------


def test_vafe_simulator_starts_nominal_and_frames_match_the_config():
    config = rc.NominalConfig.load()
    simulator = rt.VafeSimulator(config)
    frames = dict(simulator.burst())
    assert set(frames) == {0x405, 0x401, 0x402, 0x403, 0x404}
    assert rc.read_u16le(frames[0x401], 0) == config.cell_voltage_mv
    assert frames[0x405][0] == 0


def test_vafe_simulator_counter_advances_every_burst():
    simulator = rt.VafeSimulator(rc.NominalConfig.load())
    assert simulator.burst()[0][1][0] == 0
    assert simulator.burst()[0][1][0] == 1
    assert simulator.counter == 2


def test_vafe_simulator_can_inject_one_bad_cell_and_restore_it():
    config = rc.NominalConfig.load()
    simulator = rt.VafeSimulator(config)
    simulator.set_cell(0, config.cell_ov_mv)
    frames = dict(simulator.burst())
    assert rc.read_u16le(frames[0x401], 0) == config.cell_ov_mv
    assert rc.read_u16le(frames[0x401], 2) == config.cell_voltage_mv

    simulator.restore_nominal()
    assert simulator.cells_mv == config.nominal_cells()


def test_vafe_simulator_rejects_an_out_of_range_cell():
    simulator = rt.VafeSimulator(rc.NominalConfig.load())
    with pytest.raises(ValueError):
        simulator.set_cell(16, 3700)
    with pytest.raises(ValueError):
        simulator.set_all([3700] * 15)


def test_vpack_simulator_uses_the_configured_nominal_values():
    config = rc.NominalConfig.load()
    simulator = rt.VpackSimulator(config)
    frames = dict(simulator.burst())
    assert rc.read_s32le(frames[0x410], 0) == config.pack_current_ma
    assert frames[0x410][7] == 0, "a non-zero status byte would raise FAULT_VPACK_DEVICE_FAULT"
    assert rc.read_u32le(frames[0x411], 0) == config.pack_voltage_mv
    assert rc.read_u32le(frames[0x411], 4) == config.bus_voltage_mv


def test_vpack_simulator_alive_counter_advances_by_one_modulo_sixteen():
    simulator = rt.VpackSimulator(rc.NominalConfig.load())
    seen = [dict(simulator.burst())[0x410][6] for _ in range(20)]
    for previous, current in zip(seen, seen[1:]):
        assert current == (previous + 1) & 0x0F


class _RecordingTransport:
    """Minimal transport that records what the stimulus runner sent."""

    def __init__(self, open_buses):
        self.sent = []
        self._open = set(open_buses)

    def is_open(self, bus):
        return bus.upper() in self._open

    def states(self):
        return {}

    def send(self, bus, can_id, data):
        self.sent.append((bus.upper(), can_id, bytes(data)))

    def recv(self, bus, timeout_s=0.0):
        return None


def test_stimulus_runner_sends_both_buses_and_reports_skipped_ones():
    import time as _time

    config = rc.NominalConfig.load()
    transport = _RecordingTransport({"CAN1"})
    runner = rt.StimulusRunner(
        transport,
        rt.VafeSimulator(config),
        rt.VpackSimulator(config),
        period_s=0.01,
        buses=("CAN1", "CAN2"),
    )
    runner.start()
    _time.sleep(0.06)
    runner.stop()

    sent_ids = {can_id for _, can_id, _ in transport.sent}
    assert 0x405 in sent_ids and 0x401 in sent_ids
    assert 0x410 not in sent_ids, "CAN2 is not open so nothing may be sent on it"
    summary = runner.summary()
    assert summary["vafe_bursts_sent"] > 0
    assert summary["vpack_bursts_sent"] == 0
    assert "CAN2" in summary["skipped_buses"]
    assert runner.errors == []


def test_stimulus_runner_records_a_send_failure_without_dying():
    import time as _time

    class Failing(_RecordingTransport):
        def send(self, bus, can_id, data):
            raise regression_can.CanOperationFailed(f"{bus}: send 0x{can_id:03X} failed: bus off")

    config = rc.NominalConfig.load()
    runner = rt.StimulusRunner(
        Failing({"CAN1"}),
        rt.VafeSimulator(config),
        rt.VpackSimulator(config),
        period_s=0.01,
        buses=("CAN1",),
    )
    runner.start()
    _time.sleep(0.05)
    runner.stop()
    assert runner.errors
    assert "bus off" in runner.errors[0]
