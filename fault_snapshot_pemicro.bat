@echo off
setlocal enabledelayedexpansion

rem Non-interactive fault snapshot for PEmicro. Starts its own server (one reset at
rem server startup, like flash_pemicro.bat), lets the target run briefly, pauses, then
rem captures state.
rem
rem PEmicro-specific: pegdbserver_console has NO "monitor go"/"monitor halt" (see
rem /memories/repo/build.md - both are "Unrecognized Command" and can desync the GDB
rem remote protocol), so this uses GDB's own async run control (mi-async + "continue&" /
rem "interrupt") instead - the same technique used by debug_live_pemicro.bat.
rem
rem CONFIRMED LIVE (2026-09-14): chaining "continue&" -> shell-ping-delay -> "interrupt" ->
rem read-a-variable in ONE gdb -batch invocation is UNRELIABLE - "shell" blocks gdb's own
rem single-threaded event loop (same as documented for the original Segger
rem fault_snapshot.bat), so no delay length fixes it; observed failures: "Selected thread
rem is running." / "Cannot execute this command while the target is running." FIX: split
rem the run/pause/read sequence into 3 SEPARATE gdb invocations with an OS-level
rem (cmd "timeout", not gdb "shell") delay in between, so each invocation only ever issues
rem ONE async op (or none) before exiting - confirmed live: connect+interrupt+quit in a
rem fresh invocation, and a later fresh connect+read+detach in another, both come back
rem clean with no race errors.
rem
rem Self-contained: starts/stops its own PEmicro GDB server, so it does NOT require
rem debug_server_pemicro.bat to already be running - safe to call repeatedly for
rem HIL/periodic sampling. Each run costs the one-time reset from starting a new server
rem process (see debug_server_pemicro.bat's "attach" note - not applicable here since this
rem script always starts a fresh server).
set "CONFIG=Debug_FLASH"
set "ELF=%CONFIG%\BMS_demo.elf"
set "PEGDBSERVER=C:\NXP\S32DS.3.6.10\eclipse\plugins\com.pemicro.debug.gdbjtag.pne_6.1.8.202603121731\win32\pegdbserver_console.exe"
set "GDB_EXE=C:\NXP\S32DS.3.6.10\S32DS\tools\gdb-arm\arm32-eabi\bin\arm-none-eabi-gdb.exe"
set "DEVICE=NXP_S32K3xx_S32K344"
set "IFACE=USBMULTILINK"
set "HWPORT=USB1"
set "SPEED=5000"
set "PORT=7224"
set "MIPORT=6224"
set "RUN_SECONDS=2"

if not exist "%ELF%" (
    echo [FAIL] ELF not found: %ELF%
    echo        Run build.bat first.
    exit /b 1
)
if not exist "%PEGDBSERVER%" (
    echo [FAIL] pegdbserver_console.exe not found: %PEGDBSERVER%
    exit /b 1
)
if not exist "%GDB_EXE%" (
    echo [FAIL] arm-none-eabi-gdb.exe not found: %GDB_EXE%
    exit /b 1
)

for /f "delims=" %%i in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set "TS=%%i"
set "OUTFILE=fault_snapshot_%TS%.txt"

echo Starting PEmicro GDB Server...
start "PEGDBServer" /min "%PEGDBSERVER%" -device=%DEVICE% -startserver -interface=%IFACE% -port=%HWPORT% -speed=%SPEED% -serverport=%PORT% -gdbmiport=%MIPORT%

timeout /t 2 >nul

echo Resuming target (invocation 1/3)...
"%GDB_EXE%" -batch -ex "set mi-async on" -ex "target remote localhost:%PORT%" -ex "continue&" -ex "quit" "%ELF%" >nul 2>&1

echo Letting it run for %RUN_SECONDS%s...
timeout /t %RUN_SECONDS% >nul

echo Pausing target (invocation 2/3)...
"%GDB_EXE%" -batch -ex "set mi-async on" -ex "target remote localhost:%PORT%" -ex "interrupt" -ex "quit" "%ELF%" >nul 2>&1

timeout /t 1 >nul

echo Capturing snapshot to %OUTFILE% (invocation 3/3)...
"%GDB_EXE%" -batch ^
    -ex "set pagination off" ^
    -ex "source %~dp0fault_decode.gdb" ^
    -ex "target remote localhost:%PORT%" ^
    -ex "set logging file %OUTFILE%" ^
    -ex "set logging overwrite on" ^
    -ex "set logging enabled on" ^
    -ex "printf \"=== Fault snapshot %TS% ===\n\"" ^
    -ex "printf \"PC = \"" ^
    -ex "p/x $pc" ^
    -ex "faultdump" ^
    -ex "printf \"Backtrace:\n\"" ^
    -ex "bt" ^
    -ex "detach" ^
    "%ELF%"
set "GDB_RC=%ERRORLEVEL%"

taskkill /FI "WINDOWTITLE eq PEGDBServer" /F >nul 2>&1
rem the taskkill above only matches by window title; confirm it actually worked, since a
rem leftover pegdbserver_console.exe holding USB1/port 7224 breaks the NEXT run
tasklist /FI "IMAGENAME eq pegdbserver_console.exe" 2>nul | find /i "pegdbserver_console.exe" >nul
if not errorlevel 1 (
    echo [WARN] pegdbserver_console.exe still running after taskkill - close it manually
    echo        before the next run ^(tasklist ^| findstr /i pegdbserver^).
)

echo.
if "%GDB_RC%"=="0" (
    echo [OK] Snapshot written: %OUTFILE%
) else (
    echo [FAIL] gdb exited with code %GDB_RC% - snapshot may be incomplete: %OUTFILE%
)
exit /b %GDB_RC%
