"""Shared bench access for the Bms_Sop HIL tests.

ELF symbol queries run through arm-none-eabi-gdb on the ELF file only; GDB
never connects to the board. The board is reached through pylink-square, the
Python wrapper for the SEGGER J-Link DLL: connect, flash check, reset, and
memory reads and writes while the core runs. pylink has no Python methods for
J-Link High-Speed Sampling (HSS, the engine behind J-Scope), so Bench calls
those four DLL functions through pylink's DLL handle.

Connecting with device name S32K344 halts the core and runs the DLL's
built-in S32K344 setup, which fills the application RAM with 0xDEADBEEF.
Bench.reset_and_run() must follow every connect. The same applies to the
jlink-mcp memory tools, so do not use them on a running board.
"""

from __future__ import annotations

import argparse
import ctypes
import datetime as dt
import hashlib
import os
import re
import struct
import subprocess
import tempfile
import time
from dataclasses import dataclass, field
from pathlib import Path

import pylink

REPO = Path(__file__).resolve().parents[1]
DEFAULT_ELF = REPO / "Debug_FLASH" / "BMS_demo.elf"
DEFAULT_GDB = (Path(os.environ.get("S32DS_ROOT", r"C:\NXP\S32DS.3.6.10\S32DS"))
               / "tools" / "gdb-arm" / "arm32-eabi" / "bin" / "arm-none-eabi-gdb.exe")
DEFAULT_DLL = Path(r"C:\Program Files\SEGGER\JLink\JLink_x64.dll")

HSS_FLAG_TIMESTAMP_US = 1
HSS_TIMESTAMP_BYTES = 4
BLOCK_MERGE_GAP = 64          # bytes: fields closer than this share one memory block

MARKER = re.compile(r"@@([\w.]+)=(-?\d+)")


class SetupError(RuntimeError):
    """The test cannot run: missing tool, no probe, wrong firmware."""


# --------------------------------------------------------------------------------------------------
# Host helpers
# --------------------------------------------------------------------------------------------------

def git(*args: str) -> str:
    try:
        return subprocess.run(["git", *args], cwd=REPO, capture_output=True, text=True,
                              timeout=15).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        return ""


def ts(t: dt.datetime) -> str:
    return t.strftime("%Y-%m-%d %H:%M:%S")


def gdb_version(gdb: Path) -> str:
    try:
        out = subprocess.run([str(gdb), "--version"], capture_output=True, text=True, timeout=15)
        return out.stdout.splitlines()[0].strip()
    except (OSError, subprocess.SubprocessError, IndexError):
        return "unknown"


def gdb_server_running() -> bool:
    try:
        out = subprocess.run(["tasklist", "/NH"], capture_output=True, text=True, timeout=15).stdout
    except (OSError, subprocess.SubprocessError):
        return False
    return "JLinkGDBServer" in out


def add_target_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--elf", type=Path, default=DEFAULT_ELF)
    parser.add_argument("--gdb", type=Path, default=DEFAULT_GDB,
                        help="arm-none-eabi-gdb, used offline for ELF symbol queries")
    parser.add_argument("--jlink-dll", type=Path, default=DEFAULT_DLL)
    parser.add_argument("--device", default="S32K344")
    parser.add_argument("--speed", type=int, default=4000, help="SWD speed in kHz")
    parser.add_argument("--usb-sn", type=int, default=None, help="J-Link serial number")


def check_host(args: argparse.Namespace) -> None:
    if not args.gdb.exists():
        raise SetupError(f"GDB not found: {args.gdb}. Set S32DS_ROOT or pass --gdb.")
    if not args.elf.exists():
        raise SetupError(f"ELF not found: {args.elf}. Run build.bat first.")
    if not args.jlink_dll.exists():
        raise SetupError(f"J-Link DLL not found: {args.jlink_dll}. Install the SEGGER J-Link "
                         "software or pass --jlink-dll.")
    if gdb_server_running():
        raise SetupError("A J-Link GDB server is running and holds the probe. Stop it first.")


# --------------------------------------------------------------------------------------------------
# ELF
# --------------------------------------------------------------------------------------------------

def gdb_offline(gdb: Path, elf: Path, commands: list[str]) -> str:
    """Runs GDB against the ELF file only. No target, so nothing touches the board."""
    fd, path = tempfile.mkstemp(suffix=".gdb", prefix="hil_")
    try:
        with os.fdopen(fd, "w", encoding="ascii") as f:
            f.write("\n".join(["set pagination off", *commands]) + "\n")
        proc = subprocess.run([str(gdb), "-batch", "-nx", "-x", path, str(elf)],
                              capture_output=True, text=True, timeout=60)
    finally:
        os.unlink(path)
    return proc.stdout + proc.stderr


def elf_ints(gdb: Path, elf: Path, exprs: dict[str, str]) -> dict[str, int]:
    """Evaluates integer C expressions (addresses, sizeof) against the ELF."""
    out = gdb_offline(gdb, elf, [f'printf "@@{k}=%lu\\n", (unsigned long)({v})'
                                 for k, v in exprs.items()])
    values = {m.group(1): int(m.group(2)) for m in MARKER.finditer(out)}
    missing = set(exprs) - set(values)
    if missing:
        raise SetupError(f"The ELF has no {sorted(exprs[k] for k in missing)}.\n{out.strip()}")
    return values


def elf_section(gdb: Path, elf: Path, name: str) -> tuple[int, bytes]:
    out = gdb_offline(gdb, elf, ["info files"])
    m = re.search(rf"(0x[0-9a-f]+) - (0x[0-9a-f]+) is {re.escape(name)}\s*$", out, re.M)
    if not m:
        raise SetupError(f"The ELF has no {name} section.")
    start, end = int(m.group(1), 16), int(m.group(2), 16)
    with tempfile.TemporaryDirectory(prefix="hil_") as tmp:
        dump = Path(tmp) / "section.bin"
        gdb_offline(gdb, elf, [f"dump binary memory {dump.as_posix()} {start} {end}"])
        return start, dump.read_bytes()


def elf_identity(elf: Path) -> dict:
    """Size, SHA256 and mtime of the ELF file on the host.

    This describes the build artifact only. It is not evidence of what the MCU is
    running - use Bench.verify_flash() for that, and keep the two apart in reports.
    """
    if not elf.is_file():
        return {"path": str(elf), "exists": False, "size": 0, "sha256": "", "mtime_utc": ""}
    blob = elf.read_bytes()
    stamp = dt.datetime.fromtimestamp(elf.stat().st_mtime, tz=dt.timezone.utc)
    return {
        "path": str(elf),
        "exists": True,
        "size": len(blob),
        "sha256": hashlib.sha256(blob).hexdigest(),
        "mtime_utc": ts(stamp),
    }


@dataclass
class Layout:
    """Where each sampled field sits: memory blocks, and each field's offset in one sample."""
    fields: dict[str, tuple[int, int, str]]                 # key -> (address, size, struct code)
    blocks: list[tuple[int, int]] = field(default_factory=list)
    offsets: dict[str, int] = field(default_factory=dict)
    sample_size: int = 0

    def decode(self, data: bytes, base: int = 0) -> dict[str, int | float]:
        return {key: struct.unpack_from("<" + code, data, base + self.offsets[key])[0]
                for key, (_, _, code) in self.fields.items()}


def build_layout(gdb: Path, elf: Path, fields: list[tuple[str, str, str]],
                 header_bytes: int = 0) -> Layout:
    """Addresses and sizes come from the ELF, and each size must match its struct code."""
    exprs = {}
    for key, expr, _ in fields:
        exprs[f"{key}.addr"] = f"&({expr})"
        exprs[f"{key}.size"] = f"sizeof({expr})"
    values = elf_ints(gdb, elf, exprs)
    layout = Layout(fields={})
    for key, expr, code in fields:
        size = values[f"{key}.size"]
        if size != struct.calcsize("<" + code):
            raise SetupError(f"{expr} is {size} bytes, the script expects "
                             f"{struct.calcsize('<' + code)}. Update the field list.")
        layout.fields[key] = (values[f"{key}.addr"], size, code)

    merged: list[list[int]] = []
    for start, end in sorted((a, a + s) for a, s, _ in layout.fields.values()):
        if merged and start - merged[-1][1] <= BLOCK_MERGE_GAP:
            merged[-1][1] = max(merged[-1][1], end)
        else:
            merged.append([start, end])
    layout.blocks = [(start, end - start) for start, end in merged]

    base = header_bytes
    for b_addr, b_len in layout.blocks:
        for key, (addr, _, _) in layout.fields.items():
            if b_addr <= addr < b_addr + b_len:
                layout.offsets[key] = base + addr - b_addr
        base += b_len
    layout.sample_size = base
    return layout


# --------------------------------------------------------------------------------------------------
# Bench: pylink session
# --------------------------------------------------------------------------------------------------

class _HssBlock(ctypes.Structure):
    _fields_ = [("Addr", ctypes.c_uint32), ("NumBytes", ctypes.c_uint32),
                ("Flags", ctypes.c_uint32), ("Dummy", ctypes.c_uint32)]


class Bench:
    """One J-Link session through pylink. Use as a context manager."""

    def __init__(self, args: argparse.Namespace):
        self.args = args
        self.log: list[str] = []
        self.jl: pylink.JLink | None = None
        self._hss_buf = (ctypes.c_uint8 * 65536)()

    def __enter__(self) -> "Bench":
        keep = lambda s: self.log.append(str(s).rstrip())
        try:
            lib = pylink.Library(str(self.args.jlink_dll))
            self.jl = pylink.JLink(lib=lib, log=keep, detailed_log=lambda s: None,
                                   error=keep, warn=keep)
            self.jl.open(serial_no=self.args.usb_sn)
            self.jl.set_tif(pylink.enums.JLinkInterfaces.SWD)
            self.jl.connect(self.args.device, self.args.speed)
        except pylink.errors.JLinkException as exc:
            self.close()
            raise SetupError(f"J-Link connect failed: {exc}\n" + "\n".join(self.log[-15:]))
        return self

    def __exit__(self, *exc) -> None:
        self.close()

    def close(self) -> None:
        if self.jl is None:
            return
        try:
            if self.jl.opened():
                self.jl.close()
        finally:
            # pylink's finalizer calls the DLL again at interpreter exit, after the
            # DLL may be unloaded, and crashes. The session is closed, so skip it.
            self.jl._initialized = False
            self.jl = None

    @property
    def description(self) -> str:
        return (f"pylink-square {pylink.__version__}, J-Link DLL V{self.jl.version}, "
                f"probe S/N {self.jl.serial_number}")

    def read(self, addr: int, length: int) -> bytes:
        try:
            return bytes(self.jl.memory_read8(addr, length))
        except pylink.errors.JLinkException as exc:
            raise SetupError(f"Memory read failed at 0x{addr:08X}: {exc}")

    def write(self, addr: int, data: bytes) -> None:
        try:
            self.jl.memory_write8(addr, list(data))
        except pylink.errors.JLinkException as exc:
            raise SetupError(f"Memory write failed at 0x{addr:08X}: {exc}")

    def read_layout(self, layout: Layout) -> dict[str, int | float]:
        """Reads every block while the core runs. Blocks are read one after the other."""
        data = b"".join(self.read(addr, length) for addr, length in layout.blocks)
        return layout.decode(data)

    def read_until(self, layout: Layout, predicate, timeout_s: float,
                   period_s: float = 0.02) -> tuple[dict[str, int | float], float, list]:
        """Polls a layout while the core runs until ``predicate(values)`` is true.

        Returns ``(last_values, elapsed_s, samples)`` where ``samples`` is every
        ``(elapsed_s, values)`` reading taken, so a caller can report the transitions
        it actually observed instead of only the final value. It never raises on
        timeout: the caller decides whether the last reading is a failure.
        """
        started = time.monotonic()
        samples: list[tuple[float, dict[str, int | float]]] = []
        while True:
            values = self.read_layout(layout)
            elapsed = time.monotonic() - started
            samples.append((elapsed, values))
            if predicate(values) or elapsed >= timeout_s:
                return values, elapsed, samples
            time.sleep(period_s)

    def sample(self, layout: Layout, duration_s: float, period_s: float = 0.01) -> list:
        """Collects ``(elapsed_s, values)`` while the core runs for a fixed window."""
        started = time.monotonic()
        samples: list[tuple[float, dict[str, int | float]]] = []
        while True:
            elapsed = time.monotonic() - started
            if elapsed > duration_s:
                return samples
            samples.append((elapsed, self.read_layout(layout)))
            time.sleep(period_s)

    def verify_flash(self, gdb: Path, elf: Path, section: str = ".pflash") -> str:
        addr, image = elf_section(gdb, elf, section)
        on_target = self.read(addr, len(image))
        if on_target == image:
            return "matched"
        first = next(i for i, (a, b) in enumerate(zip(on_target, image)) if a != b)
        return f"MISMATCH at 0x{addr + first:08X}"

    def reset_and_run(self) -> None:
        """Resets the MCU and releases it. The startup code then sets up RAM again."""
        self.jl.reset(ms=0, halt=False)
        if self.jl.halted():
            self.jl.restart()

    # -- HSS through pylink's DLL handle ---------------------------------------------------------

    def hss_caps(self) -> dict[str, int]:
        caps = (ctypes.c_uint32 * 16)()
        if self.jl._dll.JLINK_HSS_GetCaps(caps) < 0:
            raise SetupError("This J-Link does not support HSS.")
        return {"max_blocks": caps[0], "max_freq_hz": caps[1], "caps": caps[2]}

    def hss_start(self, blocks: list[tuple[int, int]], period_us: int) -> None:
        descs = (_HssBlock * len(blocks))(*[_HssBlock(a, n, 0, 0) for a, n in blocks])
        rc = self.jl._dll.JLINK_HSS_Start(ctypes.byref(descs), len(blocks), period_us,
                                          HSS_FLAG_TIMESTAMP_US)
        if rc < 0:
            raise SetupError(f"HSS start failed ({rc}).")

    def hss_read(self) -> bytes:
        n = self.jl._dll.JLINK_HSS_Read(ctypes.byref(self._hss_buf), len(self._hss_buf))
        if n < 0:
            raise SetupError(f"HSS read failed ({n}).")
        return bytes(self._hss_buf[:n])

    def hss_stop(self) -> None:
        self.jl._dll.JLINK_HSS_Stop()
