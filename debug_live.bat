@echo off
rem CMD #2, MODE 2 - "live attach": inspect a BMS that is already running without a
rem second reset. Attaching still halts the core once (SWD connect requirement on this
rem MCU - unavoidable), but this immediately resumes it, so you can then pause/read/
rem resume repeatedly without disturbing state further.
rem
rem IMPORTANT: do NOT use "monitor go" / "monitor halt" here - GDB does not refresh its
rem register/memory cache after those raw pass-through commands, so p/x reads can show
rem stale data from the very first halt (verified: PC stayed "stuck" at _start and
rem g_SystemFaults looked frozen even though the real target had moved on). Use GDB's
rem own async continue + interrupt instead, which properly resyncs GDB's state.
rem
rem Breakpoints on the fault set/clear entry points are preloaded (numbered in this
rem fixed order, so "condition N ..." below always targets the right one):
rem   1 = FaultManager_SetSystem     2 = FaultManager_SetPack
rem   3 = FaultManager_ClearSystem   4 = FaultManager_ClearPack
rem gdb stops there on its own the next time any fault is set/cleared, instead of you
rem just polling the mask - breakpoint hits are real stop events, so state is always
rem accurate (same as "interrupt"). Once stopped:
rem   faultname fault        <- decode the bitmask to FAULT_* names (e.g. FAULT_VPACK_COMM_TIMEOUT)
rem   bt                     <- call chain that set/cleared it
rem   faultdump              <- full current+latched snapshot of system and all 3 packs, decoded
rem   continue&              <- resume
rem
rem Too noisy (some faults fire constantly)? Restrict a breakpoint to one fault value:
rem   condition 1 fault == 0x80000   <- breakpoint 1 (SetSystem) only stops for FAULT_PACK1_VOLTAGE_TIMEOUT
rem   condition 1                    <- clear the condition again, back to catching every fault
rem
rem "Who called the setter" isn't always enough - to catch a DIRECT write to a fault mask
rem (assignment/|=/&= from ANY code path, not just through FaultManager_Set*/Clear*), set a
rem watchpoint instead (uses separate DWT hardware, so it stacks fine with the breakpoints
rem above): "wsysfault" / "wsyslast" watch g_SystemFaults / g_LastSystemFaults and
rem auto-print the decoded value + a short backtrace on every hit. For anything else
rem (g_PackFaults[i], a contactor/ADC variable, etc.) just use plain gdb: e.g.
rem   watch g_PackFaults[0]
rem   continue&
rem and once it stops, decode/inspect manually (faultname g_PackFaults[0], bt).
rem
rem faultsnapshot captures a one-shot record (host timestamp + PC + faultdump + full bt) -
rem handy to paste into a bug report right after any breakpoint/watchpoint hit above.
rem
rem Once at the (gdb) prompt, the target is already running. Manual workflow (no fault
rem hit yet, or you just want a snapshot):
rem   interrupt              <- pause (accurate halt, no reset)
rem   faultname g_SystemFaults
rem   faultname g_LastSystemFaults
rem   faultname g_PackFaults[0]
rem   continue&              <- resume
rem   (repeat interrupt / continue& as needed; "detach" only while halted)
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

"%GDB_EXE%" -ex "set mi-async on" -ex "set pagination off" -ex "source %~dp0fault_decode.gdb" ^
    -ex "target remote localhost:%PORT%" ^
    -ex "break FaultManager_SetSystem" -ex "break FaultManager_SetPack" ^
    -ex "break FaultManager_ClearSystem" -ex "break FaultManager_ClearPack" ^
    -ex "continue&" "%ELF%"
