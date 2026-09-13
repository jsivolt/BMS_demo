#!/usr/bin/env python3
"""Bms_Sop hardware-in-the-loop test on the S32K344 bench board.

The test writes replacement inputs into g_BmsSopTestOverride, lets the
firmware run, then reads g_BmsSopData and compares it with values derived by
hand from the placeholder calibration in Bms_BattCfg.c. It writes a Markdown
report to hil/reports/sop_hil.md.

Prerequisites:
  1. Bms_Sop.h has BMS_SOP_TEST_OVERRIDE set to 1U.
  2. build.bat and flash.bat were run, so the board runs Debug_FLASH/BMS_demo.elf.
  3. debug_server.bat runs in another window (J-Link GDB server on port 2331).

Usage:
  python hil/sop_hil.py [--port 2331] [--settle 0.6] [--elf ...] [--gdb ...]

Exit status: 0 all cases pass, 1 a case failed or was blocked, 2 setup error.

Each step is one arm-none-eabi-gdb batch session: attach, read or write,
detach. Reconnecting to the same GDB server does not reset the core. Do not
use the jlink-mcp memory tools on this board instead: each new J-Link
connection runs the S32K344 J-Link script, which fills the application RAM
with 0xDEADBEEF. Register reads through this GDB server also return
0xDEADBEEF, so firmware health comes from counters, not from the PC.
"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import os
import platform
import re
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable

REPO = Path(__file__).resolve().parents[1]
DEFAULT_ELF = REPO / "Debug_FLASH" / "BMS_demo.elf"
DEFAULT_GDB = (Path(os.environ.get("S32DS_ROOT", r"C:\NXP\S32DS.3.6.10\S32DS"))
               / "tools" / "gdb-arm" / "arm32-eabi" / "bin" / "arm-none-eabi-gdb.exe")
DEFAULT_REPORT = REPO / "hil" / "reports" / "sop_hil.md"

# Bms_Sop.h BMS_SOP_OVR_* bits.
OVR_MIN_CELL = 0x01
OVR_MAX_CELL = 0x02
OVR_MAX_TEMP = 0x04
OVR_SOC_MIN = 0x08
OVR_SOC_MAX = 0x10
OVR_ALL = 0x1F

MODE_DISCHARGE = 0
MODE_CHARGE = 1

# Published fields, as report name -> GDB expression.
OUTPUT_FIELDS = {
    "D.table":  "g_BmsSopData.Discharge.Table_dA",
    "D.factor": "g_BmsSopData.Discharge.DerateFactor",
    "D.final":  "g_BmsSopData.Discharge.Final_dA",
    "R.table":  "g_BmsSopData.Regen.Table_dA",
    "R.factor": "g_BmsSopData.Regen.DerateFactor",
    "R.final":  "g_BmsSopData.Regen.Final_dA",
    "C.table":  "g_BmsSopData.Charge.Table_dA",
    "C.factor": "g_BmsSopData.Charge.DerateFactor",
    "C.final":  "g_BmsSopData.Charge.Final_dA",
    "mode":     "g_BmsSopData.Mode",
    "vlow":     "g_BmsSopData.DerateActiveVLow",
    "vhigh":    "g_BmsSopData.DerateActiveVHigh",
    "thigh":    "g_BmsSopData.DerateActiveTHigh",
    "tlow":     "g_BmsSopData.DerateActiveTLow",
    "inputs_valid": "g_BmsSopData.InputsValid",
}

# Measured Bms_Sop inputs, read before the override is applied.
MEASURED_FIELDS = {
    "min_mV":  "(int)(g_BatteryData.MinCellVoltage * 1000.0 + 0.5)",
    "max_mV":  "(int)(g_BatteryData.MaxCellVoltage * 1000.0 + 0.5)",
    "temp_dC": "g_BatteryData.MaxPackTemperature_dC",
    "socmin":  "g_BmsSocPack.Min.Soc_pct_x10",
    "socmax":  "g_BmsSocPack.Max.Soc_pct_x10",
}

HEALTH_FIELDS = {
    "led":       "g_LedCounter",
    "hf_hfsr":   "g_HardFault_HFSR",
    "hf_cfsr":   "g_HardFault_CFSR",
    "ovr_size":  "sizeof(g_BmsSopTestOverride)",
}


class SetupError(RuntimeError):
    """The test cannot run: no GDB, no server, wrong firmware."""


# --------------------------------------------------------------------------------------------------
# Target access
# --------------------------------------------------------------------------------------------------

class Target:
    """One arm-none-eabi-gdb batch session per call, against a running GDB server."""

    def __init__(self, gdb: Path, elf: Path, port: int):
        self.gdb = gdb
        self.elf = elf
        self.port = port

    def run(self, commands: list[str]) -> str:
        # A failed command ends a gdb -x script, so a missing marker means the
        # connection failed.
        script = ["set pagination off", "set confirm off",
                  f"target remote localhost:{self.port}", 'printf "@@connected=1\\n"',
                  *commands, "detach"]
        fd, path = tempfile.mkstemp(suffix=".gdb", prefix="sop_hil_")
        try:
            with os.fdopen(fd, "w", encoding="ascii") as f:
                f.write("\n".join(script) + "\n")
            proc = subprocess.run([str(self.gdb), "-batch", "-nx", "-x", path, str(self.elf)],
                                  capture_output=True, text=True, timeout=60)
        finally:
            os.unlink(path)
        out = proc.stdout + proc.stderr
        if "@@connected=1" not in out:
            raise SetupError(f"Cannot connect to the GDB server on localhost:{self.port}. "
                             f"Start debug_server.bat first.\n{out.strip()}")
        if re.search(r"Connection refused|Connection timed out|Remote communication error|"
                     r"Remote connection closed|No symbol", out):
            raise SetupError(f"GDB session failed:\n{out.strip()}")
        return out

    def read(self, fields: dict[str, str]) -> dict[str, int]:
        cmds = [f'printf "@@{name}=%d\\n", (int)({expr})' for name, expr in fields.items()]
        out = self.run(cmds)
        values = {m.group(1): int(m.group(2)) for m in re.finditer(r"@@([\w.]+)=(-?\d+)", out)}
        missing = set(fields) - set(values)
        if missing:
            raise SetupError(f"Could not read {sorted(missing)}:\n{out.strip()}")
        return values

    def write_override(self, enable: int, min_mV: int, max_mV: int, temp_dC: int,
                       socmin: int, socmax: int, mode: int) -> None:
        # Enable goes last. The core is halted for the whole session, so the
        # firmware sees the complete set either way, but the order keeps the
        # write safe if the session ever breaks off part way.
        self.run([
            f"set var g_BmsSopTestOverride.MinCell_mV = {min_mV}",
            f"set var g_BmsSopTestOverride.MaxCell_mV = {max_mV}",
            f"set var g_BmsSopTestOverride.MaxTemp_dC = {temp_dC}",
            f"set var g_BmsSopTestOverride.SocMin_pct_x10 = {socmin}",
            f"set var g_BmsSopTestOverride.SocMax_pct_x10 = {socmax}",
            f"set var g_BmsSopMode = {mode}",
            f"set var g_BmsSopTestOverride.Enable = {enable}",
        ])

    def compare_flash(self) -> str:
        out = self.run(["compare-sections .pflash"])
        m = re.search(r"Section \.pflash.*?: (matched|MIS-MATCHED)", out)
        return m.group(1) if m else "unknown"


# --------------------------------------------------------------------------------------------------
# Test cases
# --------------------------------------------------------------------------------------------------

@dataclass
class Expect:
    """One expected field value: exact, within a tolerance, or a lower bound."""
    value: int
    tol: int = 0
    greater_than: bool = False

    def check(self, actual: int) -> bool:
        if self.greater_than:
            return actual > self.value
        return abs(actual - self.value) <= self.tol

    def __str__(self) -> str:
        if self.greater_than:
            return f"> {self.value}"
        return f"{self.value} ±{self.tol}" if self.tol else str(self.value)


@dataclass
class Case:
    id: str
    requirement: str
    title: str
    purpose: str
    enable: int
    min_mV: int
    max_mV: int
    temp_dC: int
    socmin: int
    socmax: int
    mode: int
    expect: dict[str, Expect] | Callable[[dict], dict[str, Expect]]
    precondition: Callable[[dict], str | None] | None = None

    # Filled in by the run.
    result: str = "NOT RUN"
    note: str = ""
    actual: dict[str, int] = field(default_factory=dict)
    resolved: dict[str, Expect] = field(default_factory=dict)


def limits(d: tuple[int, int, int], r: tuple[int, int, int], c: tuple[int, int, int],
           mode: int = MODE_DISCHARGE, vlow: int = 0, vhigh: int = 0, thigh: int = 0,
           tlow: int = 0, tol: dict[str, int] | None = None) -> dict[str, Expect]:
    """Expected outputs as (table, factor, final) per limit plus mode and flags."""
    tol = tol or {}
    exp = {}
    for prefix, triple in (("D", d), ("R", r), ("C", c)):
        for name, value in zip(("table", "factor", "final"), triple):
            key = f"{prefix}.{name}"
            exp[key] = Expect(value, tol.get(key, 0))
    exp["mode"] = Expect(mode)
    exp["vlow"] = Expect(vlow)
    exp["vhigh"] = Expect(vhigh)
    exp["thigh"] = Expect(thigh)
    exp["tlow"] = Expect(tlow)
    # Every override case sets all five bits, so every input counts as valid.
    exp["inputs_valid"] = Expect(1)
    return exp


def case_list() -> list[Case]:
    # Derivations use Bms_BattCfg.c: SOC axis 0..1000, temperature axis -200..600,
    # VLow ramp 2900 -> 2600 mV, VHigh ramp 4100 -> 4200 mV, THigh ramp 450 -> 600 dC,
    # each ramp ending at factor 0, and every limit 0 at or below -20.0 degC.
    # Final = Table * Factor / 1000, truncated. Nominal inputs are 3600 mV on both
    # cells, 25.0 degC and 60 % SOC, which is a map breakpoint.
    nominal = dict(min_mV=3600, max_mV=3600, temp_dC=250, socmin=600, socmax=600)
    interp = {"D.table": 1, "R.table": 1, "C.table": 1, "D.final": 1, "R.final": 1}

    def nom(**changes):
        return {**nominal, **changes}

    def running_inputs_safe(m: dict) -> str | None:
        if not 2900 <= m["min_mV"] or not m["max_mV"] <= 4100:
            return (f"measured cells {m['min_mV']} / {m['max_mV']} mV are outside "
                    f"2900..4100 mV, so the voltage factors are not 1000")
        if m["socmin"] < 100 or m["socmax"] < 100:
            return f"measured SOC {m['socmin']} / {m['socmax']} is below 10.0 %"
        return None

    def restore_expect(base: dict) -> dict[str, Expect]:
        exp = {key: Expect(base[key], 15) for key in ("D.table", "R.table", "C.table")}
        exp.update({key: Expect(base[key])
                    for key in ("mode", "vlow", "vhigh", "thigh", "tlow", "inputs_valid")})
        exp["C.final"] = Expect(0)
        return exp

    cases = [
        ("SP-01", "Map value at a breakpoint",
         "All inputs sit on map breakpoints, so each table value is the stored value. "
         "No derate applies. Discharge mode forces Charge.Final_dA to 0.",
         OVR_ALL, nominal, MODE_DISCHARGE,
         limits((880, 1000, 880), (520, 1000, 520), (500, 1000, 0))),
        ("SP-01", "Interpolation along SOC",
         "SOC 50.0 % is halfway between the 40 % and 60 % breakpoints at 25.0 degC. "
         "Discharge (800+880)/2, regen (640+520)/2, charge (600+500)/2.",
         OVR_ALL, nom(socmin=500, socmax=500), MODE_DISCHARGE,
         limits((840, 1000, 840), (580, 1000, 580), (550, 1000, 0), tol=interp)),
        ("SP-01", "Interpolation along temperature",
         "32.5 degC is halfway between the 25.0 and 40.0 degC rows at 60 % SOC. "
         "It is below the 45.0 degC THigh start, so no derate applies.",
         OVR_ALL, nom(temp_dC=325), MODE_DISCHARGE,
         limits((860, 1000, 860), (500, 1000, 500), (475, 1000, 0), tol=interp)),
        ("SP-01", "Clamp above the highest SOC",
         "SOC 120.0 % is above the 100 % breakpoint, so the lookup uses that column. "
         "Regen and charge are 0 at full.",
         OVR_ALL, nom(socmin=1200, socmax=1200), MODE_DISCHARGE,
         limits((900, 1000, 900), (0, 1000, 0), (0, 1000, 0))),
        ("SP-02", "Discharge follows SocMin, regen and charge follow SocMax",
         "SocMin 10.0 % and SocMax 90.0 % at 25.0 degC. Discharge reads the 10 % "
         "column (300). Regen (160) and charge (150) read the 90 % column.",
         OVR_ALL, nom(socmin=100, socmax=900), MODE_DISCHARGE,
         limits((300, 1000, 300), (160, 1000, 160), (150, 1000, 0))),
        ("SP-03", "VLow ramp midpoint",
         "Minimum cell 2750 mV is halfway down the 2900 to 2600 mV ramp, so "
         "kVLow = 500. It trims discharge only: 880 * 500 / 1000 = 440.",
         OVR_ALL, nom(min_mV=2750), MODE_DISCHARGE,
         limits((880, 500, 440), (520, 1000, 520), (500, 1000, 0), vlow=1)),
        ("SP-03", "VLow ramp at its start",
         "Minimum cell 2900 mV is exactly the ramp start, so kVLow = 1000 and the "
         "flag stays clear.",
         OVR_ALL, nom(min_mV=2900), MODE_DISCHARGE,
         limits((880, 1000, 880), (520, 1000, 520), (500, 1000, 0))),
        ("SP-03", "VHigh ramp midpoint",
         "Maximum cell 4150 mV is halfway up the 4100 to 4200 mV ramp, so "
         "kVHigh = 500. It trims regen and charge: regen 520 * 500 / 1000 = 260.",
         OVR_ALL, nom(max_mV=4150), MODE_DISCHARGE,
         limits((880, 1000, 880), (520, 500, 260), (500, 500, 0), vhigh=1)),
        ("SP-03", "THigh ramp midpoint",
         "52.5 degC is halfway along the 45.0 to 60.0 degC ramp, so kTHigh = 500 on "
         "all three limits. The tables come from 25 % of the way between the "
         "50.0 and 60.0 degC rows: discharge 525, regen 252.5, charge 235.",
         OVR_ALL, nom(temp_dC=525), MODE_DISCHARGE,
         limits((525, 500, 262), (253, 500, 126), (235, 500, 0), thigh=1, tol=interp)),
        ("SP-04", "Combined derate is the minimum, not the product",
         "kVLow = 500 (2750 mV) and kTHigh = 667 (50.0 degC). Discharge uses "
         "min(500, 667) = 500, so 600 -> 300. The product rule would give 200. "
         "Regen and charge use min(1000, 667) = 667.",
         OVR_ALL, nom(min_mV=2750, temp_dC=500), MODE_DISCHARGE,
         limits((600, 500, 300), (300, 667, 200), (280, 667, 0), vlow=1, thigh=1,
                tol={"R.factor": 1, "C.factor": 1, "R.final": 1})),
        ("SP-13", "Discharge is 0 past the VLow end",
         "Minimum cell 2500 mV is below the 2600 mV end, so kVLow = 0 and discharge "
         "publishes 0. Regen and charge keep their values, so the pack can still be "
         "charged.",
         OVR_ALL, nom(min_mV=2500), MODE_DISCHARGE,
         limits((880, 0, 0), (520, 1000, 520), (500, 1000, 0), vlow=1)),
        ("SP-13", "Regen and charge are 0 past the VHigh end",
         "Maximum cell 4250 mV is above the 4200 mV end, so kVHigh = 0. Regen and "
         "charge publish 0, discharge keeps its value.",
         OVR_ALL, nom(max_mV=4250), MODE_DISCHARGE,
         limits((880, 1000, 880), (520, 0, 0), (500, 0, 0), vhigh=1)),
        ("SP-13", "Every limit is 0 past the THigh end",
         "65.0 degC is above the 60.0 degC end, so kTHigh = 0 on all three limits. "
         "The tables still show the 60.0 degC row.",
         OVR_ALL, nom(temp_dC=650), MODE_DISCHARGE,
         limits((300, 0, 0), (110, 0, 0), (100, 0, 0), thigh=1)),
        ("SP-13", "Every limit is 0 at the under-temperature set point",
         "-20.0 degC is the under-temperature fault set point, so every factor is 0 "
         "and the TLow flag is set.",
         OVR_ALL, nom(temp_dC=-200), MODE_DISCHARGE,
         limits((280, 0, 0), (0, 0, 0), (0, 0, 0), tlow=1)),
        ("SP-13", "Every limit is 0 below the under-temperature set point",
         "-30.0 degC is below the set point. The lookup clamps to the -20.0 degC row "
         "and every factor is 0.",
         OVR_ALL, nom(temp_dC=-300), MODE_DISCHARGE,
         limits((280, 0, 0), (0, 0, 0), (0, 0, 0), tlow=1)),
        ("SP-13", "Just inside the cold limit the maps apply",
         "-19.0 degC is 10 % of the way from the -20.0 to the -10.0 degC row: "
         "discharge 280 + 17 = 297, regen 3, charge 2. No factor is cut.",
         OVR_ALL, nom(temp_dC=-190), MODE_DISCHARGE,
         limits((297, 1000, 297), (3, 1000, 3), (2, 1000, 0), tol=interp)),
        ("SP-05", "Charge mode gating",
         "Charge mode forces Discharge.Final_dA and Regen.Final_dA to 0. Their "
         "Table_dA and DerateFactor keep the computed values (SOP-FR-03).",
         OVR_ALL, nominal, MODE_CHARGE,
         limits((880, 1000, 0), (520, 1000, 0), (500, 1000, 500), mode=MODE_CHARGE)),
        ("SP-05", "Charge mode with a VHigh derate",
         "Charge mode with maximum cell 4150 mV. Charge 500 * 500 / 1000 = 250. "
         "Regen keeps factor 500 but publishes 0.",
         OVR_ALL, nom(max_mV=4150), MODE_CHARGE,
         limits((880, 1000, 0), (520, 500, 0), (500, 500, 250), mode=MODE_CHARGE, vhigh=1)),
        ("SP-05", "Invalid mode value reads as Discharge",
         "g_BmsSopMode = 7 is not a valid mode, so the module runs in Discharge "
         "mode and publishes no charge limit.",
         OVR_ALL, nominal, 7,
         limits((880, 1000, 880), (520, 1000, 520), (500, 1000, 0))),
    ]
    out = [Case(f"HIL-SOP-{i:02d}", req, title, purpose, enable, **inputs, mode=mode, expect=exp)
           for i, (req, title, purpose, enable, inputs, mode, exp) in enumerate(cases, start=1)]
    n = len(out)
    out += [
        Case(f"HIL-SOP-{n + 1:02d}", "Test hook", "Override one input, keep the others measured",
             "Only the MAX_TEMP bit is set, with 52.5 degC. The voltage and SOC fields "
             "hold values that would zero a factor or a table if their bits were "
             "wrongly honored. The measured cells are in the safe region, so every "
             "factor must be kTHigh = 500 and only the THigh flag is set.",
             OVR_MAX_TEMP, min_mV=0, max_mV=65535, temp_dC=525, socmin=0, socmax=0,
             mode=MODE_DISCHARGE,
             expect={"D.factor": Expect(500), "R.factor": Expect(500), "C.factor": Expect(500),
                     "D.table": Expect(0, greater_than=True),
                     "D.final": Expect(0, greater_than=True),
                     "vlow": Expect(0), "vhigh": Expect(0), "thigh": Expect(1),
                     "inputs_valid": Expect(1), "C.final": Expect(0)},
             precondition=running_inputs_safe),
        Case(f"HIL-SOP-{n + 2:02d}", "Test hook", "Override off restores the measured inputs",
             "Enable = 0. The tables return to the baseline read before the first case, "
             "within 1.5 A for drift in the live measurements. Mode and flags match the "
             "baseline.",
             0, min_mV=0, max_mV=0, temp_dC=0, socmin=0, socmax=0, mode=MODE_DISCHARGE,
             expect=restore_expect),
    ]
    return out


# --------------------------------------------------------------------------------------------------
# Run
# --------------------------------------------------------------------------------------------------

@dataclass
class RunInfo:
    started: dt.datetime
    finished: dt.datetime | None = None
    flash_match: str = "unknown"
    ovr_size: int = 0
    running: bool = False
    led_samples: list[int] = field(default_factory=list)
    fault_before: tuple[int, int] = (0, 0)
    fault_after: tuple[int, int] | None = None
    measured: dict[str, int] = field(default_factory=dict)
    baseline: dict[str, int] = field(default_factory=dict)
    restored: bool = False


def firmware_running(target: Target, samples: int = 3, gap_s: float = 0.25) -> list[int]:
    values = []
    for _ in range(samples):
        values.append(target.read({"led": "g_LedCounter"})["led"])
        time.sleep(gap_s)
    return values


def run_cases(target: Target, cases: list[Case], settle_s: float, info: RunInfo) -> None:
    info.flash_match = target.compare_flash()
    if info.flash_match != "matched":
        raise SetupError(f"Flash on the target does not match {target.elf} "
                         f"(compare-sections: {info.flash_match}). Flash the ELF first.")

    health = target.read(HEALTH_FIELDS)
    info.ovr_size = health["ovr_size"]
    info.fault_before = (health["hf_hfsr"], health["hf_cfsr"])

    info.led_samples = firmware_running(target)
    info.running = len(set(info.led_samples)) > 1
    if not info.running:
        raise SetupError(f"g_LedCounter did not change ({info.led_samples}). "
                         f"The firmware is not running.")

    # Baseline with the override off, so a value left by an earlier run cannot leak in.
    target.write_override(0, 0, 0, 0, 0, 0, MODE_DISCHARGE)
    time.sleep(settle_s)
    info.measured = target.read(MEASURED_FIELDS)
    info.baseline = target.read(OUTPUT_FIELDS)

    try:
        for case in cases:
            print(f"{case.id}  {case.title} ... ", end="", flush=True)
            if case.precondition:
                blocked = case.precondition(info.measured)
                if blocked:
                    case.result, case.note = "BLOCKED", blocked
                    print(case.result)
                    continue
            target.write_override(case.enable, case.min_mV, case.max_mV, case.temp_dC,
                                  case.socmin, case.socmax, case.mode)
            time.sleep(settle_s)
            case.actual = target.read(OUTPUT_FIELDS)
            case.resolved = case.expect(info.baseline) if callable(case.expect) else case.expect
            failed = [k for k, e in case.resolved.items() if not e.check(case.actual[k])]
            case.result = "FAIL" if failed else "PASS"
            if failed:
                case.note = "mismatch: " + ", ".join(failed)
            print(case.result + (f" ({case.note})" if case.note else ""))
    finally:
        target.write_override(0, 0, 0, 0, 0, 0, MODE_DISCHARGE)
        info.restored = True

    health = target.read(HEALTH_FIELDS)
    info.fault_after = (health["hf_hfsr"], health["hf_cfsr"])


# --------------------------------------------------------------------------------------------------
# Report
# --------------------------------------------------------------------------------------------------

def slug(text: str) -> str:
    return re.sub(r"[^a-z0-9 -]", "", text.lower()).replace(" ", "-")


def git(*args: str) -> str:
    try:
        return subprocess.run(["git", *args], cwd=REPO, capture_output=True, text=True,
                              timeout=15).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        return ""


def gdb_version(gdb: Path) -> str:
    try:
        out = subprocess.run([str(gdb), "--version"], capture_output=True, text=True, timeout=15)
        return out.stdout.splitlines()[0].strip()
    except (OSError, subprocess.SubprocessError, IndexError):
        return "unknown"


def ts(t: dt.datetime) -> str:
    return t.strftime("%Y-%m-%d %H:%M:%S")


def inputs_row(case: Case) -> str:
    bits = [name for bit, name in ((OVR_MIN_CELL, "MIN_CELL"), (OVR_MAX_CELL, "MAX_CELL"),
                                   (OVR_MAX_TEMP, "MAX_TEMP"), (OVR_SOC_MIN, "SOC_MIN"),
                                   (OVR_SOC_MAX, "SOC_MAX")) if case.enable & bit]
    enable = f"0x{case.enable:02X} ({', '.join(bits) if bits else 'off'})"
    return (f"| {enable} | {case.min_mV} | {case.max_mV} | {case.temp_dC} | "
            f"{case.socmin} | {case.socmax} | {case.mode} |")


def write_report(path: Path, cases: list[Case], info: RunInfo, args: argparse.Namespace,
                 setup_error: str | None) -> None:
    elf = args.elf
    elf_stat = elf.stat() if elf.exists() else None
    elf_hash = hashlib.sha256(elf.read_bytes()).hexdigest()[:12] if elf_stat else "missing"
    commit = git("rev-parse", "--short", "HEAD") or "unknown"
    dirty = " (working tree has uncommitted changes)" if git("status", "--porcelain",
                                                             "--untracked-files=no") else ""
    counts = {r: sum(c.result == r for c in cases) for r in ("PASS", "FAIL", "BLOCKED", "NOT RUN")}
    verdict = "PASS" if setup_error is None and counts["PASS"] == len(cases) else "FAIL"
    elapsed = (info.finished - info.started).total_seconds() if info.finished else 0.0

    L: list[str] = []
    L += ["# Bms_Sop HIL Test Report", "",
          "> Generated by `hil/sop_hil.py`. Do not edit by hand. Run the script again to "
          "update it.", "",
          "## Execution environment", "",
          "| | |", "|---|---|",
          f"| Executed (UTC) | {ts(info.started)} |",
          f"| Duration | {elapsed:.1f} s |",
          f"| Host | {platform.system()} {platform.release()} ({platform.machine()}) |",
          f"| Python | {platform.python_version()} |",
          f"| GDB | {gdb_version(args.gdb)} |",
          f"| GDB server | J-Link GDB server from `debug_server.bat`, localhost:{args.port} |",
          "| Target | NXP S32K344 bench board, SWD |",
          (f"| ELF | `{elf.relative_to(REPO) if elf.is_relative_to(REPO) else elf}`, "
           f"{elf_stat.st_size} bytes, built "
           f"{ts(dt.datetime.fromtimestamp(elf_stat.st_mtime, dt.timezone.utc))}, "
           f"sha256 {elf_hash} |" if elf_stat else f"| ELF | `{elf}` missing |"),
          f"| Flash on target | `compare-sections .pflash`: {info.flash_match} |",
          f"| Repo commit | {commit}{dirty} |",
          f"| Settle time per step | {args.settle:.2f} s ({args.settle / 0.1:.0f} runs of the "
          f"100 ms task) |", ""]

    L += ["## Summary", ""]
    if setup_error:
        L += ["The test stopped before it finished, because a setup check failed:", "",
              "```", setup_error, "```", ""]
    L += [f"{len(cases)} test cases.", "",
          "| Outcome | Count |", "|---|---|"]
    L += [f"| {r} | {n} |" for r, n in counts.items() if n]
    L += ["", f"Verdict: {verdict}", ""]

    L += ["## Preconditions and health", "",
          "| Check | Expected | Actual | Result |", "|---|---|---|---|",
          f"| Flash matches the ELF | matched | {info.flash_match} | "
          f"{'PASS' if info.flash_match == 'matched' else 'FAIL'} |",
          f"| Override hook compiled in | sizeof = 12 | {info.ovr_size} | "
          f"{'PASS' if info.ovr_size == 12 else 'FAIL'} |",
          f"| Firmware runs | g_LedCounter changes | {info.led_samples} | "
          f"{'PASS' if info.running else 'FAIL'} |",
          f"| No HardFault before the run | HFSR = 0, CFSR = 0 | "
          f"0x{info.fault_before[0]:08X}, 0x{info.fault_before[1]:08X} | "
          f"{'PASS' if info.fault_before == (0, 0) else 'FAIL'} |"]
    if info.fault_after is not None:
        L += [f"| No HardFault after the run | HFSR = 0, CFSR = 0 | "
              f"0x{info.fault_after[0]:08X}, 0x{info.fault_after[1]:08X} | "
              f"{'PASS' if info.fault_after == (0, 0) else 'FAIL'} |"]
    L += [f"| Override off at exit | Enable = 0, mode = 0 | "
          f"{'written' if info.restored else 'not written'} | "
          f"{'PASS' if info.restored else 'FAIL'} |", ""]

    if info.measured:
        m, b = info.measured, info.baseline
        L += ["Measured inputs and published limits with the override off, before the first "
              "case. HIL-SOP-18 and HIL-SOP-19 use these values.", "",
              "| Min cell mV | Max cell mV | Max temp 0.1 degC | SOC min 0.1 % | SOC max 0.1 % |",
              "|---|---|---|---|---|",
              f"| {m['min_mV']} | {m['max_mV']} | {m['temp_dC']} | {m['socmin']} | {m['socmax']} |",
              "",
              "| Limit | Table_dA | DerateFactor | Final_dA |", "|---|---|---|---|",
              f"| Discharge | {b['D.table']} | {b['D.factor']} | {b['D.final']} |",
              f"| Regen | {b['R.table']} | {b['R.factor']} | {b['R.final']} |",
              f"| Charge | {b['C.table']} | {b['C.factor']} | {b['C.final']} |", "",
              f"Mode {b['mode']}, DerateActiveVLow {b['vlow']}, DerateActiveVHigh {b['vhigh']}, "
              f"DerateActiveTHigh {b['thigh']}, DerateActiveTLow {b['tlow']}, "
              f"InputsValid {b['inputs_valid']}.", ""]

    L += ["## Method", "",
          "The test writes replacement inputs into `g_BmsSopTestOverride` and "
          "`g_BmsSopMode` from a GDB session, then detaches so that the firmware runs. "
          "After the settle time, a second GDB session reads `g_BmsSopData`. Each GDB "
          "attach halts the core for a short time.", "",
          "The override replaces the inputs inside `Bms_Sop_MainFunction()` only. "
          "`Battery_Monitor`, `Bms_Soc` and the SOC saved to NVM keep the measured values.", "",
          "The expected values come from the placeholder maps and derate windows in "
          "`src/battery/Bms_BattCfg.c`, derived by hand. A tolerance of ±1 applies only "
          "where the lookup interpolates between breakpoints, because of integer "
          "rounding. Units: limits in 0.1 A, factors in "
          "0.001, voltages in mV, temperature in 0.1 degC, SOC in 0.1 %.", "",
          "Requirement IDs refer to the validation plan in `src/battery/SOP_DESIGN.md` "
          "section 4.", ""]

    L += ["## Cases", "",
          "| Case | Requirement | Title | Result |", "|---|---|---|---|"]
    for c in cases:
        L += [f"| [{c.id}](#{slug(c.id + ' ' + c.title)}) | {c.requirement} | {c.title} | "
              f"{c.result} |"]
    L += [""]

    for c in cases:
        L += [f"### {c.id} {c.title}", "",
              f"Requirement: {c.requirement}. Result: {c.result}.", "", c.purpose, ""]
        if c.note:
            L += [f"Note: {c.note}", ""]
        L += ["| Enable | MinCell mV | MaxCell mV | MaxTemp 0.1 degC | SocMin 0.1 % | "
              "SocMax 0.1 % | g_BmsSopMode |", "|---|---|---|---|---|---|---|",
              inputs_row(c), ""]
        if c.resolved:
            L += ["| Field | Expected | Actual | Result |", "|---|---|---|---|"]
            for key, exp in c.resolved.items():
                act = c.actual[key]
                L += [f"| {key} | {exp} | {act} | {'PASS' if exp.check(act) else 'FAIL'} |"]
            L += [""]

    L += ["## Limitations", "",
          "- The override enters after `Battery_Monitor` and `Bms_Soc`, so this test does "
          "not cover the signal chain from CAN1 or the ADC (SP-06, SP-07).",
          "- The test does not read the CAN frame `0x30C` (SP-08).",
          "- The maps and derate windows are placeholder calibration. The test checks the "
          "arithmetic and the gating, not that a limit is safe for a real cell.",
          "- Each GDB attach halts the core, which delays the scheduler during the read "
          "and the write.",
          "- Do not use the jlink-mcp memory tools on this board. Each new J-Link "
          "connection runs the S32K344 J-Link script, which fills the application RAM "
          "with 0xDEADBEEF.",
          "- Core register reads through this GDB server return 0xDEADBEEF. The health "
          "checks use `g_LedCounter` and the HardFault capture variables instead.",
          "- Set `BMS_SOP_TEST_OVERRIDE` to `0U` in `Bms_Sop.h` before any build that "
          "goes on a vehicle.", ""]

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(L), encoding="utf-8")


# --------------------------------------------------------------------------------------------------
# Main
# --------------------------------------------------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--elf", type=Path, default=DEFAULT_ELF)
    parser.add_argument("--gdb", type=Path, default=DEFAULT_GDB)
    parser.add_argument("--port", type=int, default=2331)
    parser.add_argument("--settle", type=float, default=0.6,
                        help="seconds the firmware runs after each write (default 0.6)")
    parser.add_argument("--report", type=Path, default=DEFAULT_REPORT)
    args = parser.parse_args()

    cases = case_list()
    info = RunInfo(started=dt.datetime.now(dt.timezone.utc))
    setup_error = None

    try:
        if not args.gdb.exists():
            raise SetupError(f"GDB not found: {args.gdb}. Set S32DS_ROOT or pass --gdb.")
        if not args.elf.exists():
            raise SetupError(f"ELF not found: {args.elf}. Run build.bat first.")
        run_cases(Target(args.gdb, args.elf, args.port), cases, args.settle, info)
    except SetupError as exc:
        setup_error = str(exc)
        print(f"\nSETUP ERROR: {setup_error}", file=sys.stderr)
    finally:
        info.finished = dt.datetime.now(dt.timezone.utc)
        write_report(args.report, cases, info, args, setup_error)
        print(f"\nReport: {args.report}")

    if setup_error:
        return 2
    return 0 if all(c.result == "PASS" for c in cases) else 1


if __name__ == "__main__":
    sys.exit(main())
