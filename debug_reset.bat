@echo off
rem CMD #2, MODE 1 - "reset debug": connect, halt at _start (reset vector), then step/
rem breakpoint through boot. NOTE: attaching to this MCU always halts the core at reset
rem (SWD connect requirement for this device/jlinkscript) - this mode embraces that.
rem Use debug_live.bat instead if you want to inspect a BMS that's already running
rem without losing transient fault state.
set "CONFIG=Debug_FLASH"
set "ELF=%CONFIG%\BMS_demo.elf"
set "GDB_EXE=C:\NXP\S32DS.3.6.10\S32DS\tools\gdb-arm\arm32-eabi\bin\arm-none-eabi-gdb.exe"
set "PORT=2331"

if not exist "%ELF%" (
    echo [FAIL] ELF not found: %ELF%
    echo        Run build.bat first.
    exit /b 1
)
if not exist "%GDB_EXE%" (
    echo [FAIL] arm-none-eabi-gdb.exe not found: %GDB_EXE%
    exit /b 1
)

"%GDB_EXE%" -ex "target remote localhost:%PORT%" -ex "monitor halt" "%ELF%"
