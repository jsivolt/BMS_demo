"""Payload codec tests - the frame layouts must match the production decoders.

Everything here is host-only: no probe, no CAN adapter.
"""

from __future__ import annotations

import pytest

import regression_common as rc


# --------------------------------------------------------------------------------------------------
# vAFE (CAN1)
# --------------------------------------------------------------------------------------------------


def test_vafe_header_frame_is_counter_then_status():
    blob = rc.vafe_header_frame(0x1A, 0x00)
    assert blob == b"\x1a\x00"
    assert len(blob) == 2


def test_vafe_header_counter_wraps_at_8_bits():
    assert rc.vafe_header_frame(256)[0] == 0
    assert rc.vafe_header_frame(-1)[0] == 0xFF


def test_vafe_cell_frame_is_four_uint16_little_endian():
    blob = rc.vafe_cell_frame([0x1234, 0x0001, 3700, 4260])
    assert len(blob) == 8
    # 0x1234 little-endian is 34 12, not 12 34.
    assert blob[0:2] == b"\x34\x12"
    assert blob[2:4] == b"\x01\x00"
    assert rc.read_u16le(blob, 4) == 3700
    assert rc.read_u16le(blob, 6) == 4260


def test_vafe_cell_frame_rejects_the_wrong_cell_count():
    with pytest.raises(ValueError):
        rc.vafe_cell_frame([3700, 3700, 3700])


def test_vafe_burst_order_is_header_then_cell_frames():
    burst = rc.vafe_burst([3700] * 16, counter=7)
    assert [can_id for can_id, _ in burst] == [0x405, 0x401, 0x402, 0x403, 0x404]


def test_vafe_burst_maps_cells_to_the_right_frame():
    cells = [1000 + index for index in range(16)]
    payloads = dict(rc.vafe_burst(cells, counter=0))

    assert rc.decode_cell_voltage(payloads[0x401]) == (1000, 1001, 1002, 1003)
    assert rc.decode_cell_voltage(payloads[0x402]) == (1004, 1005, 1006, 1007)
    assert rc.decode_cell_voltage(payloads[0x403]) == (1008, 1009, 1010, 1011)
    assert rc.decode_cell_voltage(payloads[0x404]) == (1012, 1013, 1014, 1015)


def test_vafe_burst_requires_all_sixteen_cells():
    with pytest.raises(ValueError):
        rc.vafe_burst([3700] * 15, counter=0)


def test_vafe_counter_sequence_rolls_forward():
    assert rc.vafe_counter_sequence(3, start=0xFE) == [0xFE, 0xFF, 0x00]


def test_all_nominal_cells_round_trip_to_3700_mv():
    payloads = dict(rc.vafe_burst([3700] * 16, counter=0))
    joined: list[int] = []
    for can_id in rc.CAN1_VAFE_CELL_IDS:
        joined.extend(rc.decode_cell_voltage(payloads[can_id]))
    assert joined == [3700] * 16


# --------------------------------------------------------------------------------------------------
# vPACK (CAN2)
# --------------------------------------------------------------------------------------------------


def test_vpack_current_frame_layout():
    blob = rc.vpack_current_frame(current_ma=-1500, shunt_uv=-25, alive_counter=3, status=0)
    assert len(blob) == 8
    assert rc.read_s32le(blob, 0) == -1500
    assert rc.read_s16le(blob, 4) == -25
    assert blob[6] == 3
    assert blob[7] == 0


def test_vpack_current_frame_encodes_negative_current_as_twos_complement():
    blob = rc.vpack_current_frame(current_ma=-1)
    assert blob[0:4] == b"\xff\xff\xff\xff"


def test_vpack_voltage_frame_layout():
    blob = rc.vpack_voltage_frame(59200, 59000)
    assert len(blob) == 8
    assert rc.read_u32le(blob, 0) == 59200
    assert rc.read_u32le(blob, 4) == 59000


def test_next_vpack_alive_wraps_at_four_bits():
    """Bms_Vpack.c masks with BMS_VPACK_ALIVE_MAX_VALUE = 15, not the 8-bit DBC field."""
    assert rc.next_vpack_alive(0) == 1
    assert rc.next_vpack_alive(14) == 15
    assert rc.next_vpack_alive(15) == 0


def test_vpack_alive_sequence_is_accepted_as_a_chain():
    counter = 0
    seen = []
    for _ in range(40):
        counter = rc.next_vpack_alive(counter)
        seen.append(counter)
    # Every consecutive pair must differ by exactly +1 modulo 16.
    for previous, current in zip(seen, seen[1:]):
        assert current == (previous + 1) & 0x0F


def test_vpack_burst_contains_both_ids():
    burst = rc.vpack_burst(0, 59200, 59000, alive_counter=0)
    assert [can_id for can_id, _ in burst] == [0x410, 0x411]


# --------------------------------------------------------------------------------------------------
# Fault mask decoding
# --------------------------------------------------------------------------------------------------


def test_decode_fault_mask_names_every_set_bit():
    mask = (1 << rc.FAULT_CELL_OV_BIT) | (1 << rc.FAULT_CELL_IMBALANCE_BIT)
    assert rc.decode_fault_mask(mask) == ["FAULT_CELL_OV", "FAULT_CELL_IMBALANCE"]


def test_decode_fault_mask_of_zero_is_empty():
    assert rc.decode_fault_mask(0) == []


def test_describe_fault_mask_reports_criticality():
    critical = rc.describe_fault_mask(1 << rc.FAULT_CELL_OV_BIT)
    assert critical["is_critical"] is True

    not_critical = rc.describe_fault_mask(1 << rc.FAULT_CELL_IMBALANCE_BIT)
    assert not_critical["is_critical"] is False
    assert not_critical["mask_hex"] == "0x00008000"


def test_critical_mask_constant_matches_the_source_definition():
    """FAULT_CRITICAL_MASK is bits 0-4, 6, 9-13, 16-21 as written in Fault_Manager.h."""
    expected = 0
    for bit in (0, 1, 2, 3, 4, 6, 9, 10, 11, 12, 13, 16, 17, 18, 19, 20, 21):
        expected |= 1 << bit
    assert expected == rc.FAULT_CRITICAL_MASK
    assert rc.FAULT_CRITICAL_MASK == 0x003F3E5F


# --------------------------------------------------------------------------------------------------
# CAN0 decoders
# --------------------------------------------------------------------------------------------------


def test_decode_0x300_status():
    data = bytes([1, 0x2C, 0x01, 0x00, 0x00, 0x00, 0x00, 0x06])
    decoded = rc.decode_0x300(data)
    assert decoded["state"] == 1
    assert decoded["state_name"] == "STANDBY"
    assert decoded["pack_temperature_dc"][0] == 300
    assert decoded["critical_fault"] is False
    assert decoded["any_fault"] is True


def test_decode_0x301_pack_status_scales_voltage_to_tenths():
    data = bytes([0xD0, 0x02, 0x00, 0x00, 0x00, 0x00, 0x51, 0x01])
    decoded = rc.decode_0x301(data)
    assert decoded["pack_voltage_dv"][0] == 0x02D0
    assert decoded["pack_voltage_v"][0] == pytest.approx(72.0)
    assert decoded["alive_counter"] == 5
    assert decoded["monitor_status"] == 1
    assert decoded["pack_voltage_valid"] is True


def test_decode_0x302_contactor_outputs_use_bit0_negative_bit1_positive_bit2_precharge():
    data = bytes([4, 0, 0, 0b101, 0x00, 0x00, 0x07, 0x00])
    decoded = rc.decode_0x302(data)
    assert decoded["states"] == (4, 0, 0)
    assert decoded["state_names"][0] == "RUN"
    assert decoded["pack1_negative"] is True
    assert decoded["pack1_positive"] is False
    assert decoded["pack1_precharge"] is True
    assert decoded["pack1_all_off"] is False
    assert decoded["alive_counter"] == 7


def test_decode_0x302_all_off_detects_the_safe_state():
    data = bytes([0, 0, 0, 0, 0, 0, 0, 0])
    decoded = rc.decode_0x302(data)
    assert decoded["pack1_all_off"] is True
    assert decoded["state_names"] == ("OFF", "OFF", "OFF")


def test_decode_0x303_and_0x304_split_live_masks():
    pack1 = 1 << rc.FAULT_CELL_OV_BIT
    system = 1 << 16
    live12 = rc.decode_0x303(rc.u32le(pack1) + rc.u32le(0))
    live3s = rc.decode_0x304(rc.u32le(0) + rc.u32le(system))
    assert live12["pack1"]["bits"] == ["FAULT_CELL_OV"]
    assert live12["pack2"]["mask"] == 0
    assert live3s["system"]["bits"] == ["FAULT_VPACK_COMM_TIMEOUT"]


def test_decode_0x309_and_0x30A_split_latched_masks():
    latched = 1 << rc.FAULT_CELL_OV_BIT
    last12 = rc.decode_0x309(rc.u32le(latched) + rc.u32le(0))
    last3s = rc.decode_0x30A(rc.u32le(0) + rc.u32le(latched))
    assert last12["last_pack1"]["bits"] == ["FAULT_CELL_OV"]
    assert last3s["last_system"]["bits"] == ["FAULT_CELL_OV"]


def test_decode_0x305_cell_summary():
    data = bytes([0x74, 0x0E, 0x74, 0x0E, 0x00, 0x00, 0x31, 0x01])
    decoded = rc.decode_0x305(data)
    assert decoded["min_cell_mv"] == 3700
    assert decoded["max_cell_mv"] == 3700
    assert decoded["delta_cell_mv"] == 0
    assert decoded["min_cell_index"] == 1
    assert decoded["max_cell_index"] == 3
    assert decoded["cell_voltage_valid"] is True
    assert decoded["cell_imbalance_fault"] is False


def test_decode_0x306_current_is_signed_tenths_of_an_amp():
    data = bytes([0x9C, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02])
    decoded = rc.decode_0x306(data)
    assert decoded["pack_current_da"][0] == -100
    assert decoded["pack_current_valid"] == (True, False, False)
    assert decoded["alive_counter"] == 2


def test_decode_0x307_pack_power_is_signed_watts():
    decoded = rc.decode_0x307(rc.s32le(-1234) + bytes([0x01, 0x00, 0x00, 0x03]))
    assert decoded["pack_power_w"] == -1234
    assert decoded["pack_power_valid"] is True
    assert decoded["alive_counter"] == 3


def test_decode_0x308_soc_status():
    decoded = rc.decode_0x308(bytes([0xD4, 0x02, 0x05, 0, 0, 0, 0, 0x09]))
    assert decoded["soc_x10"] == 724
    assert decoded["soc_valid"] is True
    assert decoded["soc_init_source"] == 2
    assert decoded["alive_counter"] == 9


def test_decode_0x30c_sop_limits():
    data = bytes([0x33, 0x03, 0x4D, 0x02, 0x00, 0x00, 0x02, 0x0A])
    decoded = rc.decode_0x30C(data)
    assert decoded["discharge_da"] == 819
    assert decoded["regen_da"] == 589
    assert decoded["charge_da"] == 0
    assert decoded["vlow_derate"] is False
    assert decoded["vhigh_derate"] is True
    assert decoded["alive_counter"] == 10


def test_decode_cells_from_frames_requires_all_four_frames():
    nominal = rc.NominalConfig.load().nominal_cells()
    frames = {
        0x310: rc.vafe_cell_frame(nominal[0:4]),
        0x311: rc.vafe_cell_frame(nominal[4:8]),
        0x312: rc.vafe_cell_frame(nominal[8:12]),
        0x313: rc.vafe_cell_frame(nominal[12:16]),
    }
    assert rc.decode_cells_from_frames(frames) == [3700] * 16

    del frames[0x312]
    with pytest.raises(KeyError):
        rc.decode_cells_from_frames(frames)


def test_every_expected_can0_id_has_a_name_and_the_periodic_set_is_exact():
    expected = set(range(0x300, 0x30D)) | set(range(0x310, 0x314))
    assert set(rc.CAN0_TX_IDS) == expected
    assert len(rc.CAN0_TX_IDS) == 17
    assert set(rc.CAN0_TX_NAMES) == expected
    # 0x400 is a CAN1 frame with a completely different meaning; it must never be listed.
    assert rc.CAN1_TEST_ID not in rc.CAN0_TX_IDS


# --------------------------------------------------------------------------------------------------
# XCP codecs
# --------------------------------------------------------------------------------------------------


def test_xcp_connect_request():
    assert rc.xcp_connect_request() == b"\xff\x00"


def test_xcp_short_upload_request_is_eight_bytes_and_little_endian():
    blob = rc.xcp_short_upload_request(4, 0x20400188)
    assert len(blob) == 8
    assert blob[0] == 0xF4
    assert blob[1] == 4
    assert blob[2] == 0
    assert blob[3] == 0
    assert blob[4:] == b"\x88\x01\x40\x20"


def test_xcp_short_upload_rejects_an_impossible_count():
    with pytest.raises(ValueError):
        rc.xcp_short_upload_request(0, 0x20400000)
    with pytest.raises(ValueError):
        rc.xcp_short_upload_request(8, 0x20400000)


def test_xcp_connect_response_fields():
    fields = rc.xcp_connect_fields(bytes([0xFF, 0x00, 0x00, 0x08, 0x08, 0x00, 0x10, 0x10]))
    assert fields is not None
    assert fields["max_cto"] == 8
    assert fields["max_dto"] == 8
    assert fields["protocol_version"] == 0x10
    assert fields["transport_version"] == 0x10


def test_xcp_error_response_is_recognised():
    parsed = rc.xcp_parse_response(bytes([0xFE, 0x22, 0, 0, 0, 0, 0, 0]))
    assert parsed["is_error"] is True
    assert parsed["error_code"] == rc.XCP_ERR_OUT_OF_RANGE


def test_xcp_connect_fields_rejects_a_negative_response():
    assert rc.xcp_connect_fields(bytes([0xFE, 0x22])) is None


# --------------------------------------------------------------------------------------------------
# Nominal configuration
# --------------------------------------------------------------------------------------------------


def test_nominal_config_loads_the_committed_defaults():
    config = rc.NominalConfig.load()
    assert config.cell_voltage_mv == 3700
    assert config.pack_current_ma == 0
    assert config.vpack_status == 0
    assert config.nominal_cells() == [3700] * 16
    assert config.can0_expected_period_ms == 100
    assert config.stimulus_period_s == pytest.approx(0.05)


def test_nominal_config_can_inject_one_out_of_range_cell():
    config = rc.NominalConfig.load()
    cells = config.stimulus_with_cell(config.fault_cell_index, config.cell_ov_mv)
    assert len(cells) == 16
    assert cells[config.fault_cell_index] == 4260
    assert all(value == 3700 for index, value in enumerate(cells) if index != config.fault_cell_index)


def test_nominal_config_rejects_a_cell_index_outside_the_pack():
    config = rc.NominalConfig.load()
    with pytest.raises(rc.ConfigError):
        config.stimulus_with_cell(16, 3700)


def test_missing_config_file_is_reported_clearly(tmp_path):
    with pytest.raises(rc.ConfigError):
        rc.load_json_config(tmp_path / "nope.json")


def test_malformed_config_file_is_reported_clearly(tmp_path):
    bad = tmp_path / "bad.json"
    bad.write_text("{not json", encoding="utf-8")
    with pytest.raises(rc.ConfigError):
        rc.load_json_config(bad)


def test_config_missing_a_required_section_is_reported_clearly(tmp_path):
    partial = tmp_path / "partial.json"
    partial.write_text('{"nominal": {}}', encoding="utf-8")
    with pytest.raises(rc.ConfigError):
        rc.load_json_config(partial, ("nominal", "stimulus"))
