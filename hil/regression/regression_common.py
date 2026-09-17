"""Pure helpers for the BMS_demo regression framework.

No hardware imports live here: no pylink, no python-can, no subprocess-heavy setup at
import time.  Everything in this module is exercised by the host-only test suite.

Contents
--------
* payload builders for the CAN1 virtual AFE and CAN2 virtual pack monitor
* decoders for every periodic CAN0 frame the firmware publishes
* the FAULT_* bit table and its symbolic decoder
* inter-arrival timing statistics for the CAN0 periodic check
* the two host-side regression stages, REG-BUILD-001 and REG-SIL-001
"""

from __future__ import annotations

import json
import os
import re
import shutil
import struct
import subprocess
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Iterable, Sequence

REPO = Path(__file__).resolve().parents[2]
REG_DIR = Path(__file__).resolve().parent
DEFAULT_CAN_CONFIG = REG_DIR / "can_config.json"
DEFAULT_NOMINAL_CONFIG = REG_DIR / "nominal_config.json"
REPORTS_DIR = REG_DIR / "reports"
SIL_REPORT_SNAPSHOT = REPORTS_DIR / "sil"

# --------------------------------------------------------------------------------------------------
# Firmware constants (mirrored from source - source is the authority, this is the decoder)
# --------------------------------------------------------------------------------------------------

#: src/control/Bms_Contactor.h
CONTACTOR_STATE_NAMES: dict[int, str] = {
    0: "OFF",
    1: "NEG_ON",
    2: "PRECHARGE",
    3: "POS_ON",
    4: "RUN",
    5: "OPENING",
    6: "FAULT",
}

#: src/app/Bms_StateMachine.h
BMS_STATE_NAMES: dict[int, str] = {
    0: "INIT",
    1: "STANDBY",
    2: "ACTIVE",
    3: "FAULT",
}

#: src/safety/Fault_Manager.h - bit index -> symbol
FAULT_BITS: dict[int, str] = {
    0: "FAULT_PACK_OV",
    1: "FAULT_PACK_UV",
    2: "FAULT_CELL_OV",
    3: "FAULT_CELL_UV",
    4: "FAULT_OVER_TEMP",
    5: "FAULT_UNDER_TEMP",
    6: "FAULT_AFE_COMM",
    7: "FAULT_CAN_TIMEOUT",
    8: "FAULT_SPI_TIMEOUT",
    9: "FAULT_PRECHARGE_TIMEOUT",
    10: "FAULT_CONTACTOR_FEEDBACK",
    11: "FAULT_CONTACTOR_WELD",
    12: "FAULT_OVER_CURRENT",
    13: "FAULT_TEMP_SENSOR",
    14: "FAULT_TEMP_DELTA",
    15: "FAULT_CELL_IMBALANCE",
    16: "FAULT_VPACK_COMM_TIMEOUT",
    17: "FAULT_VPACK_ALIVE_ERROR",
    18: "FAULT_VPACK_DEVICE_FAULT",
    19: "FAULT_PACK1_VOLTAGE_TIMEOUT",
    20: "FAULT_PACK_DISCHARGE_OC",
    21: "FAULT_PACK_CHARGE_OC",
}

#: src/safety/Fault_Manager.h FAULT_CRITICAL_MASK
FAULT_CRITICAL_MASK = 0x003F3E5F

#: Bits that no code path in the current firmware ever raises.
FAULT_BITS_NEVER_RAISED = (
    "FAULT_PACK_OV",
    "FAULT_PACK_UV",
    "FAULT_SPI_TIMEOUT",
    "FAULT_CONTACTOR_FEEDBACK",
    "FAULT_CONTACTOR_WELD",
    "FAULT_OVER_CURRENT",
)

FAULT_CELL_OV_BIT = 2
FAULT_CELL_IMBALANCE_BIT = 15

# --------------------------------------------------------------------------------------------------
# CAN identities
# --------------------------------------------------------------------------------------------------

#: Every CAN0 id the firmware publishes every 100 ms (src/communication/Bms_Can.c).
CAN0_TX_IDS: tuple[int, ...] = tuple(range(0x300, 0x30D)) + tuple(range(0x310, 0x314))

CAN0_TX_NAMES: dict[int, str] = {
    0x300: "BMS_Status",
    0x301: "BMS_PackStatus",
    0x302: "BMS_ContactorStatus",
    0x303: "BMS_Pack12Fault",
    0x304: "BMS_Pack3SystemFault",
    0x305: "BMS_CellSummary",
    0x306: "BMS_PackCurrent",
    0x307: "BMS_PackPower",
    0x308: "BMS_SocStatus",
    0x309: "BMS_LastFault12",
    0x30A: "BMS_LastFault3System",
    0x30B: "BMS_CellSoc",
    0x30C: "BMS_SOPLimits",
    0x310: "BMS_CellVoltage_01_04",
    0x311: "BMS_CellVoltage_05_08",
    0x312: "BMS_CellVoltage_09_12",
    0x313: "BMS_CellVoltage_13_16",
}

CAN0_RX_DEBUG_ID = 0x200
CAN0_RX_CONTROL_ID = 0x201

CAN1_VAFE_HEADER_ID = 0x405
CAN1_VAFE_CELL_IDS: tuple[int, ...] = (0x401, 0x402, 0x403, 0x404)
CAN1_TEST_ID = 0x400

CAN2_VPACK_CURRENT_ID = 0x410
CAN2_VPACK_VOLTAGE_ID = 0x411

CAN5_XCP_RX_ID = 0x600
CAN5_XCP_TX_ID = 0x601

#: src/communication/xcp/Xcp_Cfg.h
XCP_CMD_CONNECT = 0xFF
XCP_CMD_GET_STATUS = 0xFD
XCP_CMD_SET_MTA = 0xF6
XCP_CMD_UPLOAD = 0xF5
XCP_CMD_SHORT_UPLOAD = 0xF4
XCP_CMD_DOWNLOAD = 0xF0
XCP_POSITIVE_RESPONSE = 0xFF
XCP_ERROR = 0xFE
XCP_ERR_OUT_OF_RANGE = 0x22

#: src/communication/Bms_Can_Cfg.h control command byte (CAN0 0x201 byte 0)
CMD_ENABLE = 0x01
CMD_DISABLE = 0x02
CMD_CLEAR_FAULT = 0x03
CMD_CLEAR_FAULT_HISTORY = 0x04

#: src/battery/vPACK/Bms_Vpack.h - the alive check is a 4-bit wrap, not the 8-bit DBC field.
VPACK_ALIVE_MASK = 0x0F

CELL_COUNT = 16
VAFE_CELLS_PER_FRAME = 4

# --------------------------------------------------------------------------------------------------
# Configuration
# --------------------------------------------------------------------------------------------------


class ConfigError(RuntimeError):
    """The configuration file is missing or malformed."""


def load_json_config(path: Path, required_sections: Sequence[str] = ()) -> dict[str, Any]:
    if not path.is_file():
        raise ConfigError(f"configuration file not found: {path}")
    try:
        raw = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise ConfigError(f"{path} is not valid JSON: {exc}") from exc
    if not isinstance(raw, dict):
        raise ConfigError(f"{path} must contain a JSON object")
    for section in required_sections:
        if section not in raw or not isinstance(raw[section], dict):
            raise ConfigError(f"{path} has no '{section}' object")
    return raw


@dataclass
class NominalConfig:
    """Baseline stimulus and timing policy."""

    raw: dict[str, Any] = field(default_factory=dict)

    @classmethod
    def load(cls, path: Path | None = None) -> "NominalConfig":
        target = path or DEFAULT_NOMINAL_CONFIG
        return cls(raw=load_json_config(target, ("nominal", "stimulus", "timing", "timeouts")))

    # -- stimulus ----------------------------------------------------------------------------------

    @property
    def cell_voltage_mv(self) -> int:
        return int(self.raw["nominal"]["cell_voltage_mv"])

    @property
    def pack_voltage_mv(self) -> int:
        return int(self.raw["nominal"]["pack_voltage_mv"])

    @property
    def bus_voltage_mv(self) -> int:
        return int(self.raw["nominal"]["bus_voltage_mv"])

    @property
    def pack_current_ma(self) -> int:
        return int(self.raw["nominal"]["pack_current_ma"])

    @property
    def shunt_voltage_uv(self) -> int:
        return int(self.raw["nominal"]["shunt_voltage_uv"])

    @property
    def vpack_status(self) -> int:
        return int(self.raw["nominal"]["vpack_status"])

    @property
    def temperature_c(self) -> int:
        return int(self.raw["nominal"]["temperature_c"])

    @property
    def stimulus_period_s(self) -> float:
        return float(self.raw["stimulus"]["period_ms"]) / 1000.0

    def nominal_cells(self) -> list[int]:
        return [self.cell_voltage_mv] * CELL_COUNT

    # -- timing ------------------------------------------------------------------------------------

    @property
    def can0_expected_period_ms(self) -> float:
        return float(self.raw["timing"]["can0_expected_period_ms"])

    @property
    def can0_period_tolerance_ms(self) -> float:
        return float(self.raw["timing"]["can0_period_tolerance_ms"])

    @property
    def can0_burst_threshold_ms(self) -> float:
        return float(self.raw["timing"]["can0_burst_threshold_ms"])

    @property
    def can0_min_coverage_ratio(self) -> float:
        return float(self.raw["timing"].get("can0_min_coverage_ratio", 0.8))

    @property
    def can_collect_s(self) -> float:
        return float(self.raw["timing"]["can_collect_s"])

    def timeout(self, key: str) -> float:
        return float(self.raw["timeouts"][key])

    # -- fault injection ---------------------------------------------------------------------------

    @property
    def fault_cell_index(self) -> int:
        return int(self.raw["fault_injection"]["cell_index"])

    @property
    def cell_ov_mv(self) -> int:
        return int(self.raw["fault_injection"]["cell_ov_mv"])

    @property
    def cell_ov_hold_mv(self) -> int:
        return int(self.raw["fault_injection"]["cell_ov_hold_mv"])

    @property
    def cell_ov_clear_mv(self) -> int:
        return int(self.raw["fault_injection"]["cell_ov_clear_mv"])

    def stimulus_with_cell(self, index: int, value_mv: int) -> list[int]:
        """Nominal cells with one cell replaced - used by the fault-injection cases."""
        cells = self.nominal_cells()
        if not 0 <= index < CELL_COUNT:
            raise ConfigError(f"cell index {index} is outside 0..{CELL_COUNT - 1}")
        cells[index] = int(value_mv)
        return cells


# --------------------------------------------------------------------------------------------------
# Little-endian payload primitives
# --------------------------------------------------------------------------------------------------


def u16le(value: int) -> bytes:
    return struct.pack("<H", int(value) & 0xFFFF)


def s16le(value: int) -> bytes:
    return struct.pack("<h", int(value))


def u32le(value: int) -> bytes:
    return struct.pack("<I", int(value) & 0xFFFFFFFF)


def s32le(value: int) -> bytes:
    return struct.pack("<i", int(value))


def read_u16le(blob: bytes, offset: int) -> int:
    return struct.unpack_from("<H", blob, offset)[0]


def read_s16le(blob: bytes, offset: int) -> int:
    return struct.unpack_from("<h", blob, offset)[0]


def read_u32le(blob: bytes, offset: int) -> int:
    return struct.unpack_from("<I", blob, offset)[0]


def read_s32le(blob: bytes, offset: int) -> int:
    return struct.unpack_from("<i", blob, offset)[0]


# --------------------------------------------------------------------------------------------------
# CAN1 virtual AFE builders
# --------------------------------------------------------------------------------------------------


def vafe_header_frame(counter: int, afe_status: int = 0) -> bytes:
    """0x405 - byte 0 rolling measurement counter, byte 1 AFE status (ignored by the decoder)."""
    return struct.pack("<BB", int(counter) & 0xFF, int(afe_status) & 0xFF)


def vafe_cell_frame(cells_mv: Sequence[int]) -> bytes:
    """Four cells as uint16 little-endian at 1 mV/bit."""
    if len(cells_mv) != VAFE_CELLS_PER_FRAME:
        raise ValueError(f"a vAFE cell frame carries exactly 4 cells, got {len(cells_mv)}")
    return b"".join(u16le(value) for value in cells_mv)


def vafe_burst(cells_mv: Sequence[int], counter: int) -> list[tuple[int, bytes]]:
    """One AFE measurement cycle in the order a real AFE sends it: header, then the frames.

    The firmware reads the five CAN1 mailboxes in fixed slot order (0x401..0x405), so the
    header is processed last inside one 100 ms pass and the cycle publishes on the next
    pass.  Holding the payload steady makes that deterministic.
    """
    if len(cells_mv) != CELL_COUNT:
        raise ValueError(f"expected {CELL_COUNT} cell voltages, got {len(cells_mv)}")
    burst: list[tuple[int, bytes]] = [(CAN1_VAFE_HEADER_ID, vafe_header_frame(counter))]
    for index, can_id in enumerate(CAN1_VAFE_CELL_IDS):
        start = index * VAFE_CELLS_PER_FRAME
        burst.append((can_id, vafe_cell_frame(cells_mv[start : start + VAFE_CELLS_PER_FRAME])))
    return burst


def vafe_counter_sequence(count: int, start: int = 0) -> list[int]:
    """Rolling 8-bit header counter values."""
    return [(int(start) + index) & 0xFF for index in range(count)]


# --------------------------------------------------------------------------------------------------
# CAN2 virtual pack monitor builders
# --------------------------------------------------------------------------------------------------


def next_vpack_alive(counter: int) -> int:
    """The exact sequence Bms_Vpack.c requires: previous + 1, wrapped to 4 bits."""
    return (int(counter) + 1) & VPACK_ALIVE_MASK


def vpack_current_frame(
    current_ma: int,
    shunt_uv: int = 0,
    alive_counter: int = 0,
    status: int = 0,
) -> bytes:
    """0x410 - bytes 0-3 sint32 LE current in mA, 4-5 sint16 LE shunt in uV, 6 alive, 7 status.

    ``status`` must stay 0 during nominal stimulus: Battery_Monitor raises
    FAULT_VPACK_DEVICE_FAULT on any non-zero value.
    """
    return (
        s32le(current_ma)
        + s16le(shunt_uv)
        + struct.pack("<BB", int(alive_counter) & 0xFF, int(status) & 0xFF)
    )


def vpack_voltage_frame(pack_mv: int, bus_mv: int) -> bytes:
    """0x411 - bytes 0-3 uint32 LE pack voltage in mV, 4-7 uint32 LE bus voltage in mV."""
    return u32le(pack_mv) + u32le(bus_mv)


def vpack_burst(
    current_ma: int,
    pack_mv: int,
    bus_mv: int,
    alive_counter: int,
    shunt_uv: int = 0,
    status: int = 0,
) -> list[tuple[int, bytes]]:
    return [
        (CAN2_VPACK_CURRENT_ID, vpack_current_frame(current_ma, shunt_uv, alive_counter, status)),
        (CAN2_VPACK_VOLTAGE_ID, vpack_voltage_frame(pack_mv, bus_mv)),
    ]


# --------------------------------------------------------------------------------------------------
# Fault decoding
# --------------------------------------------------------------------------------------------------


def decode_fault_mask(mask: int) -> list[str]:
    """Every set bit in a 32-bit fault mask, by symbol name."""
    mask = int(mask) & 0xFFFFFFFF
    return [name for bit, name in sorted(FAULT_BITS.items()) if mask & (1 << bit)]


def describe_fault_mask(mask: int) -> dict[str, Any]:
    mask = int(mask) & 0xFFFFFFFF
    return {
        "mask": mask,
        "mask_hex": f"0x{mask:08X}",
        "bits": decode_fault_mask(mask),
        "is_critical": bool(mask & FAULT_CRITICAL_MASK),
    }


def mask_bit(mask: int, bit: int) -> bool:
    return bool(int(mask) & (1 << int(bit)))


def fault_snapshot(values: dict[str, Any]) -> dict[str, Any]:
    """All four masks decoded symbolically from a target symbol reading.

    Live and latched are kept apart on purpose: a non-zero latched mask is history, not
    evidence of a live fault, and must never be treated as a regression failure by itself.
    """
    packs = []
    last_packs = []
    for index in (1, 2, 3):
        packs.append(describe_fault_mask(values.get(f"pack{index}_faults", 0)))
        last_packs.append(describe_fault_mask(values.get(f"pack{index}_last", 0)))
    return {
        "current_system_faults": describe_fault_mask(values.get("sys_faults", 0)),
        "last_system_faults": describe_fault_mask(values.get("sys_last", 0)),
        "current_pack_faults": packs,
        "last_pack_faults": last_packs,
    }


def fault_snapshot_text(snapshot: dict[str, Any]) -> str:
    """Compact one-line-per-mask rendering used inside a failing test's details."""

    def render(entry: dict[str, Any]) -> str:
        bits = entry.get("bits") or []
        return f"{entry.get('mask_hex', '?')} {'|'.join(bits) if bits else '(none)'}"

    lines = [
        f"current_system_faults : {render(snapshot['current_system_faults'])}",
        f"last_system_faults    : {render(snapshot['last_system_faults'])}",
    ]
    for index, entry in enumerate(snapshot["current_pack_faults"], start=1):
        lines.append(f"current_pack_faults[{index - 1}] : {render(entry)}")
    for index, entry in enumerate(snapshot["last_pack_faults"], start=1):
        lines.append(f"last_pack_faults[{index - 1}]    : {render(entry)}")
    return "\n".join(lines)


# --------------------------------------------------------------------------------------------------
# CAN0 decoders
# --------------------------------------------------------------------------------------------------


def decode_0x300(data: bytes) -> dict[str, Any]:
    """BMS_Status."""
    flags = data[7]
    return {
        "state": data[0],
        "state_name": BMS_STATE_NAMES.get(data[0], f"UNKNOWN({data[0]})"),
        "pack_temperature_dc": (
            read_s16le(data, 1),
            read_s16le(data, 3),
            read_s16le(data, 5),
        ),
        "critical_fault": bool(flags & 0x01),
        "any_fault": bool(flags & 0x02),
        "pack_temperature_valid": (bool(flags & 0x04), bool(flags & 0x08), bool(flags & 0x10)),
    }


def decode_0x301(data: bytes) -> dict[str, Any]:
    """BMS_PackStatus."""
    return {
        "pack_voltage_dv": (read_u16le(data, 0), read_u16le(data, 2), read_u16le(data, 4)),
        "pack_voltage_v": tuple(read_u16le(data, offset) / 10.0 for offset in (0, 2, 4)),
        "monitor_status": data[6] & 0x0F,
        "alive_counter": (data[6] >> 4) & 0x0F,
        "pack_voltage_valid": bool(data[7] & 0x01),
    }


def decode_0x302(data: bytes) -> dict[str, Any]:
    """BMS_ContactorStatus - byte 0-2 state, byte 3-5 outputs, byte 6 alive counter."""
    outputs = []
    for index in range(3):
        raw = data[3 + index]
        outputs.append(
            {
                "raw": raw,
                "negative": bool(raw & 0x01),
                "positive": bool(raw & 0x02),
                "precharge": bool(raw & 0x04),
            }
        )
    states = tuple(data[index] for index in range(3))
    return {
        "states": states,
        "state_names": tuple(CONTACTOR_STATE_NAMES.get(value, f"UNKNOWN({value})") for value in states),
        "outputs": tuple(outputs),
        # Pack 1 is the only pack with relays wired, so these are the ones that matter.
        "pack1_negative": outputs[0]["negative"],
        "pack1_positive": outputs[0]["positive"],
        "pack1_precharge": outputs[0]["precharge"],
        "pack1_all_off": not (outputs[0]["negative"] or outputs[0]["positive"] or outputs[0]["precharge"]),
        "alive_counter": data[6] & 0x0F,
    }


def decode_fault_pair(data: bytes) -> dict[str, int]:
    """Two uint32 LE masks, used by 0x303, 0x304, 0x309 and 0x30A."""
    return {"first": read_u32le(data, 0), "second": read_u32le(data, 4)}


def decode_0x303(data: bytes) -> dict[str, Any]:
    """Live pack 1 / pack 2 masks."""
    pair = decode_fault_pair(data)
    return {
        "pack1": describe_fault_mask(pair["first"]),
        "pack2": describe_fault_mask(pair["second"]),
    }


def decode_0x304(data: bytes) -> dict[str, Any]:
    """Live pack 3 / system masks."""
    pair = decode_fault_pair(data)
    return {
        "pack3": describe_fault_mask(pair["first"]),
        "system": describe_fault_mask(pair["second"]),
    }


def decode_0x309(data: bytes) -> dict[str, Any]:
    """Latched pack 1 / pack 2 masks."""
    pair = decode_fault_pair(data)
    return {
        "last_pack1": describe_fault_mask(pair["first"]),
        "last_pack2": describe_fault_mask(pair["second"]),
    }


def decode_0x30A(data: bytes) -> dict[str, Any]:
    """Latched pack 3 / system masks."""
    pair = decode_fault_pair(data)
    return {
        "last_pack3": describe_fault_mask(pair["first"]),
        "last_system": describe_fault_mask(pair["second"]),
    }


def decode_0x305(data: bytes) -> dict[str, Any]:
    """BMS_CellSummary - 1 mV/bit, indices are 0-based like the firmware's own."""
    return {
        "min_cell_mv": read_u16le(data, 0),
        "max_cell_mv": read_u16le(data, 2),
        "delta_cell_mv": read_u16le(data, 4),
        "min_cell_index": data[6] & 0x0F,
        "max_cell_index": (data[6] >> 4) & 0x0F,
        "cell_voltage_valid": bool(data[7] & 0x01),
        "cell_imbalance_fault": bool(data[7] & 0x02),
    }


def decode_0x306(data: bytes) -> dict[str, Any]:
    """BMS_PackCurrent - int16 LE, 0.1 A/bit, positive = charge."""
    return {
        "pack_current_da": (read_s16le(data, 0), read_s16le(data, 2), read_s16le(data, 4)),
        "pack_current_valid": (bool(data[6] & 0x01), bool(data[6] & 0x02), bool(data[6] & 0x04)),
        "alive_counter": data[7] & 0x0F,
    }


def decode_0x307(data: bytes) -> dict[str, Any]:
    """BMS_PackPower - int32 LE, 1 W/bit."""
    return {
        "pack_power_w": read_s32le(data, 0),
        "pack_power_valid": bool(data[4] & 0x01),
        "alive_counter": data[7] & 0x0F,
    }


def decode_0x308(data: bytes) -> dict[str, Any]:
    """BMS_SocStatus - uint16 LE, 0.1 %/bit."""
    return {
        "soc_x10": read_u16le(data, 0),
        "soc_valid": bool(data[2] & 0x01),
        "soc_init_source": (data[2] >> 1) & 0x07,
        "alive_counter": data[7] & 0x0F,
    }


def decode_0x30C(data: bytes) -> dict[str, Any]:
    """BMS_SOPLimits - uint16 LE, 0.1 A/bit."""
    return {
        "discharge_da": read_u16le(data, 0),
        "regen_da": read_u16le(data, 2),
        "charge_da": read_u16le(data, 4),
        "vlow_derate": bool(data[6] & 0x01),
        "vhigh_derate": bool(data[6] & 0x02),
        "thigh_derate": bool(data[6] & 0x04),
        "alive_counter": data[7] & 0x0F,
    }


def decode_cell_voltage(data: bytes) -> tuple[int, int, int, int]:
    """0x310-0x313 - four cells as uint16 LE at 1 mV/bit."""
    return (read_u16le(data, 0), read_u16le(data, 2), read_u16le(data, 4), read_u16le(data, 6))


def decode_cells_from_frames(frames: dict[int, bytes]) -> list[int]:
    """Collects all 16 cells from a 0x310-0x313 set.  Missing frames are an error."""
    cells: list[int] = []
    for can_id in (0x310, 0x311, 0x312, 0x313):
        if can_id not in frames:
            raise KeyError(f"no payload captured for CAN0 0x{can_id:03X}")
        cells.extend(decode_cell_voltage(frames[can_id]))
    return cells


CAN0_DECODERS = {
    0x300: decode_0x300,
    0x301: decode_0x301,
    0x302: decode_0x302,
    0x303: decode_0x303,
    0x304: decode_0x304,
    0x305: decode_0x305,
    0x306: decode_0x306,
    0x307: decode_0x307,
    0x308: decode_0x308,
    0x309: decode_0x309,
    0x30A: decode_0x30A,
    0x30C: decode_0x30C,
}


def latest_by_id(frames: Iterable[Any]) -> dict[int, bytes]:
    """Last payload seen per CAN id, from an iterable of objects with can_id/data."""
    out: dict[int, bytes] = {}
    for frame in frames:
        out[int(frame.can_id)] = bytes(frame.data)
    return out


# --------------------------------------------------------------------------------------------------
# XCP
# --------------------------------------------------------------------------------------------------


def xcp_connect_request(mode: int = 0x00) -> bytes:
    return bytes([XCP_CMD_CONNECT, mode & 0xFF])


def xcp_short_upload_request(count: int, address: int, extension: int = 0) -> bytes:
    """0xF4 with dlc 8: [cmd, count, reserved, ext, address LE32]."""
    if not 1 <= count <= 7:
        raise ValueError("SHORT_UPLOAD count must be 1..7")
    return (
        bytes([XCP_CMD_SHORT_UPLOAD, count & 0xFF, 0x00, extension & 0xFF])
        + u32le(address)
    )


def xcp_parse_response(data: bytes) -> dict[str, Any]:
    payload = bytes(data)
    if not payload:
        return {"pid": None, "is_error": False, "empty": True}
    pid = payload[0]
    if pid == XCP_ERROR:
        return {
            "pid": pid,
            "is_error": True,
            "error_code": payload[1] if len(payload) > 1 else None,
            "payload": payload,
        }
    return {"pid": pid, "is_error": False, "payload": payload}


def xcp_connect_fields(response: bytes) -> dict[str, int] | None:
    """Splits a CONNECT positive response into its documented fields."""
    if len(response) < 8 or response[0] != XCP_POSITIVE_RESPONSE:
        return None
    return {
        "resource": response[1],
        "comm_mode_basic": response[2],
        "max_cto": response[3],
        "max_dto": response[4],
        "reserved": response[5],
        "protocol_version": response[6],
        "transport_version": response[7],
    }


# --------------------------------------------------------------------------------------------------
# Timing statistics
# --------------------------------------------------------------------------------------------------


def percentile(sorted_values: Sequence[float], fraction: float) -> float:
    if not sorted_values:
        return 0.0
    if len(sorted_values) == 1:
        return float(sorted_values[0])
    position = fraction * (len(sorted_values) - 1)
    lower = int(position)
    upper = min(lower + 1, len(sorted_values) - 1)
    weight = position - lower
    return float(sorted_values[lower] * (1.0 - weight) + sorted_values[upper] * weight)


def inter_arrival_stats(
    timestamps: Sequence[float],
    expected_period_ms: float,
    tolerance_ms: float,
    burst_threshold_ms: float,
    coverage_min_ratio: float = 0.8,
) -> dict[str, Any]:
    """Inter-arrival statistics for one CAN id.

    Average-only checks hide the cycle-time collapse this test exists to catch, so the
    result always carries the raw short/long interval lists and an explicit burst flag.
    """
    stamps = sorted(float(value) for value in timestamps)
    stats: dict[str, Any] = {
        "frame_count": len(stamps),
        "interval_count": 0,
        "mean_period_ms": 0.0,
        "min_period_ms": 0.0,
        "max_period_ms": 0.0,
        "p95_period_ms": 0.0,
        "expected_period_ms": float(expected_period_ms),
        "tolerance_ms": float(tolerance_ms),
        "burst_threshold_ms": float(burst_threshold_ms),
        "window_ms": 0.0,
        "expected_frames": 0,
        "missing_frames": 0,
        "coverage_ratio": 0.0,
        "over_tolerance_count": 0,
        "short_interval_count": 0,
        "short_intervals_ms": [],
        "long_intervals_ms": [],
        "burst_detected": False,
    }
    if len(stamps) < 2:
        return stats

    intervals = [(stamps[index + 1] - stamps[index]) * 1000.0 for index in range(len(stamps) - 1)]
    ordered = sorted(intervals)
    window_ms = (stamps[-1] - stamps[0]) * 1000.0
    expected_frames = int(round(window_ms / expected_period_ms)) + 1 if expected_period_ms > 0 else len(stamps)

    short = [value for value in intervals if value < burst_threshold_ms]
    long_ones = [value for value in intervals if value > expected_period_ms + tolerance_ms]
    over = [value for value in intervals if abs(value - expected_period_ms) > tolerance_ms]

    stats.update(
        {
            "interval_count": len(intervals),
            "mean_period_ms": round(sum(intervals) / len(intervals), 3),
            "min_period_ms": round(min(intervals), 3),
            "max_period_ms": round(max(intervals), 3),
            "p95_period_ms": round(percentile(ordered, 0.95), 3),
            "window_ms": round(window_ms, 3),
            "expected_frames": expected_frames,
            "missing_frames": max(0, expected_frames - len(stamps)),
            "coverage_ratio": round(len(stamps) / expected_frames, 4) if expected_frames else 0.0,
            "over_tolerance_count": len(over),
            "short_interval_count": len(short),
            "short_intervals_ms": [round(value, 3) for value in short[:20]],
            "long_intervals_ms": [round(value, 3) for value in long_ones[:20]],
            "burst_detected": bool(short),
        }
    )
    return stats


def timing_verdict(stats: dict[str, Any], coverage_min_ratio: float = 0.8) -> list[str]:
    """Human-readable problems found by ``inter_arrival_stats``.  Empty means acceptable."""
    problems: list[str] = []
    expected = float(stats.get("expected_period_ms", 0.0))
    if stats.get("frame_count", 0) < 2:
        problems.append("fewer than two frames captured")
        return problems
    if stats.get("burst_detected"):
        problems.append(
            f"{stats['short_interval_count']} inter-arrival interval(s) below "
            f"{stats['burst_threshold_ms']} ms (shortest {stats['min_period_ms']} ms) - "
            "this is the periodic-traffic collapse pattern, not normal jitter"
        )
    if stats.get("missing_frames", 0) > 0:
        coverage = float(stats.get("coverage_ratio", 0.0))
        if coverage < coverage_min_ratio:
            problems.append(
                f"only {stats['frame_count']} of about {stats['expected_frames']} expected "
                f"frames arrived (coverage {coverage:.2f} < {coverage_min_ratio:.2f})"
            )
    if stats.get("over_tolerance_count", 0) > 0 and not stats.get("burst_detected"):
        problems.append(
            f"{stats['over_tolerance_count']} interval(s) outside {expected:.0f} ms "
            f"+/- {stats['tolerance_ms']:.0f} ms"
        )
    return problems


# --------------------------------------------------------------------------------------------------
# Repo / git
# --------------------------------------------------------------------------------------------------


def git_info(repo: Path = REPO) -> dict[str, Any]:
    def run(*args: str) -> str:
        try:
            proc = subprocess.run(
                ["git", *args],
                cwd=repo,
                capture_output=True,
                text=True,
                timeout=20,
            )
        except (OSError, subprocess.SubprocessError):
            return ""
        return proc.stdout.strip()

    # Do NOT strip the porcelain output: the first line starts with the two status
    # characters and a space, so trimming the block would shift that path by one column
    # (' M CHANGELOG.md' -> 'M CHANGELOG.md' -> 'HANGELOG.md').
    try:
        status = subprocess.run(
            ["git", "status", "--porcelain", "--untracked-files=no"],
            cwd=repo,
            capture_output=True,
            text=True,
            timeout=20,
        ).stdout
    except (OSError, subprocess.SubprocessError):
        status = ""
    files = [line[3:] for line in status.splitlines() if line.strip()]

    return {
        "sha": run("rev-parse", "HEAD"),
        "short_sha": run("rev-parse", "--short", "HEAD"),
        "dirty": bool(files),
        "dirty_files": files,
        "branch": run("rev-parse", "--abbrev-ref", "HEAD"),
    }


def tail(text: str, lines: int = 40) -> str:
    parts = [line for line in (text or "").splitlines() if line.strip()]
    return "\n".join(parts[-lines:])


# --------------------------------------------------------------------------------------------------
# Host stage: REG-BUILD-001
# --------------------------------------------------------------------------------------------------


@dataclass
class CommandOutcome:
    command: str
    returncode: int
    seconds: float
    stdout: str = ""
    stderr: str = ""


def run_command(
    args: Sequence[str],
    cwd: Path,
    timeout_s: float,
    env: dict[str, str] | None = None,
) -> CommandOutcome:
    started = time.monotonic()
    try:
        proc = subprocess.run(
            list(args),
            cwd=str(cwd),
            capture_output=True,
            text=True,
            timeout=timeout_s,
            env=env,
        )
    except subprocess.TimeoutExpired as exc:
        return CommandOutcome(
            command=" ".join(str(a) for a in args),
            returncode=124,
            seconds=time.monotonic() - started,
            stdout=(exc.stdout or "") if isinstance(exc.stdout, str) else "",
            stderr=f"timed out after {timeout_s:.0f} s",
        )
    except OSError as exc:
        return CommandOutcome(
            command=" ".join(str(a) for a in args),
            returncode=127,
            seconds=time.monotonic() - started,
            stderr=str(exc),
        )
    return CommandOutcome(
        command=" ".join(str(a) for a in args),
        returncode=proc.returncode,
        seconds=time.monotonic() - started,
        stdout=proc.stdout,
        stderr=proc.stderr,
    )


def elf_path_for(repo: Path, build_config: str = "Debug_FLASH") -> Path:
    return repo / build_config / "BMS_demo.elf"


def build_regression(
    repo: Path = REPO,
    build_config: str = "Debug_FLASH",
    timeout_s: float = 1200.0,
    enabled: bool = True,
) -> tuple[Any, dict[str, Any]]:
    """REG-BUILD-001 - build through the project's own build.bat and check the ELF.

    The Eclipse-generated makefile's ``clean`` target is never invoked: it is ``rm -rf``
    inside the config directory and destroys the CDT ``.args`` response files.
    """
    from regression_result import ArtifactIdentity, Evidence, RegressionResult, Status

    test_id = "REG-BUILD-001"
    name = f"{build_config} build"
    expected = (
        f"build.bat returns 0; {build_config}/BMS_demo.elf exists and is non-empty"
    )
    script = repo / "build.bat"
    evidence: dict[str, Any] = {}

    if not enabled:
        return (
            RegressionResult.skip(
                test_id,
                name,
                "--no-build was given",
                area="Build",
                expected=expected,
            ),
            evidence,
        )

    if not script.is_file():
        return (
            RegressionResult.infra(
                test_id,
                name,
                f"build script not found: {script}",
                area="Build",
                expected=expected,
            ),
            evidence,
        )

    started = time.monotonic()
    outcome = run_command(["cmd", "/c", "build.bat", build_config], repo, timeout_s)
    duration = time.monotonic() - started
    evidence["command"] = outcome.command
    evidence["returncode"] = outcome.returncode
    evidence["stdout_tail"] = tail(outcome.stdout, 30)
    evidence["stderr_tail"] = tail(outcome.stderr, 30)

    elf = elf_path_for(repo, build_config)
    identity = ArtifactIdentity.from_path(elf)
    evidence["elf"] = identity.to_dict()

    if outcome.returncode != 0:
        return (
            RegressionResult(
                test_id=test_id,
                name=name,
                status=Status.FAIL,
                duration_s=duration,
                expected=expected,
                observed=f"build.bat exited {outcome.returncode}",
                details=(
                    f"Command: {outcome.command}\n"
                    f"--- stdout (tail) ---\n{tail(outcome.stdout, 25)}\n"
                    f"--- stderr (tail) ---\n{tail(outcome.stderr, 25)}"
                ),
                area="Build",
                evidence=Evidence.HOST,
                diagnostics=evidence,
            ),
            evidence,
        )

    if not identity.exists:
        return (
            RegressionResult(
                test_id=test_id,
                name=name,
                status=Status.FAIL,
                duration_s=duration,
                expected=expected,
                observed=f"build.bat succeeded but {elf} was not produced",
                details=tail(outcome.stdout, 20),
                area="Build",
                evidence=Evidence.HOST,
                diagnostics=evidence,
            ),
            evidence,
        )

    if identity.size == 0:
        return (
            RegressionResult(
                test_id=test_id,
                name=name,
                status=Status.FAIL,
                duration_s=duration,
                expected=expected,
                observed=f"{elf} exists but is empty",
                area="Build",
                evidence=Evidence.HOST,
                diagnostics=evidence,
            ),
            evidence,
        )

    return (
        RegressionResult(
            test_id=test_id,
            name=name,
            status=Status.PASS,
            duration_s=duration,
            expected=expected,
            observed=(
                f"build.bat exited 0; {elf.name} {identity.size} bytes, "
                f"sha256 {identity.sha256[:16]}..."
            ),
            details=tail(outcome.stdout, 12),
            area="Build",
            observations=(),
            evidence=Evidence.HOST,
            diagnostics=evidence,
        ),
        evidence,
    )


# --------------------------------------------------------------------------------------------------
# Host stage: REG-SIL-001
# --------------------------------------------------------------------------------------------------

_SIL_REPORTS = (
    "TEST_REPORT.md",
    "reports/lib_interp.md",
    "reports/persistence.md",
    "reports/soc.md",
    "reports/sop.md",
)

_PYTEST_COUNT = re.compile(r"(\d+)\s+(passed|failed|xfailed|xpassed|skipped|error|errors|deselected)")
_PYTEST_DURATION = re.compile(r"in\s+([0-9.]+)s")


def parse_pytest_summary(text: str) -> dict[str, int]:
    """Counts from pytest's final summary line."""
    counts = {
        "passed": 0,
        "failed": 0,
        "xfailed": 0,
        "xpassed": 0,
        "skipped": 0,
        "errors": 0,
        "deselected": 0,
    }
    for line in reversed(text.splitlines()):
        if not line.strip():
            continue
        matches = _PYTEST_COUNT.findall(line)
        if not matches:
            continue
        for number, word in matches:
            key = "errors" if word in ("error", "errors") else word
            counts[key] += int(number)
        return counts
    return counts


def parse_pytest_duration(text: str) -> float:
    matches = _PYTEST_DURATION.findall(text)
    return float(matches[-1]) if matches else 0.0


def _snapshot_files(base: Path, relatives: Sequence[str]) -> dict[str, bytes]:
    snapshot: dict[str, bytes] = {}
    for relative in relatives:
        target = base / relative
        if target.is_file():
            snapshot[relative] = target.read_bytes()
    return snapshot


def _restore_files(base: Path, snapshot: dict[str, bytes]) -> None:
    for relative, blob in snapshot.items():
        target = base / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(blob)


def sil_regression(
    repo: Path = REPO,
    snapshot_dir: Path = SIL_REPORT_SNAPSHOT,
    timeout_s: float = 900.0,
    enabled: bool = True,
    collect_only: bool = False,
    python_executable: str | None = None,
) -> tuple[Any, dict[str, Any]]:
    """REG-SIL-001 - run the existing SIL build and pytest suite unchanged.

    The SIL ``conftest.py`` rewrites committed report files on every unfiltered run.
    Their bytes are snapshotted first, copied into ``hil/regression/reports/sil/`` as
    evidence, then restored so a regression run leaves the working tree clean.
    """
    from regression_result import Evidence, RegressionResult, Status

    test_id = "REG-SIL-001"
    name = "Existing SIL suite"
    expected = (
        "sil/build.py produces the SIL library and pytest reports no failures; "
        "the documented strict xfail stays an xfail"
    )
    evidence: dict[str, Any] = {}
    python = python_executable or sys.executable

    if not enabled:
        return (
            RegressionResult.skip(test_id, name, "--no-sil was given", area="SIL", expected=expected),
            evidence,
        )

    sil_dir = repo / "sil"
    if not sil_dir.is_dir():
        return (
            RegressionResult.infra(
                test_id, name, f"sil directory not found: {sil_dir}", area="SIL", expected=expected
            ),
            evidence,
        )

    if collect_only:
        outcome = run_command([python, "-m", "pytest", "--collect-only", "-q"], sil_dir, 300.0)
        evidence["collect_command"] = outcome.command
        evidence["collect_output"] = tail(outcome.stdout + outcome.stderr, 30)
        status = Status.PASS if outcome.returncode == 0 else Status.FAIL
        return (
            RegressionResult(
                test_id=test_id,
                name=name,
                status=status,
                duration_s=outcome.seconds,
                expected=expected,
                observed=f"collect-only exited {outcome.returncode}",
                details=tail(outcome.stdout + outcome.stderr, 25),
                area="SIL",
                evidence=Evidence.HOST,
                diagnostics=evidence,
            ),
            evidence,
        )

    snapshot = _snapshot_files(sil_dir, _SIL_REPORTS)
    evidence["report_files_snapshotted"] = sorted(snapshot)
    started = time.monotonic()

    try:
        build = run_command([python, "build.py"], sil_dir, timeout_s)
        evidence["build_command"] = build.command
        evidence["build_returncode"] = build.returncode
        combined_build = (build.stdout or "") + (build.stderr or "")
        evidence["build_tail"] = tail(combined_build, 30)

        if build.returncode != 0:
            duration = time.monotonic() - started
            compiler_missing = "No host C compiler found" in combined_build
            if compiler_missing:
                return (
                    RegressionResult.infra(
                        test_id,
                        name,
                        "sil/build.py cannot run: no host C compiler on this machine "
                        "(install MinGW-w64 or set SIL_CC)",
                        area="SIL",
                        expected=expected,
                        observed="sil/build.py exited without a compiler",
                        duration_s=duration,
                    ),
                    evidence,
                )
            return (
                RegressionResult.infra(
                    test_id,
                    name,
                    f"sil/build.py exited {build.returncode}; the SIL library could not be built",
                    area="SIL",
                    expected=expected,
                    observed=f"sil/build.py exited {build.returncode}",
                    duration_s=duration,
                ),
                evidence,
            )

        tests = run_command(
            [python, "-m", "pytest", "-q", "--tb=short", "-p", "no:cacheprovider"],
            sil_dir,
            timeout_s,
        )
        duration = time.monotonic() - started
        combined = (tests.stdout or "") + (tests.stderr or "")
        counts = parse_pytest_summary(combined)
        suite_seconds = parse_pytest_duration(combined)
        evidence["pytest_command"] = tests.command
        evidence["pytest_returncode"] = tests.returncode
        evidence["pytest_counts"] = counts
        evidence["pytest_duration_s"] = suite_seconds
        evidence["pytest_tail"] = tail(combined, 40)

        _copy_sil_evidence(sil_dir, snapshot_dir, evidence)

        failed = counts["failed"]
        errored = counts["errors"]
        xpassed = counts["xpassed"]

        if errored:
            status = Status.ERROR
            reason = f"pytest reported {errored} error(s) - the SIL suite itself is broken"
        elif failed or xpassed or tests.returncode != 0:
            status = Status.FAIL
            reason = ""
        else:
            status = Status.PASS
            reason = ""

        observed = (
            f"{counts['passed']} passed, {failed} failed, {counts['xfailed']} xfailed, "
            f"{xpassed} xpassed, {counts['skipped']} skipped in {suite_seconds:.2f} s"
        )

        if status is Status.ERROR:
            return (
                RegressionResult.infra(
                    test_id,
                    name,
                    reason,
                    area="SIL",
                    expected=expected,
                    observed=observed,
                    duration_s=duration,
                ),
                evidence,
            )

        diagnostics = dict(evidence)
        details = tail(combined, 25)
        if xpassed:
            details = (
                "A strict xfail passed, which means the documented defect behind it may have "
                "been fixed and the test now needs re-triaging.\n\n" + details
            )
        return (
            RegressionResult(
                test_id=test_id,
                name=name,
                status=status,
                duration_s=duration,
                expected=expected,
                observed=observed,
                details=details,
                area="SIL",
                evidence=Evidence.HOST,
                diagnostics=diagnostics,
            ),
            evidence,
        )
    finally:
        _restore_files(sil_dir, snapshot)


def _copy_sil_evidence(sil_dir: Path, snapshot_dir: Path, evidence: dict[str, Any]) -> None:
    """Keeps a copy of the SIL reports a run produced, without dirtying the tree."""
    copied: list[str] = []
    try:
        snapshot_dir.mkdir(parents=True, exist_ok=True)
        for relative in _SIL_REPORTS:
            source = sil_dir / relative
            if source.is_file():
                destination = snapshot_dir / Path(relative).name
                shutil.copy2(source, destination)
                copied.append(str(destination.relative_to(REPO)).replace(os.sep, "/"))
    except OSError as exc:  # evidence capture must never break the run
        evidence["sil_report_copy_error"] = str(exc)
    evidence["sil_report_copies"] = copied
