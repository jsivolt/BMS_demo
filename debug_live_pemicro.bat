@echo off
rem CMD #2, MODE 2 - "live attach": inspect a BMS that is already running without a reset.
rem
rem PEmicro-specific: pegdbserver_console has NO "monitor halt"/"monitor go" (confirmed
rem live: both are "Unrecognized Command", and sending an unrecognized monitor command can
rem desync the GDB remote protocol session until you reconnect to a fresh server) - only
rem "monitor reset" is a recognized pass-through command, and it is NOT used here on
rem purpose. So this relies entirely on GDB's own async run control (continue& /
rem interrupt), same technique already used by debug_live.bat for the (different) Segger
rem register-cache-staleness bug - for PEmicro it's not just the safer choice, it's close
rem to the only choice.
rem
rem To guarantee ZERO resets, including the one pegdbserver_console does at its own
rem startup, start the shared server as: debug_server_pemicro.bat attach
rem
rem Breakpoints on the fault set/clear entry points are preloaded (numbered in this
rem fixed order, so "condition N ..." below always targets the right one):
rem   1 = FaultManager_SetSystem     2 = FaultManager_SetPack
rem   3 = FaultManager_ClearSystem   4 = FaultManager_ClearPack
rem gdb stops there on its own the next time any fault is set/cleared. Once stopped:
rem   faultname fault        <- decode the bitmask to FAULT_* names
rem   bt                     <- call chain that set/cleared it
rem   faultdump              <- full current+latched snapshot of system and all 3 packs
rem   continue&              <- resume
rem
rem Once at the (gdb) prompt, the target is already running (or was just resumed below):
rem   interrupt              <- pause (accurate halt, no reset)
rem   faultname g_SystemFaults
rem   continue&              <- resume
set "CONFIG=Debug_FLASH"
set "ELF=%CONFIG%\BMS_demo.elf"
set "GDB_EXE=C:\NXP\S32DS.3.6.10\S32DS\tools\gdb-arm\arm32-eabi\bin\arm-none-eabi-gdb.exe"
set "PORT=7224"

if not exist "%ELF%" (
    echo [FAIL] ELF not found: %ELF%
    echo        Run build.bat first.
    exit /b 1
)
if not exist "%GDB_EXE%" (
    echo [FAIL] arm-none-eabi-gdb.exe not found: %GDB_EXE%
    exit /b 1
)

"%GDB_EXE%" -ex "set mi-async on" -ex "set pagination off" -ex "source %~dp0fault_decode.gdb" ^
    -ex "target remote localhost:%PORT%" ^
    -ex "break FaultManager_SetSystem" -ex "break FaultManager_SetPack" ^
    -ex "break FaultManager_ClearSystem" -ex "break FaultManager_ClearPack" ^
    -ex "continue&" "%ELF%"
