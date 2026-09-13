"""Minimal ctypes wrapper for the SEGGER J-Link DLL: connect, reset, memory read and HSS.

HSS (High-Speed Sampling) is the engine behind SEGGER J-Scope. The J-Link reads
the given memory blocks at a fixed period while the core runs, without halting
it, and buffers the samples for the host.

Connecting with device name S32K344 halts the core and runs the DLL's built-in
S32K344 setup, which fills the application RAM with 0xDEADBEEF. Use this
wrapper only where a reset follows the connect, never to watch a board that
is already running.
"""

from __future__ import annotations

import ctypes as C
from pathlib import Path

DEFAULT_DLL = Path(r"C:\Program Files\SEGGER\JLink\JLink_x64.dll")

TIF_SWD = 1
HSS_FLAG_TIMESTAMP_US = 1
HSS_TIMESTAMP_BYTES = 4

_LOGFN = C.CFUNCTYPE(None, C.c_char_p)


class JLinkError(RuntimeError):
    """A J-Link DLL call failed."""


class _HssBlock(C.Structure):
    _fields_ = [("Addr", C.c_uint32), ("NumBytes", C.c_uint32),
                ("Flags", C.c_uint32), ("Dummy", C.c_uint32)]


class JLink:
    def __init__(self, dll_path: Path = DEFAULT_DLL, device: str = "S32K344",
                 speed_khz: int = 4000, usb_sn: int | None = None):
        if not Path(dll_path).exists():
            raise JLinkError(f"J-Link DLL not found: {dll_path}")
        self._dll = C.CDLL(str(dll_path))
        self._dll.JLINKARM_OpenEx.restype = C.c_char_p
        self._dll.JLINKARM_ExecCommand.argtypes = [C.c_char_p, C.c_char_p, C.c_int]
        self._dll.JLINKARM_ReadMemEx.argtypes = [C.c_uint32, C.c_uint32, C.c_void_p, C.c_uint32]
        self._dll.JLINK_HSS_Start.argtypes = [C.c_void_p, C.c_int, C.c_int, C.c_int]
        self._dll.JLINK_HSS_Read.argtypes = [C.c_void_p, C.c_uint32]
        self.device = device
        self.speed_khz = speed_khz
        self.usb_sn = usb_sn
        self.log: list[str] = []
        # Keep references, or the callbacks are collected while the DLL still uses them.
        self._log_cb = _LOGFN(lambda s: self.log.append(s.decode(errors="replace").rstrip()))
        self._err_cb = _LOGFN(lambda s: self.log.append("ERROR: " + s.decode(errors="replace").rstrip()))
        self._open = False
        self._read_buf = (C.c_uint8 * 65536)()

    @property
    def dll_version(self) -> str:
        v = self._dll.JLINKARM_GetDLLVersion()
        return f"{v // 10000}.{(v // 100) % 100:02d}"

    def open(self) -> "JLink":
        if self.usb_sn is not None:
            self._dll.JLINKARM_EMU_SelectByUSBSN(self.usb_sn)
        err = self._dll.JLINKARM_OpenEx(self._log_cb, self._err_cb)
        if err:
            raise JLinkError(f"Cannot open the J-Link: {err.decode(errors='replace')}")
        self._open = True
        msg = C.create_string_buffer(256)
        self._dll.JLINKARM_ExecCommand(f"device = {self.device}".encode(), msg, len(msg))
        if msg.value:
            raise JLinkError(f"device = {self.device}: {msg.value.decode(errors='replace')}")
        self._dll.JLINKARM_TIF_Select(TIF_SWD)
        self._dll.JLINKARM_SetSpeed(self.speed_khz)
        if self._dll.JLINKARM_Connect() < 0:
            raise JLinkError("Cannot connect to the target:\n" + "\n".join(self.log[-15:]))
        return self

    def close(self) -> None:
        if self._open:
            self._dll.JLINKARM_Close()
            self._open = False

    def __enter__(self) -> "JLink":
        return self.open()

    def __exit__(self, *exc) -> None:
        self.close()

    def is_halted(self) -> bool:
        rc = self._dll.JLINKARM_IsHalted()
        if rc < 0:
            raise JLinkError("Cannot read the core state.")
        return rc == 1

    def read(self, addr: int, length: int) -> bytes:
        out = bytearray()
        while len(out) < length:
            n = min(0x10000, length - len(out))
            buf = (C.c_uint8 * n)()
            if self._dll.JLINKARM_ReadMemEx(addr + len(out), n, buf, 0) != n:
                raise JLinkError(f"Memory read failed at 0x{addr + len(out):08X}")
            out += bytes(buf)
        return bytes(out)

    def reset_and_go(self) -> None:
        if self._dll.JLINKARM_Reset() < 0:
            raise JLinkError("Reset failed.")
        self._dll.JLINKARM_Go()

    def go(self) -> None:
        self._dll.JLINKARM_Go()

    def hss_caps(self) -> dict[str, int]:
        caps = (C.c_uint32 * 16)()
        if self._dll.JLINK_HSS_GetCaps(caps) < 0:
            raise JLinkError("This J-Link does not support HSS.")
        return {"max_blocks": caps[0], "max_freq_hz": caps[1], "caps": caps[2]}

    def hss_start(self, blocks: list[tuple[int, int]], period_us: int) -> None:
        descs = (_HssBlock * len(blocks))(*[_HssBlock(a, n, 0, 0) for a, n in blocks])
        rc = self._dll.JLINK_HSS_Start(descs, len(blocks), period_us, HSS_FLAG_TIMESTAMP_US)
        if rc < 0:
            raise JLinkError(f"HSS start failed ({rc}).")

    def hss_read(self) -> bytes:
        n = self._dll.JLINK_HSS_Read(self._read_buf, len(self._read_buf))
        if n < 0:
            raise JLinkError(f"HSS read failed ({n}).")
        return bytes(self._read_buf[:n])

    def hss_stop(self) -> None:
        self._dll.JLINK_HSS_Stop()
