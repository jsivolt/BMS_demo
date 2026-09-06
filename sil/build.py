#!/usr/bin/env python3
"""Build the BMS SIL shared library.

Compiles the in-scope application-layer sources from src/ unmodified, together
with the SIL fakes and harness, into a single shared library that the Python
test layer loads with ctypes.

Usage:
    python sil/build.py            # build
    python sil/build.py --clean    # remove build outputs first
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

SIL_DIR = Path(__file__).resolve().parent
REPO = SIL_DIR.parent
BUILD = SIL_DIR / "build"

LIB_NAME = "bms_sil.dll" if os.name == "nt" else "libbms_sil.so"

# Production sources in the SOC vertical slice. Compiled straight from src/,
# with no SIL-specific modification.
PRODUCTION_SOURCES = [
    "src/battery/Bms_Soc.c",
    "src/battery/Battery_Monitor.c",
    "src/battery/vAFE/Bms_Vafe.c",
    "src/battery/vPACK/Bms_Vpack.c",
    "src/common/Lib_Interp.c",
    "src/safety/Fault_Manager.c",
    "src/storage/Bms_Nvm.c",
]

SIL_SOURCES = [
    "sil/fakes/C40_Ip.c",
    "sil/fakes/Sil_AppDoubles.c",
    "sil/harness/sil_main.c",
]

# sil/fakes comes first so its Std_Types.h shadows any target header.
INCLUDE_DIRS = [
    "sil/fakes",
    "sil/harness",
    "src/app",
    "src/battery",
    "src/battery/vAFE",
    "src/battery/vPACK",
    "src/common",
    "src/communication",
    "src/control",
    "src/safety",
    "src/storage",
]

# Same dialect and warning level as the target build, so SIL does not accept
# code the ARM build would reject.
CFLAGS = [
    "-std=c99",
    "-O0",
    "-g",
    "-fPIC",
    "-funsigned-char",
    "-fno-common",
    "-Wall",
    "-Wextra",
    "-Wundef",
    "-Wsign-compare",
    "-Wunused",
    "-Wstrict-prototypes",
    "-Wdouble-promotion",
    "-Werror=implicit-function-declaration",
]

WINLIBS_GCC = (
    Path(os.environ.get("LOCALAPPDATA", ""))
    / "Microsoft/WinGet/Packages"
    / "BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe"
    / "mingw64/bin/gcc.exe"
)


def find_compiler() -> str:
    """Locate a host C compiler, preferring one already on PATH."""
    override = os.environ.get("SIL_CC")
    if override:
        return override

    for name in ("gcc", "clang", "cc"):
        found = shutil.which(name)
        if found:
            return found

    if WINLIBS_GCC.is_file():
        return str(WINLIBS_GCC)

    sys.exit(
        "No host C compiler found.\n"
        "Install one (e.g. winget install -e --id BrechtSanders.WinLibs.POSIX.UCRT)\n"
        "or set SIL_CC to its full path."
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--clean", action="store_true", help="remove build outputs first")
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args()

    if args.clean and BUILD.exists():
        shutil.rmtree(BUILD)

    BUILD.mkdir(parents=True, exist_ok=True)

    cc = find_compiler()
    includes = [f"-I{REPO / d}" for d in INCLUDE_DIRS]
    sources = PRODUCTION_SOURCES + SIL_SOURCES

    print(f"compiler : {cc}")
    print(f"sources  : {len(PRODUCTION_SOURCES)} production + {len(SIL_SOURCES)} SIL")

    objects = []
    failed = False

    for rel in sources:
        src = REPO / rel
        if not src.is_file():
            print(f"  MISSING  {rel}")
            failed = True
            continue

        obj = BUILD / (rel.replace("/", "_").replace(".c", ".o"))
        cmd = [cc, *CFLAGS, *includes, "-c", str(src), "-o", str(obj)]

        if args.verbose:
            print("  " + " ".join(cmd))

        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0 or result.stderr.strip():
            status = "FAIL" if result.returncode != 0 else "WARN"
            print(f"  {status}  {rel}")
            print(result.stderr.rstrip())
            if result.returncode != 0:
                failed = True
                continue
        else:
            print(f"  ok    {rel}")

        objects.append(str(obj))

    if failed:
        print("\nbuild FAILED")
        return 1

    lib = BUILD / LIB_NAME
    link = [cc, "-shared", "-o", str(lib), *objects]
    result = subprocess.run(link, capture_output=True, text=True)

    if result.returncode != 0:
        print("\nlink FAILED")
        print(result.stderr.rstrip())
        return 1

    print(f"\nbuilt {lib}  ({lib.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
