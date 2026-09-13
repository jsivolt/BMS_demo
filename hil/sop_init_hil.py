#!/usr/bin/env python3
"""Bms_Sop startup HIL test: reset the MCU and record the SOP time series with J-Link HSS.

The test connects through the J-Link DLL, makes sure that the flash matches the
ELF, resets the MCU and starts J-Link High-Speed Sampling (the J-Scope engine)
right after the reset is released. HSS reads the SOP inputs, their validity
bits, the SOC init source and the published limits every 10 ms while the core
runs, so the startup sequence is recorded without a halt and without any trace
code in the firmware.

Outputs:
  hil/reports/sop_init_trace.csv   one row per HSS sample
  hil/reports/sop_init_trace.html  interactive Plotly plot of the CSV (plot_sop_trace.py)
  hil/reports/sop_init_hil.md      checks, event timeline and findings

Prerequisites:
  1. build.bat and flash.bat were run, so the board runs Debug_FLASH/BMS_demo.elf.
  2. No J-Link GDB server runs. Stop debug_server.bat first.
  3. SEGGER J-Link software is installed (JLink_x64.dll).

Usage:
  python hil/sop_init_hil.py [--duration 20] [--period-ms 10] [--elf ...] [--gdb ...]

Exit status: 0 all checks pass, 1 a check failed or was inconclusive, 2 setup error.

The connect fills the application RAM with 0xDEADBEEF (built-in S32K344 setup
of the J-Link DLL). That is harmless here because the reset follows, but it
means this method cannot watch a board that is already running.
"""

from __future__ import annotations

import argparse
import csv
import datetime as dt
import os
import platform
import re
import struct
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from jlink_hss import DEFAULT_DLL, HSS_TIMESTAMP_BYTES, JLink, JLinkError  # noqa: E402
from plot_sop_trace import write_html  # noqa: E402
from sop_hil import DEFAULT_ELF, DEFAULT_GDB, REPO, SetupError, gdb_version, git, ts  # noqa: E402

DEFAULT_REPORT = REPO / "hil" / "reports" / "sop_init_hil.md"
DEFAULT_CSV = REPO / "hil" / "reports" / "sop_init_trace.csv"

TASK_PERIOD_MS = 100
BLOCK_MERGE_GAP = 64          # bytes: fields closer than this share one HSS block

INIT_SOURCE = {0: "DEFAULT", 1: "OCV", 2: "NVM", 3: "PENDING"}
INIT_PENDING = 3
MODE_NAME = {0: "DISCHARGE", 1: "CHARGE"}
LIMIT_NAME = {"D": "Discharge", "R": "Regen", "C": "Charge"}

# Sampled fields: (key, C expression, struct code). Addresses and sizes come
# from the ELF at run time, and each size must match its struct code.
FIELDS = [
    ("missed_ticks",  "g_BmsSchedulerMissedTickCount",          "I"),
    ("led",           "g_LedCounter",                           "I"),
    ("hf_hfsr",       "g_HardFault_HFSR",                       "I"),
    ("hf_cfsr",       "g_HardFault_CFSR",                       "I"),
    ("min_cell_V",    "g_BatteryData.MinCellVoltage",           "f"),
    ("max_cell_V",    "g_BatteryData.MaxCellVoltage",           "f"),
    ("cell_valid",    "g_BatteryData.CellVoltageValid",         "B"),
    ("max_temp_dC",   "g_BatteryData.MaxPackTemperature_dC",    "h"),
    ("temp_valid",    "g_BatteryData.TemperatureSummaryValid",  "B"),
    ("soc_min_x10",   "g_BmsSocPack.Min.Soc_pct_x10",           "H"),
    ("soc_min_valid", "g_BmsSocPack.Min.Valid",                 "B"),
    ("soc_max_x10",   "g_BmsSocPack.Max.Soc_pct_x10",           "H"),
    ("soc_max_valid", "g_BmsSocPack.Max.Valid",                 "B"),
    ("init_source",   "g_BmsSocPack.InitSource",                "I"),
    ("ocv_wait_ms",   "g_BmsSocOcvWaitTimeout_ms",              "H"),
    ("D_table",       "g_BmsSopData.Discharge.Table_dA",        "H"),
    ("D_factor",      "g_BmsSopData.Discharge.DerateFactor",    "H"),
    ("D_final",       "g_BmsSopData.Discharge.Final_dA",        "H"),
    ("R_table",       "g_BmsSopData.Regen.Table_dA",            "H"),
    ("R_factor",      "g_BmsSopData.Regen.DerateFactor",        "H"),
    ("R_final",       "g_BmsSopData.Regen.Final_dA",            "H"),
    ("C_table",       "g_BmsSopData.Charge.Table_dA",           "H"),
    ("C_factor",      "g_BmsSopData.Charge.DerateFactor",       "H"),
    ("C_final",       "g_BmsSopData.Charge.Final_dA",           "H"),
    ("mode",          "g_BmsSopData.Mode",                      "I"),
    ("vlow",          "g_BmsSopData.DerateActiveVLow",          "B"),
    ("vhigh",         "g_BmsSopData.DerateActiveVHigh",         "B"),
    ("thigh",         "g_BmsSopData.DerateActiveTHigh",         "B"),
    ("tlow",          "g_BmsSopData.DerateActiveTLow",          "B"),
    ("inputs_valid",  "g_BmsSopData.InputsValid",               "B"),
    ("ovr_enable",    "g_BmsSopTestOverride.Enable",            "B"),
]

BOOL_KEYS = ("cell_valid", "temp_valid", "soc_min_valid", "soc_max_valid", "vlow", "vhigh", "thigh",
             "tlow", "inputs_valid")
INPUT_KEYS = ("min_cell_V", "max_cell_V", "max_temp_dC", "soc_min_x10", "soc_max_x10",
              "cell_valid", "temp_valid", "soc_min_valid", "soc_max_valid", "init_source",
              "ovr_enable")
OUTPUT_KEYS = ("D_table", "D_factor", "D_final", "R_table", "R_factor", "R_final",
               "C_table", "C_factor", "C_final", "mode", "vlow", "vhigh", "thigh", "tlow",
               "inputs_valid")
LIMIT_KEYS = OUTPUT_KEYS[:9]

MARKER = re.compile(r"@@([\w.]+)=(-?\d+)")


# --------------------------------------------------------------------------------------------------
# ELF
# --------------------------------------------------------------------------------------------------

def gdb_offline(gdb: Path, elf: Path, commands: list[str]) -> str:
    """Runs GDB against the ELF file only. No target, so nothing touches the board."""
    fd, path = tempfile.mkstemp(suffix=".gdb", prefix="sop_init_")
    try:
        with os.fdopen(fd, "w", encoding="ascii") as f:
            f.write("\n".join(["set pagination off", *commands]) + "\n")
        proc = subprocess.run([str(gdb), "-batch", "-nx", "-x", path, str(elf)],
                              capture_output=True, text=True, timeout=60)
    finally:
        os.unlink(path)
    return proc.stdout + proc.stderr


@dataclass
class Layout:
    fields: dict[str, tuple[int, int, str]]              # key -> (address, size, code)
    blocks: list[tuple[int, int]] = field(default_factory=list)
    offsets: dict[str, int] = field(default_factory=dict)  # key -> offset in one sample
    sample_size: int = 0


def build_layout(gdb: Path, elf: Path) -> Layout:
    cmds = []
    for key, expr, _ in FIELDS:
        cmds.append(f'printf "@@{key}.addr=%lu\\n", (unsigned long)&({expr})')
        cmds.append(f'printf "@@{key}.size=%d\\n", (int)sizeof({expr})')
    out = gdb_offline(gdb, elf, cmds)
    values = {m.group(1): int(m.group(2)) for m in MARKER.finditer(out)}
    layout = Layout(fields={})
    for key, expr, code in FIELDS:
        if f"{key}.addr" not in values:
            raise SetupError(f"The ELF has no {expr}.\n{out.strip()}")
        size = values[f"{key}.size"]
        if size != struct.calcsize("<" + code):
            raise SetupError(f"{expr} is {size} bytes, the script expects "
                             f"{struct.calcsize('<' + code)}. Update FIELDS.")
        layout.fields[key] = (values[f"{key}.addr"], size, code)

    spans = sorted((addr, addr + size) for addr, size, _ in layout.fields.values())
    merged: list[list[int]] = []
    for start, end in spans:
        if merged and start - merged[-1][1] <= BLOCK_MERGE_GAP:
            merged[-1][1] = max(merged[-1][1], end)
        else:
            merged.append([start, end])
    layout.blocks = [(start, end - start) for start, end in merged]

    base = HSS_TIMESTAMP_BYTES
    block_base = []
    for addr, length in layout.blocks:
        block_base.append((addr, length, base))
        base += length
    layout.sample_size = base
    for key, (addr, _, _) in layout.fields.items():
        for b_addr, b_len, b_off in block_base:
            if b_addr <= addr < b_addr + b_len:
                layout.offsets[key] = b_off + addr - b_addr
    return layout


def elf_section(gdb: Path, elf: Path, name: str, dump_path: Path) -> tuple[int, bytes]:
    out = gdb_offline(gdb, elf, ["info files"])
    m = re.search(rf"(0x[0-9a-f]+) - (0x[0-9a-f]+) is {re.escape(name)}\s*$", out, re.M)
    if not m:
        raise SetupError(f"The ELF has no {name} section.")
    start, end = int(m.group(1), 16), int(m.group(2), 16)
    gdb_offline(gdb, elf, [f"dump binary memory {dump_path.as_posix()} {start} {end}"])
    return start, dump_path.read_bytes()


def gdb_server_running() -> bool:
    try:
        out = subprocess.run(["tasklist", "/NH"], capture_output=True, text=True, timeout=15).stdout
    except (OSError, subprocess.SubprocessError):
        return False
    return "JLinkGDBServer" in out


# --------------------------------------------------------------------------------------------------
# Decode
# --------------------------------------------------------------------------------------------------

def decode(raw: bytes, layout: Layout) -> list[dict]:
    samples = []
    for i in range(len(raw) // layout.sample_size):
        base = i * layout.sample_size
        s = {"time_ms": struct.unpack_from("<I", raw, base)[0] / 1000.0}
        for key, (_, _, code) in layout.fields.items():
            s[key] = struct.unpack_from("<" + code, raw, base + layout.offsets[key])[0]
        s["min_cell_mV"] = int(round(s["min_cell_V"] * 1000)) if 0 <= s["min_cell_V"] < 65.535 else -1
        s["max_cell_mV"] = int(round(s["max_cell_V"] * 1000)) if 0 <= s["max_cell_V"] < 65.535 else -1
        # Before the startup code clears .bss, RAM still holds the connect-time fill.
        s["ram_ready"] = (all(s[k] in (0, 1) for k in BOOL_KEYS)
                          and s["init_source"] in INIT_SOURCE and s["mode"] in MODE_NAME
                          and s["missed_ticks"] != 0xDEADBEEF)
        samples.append(s)
    return samples


# --------------------------------------------------------------------------------------------------
# Analysis
# --------------------------------------------------------------------------------------------------

@dataclass
class Check:
    id: str
    title: str
    expected: str
    actual: str = ""
    result: str = "NOT RUN"


def inputs_valid(s: dict, limit: str) -> bool:
    soc_valid = s["soc_min_valid"] if limit == "D" else s["soc_max_valid"]
    return bool(s["cell_valid"] and s["temp_valid"] and soc_valid
                and s["init_source"] != INIT_PENDING)


def all_valid(s: dict) -> bool:
    return inputs_valid(s, "D") and inputs_valid(s, "R")


def invalid_inputs(s: dict, limit: str) -> list[str]:
    names = []
    if not s["cell_valid"]:
        names.append("cell voltages")
    if not s["temp_valid"]:
        names.append("temperature")
    if not (s["soc_min_valid"] if limit == "D" else s["soc_max_valid"]):
        names.append("SocMin" if limit == "D" else "SocMax")
    if s["init_source"] == INIT_PENDING:
        names.append("SOC init pending")
    return names


def first(samples: list[dict], pred) -> dict | None:
    return next((s for s in samples if pred(s)), None)


def at(s: dict | None) -> str:
    return f"{s['time_ms']:.0f} ms" if s else "never in the capture"


def first_update(samples: list[dict]) -> dict | None:
    """First sample after the first Bms_Sop_MainFunction call: Init leaves every table and factor 0."""
    return first(samples, lambda s: any(s[k] for k in ("D_table", "D_factor", "R_table",
                                                       "R_factor", "C_table", "C_factor")))


def consistent(s: dict) -> bool:
    """Every Final_dA equals Table_dA x DerateFactor / 1000, or 0 where the mode or the gate forces it."""
    for p in "DRC":
        forced = ((p == "C") != (s["mode"] == 1)) or not s["inputs_valid"]
        expect = 0 if forced else s[f"{p}_table"] * s[f"{p}_factor"] // 1000
        if s[f"{p}_final"] != expect:
            return False
    return True


def run_checks(samples: list[dict], ready: list[dict], args, period_ms: float) -> list[Check]:
    checks = []

    c = Check("INIT-01", "HSS sampling covered the whole capture",
              f"at least 90 % of {args.duration / period_ms * 1000:.0f} samples, no gap "
              f"of {TASK_PERIOD_MS} ms or more")
    gaps = [b["time_ms"] - a["time_ms"] for a, b in zip(samples, samples[1:])]
    max_gap = max(gaps) if gaps else 0.0
    expected = args.duration * 1000 / period_ms
    c.actual = f"{len(samples)} samples, largest gap {max_gap:.1f} ms"
    c.result = "PASS" if len(samples) >= 0.9 * expected and max_gap < TASK_PERIOD_MS else "FAIL"
    checks.append(c)

    last = ready[-1] if ready else None
    c = Check("INIT-02", "Sampling did not disturb the scheduler",
              "g_BmsSchedulerMissedTickCount = 0 at the end")
    c.actual = f"{last['missed_ticks']}" if last else "no sample after RAM init"
    c.result = "PASS" if last and last["missed_ticks"] == 0 else "FAIL"
    checks.append(c)

    c = Check("INIT-03", "Firmware runs with no HardFault",
              "g_LedCounter changes in the last second, HFSR = 0, CFSR = 0")
    if last:
        tail = [s["led"] for s in ready if s["time_ms"] >= last["time_ms"] - 1000]
        moving = len(set(tail)) > 1
        c.actual = (f"g_LedCounter {'changes' if moving else 'stuck'}, "
                    f"HFSR 0x{last['hf_hfsr']:08X}, CFSR 0x{last['hf_cfsr']:08X}")
        c.result = "PASS" if moving and last["hf_hfsr"] == 0 and last["hf_cfsr"] == 0 else "FAIL"
    checks.append(c)

    c = Check("INIT-04", "No test override during the capture",
              "g_BmsSopTestOverride.Enable = 0 in every sample")
    hits = [s for s in ready if s["ovr_enable"]]
    c.actual = f"not 0 in {len(hits)} samples"
    c.result = "PASS" if ready and not hits else "FAIL"
    checks.append(c)

    upd = first_update(ready)
    c = Check("INIT-05", "Limits are 0 until the first Bms_Sop_MainFunction call",
              "every Table_dA, DerateFactor and Final_dA is 0 before the first derate factor appears")
    before = [s for s in ready if upd is None or s["time_ms"] < upd["time_ms"]]
    early = [s for s in before if any(s[k] for k in LIMIT_KEYS)]
    c.actual = (f"{len(before)} samples before {at(upd)}, "
                f"{len(early)} with a non-zero limit field")
    c.result = "PASS" if upd and not early else "FAIL"
    checks.append(c)

    c = Check("INIT-06", "SOC init leaves PENDING within the OCV wait budget",
              "PENDING lasts no longer than g_BmsSocOcvWaitTimeout_ms + one 100 ms task period")
    pending = [s for s in ready if s["init_source"] == INIT_PENDING]
    budget = (last["ocv_wait_ms"] if last else 0) + TASK_PERIOD_MS
    if not pending:
        seeded = first(ready, lambda s: s["init_source"] != 0 or s["soc_min_valid"])
        src = INIT_SOURCE.get(seeded["init_source"], "?") if seeded else "?"
        c.actual = f"never PENDING in any sample, seeded from {src} at {at(seeded)}"
        c.result = "PASS" if seeded else "INCONCLUSIVE"
    else:
        after = first([s for s in ready if s["time_ms"] > pending[0]["time_ms"]],
                      lambda s: s["init_source"] != INIT_PENDING)
        if after:
            span = after["time_ms"] - pending[0]["time_ms"]
            c.actual = (f"PENDING for {span:.0f} ms, then "
                        f"{INIT_SOURCE.get(after['init_source'], '?')} (budget {budget} ms)")
            c.result = "PASS" if span <= budget else "FAIL"
        else:
            c.actual = "still PENDING at the end of the capture"
            c.result = "FAIL"
    checks.append(c)

    c = Check("INIT-07", "Outputs are stateless",
              "for three consecutive samples with identical inputs, the outputs of the "
              "last two are identical")
    compared, differ = 0, []
    post = [s for s in ready if upd and s["time_ms"] >= upd["time_ms"]]
    for a, b, d in zip(post, post[1:], post[2:]):
        if all(a[k] == b[k] == d[k] for k in INPUT_KEYS):
            compared += 1
            if any(b[k] != d[k] for k in OUTPUT_KEYS):
                differ.append(d["time_ms"])
    c.actual = f"{compared} triples compared, {len(differ)} differ" + \
               (f" (first at {differ[0]:.0f} ms)" if differ else "")
    c.result = "FAIL" if differ else ("PASS" if compared else "INCONCLUSIVE")
    checks.append(c)

    c = Check("INIT-08", "HSS samples are consistent",
              "Final_dA = Table_dA x DerateFactor / 1000, or 0 where the mode or an "
              "invalid input forces it, except single samples read in the middle of an update")
    bad = [i for i, s in enumerate(post) if not consistent(s)]
    runs = [i for i in bad if i + 1 in bad]
    c.actual = f"{len(bad)} of {len(post)} samples inconsistent, {len(runs)} in a row"
    c.result = "PASS" if post and not runs and len(bad) <= max(1, len(post) // 100) else "FAIL"
    checks.append(c)

    c = Check("INIT-09", "No limit is published from an input that is not valid (SOP-FR-12)",
              "Final_dA = 0 while the cell voltages, the temperature or the SOC init are not "
              "valid, except single samples read in the middle of an update")
    idx = {id(s): i for i, s in enumerate(post)}
    hits = sorted({idx[id(s)] for w in invalid_input_windows(post) for s in w.samples})
    runs = [i for i in hits if i + 1 in hits]
    c.actual = f"{len(hits)} of {len(post)} samples with a limit above 0, {len(runs)} in a row"
    c.result = "PASS" if post and not runs else "FAIL"
    checks.append(c)

    c = Check("INIT-10", "InputsValid matches the input validity bits",
              "g_BmsSopData.InputsValid equals the cell, temperature and SOC init validity, "
              "except single samples read in the middle of an update")
    bad = [i for i, s in enumerate(post) if bool(s["inputs_valid"]) != all_valid(s)]
    runs = [i for i in bad if i + 1 in bad]
    c.actual = f"{len(bad)} of {len(post)} samples differ, {len(runs)} in a row"
    c.result = "PASS" if post and not runs else "FAIL"
    checks.append(c)

    return checks


@dataclass
class Window:
    limit: str
    samples: list[dict] = field(default_factory=list)
    peak: dict | None = None
    reference: dict | None = None
    invalid: set[str] = field(default_factory=set)


def invalid_input_windows(post: list[dict]) -> list[Window]:
    windows = []
    for limit in "DRC":
        w = Window(limit)
        for s in post:
            if s[f"{limit}_final"] > 0 and not inputs_valid(s, limit):
                w.samples.append(s)
                w.invalid.update(invalid_inputs(s, limit))
                if w.peak is None or s[f"{limit}_final"] > w.peak[f"{limit}_final"]:
                    w.peak = s
        # Reference: two samples after the inputs turn valid, so Bms_Sop has run on them.
        idx = next((i for i, s in enumerate(post) if inputs_valid(s, limit)), None)
        if idx is not None:
            w.reference = post[min(idx + 2, len(post) - 1)]
        windows.append(w)
    return windows


# --------------------------------------------------------------------------------------------------
# Output files
# --------------------------------------------------------------------------------------------------

CSV_COLUMNS = ["time_ms", "missed_ticks", "min_cell_mV", "max_cell_mV", "max_temp_dC",
               "soc_min_x10", "soc_max_x10", "cell_valid", "temp_valid", "soc_min_valid",
               "soc_max_valid", "override", "init_source", "D_table", "D_factor", "D_final",
               "R_table", "R_factor", "R_final", "C_table", "C_factor", "C_final", "mode",
               "vlow", "vhigh", "thigh", "tlow", "inputs_valid"]


def write_csv(path: Path, ready: list[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(CSV_COLUMNS)
        for s in ready:
            row = dict(s, time_ms=f"{s['time_ms']:.1f}", override=int(s["ovr_enable"] != 0),
                       init_source=INIT_SOURCE.get(s["init_source"], s["init_source"]))
            w.writerow([row[c] for c in CSV_COLUMNS])


def flags_text(s: dict) -> str:
    return "".join(ch if s[k] else "-" for ch, k in (("C", "cell_valid"), ("T", "temp_valid"),
                                                     ("m", "soc_min_valid"),
                                                     ("M", "soc_max_valid")))


def derate_text(s: dict) -> str:
    names = [n for n, k in (("VLow", "vlow"), ("VHigh", "vhigh"), ("THigh", "thigh"),
                            ("TLow", "tlow")) if s[k]]
    if not s["inputs_valid"]:
        names.append("inputs not valid")
    return ", ".join(names) if names else "-"


def sample_row(s: dict) -> str:
    return (f"| {s['time_ms']:.0f} | {s['min_cell_mV']} | {s['max_cell_mV']} | "
            f"{s['max_temp_dC']} | {s['soc_min_x10']} | {s['soc_max_x10']} | {flags_text(s)} | "
            f"{INIT_SOURCE.get(s['init_source'], s['init_source'])} | "
            f"{s['D_table']} / {s['D_factor']} / {s['D_final']} | "
            f"{s['R_table']} / {s['R_factor']} / {s['R_final']} | "
            f"{s['C_table']} / {s['C_factor']} / {s['C_final']} | "
            f"{MODE_NAME.get(s['mode'], s['mode'])} | {derate_text(s)} |")


SAMPLE_HEADER = [
    "| t ms | MinCell mV | MaxCell mV | MaxTemp 0.1 degC | SocMin 0.1 % | SocMax 0.1 % | "
    "Valid | SOC init | Discharge tbl / k / final | Regen tbl / k / final | "
    "Charge tbl / k / final | Mode | Derate or gate |",
    "|---|---|---|---|---|---|---|---|---|---|---|---|---|",
]

SHOWN_KEYS = ("min_cell_mV", "max_cell_mV", "max_temp_dC", "soc_min_x10", "soc_max_x10",
              "cell_valid", "temp_valid", "soc_min_valid", "soc_max_valid", "init_source",
              *OUTPUT_KEYS)
DISCRETE_KEYS = ("cell_valid", "temp_valid", "soc_min_valid", "soc_max_valid", "init_source",
                 "mode", "vlow", "vhigh", "thigh", "tlow", "inputs_valid")


@dataclass
class RunInfo:
    started: dt.datetime
    finished: dt.datetime | None = None
    dll_version: str = "unknown"
    hss_caps: dict = field(default_factory=dict)
    flash_match: str = "not checked"
    layout: Layout | None = None
    hss_latency_ms: float = 0.0
    raw_bytes: int = 0
    pre_ram: int = 0
    period_ms: float = 0.0


def write_report(path: Path, args: argparse.Namespace, info: RunInfo, ready: list[dict],
                 checks: list[Check], windows: list[Window], setup_error: str | None) -> None:
    commit = git("rev-parse", "--short", "HEAD") or "unknown"
    dirty = " (working tree has uncommitted changes)" if git(
        "status", "--porcelain", "--untracked-files=no") else ""
    results = [c.result for c in checks]
    if setup_error or not checks or "FAIL" in results or "NOT RUN" in results:
        verdict = "FAIL"
    elif "INCONCLUSIVE" in results:
        verdict = "INCONCLUSIVE"
    else:
        verdict = "PASS"
    elapsed = (info.finished - info.started).total_seconds() if info.finished else 0.0
    elf = args.elf
    lay = info.layout
    html_name = args.csv.with_suffix(".html").name

    L: list[str] = []
    L += ["# Bms_Sop Startup HIL Test Report", "",
          "> Generated by `hil/sop_init_hil.py`. Do not edit by hand. Run the script again to "
          "update it.", "",
          f"Time series: [{args.csv.name}]({args.csv.name}), plot: [{html_name}]({html_name})", "",
          "## Execution environment", "",
          "| | |", "|---|---|",
          f"| Executed (UTC) | {ts(info.started)} |",
          f"| Duration | {elapsed:.1f} s |",
          f"| Host | {platform.system()} {platform.release()} ({platform.machine()}) |",
          f"| Python | {platform.python_version()} |",
          f"| GDB (ELF symbol queries only) | {gdb_version(args.gdb)} |",
          f"| J-Link DLL | V{info.dll_version}, `{args.jlink_dll}` |",
          f"| HSS capability | {info.hss_caps.get('max_blocks', '-')} blocks, "
          f"{info.hss_caps.get('max_freq_hz', '-')} Hz maximum |",
          f"| Target | {args.device} bench board, SWD {args.speed} kHz |",
          f"| ELF | `{elf.relative_to(REPO) if elf.is_relative_to(REPO) else elf}` |",
          f"| Flash on target | {info.flash_match} |",
          f"| Repo commit | {commit}{dirty} |",
          f"| Sampling | period {args.period_ms} ms, {args.duration:.0f} s, "
          + (f"{len(lay.blocks)} blocks, {lay.sample_size} bytes per sample |" if lay else "- |"),
          f"| HSS start | {info.hss_latency_ms:.1f} ms after the reset was released |", ""]

    L += ["## Summary", ""]
    if setup_error:
        L += ["The test stopped before it finished, because a setup check failed:", "",
              "```", setup_error, "```", ""]
    L += [f"Verdict: {verdict}", "",
          "| Check | Title | Expected | Actual | Result |", "|---|---|---|---|---|"]
    L += [f"| {c.id} | {c.title} | {c.expected} | {c.actual} | {c.result} |" for c in checks]
    L += [""]

    if ready:
        upd = first_update(ready)
        post = [s for s in ready if upd and s["time_ms"] >= upd["time_ms"]]
        events = [
            ("RAM cleared by the startup code (first readable sample)", ready[0]),
            ("Scheduler runs (g_LedCounter first > 0)", first(ready, lambda s: s["led"] > 0)),
            ("SOC estimators seeded", first(ready, lambda s: s["init_source"] != 0
                                            or s["soc_min_valid"] or s["soc_min_x10"])),
            ("First Bms_Sop_MainFunction result", upd),
            ("TemperatureSummaryValid first TRUE", first(ready, lambda s: s["temp_valid"])),
            ("CellVoltageValid first TRUE", first(ready, lambda s: s["cell_valid"])),
            ("SOC Min.Valid and Max.Valid first TRUE",
             first(ready, lambda s: s["soc_min_valid"] and s["soc_max_valid"])),
            ("All SOP inputs valid", first(post, all_valid)),
            ("Discharge Final_dA first > 0", first(ready, lambda s: s["D_final"] > 0)),
            ("Regen Final_dA first > 0", first(ready, lambda s: s["R_final"] > 0)),
        ]
        events.sort(key=lambda e: (e[1] is None, e[1]["time_ms"] if e[1] else 0.0))
        L += ["## Event timeline", "",
              f"Time counts from the start of HSS sampling, {info.hss_latency_ms:.1f} ms after "
              f"the reset was released. Each time is the first sample that shows the event, "
              f"so the event itself happened up to {args.period_ms} ms earlier.", "",
              "| Event | Time |", "|---|---|"]
        L += [f"| {name} | {at(s)} |" for name, s in events]
        L += [""]

        L += ["## Limits published from inputs that were not valid", "",
              "SOP-FR-12 requires every limit to be 0 while the cell voltages, the temperature "
              "or the SOC init are not valid. This table lists where a limit was above 0 anyway. "
              "The reference is the same limit two samples after all of its inputs became valid. "
              "INIT-09 fails on two such samples in a row.",
              "",
              "| Limit | Time span | Samples | Inputs not valid | Peak Final_dA (time) | "
              "Reference Final_dA (time) | Peak vs reference |",
              "|---|---|---|---|---|---|---|"]
        for w in windows:
            ref = w.reference
            key = f"{w.limit}_final"
            ref_text = f"{ref[key]} ({at(ref)})" if ref else "none"
            if w.samples:
                ratio = f"{w.peak[key] / ref[key]:.2f} x" if ref and ref[key] else "reference is 0"
                L += [f"| {LIMIT_NAME[w.limit]} | {w.samples[0]['time_ms']:.0f} to "
                      f"{w.samples[-1]['time_ms']:.0f} ms | {len(w.samples)} | "
                      f"{', '.join(sorted(w.invalid))} | {w.peak[key]} ({at(w.peak)}) | "
                      f"{ref_text} | {ratio} |"]
            else:
                L += [f"| {LIMIT_NAME[w.limit]} | - | 0 | - | - | {ref_text} | - |"]
        L += [""]

        closed = [s for s in post if not s["inputs_valid"]]
        if closed:
            L += [f"The gate held the limits at 0 in {len(closed)} samples, from "
                  f"{closed[0]['time_ms']:.0f} to {closed[-1]['time_ms']:.0f} ms.", ""]

        settle = first(post, all_valid)
        settle_idx = ready.index(settle) if settle else len(ready) - 1
        changes = [ready[0]] + [b for a, b in zip(ready, ready[1:])
                                if any(a[k] != b[k] for k in SHOWN_KEYS)]
        startup = [s for s in changes if s["time_ms"] <= ready[min(settle_idx + 2,
                                                                   len(ready) - 1)]["time_ms"]]
        L += ["## Startup samples", "",
              "The first readable sample, then every sample where a shown value changed, until "
              "two samples after all inputs were valid. Valid: C cell voltages, T temperature, "
              "m SocMin, M SocMax, and '-' where the bit is FALSE. Limits in 0.1 A, k in 0.001.",
              ""]
        L += SAMPLE_HEADER + [sample_row(s) for s in startup[:40]]
        if len(startup) > 40:
            L += ["", f"{len(startup) - 40} more rows are in the CSV file."]
        L += [""]

        end_t = startup[-1]["time_ms"] if startup else 0.0
        later = [b for a, b in zip(ready, ready[1:])
                 if b["time_ms"] > end_t and any(a[k] != b[k] for k in DISCRETE_KEYS)]
        L += ["## Later discrete events", "",
              f"Samples after {end_t:.0f} ms where a validity bit, the SOC init source, the "
              f"mode or a derate flag changed: {len(later)}."
              + (" The first 40 are shown." if len(later) > 40 else ""), ""]
        L += (SAMPLE_HEADER + [sample_row(s) for s in later[:40]]) if later else ["No change."]
        L += [""]

        if settle:
            steady = ready[settle_idx:]
            L += ["## Steady-state variation", "",
                  f"From {settle['time_ms']:.0f} ms, the first sample with all inputs valid, to "
                  f"the end. `Bms_Sop` has no filter, so input noise reaches the published "
                  f"limits directly.", "",
                  "| Signal | Min | Max | Span |", "|---|---|---|---|"]
            for key, label in (("min_cell_mV", "MinCell mV"), ("max_cell_mV", "MaxCell mV"),
                               ("max_temp_dC", "MaxTemp 0.1 degC"),
                               ("soc_min_x10", "SocMin 0.1 %"), ("soc_max_x10", "SocMax 0.1 %"),
                               ("D_final", "Discharge Final_dA"), ("R_final", "Regen Final_dA"),
                               ("C_table", "Charge Table_dA"), ("C_final", "Charge Final_dA")):
                values = [s[key] for s in steady]
                L += [f"| {label} | {min(values)} | {max(values)} | {max(values) - min(values)} |"]
            L += [""]

        L += ["## Last sample", ""] + SAMPLE_HEADER + [sample_row(ready[-1]), ""]

    L += ["## Method", "",
          "The script opens the J-Link through `JLink_x64.dll` and connects with device name "
          f"`{args.device}`. The connect halts the core and fills the application RAM with "
          "0xDEADBEEF. The script reads the `.pflash` section back and compares it with the "
          "ELF, then resets the MCU, releases it and starts J-Link High-Speed Sampling (HSS) "
          "at once. HSS is the engine behind SEGGER J-Scope: the J-Link reads the listed "
          "memory blocks at a fixed period while the core runs, and tags each sample with a "
          "microsecond timestamp. The core is never halted after the reset, and the firmware "
          "has no trace code.", "",
          "Variable addresses and sizes come from the ELF through GDB, which runs on the ELF "
          "file only and never connects to the board. Fields closer than "
          f"{BLOCK_MERGE_GAP} bytes share one HSS block. Samples taken before the startup "
          f"code cleared the RAM are dropped: {info.pre_ram} in this run.", "",
          "## Limitations", "",
          "- A J-Link connect with this device name clears the RAM, so the method needs a "
          "reset. It cannot watch a board that is already running.",
          f"- An event can happen up to {args.period_ms} ms before the sample that shows it. "
          "A sample can also land between two writes of one update, which INIT-09 counts.",
          "- The inputs are the values `Battery_Monitor` and `Bms_Soc` report. The capture "
          "does not show why an input is not valid, for example a missing vAFE frame.",
          "- The J-Link reset goes through the S32K344 J-Link setup. A power-on reset can "
          "differ in the analog and CAN timing.",
          "- The test does not read the CAN frame `0x30C`.",
          "- The J-Link GDB server and HSS were not tested together, so the script refuses to "
          "run while a GDB server runs.", ""]

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(L), encoding="utf-8")


# --------------------------------------------------------------------------------------------------
# Main
# --------------------------------------------------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--elf", type=Path, default=DEFAULT_ELF)
    parser.add_argument("--gdb", type=Path, default=DEFAULT_GDB,
                        help="arm-none-eabi-gdb, used offline for ELF symbol queries")
    parser.add_argument("--jlink-dll", type=Path, default=DEFAULT_DLL)
    parser.add_argument("--device", default="S32K344")
    parser.add_argument("--speed", type=int, default=4000, help="SWD speed in kHz")
    parser.add_argument("--usb-sn", type=int, default=None, help="J-Link serial number")
    parser.add_argument("--duration", type=float, default=20.0, help="capture length in s")
    parser.add_argument("--period-ms", type=int, default=10, help="HSS sample period in ms")
    parser.add_argument("--report", type=Path, default=DEFAULT_REPORT)
    parser.add_argument("--csv", type=Path, default=DEFAULT_CSV)
    args = parser.parse_args()

    info = RunInfo(started=dt.datetime.now(dt.timezone.utc))
    ready: list[dict] = []
    checks: list[Check] = []
    windows: list[Window] = []
    setup_error = None

    try:
        if not args.gdb.exists():
            raise SetupError(f"GDB not found: {args.gdb}. Set S32DS_ROOT or pass --gdb.")
        if not args.elf.exists():
            raise SetupError(f"ELF not found: {args.elf}. Run build.bat first.")
        if gdb_server_running():
            raise SetupError("A J-Link GDB server is running. Stop debug_server.bat first, "
                             "because HSS and the GDB server were not tested together.")

        info.layout = build_layout(args.gdb, args.elf)
        with tempfile.TemporaryDirectory(prefix="sop_init_") as tmp:
            flash_addr, flash_image = elf_section(args.gdb, args.elf, ".pflash",
                                                  Path(tmp) / "pflash.bin")

        jlink = JLink(args.jlink_dll, args.device, args.speed, args.usb_sn)
        info.dll_version = jlink.dll_version
        raw = bytearray()
        try:
            print(f"Connecting (J-Link DLL V{info.dll_version}) ...", flush=True)
            jlink.open()
            info.hss_caps = jlink.hss_caps()
            if len(info.layout.blocks) > info.hss_caps["max_blocks"]:
                raise SetupError(f"{len(info.layout.blocks)} HSS blocks needed, the J-Link "
                                 f"supports {info.hss_caps['max_blocks']}.")
            if 1000 / args.period_ms > info.hss_caps["max_freq_hz"]:
                raise SetupError(f"A {args.period_ms} ms period is above the HSS maximum of "
                                 f"{info.hss_caps['max_freq_hz']} Hz.")

            on_target = jlink.read(flash_addr, len(flash_image))
            info.flash_match = ("matched" if on_target == flash_image else
                                f"MISMATCH at 0x{flash_addr + next(i for i, (a, b) in enumerate(zip(on_target, flash_image)) if a != b):08X}")
            if info.flash_match != "matched":
                raise SetupError(f".pflash on the target does not match {args.elf} "
                                 f"({info.flash_match}). Flash the ELF first.")

            print(f"Resetting and sampling for {args.duration:.0f} s every {args.period_ms} ms "
                  "without halting ...", flush=True)
            jlink.reset_and_go()
            t_go = time.perf_counter()
            jlink.hss_start(info.layout.blocks, args.period_ms * 1000)
            info.hss_latency_ms = (time.perf_counter() - t_go) * 1000
            while time.perf_counter() - t_go < args.duration:
                raw += jlink.hss_read()
                time.sleep(0.02)
            jlink.hss_stop()
            raw += jlink.hss_read()
            if jlink.is_halted():
                jlink.go()
        finally:
            jlink.close()

        info.raw_bytes = len(raw)
        if len(raw) % info.layout.sample_size:
            print(f"Warning: {len(raw) % info.layout.sample_size} trailing bytes dropped.",
                  file=sys.stderr)
        samples = decode(bytes(raw), info.layout)
        ready = [s for s in samples if s["ram_ready"]]
        info.pre_ram = len(samples) - len(ready)
        if not ready:
            raise SetupError(f"No readable sample in {len(samples)} samples.")

        checks = run_checks(samples, ready, args, args.period_ms)
        upd = first_update(ready)
        windows = invalid_input_windows([s for s in ready if upd and s["time_ms"] >= upd["time_ms"]])
        for c in checks:
            print(f"{c.id}  {c.title} ... {c.result} ({c.actual})")
        write_csv(args.csv, ready)
        write_html(args.csv, args.csv.with_suffix(".html"),
                   subtitle=f"{len(ready)} HSS samples every {args.period_ms} ms, "
                            f"reset at {ts(info.started)} UTC",
                   x_title=f"Time since HSS start, {info.hss_latency_ms:.0f} ms after reset release (ms)",
                   sample_note=f"One J-Link HSS sample every {args.period_ms} ms, read while the "
                               "core runs")
    except (SetupError, JLinkError) as exc:
        setup_error = str(exc)
        print(f"\nSETUP ERROR: {setup_error}", file=sys.stderr)
    finally:
        info.finished = dt.datetime.now(dt.timezone.utc)
        write_report(args.report, args, info, ready, checks, windows, setup_error)
        print(f"\nReport: {args.report}")
        if ready and setup_error is None:
            print(f"CSV:    {args.csv}")
            print(f"Plot:   {args.csv.with_suffix('.html')}")

    if setup_error:
        return 2
    return 0 if checks and all(c.result == "PASS" for c in checks) else 1


if __name__ == "__main__":
    sys.exit(main())
