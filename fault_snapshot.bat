@echo off
setlocal enabledelayedexpansion

rem Non-interactive fault snapshot: attach, let the target run briefly, halt, then
rem DETACH AND RECONNECT before reading anything.
rem
rem Why the extra detach/reconnect: "monitor go" + "monitor halt" (needed here because
rem gdb's async "continue&"/"interrupt" race in batch -ex mode - verified: "interrupt"
rem then reading a variable in the next -ex often hits "Selected thread is running"
rem before the async stop-reply lands, since nothing drives gdb's event loop between
rem -ex commands the way interactive typing does) leave GDB's OWN register/memory
rem cache stale (same bug as debug_live.bat's "monitor go/halt" warning). Reconnecting
rem (detach then a fresh "target remote" to the SAME still-running server) forces gdb
rem to re-read everything from scratch, which fixes the staleness.
rem
rem BONUS FINDING: that reconnect does NOT reset the core. The one-time reset-on-attach
rem only happens on a GDB server process's very first ever client connection (tied to
rem its own startup/SetupTarget sequence - verified via the server's own log: "Starting
rem target CPU" only appears around the *first* connection's monitor go/halt, a second
rem "target remote" on the same server session shows a full fresh register dump with NO
rem "Starting target CPU" in between, and lands on genuine in-progress code like
rem FlexCAN_Ip_SendBlocking, never back at _start). So detach+reconnect is basically
rem "true live attach without reset", as long as you don't restart the GDB server.
rem
rem Self-contained: starts/stops its own J-Link GDB server (-singlerun), so it does
rem NOT require debug_server.bat to already be running - safe to call repeatedly for
rem HIL/periodic sampling. Each fresh run of this .bat still costs the one-time reset
rem from starting a new server process - only detach/reconnect *within* one server
rem session is reset-free.
set "CONFIG=Debug_FLASH"
set "ELF=%CONFIG%\BMS_demo.elf"
set "JLINK_GDBSERVER=C:\NXP\S32DS.3.6.10\Drivers\Segger\JLinkGDBServerCL.exe"
set "JLINKSCRIPT=C:\NXP\S32DS.3.6.10\Drivers\Segger\JlinkScripts\S32K3xx\S32K344.jlinkscript"
set "GDB_EXE=C:\NXP\S32DS.3.6.10\S32DS\tools\gdb-arm\arm32-eabi\bin\arm-none-eabi-gdb.exe"
set "DEVICE=S32K344"
set "IFACE=SWD"
set "SPEED=4000"
set "PORT=2331"
set "RUN_SECONDS=2"

if not exist "%ELF%" (
    echo [FAIL] ELF not found: %ELF%
    echo        Run build.bat first.
    exit /b 1
)
if not exist "%JLINK_GDBSERVER%" (
    echo [FAIL] JLinkGDBServerCL.exe not found: %JLINK_GDBSERVER%
    exit /b 1
)
if not exist "%JLINKSCRIPT%" (
    echo [FAIL] J-Link script not found: %JLINKSCRIPT%
    exit /b 1
)
if not exist "%GDB_EXE%" (
    echo [FAIL] arm-none-eabi-gdb.exe not found: %GDB_EXE%
    exit /b 1
)

for /f "delims=" %%i in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set "TS=%%i"
set "OUTFILE=fault_snapshot_%TS%.txt"

echo Starting J-Link GDB Server...
rem NOT -singlerun: it must survive our internal detach+reconnect below, so we clean
rem it up ourselves via taskkill once gdb is done.
start "JLinkGDBServer" /min "%JLINK_GDBSERVER%" -device %DEVICE% -if %IFACE% -speed %SPEED% -port %PORT% -jlinkscriptfile "%JLINKSCRIPT%" -strict -timeout 8000 -nogui

timeout /t 2 >nul

echo Capturing snapshot to %OUTFILE% (running target %RUN_SECONDS%s first)...
"%GDB_EXE%" -batch ^
    -ex "set pagination off" ^
    -ex "source %~dp0fault_decode.gdb" ^
    -ex "target remote localhost:%PORT%" ^
    -ex "monitor go" ^
    -ex "shell ping -n %RUN_SECONDS% 127.0.0.1 >nul" ^
    -ex "monitor halt" ^
    -ex "detach" ^
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

taskkill /FI "WINDOWTITLE eq JLinkGDBServer" /F >nul 2>&1

echo.
if "%GDB_RC%"=="0" (
    echo [OK] Snapshot written: %OUTFILE%
) else (
    echo [FAIL] gdb exited with code %GDB_RC% - snapshot may be incomplete: %OUTFILE%
)
exit /b %GDB_RC%
