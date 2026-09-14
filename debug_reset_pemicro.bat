@echo off
rem CMD #2, MODE 1 - "reset debug": connect and force a fresh reset, halting at the
rem reset vector, for stepping/breakpointing through boot.
rem PEmicro-specific: pegdbserver_console only auto-resets ONCE, at ITS OWN startup, not
rem on every gdb connect (unlike the Segger flow) - see /memories/repo/build.md. So this
rem always sends an explicit "monitor reset" (the ONLY confirmed-working pass-through
rem monitor command on PEmicro - "monitor halt" does not exist) instead of relying on
rem connect-time behavior, regardless of how long debug_server_pemicro.bat has been up.
rem Use debug_live_pemicro.bat instead to inspect a BMS that's already running without a
rem reset (start the shared server with "debug_server_pemicro.bat attach" for that case).
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

"%GDB_EXE%" -ex "target remote localhost:%PORT%" -ex "monitor reset" "%ELF%"
