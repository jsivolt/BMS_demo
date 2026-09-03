## GDB helper: decode Fault_Manager.h FAULT_* bitmasks by name.
## Pure gdb command-language script (this GDB build has no Python support - verified,
## "python print(1)" -> "Python scripting is not supported in this copy of GDB").
## Bit list must be kept in sync with src/safety/Fault_Manager.h.
## Usage: faultname <expr>   e.g. faultname fault | faultname g_SystemFaults | faultname 0x30040
define faultname
set $__fn_val = ($arg0) & 0xffffffff
set $__fn_first = 1
if $__fn_val == 0
printf "FAULT_NONE"
else
set $__fn_bit = 0
while $__fn_bit < 22
if ($__fn_val >> $__fn_bit) & 1
if $__fn_first == 0
printf " | "
end
set $__fn_first = 0
if $__fn_bit == 0
printf "FAULT_PACK_OV"
end
if $__fn_bit == 1
printf "FAULT_PACK_UV"
end
if $__fn_bit == 2
printf "FAULT_CELL_OV"
end
if $__fn_bit == 3
printf "FAULT_CELL_UV"
end
if $__fn_bit == 4
printf "FAULT_OVER_TEMP"
end
if $__fn_bit == 5
printf "FAULT_UNDER_TEMP"
end
if $__fn_bit == 6
printf "FAULT_AFE_COMM"
end
if $__fn_bit == 7
printf "FAULT_CAN_TIMEOUT"
end
if $__fn_bit == 8
printf "FAULT_SPI_TIMEOUT"
end
if $__fn_bit == 9
printf "FAULT_PRECHARGE_TIMEOUT"
end
if $__fn_bit == 10
printf "FAULT_CONTACTOR_FEEDBACK"
end
if $__fn_bit == 11
printf "FAULT_CONTACTOR_WELD"
end
if $__fn_bit == 12
printf "FAULT_OVER_CURRENT"
end
if $__fn_bit == 13
printf "FAULT_TEMP_SENSOR"
end
if $__fn_bit == 14
printf "FAULT_TEMP_DELTA"
end
if $__fn_bit == 15
printf "FAULT_CELL_IMBALANCE"
end
if $__fn_bit == 16
printf "FAULT_VPACK_COMM_TIMEOUT"
end
if $__fn_bit == 17
printf "FAULT_VPACK_ALIVE_ERROR"
end
if $__fn_bit == 18
printf "FAULT_VPACK_DEVICE_FAULT"
end
if $__fn_bit == 19
printf "FAULT_PACK1_VOLTAGE_TIMEOUT"
end
if $__fn_bit == 20
printf "FAULT_PACK_DISCHARGE_OC"
end
if $__fn_bit == 21
printf "FAULT_PACK_CHARGE_OC"
end
end
set $__fn_bit = $__fn_bit + 1
end
end
printf " (0x%x)\n", $__fn_val
end

## Usage: faultdump   -> one-shot snapshot of every current/latched fault mask,
## decoded by name. Handy right after "faultname"/"bt" on a fault breakpoint hit.
define faultdump
printf "\nSystem current:\n  "
faultname g_SystemFaults
printf "System latched:\n  "
faultname g_LastSystemFaults
printf "Pack1 current:\n  "
faultname g_PackFaults[0]
printf "Pack1 latched:\n  "
faultname g_LastPackFaults[0]
printf "Pack2 current:\n  "
faultname g_PackFaults[1]
printf "Pack2 latched:\n  "
faultname g_LastPackFaults[1]
printf "Pack3 current:\n  "
faultname g_PackFaults[2]
printf "Pack3 latched:\n  "
faultname g_LastPackFaults[2]
printf "\n"
end

## Watchpoint helpers: catch a DIRECT write to a fault mask (assignment, |=, &=,
## whether via FaultManager_Set/ClearSystem or anything that bypasses them), not just
## calls to the setter/clearer functions. Complements the FaultManager_Set*/Clear*
## breakpoints in debug_live.bat: those tell you who *called the API*; these tell you
## who *touched the memory*, from any code path.
##
## GOTCHA (do not "genericize" these with $arg0): a "commands N ... end" block
## attached to a breakpoint/watchpoint stores its lines VERBATIM and runs them later,
## outside the defining command's own execution - "define"'s $argN substitution only
## applies while that define's body is actively running, so $arg0 used *inside* a
## "commands" block is unresolved at hit time ("Argument to arithmetic operation not
## a number or boolean" - verified). Each watch helper below must hardcode its target
## variable name for the same reason faultname's own bit table is hardcoded.
define wsysfault
watch g_SystemFaults
commands $bpnum
silent
printf "\n[watch] "
faultname g_SystemFaults
bt 6
end
end

define wsyslast
watch g_LastSystemFaults
commands $bpnum
silent
printf "\n[watch] "
faultname g_LastSystemFaults
bt 6
end
end

## One-shot capture for post-mortem/chat logs: timestamp + PC + faultdump + full backtrace.
define faultsnapshot
printf "\n=== Fault snapshot ===\n"
shell date /t
shell time /t
printf "PC = "
p/x $pc
faultdump
printf "Backtrace:\n"
bt
printf "======================\n"
end

