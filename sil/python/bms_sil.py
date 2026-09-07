"""ctypes binding for the BMS SIL shared library.

Wraps the harness control surface in sil/harness/sil_api.h and owns the task
cadence, so tests read as "run the BMS for N milliseconds under these inputs"
rather than as a sequence of raw C calls.
"""

from __future__ import annotations

import ctypes
import os
import sys
from pathlib import Path

SIL_DIR = Path(__file__).resolve().parent.parent
REPO = SIL_DIR.parent
LIB_NAME = "bms_sil.dll" if os.name == "nt" else "libbms_sil.so"
LIB_PATH = SIL_DIR / "build" / LIB_NAME

# ---------------------------------------------------------------------------
# Values mirrored from the production headers. Kept here so a test failure
# points at a real behaviour change rather than a stale magic number.
# ---------------------------------------------------------------------------

PACK_CAPACITY_MAH = 100_000      # BMS_SOC_PACK1_CAPACITY_MAH
SAMPLE_PERIOD_MS = 100           # BMS_SOC_SAMPLE_PERIOD_MS
INITIAL_PCT_X10 = 500            # BMS_SOC_INITIAL_PCT_X10 (50.0 %)
MIN_PCT_X10 = 0
MAX_PCT_X10 = 1000
SAVE_PERIOD_MS = 60_000          # BMS_SOC_SAVE_PERIOD_MS
SAVE_DELTA_X10 = 1               # BMS_SOC_SAVE_DELTA_X10 (0.1 %)
OCV_SLEEP_THRESHOLD_S = 28_800   # BMS_SOC_OCV_RESET_SLEEP_THRESHOLD_S
OCV_WAIT_TIMEOUT_MS = 500        # BMS_SOC_OCV_WAIT_TIMEOUT_MS

# Bms_Soc_InitSourceType, mirrored from Bms_Soc.h. Published on CAN 0x308
# byte 2 bits 1-3 as Pack1SOCInitSource.
INIT_SOURCE_DEFAULT = 0
INIT_SOURCE_OCV = 1
INIT_SOURCE_NVM = 2
INIT_SOURCE_PENDING = 3

# A healthy mid-range cell voltage, used as the default stimulus in tests that
# are not about cell voltage itself.
NOMINAL_CELL_MV = 3600

NVM_RECORD_SIZE = 24
NVM_SECTOR_SIZE = 0x2000
NVM_RECORDS_PER_SECTOR = NVM_SECTOR_SIZE // NVM_RECORD_SIZE   # 341

# OCV curve as compiled into Bms_Soc.c: (cell mV, SOC 0.1 %)
OCV_TABLE = [
    (3000, 0),
    (3300, 200),
    (3450, 500),
    (3600, 800),
    (3700, 900),
    (3900, 1000),
]

u8, u16, u32 = ctypes.c_uint8, ctypes.c_uint16, ctypes.c_uint32
s16, s32 = ctypes.c_int16, ctypes.c_int32
f32, bl = ctypes.c_float, ctypes.c_bool

_SIGNATURES = {
    "Sil_FlashWipe": (None, []),
    "Sil_PowerOn": (None, []),
    "Sil_PowerOnWithSleepTime": (None, [u32, bl]),
    "Sil_SetSleepTime": (None, [u32, bl]),
    "Sil_Run100ms": (None, []),
    "Sil_Run1000ms": (None, []),
    "Sil_AdvanceMs": (None, [u32]),
    "Sil_ElapsedMs": (u32, []),
    "Sil_InjectVpackCurrent": (None, [s32, s16, u8]),
    "Sil_InjectVpackVoltage": (None, [u32, u32]),
    "Sil_VpackSkipAlive": (None, []),
    "Sil_VpackFreezeAlive": (None, [bl]),
    "Sil_InjectVafeCycle": (None, [ctypes.POINTER(u16)]),
    "Sil_InjectVafeUniform": (None, [u16]),
    "Sil_SetAdcPackVoltages": (None, [u16, u16, bl]),
    "Sil_SetNtc": (None, [s16, s16, s16, bl]),
    "Sil_SocPack_pct_x10": (u16, []),
    "Sil_SocPackValid": (bl, []),
    "Sil_SocMin_pct_x10": (u16, []),
    "Sil_SocMax_pct_x10": (u16, []),
    "Sil_SocAvg_pct_x10": (u16, []),
    "Sil_SocMinCapacity_mAh": (f32, []),
    "Sil_SocMaxCapacity_mAh": (f32, []),
    "Sil_SocAvgCapacity_mAh": (f32, []),
    "Sil_SocMinValid": (bl, []),
    "Sil_SocMaxValid": (bl, []),
    "Sil_SocAvgValid": (bl, []),
    "Sil_SocLegacy_pct_x10": (u16, []),
    "Sil_SocLegacyValid": (bl, []),
    "Sil_SocInitSource": (u8, []),
    "Sil_SetSoc_pct_x10": (None, [u16]),
    "Sil_OcvToSoc": (u16, [u16]),
    "Sil_PackCurrent_mA": (s32, []),
    "Sil_PackCurrentValid": (bl, []),
    "Sil_MinCellVoltage": (f32, []),
    "Sil_MaxCellVoltage": (f32, []),
    "Sil_AverageCellVoltage": (f32, []),
    "Sil_DeltaCellVoltage": (f32, []),
    "Sil_CellVoltageValid": (bl, []),
    "Sil_PackV1": (f32, []),
    "Sil_FlashEraseCount": (u32, []),
    "Sil_FlashWriteCount": (u32, []),
    "Sil_FlashByte": (u8, [u32]),
    "Sil_FlashWriteByte": (None, [u32, u8]),
    "Sil_FlashFailNextWrite": (None, [bl]),
    "Sil_FlashFailNextErase": (None, [bl]),
    "Sil_FlashPowerLossAfterBytes": (None, [u32]),
    "Sil_NvmRecordCount": (u32, []),
    "Sil_NvmLoad": (bl, [ctypes.POINTER(u16)] * 3),
    "Sil_InterpLookup": (u16, [ctypes.POINTER(u16), u16, u16]),
}


def _load() -> ctypes.CDLL:
    if not LIB_PATH.is_file():
        raise RuntimeError(
            f"SIL library not built: {LIB_PATH}\nRun:  python sil/build.py"
        )

    lib = ctypes.CDLL(str(LIB_PATH))
    for name, (restype, argtypes) in _SIGNATURES.items():
        fn = getattr(lib, name)
        fn.restype = restype
        fn.argtypes = argtypes
    return lib


class Bms:
    """One simulated BMS instance.

    The shared library holds process-global state, so a single instance is
    reused per test session and reset between tests (see conftest.py).
    """

    def __init__(self) -> None:
        self.lib = _load()
        self._ms = 0

    # -- lifecycle ---------------------------------------------------------

    def flash_wipe(self) -> None:
        """Erase simulated Data Flash. Models a virgin/reflashed device."""
        self.lib.Sil_FlashWipe()

    def power_on(
        self,
        *,
        sleep_s: int | None = None,
        sleep_ready: bool = True,
    ) -> None:
        """Cold boot. Flash contents persist, so this models a power cycle.

        `sleep_s` / `sleep_ready` drive the Bms_SleepTime provider *before* the
        module inits run, which is what Bms_Soc_Init() reads to decide whether a
        tier 1 OCV reset is eligible. Left at their defaults the provider
        mirrors the production placeholder (ready, zero elapsed).
        """
        if sleep_s is None and sleep_ready:
            self.lib.Sil_PowerOn()
        else:
            self.lib.Sil_PowerOnWithSleepTime(int(sleep_s or 0), bool(sleep_ready))
        self._ms = 0

    def power_cycle(self, **kwargs) -> None:
        self.power_on(**kwargs)

    def set_sleep_time(self, elapsed_s: int, ready: bool = True) -> None:
        """Drive the sleep-time provider mid-run (e.g. a late-arriving RTC)."""
        self.lib.Sil_SetSleepTime(int(elapsed_s), bool(ready))

    # -- time / stimulus ---------------------------------------------------

    @property
    def elapsed_ms(self) -> int:
        return self._ms

    def run_ms(
        self,
        ms: int,
        *,
        current_mA: int | None = None,
        shunt_uV: int = 0,
        status: int = 0,
        pack_mV: int | None = None,
        bus_mV: int = 0,
        cells_mV: list[int] | int | None = None,
    ) -> None:
        """Run for `ms` of virtual time, re-injecting the given stimulus each cycle.

        Any argument left as None simply is not injected, which lets a test
        starve one input (e.g. stop sending current frames) to exercise the
        timeout paths.
        """
        if ms % SAMPLE_PERIOD_MS:
            raise ValueError(f"ms must be a multiple of {SAMPLE_PERIOD_MS}")

        for _ in range(ms // SAMPLE_PERIOD_MS):
            if current_mA is not None:
                self.lib.Sil_InjectVpackCurrent(int(current_mA), int(shunt_uV), int(status))
            if pack_mV is not None:
                self.lib.Sil_InjectVpackVoltage(int(pack_mV), int(bus_mV))
            if cells_mV is not None:
                self.inject_cells(cells_mV)

            self.lib.Sil_Run100ms()
            self._ms += SAMPLE_PERIOD_MS

            if self._ms % 1000 == 0:
                self.lib.Sil_Run1000ms()

    def run_normal(self, ms: int, *, current_mA: int = 0, cell_mV: int = 3600) -> None:
        """Run with a fully healthy signal chain: current, voltage and cells all fresh."""
        self.run_ms(
            ms,
            current_mA=current_mA,
            pack_mV=cell_mV * 16,
            bus_mV=cell_mV * 16,
            cells_mV=cell_mV,
        )

    def inject_cells(self, cells_mV: list[int] | int) -> None:
        if isinstance(cells_mV, int):
            self.lib.Sil_InjectVafeUniform(cells_mV)
        else:
            if len(cells_mV) != 16:
                raise ValueError("cells_mV must hold 16 values")
            arr = (u16 * 16)(*cells_mV)
            self.lib.Sil_InjectVafeCycle(arr)

    def skip_alive_once(self) -> None:
        self.lib.Sil_VpackSkipAlive()

    def freeze_alive(self, enable: bool = True) -> None:
        """Stop the vPACK alive counter advancing (stuck transmitter)."""
        self.lib.Sil_VpackFreezeAlive(enable)

    def set_adc_pack_voltages(self, v2_mV: int, v3_mV: int, valid: bool) -> None:
        self.lib.Sil_SetAdcPackVoltages(v2_mV, v3_mV, valid)

    def set_ntc(self, t1_dC: int, t2_dC: int, t3_dC: int, valid: bool = True) -> None:
        self.lib.Sil_SetNtc(t1_dC, t2_dC, t3_dC, valid)

    # -- SOC observation ---------------------------------------------------

    @property
    def soc(self) -> int:
        """Blended pack SOC, 0.1 % units."""
        return self.lib.Sil_SocPack_pct_x10()

    @property
    def soc_valid(self) -> bool:
        return self.lib.Sil_SocPackValid()

    @property
    def soc_min(self) -> int:
        return self.lib.Sil_SocMin_pct_x10()

    @property
    def soc_max(self) -> int:
        return self.lib.Sil_SocMax_pct_x10()

    @property
    def soc_avg(self) -> int:
        return self.lib.Sil_SocAvg_pct_x10()

    @property
    def capacity_avg_mAh(self) -> float:
        return self.lib.Sil_SocAvgCapacity_mAh()

    @property
    def init_source(self) -> int:
        """How the estimators were seeded at startup (INIT_SOURCE_*)."""
        return self.lib.Sil_SocInitSource()

    @property
    def soc_legacy(self) -> int:
        return self.lib.Sil_SocLegacy_pct_x10()

    @property
    def soc_legacy_valid(self) -> bool:
        return self.lib.Sil_SocLegacyValid()

    def set_soc(self, soc_pct_x10: int) -> None:
        self.lib.Sil_SetSoc_pct_x10(soc_pct_x10)

    def ocv_to_soc(self, voltage_mV: int) -> int:
        return self.lib.Sil_OcvToSoc(voltage_mV)

    # -- battery monitor observation ---------------------------------------

    @property
    def pack_current_mA(self) -> int:
        return self.lib.Sil_PackCurrent_mA()

    @property
    def pack_current_valid(self) -> bool:
        return self.lib.Sil_PackCurrentValid()

    @property
    def cell_min_V(self) -> float:
        return self.lib.Sil_MinCellVoltage()

    @property
    def cell_max_V(self) -> float:
        return self.lib.Sil_MaxCellVoltage()

    @property
    def cell_avg_V(self) -> float:
        return self.lib.Sil_AverageCellVoltage()

    @property
    def cell_delta_V(self) -> float:
        return self.lib.Sil_DeltaCellVoltage()

    @property
    def cell_valid(self) -> bool:
        return self.lib.Sil_CellVoltageValid()

    # -- flash / NVM -------------------------------------------------------

    @property
    def flash_erases(self) -> int:
        return self.lib.Sil_FlashEraseCount()

    @property
    def flash_writes(self) -> int:
        return self.lib.Sil_FlashWriteCount()

    @property
    def nvm_records(self) -> int:
        return self.lib.Sil_NvmRecordCount()

    def flash_byte(self, offset: int) -> int:
        return self.lib.Sil_FlashByte(offset)

    def flash_write_byte(self, offset: int, value: int) -> None:
        """Raw poke into the flash model. Test setup only."""
        self.lib.Sil_FlashWriteByte(offset, value)

    def fail_next_flash_write(self, enable: bool = True) -> None:
        self.lib.Sil_FlashFailNextWrite(enable)

    def fail_next_flash_erase(self, enable: bool = True) -> None:
        self.lib.Sil_FlashFailNextErase(enable)

    def flash_power_loss_after(self, bytes_written: int) -> None:
        self.lib.Sil_FlashPowerLossAfterBytes(bytes_written)

    def nvm_load(self) -> tuple[int, int, int] | None:
        """Newest persisted (min, max, avg) triple, or None if nothing valid."""
        a, b, c = u16(), u16(), u16()
        ok = self.lib.Sil_NvmLoad(ctypes.byref(a), ctypes.byref(b), ctypes.byref(c))
        return (a.value, b.value, c.value) if ok else None

    # -- direct unit access ------------------------------------------------

    def interp(self, table: list[tuple[int, int]], x: int) -> int:
        flat = [v for row in table for v in row]
        arr = (u16 * len(flat))(*flat)
        return self.lib.Sil_InterpLookup(arr, len(table), x)

    def interp_raw(self, flat: list[int], rows: int, x: int) -> int:
        """Escape hatch for malformed-table cases (rows deliberately wrong)."""
        arr = (u16 * max(len(flat), 1))(*flat) if flat else None
        return self.lib.Sil_InterpLookup(arr, rows, x)


# ---------------------------------------------------------------------------
# Helpers shared by tests
# ---------------------------------------------------------------------------


def soc_to_capacity_mAh(soc_pct_x10: int) -> float:
    return PACK_CAPACITY_MAH * soc_pct_x10 / 1000.0


def charge_mAh(current_mA: int, ms: int) -> float:
    return current_mA * (ms / 3_600_000.0)


def expected_blend(soc_min: int, soc_max: int, soc_avg: int) -> int:
    """Reference implementation of the SOC_DESIGN.md 3.5 blend, in Python."""
    w_max = min(soc_avg, MAX_PCT_X10)
    w_min = MAX_PCT_X10 - w_max
    return (soc_min * w_min + soc_max * w_max) // MAX_PCT_X10


if __name__ == "__main__":
    bms = Bms()
    bms.flash_wipe()
    bms.power_on()
    print(f"loaded {LIB_PATH.name}")
    print(f"  boot SOC = {bms.soc / 10:.1f} %  valid={bms.soc_valid}")
    bms.run_normal(1000, current_mA=-50_000)
    print(f"  after 1 s at -50 A: SOC = {bms.soc / 10:.1f} %  valid={bms.soc_valid}")
    sys.exit(0)
