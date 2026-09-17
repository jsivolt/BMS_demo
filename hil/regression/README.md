# BMS_demo Phase 1 regression framework

An automated build → SIL → HIL smoke regression for the S32K344 BMS demo. One command
produces a Markdown and a JSON report; no result is ever reported as a PASS unless the
criterion actually ran.

```powershell
.venv\Scripts\python.exe hil\regression\run_regression.py --smoke
```

---

## 1. Purpose

Phase 1 is deliberately **not** full regression coverage. It proves one end-to-end path
and reports honestly on everything it could not exercise:

```
Build -> SIL -> Flash / Boot -> STANDBY -> virtual measurements valid -> Enable
      -> Precharge -> ACTIVE -> Inject fault -> FAULT + contactors open
      -> recover condition -> ClearFault -> STANDBY -> XCP connectivity
```

---

## 2. Prerequisites

| Requirement | Needed for | Notes |
| --- | --- | --- |
| `Debug_FLASH/BMS_demo.elf` on disk | everything | built by `REG-BUILD-001`, or build it first |
| S32DS 3.6.10 at `C:\NXP\S32DS.3.6.10` | build, ELF symbol queries | `S32DS_ROOT` is honoured; `build.bat` auto-detects |
| SEGGER J-Link probe + `JLink_x64.dll` | every case that observes the target | without it 15 of 18 cases SKIP |
| PEAK PCAN-USB channel(s) + PCAN-Basic driver | every CAN case | one channel per bus; see §4 |
| `python-can`, `pylink-square`, `pytest` | everything | `.venv\Scripts\python.exe -m pip install -r hil\regression\requirements.txt` |
| A host C compiler (MinGW-w64) | `REG-SIL-001` | **absent on the current machine**, so SIL reports an INFRA ERROR |

Use the workspace virtual environment. The system Python has none of these packages.

---

## 3. How to run

```powershell
# full smoke set, whole configured bus topology
.venv\Scripts\python.exe hil\regression\run_regression.py --smoke

# staged runs for a bench with fewer adapters than buses
.venv\Scripts\python.exe hil\regression\run_regression.py --bus CAN0
.venv\Scripts\python.exe hil\regression\run_regression.py --bus CAN1 --bus CAN2
.venv\Scripts\python.exe hil\regression\run_regression.py --bus CAN5

# restrict the report to specific ids (the prerequisite steps still run)
.venv\Scripts\python.exe hil\regression\run_regression.py --test REG-SM-002

# everything except the parts that need hardware
.venv\Scripts\python.exe hil\regression\run_regression.py --no-build --no-sil --no-flash

# list the test ids
.venv\Scripts\python.exe hil\regression\run_regression.py --list-tests
```

Other flags: `--no-build`, `--no-sil`, `--no-flash`, `--report PATH`, `--json PATH`,
`--can-config PATH`, `--nominal-config PATH`, `--build-config NAME`, and the shared
`--elf / --gdb / --jlink-dll / --device / --speed / --usb-sn` from `hil/hil_common.py`.

Run the framework's own host-side suite (no probe, no CAN adapter needed):

```powershell
.venv\Scripts\python.exe -m pytest hil/regression
```

### Exit codes

| Code | Meaning |
| --- | --- |
| 0 | every executed requested test passed (`XFAIL` is allowed) |
| 1 | at least one DUT failure |
| 2 | incomplete run: a SKIP or an infrastructure error |

A DUT failure outranks an infrastructure error.

---

## 4. Required CAN channels and the bus mapping

Four logical buses exist. A PCAN-USB channel is physically attached to exactly one bus,
so the framework never assumes all four are present.

| Logical bus | BMS role | Bitrate | Config default |
| --- | --- | --- | --- |
| CAN0 | host control + observation | 500 kbit/s | `PCAN_USBBUS1` |
| CAN1 | virtual AFE stimulus | 1 Mbit/s | `PCAN_USBBUS2` |
| CAN2 | virtual pack monitor stimulus | 1 Mbit/s | `PCAN_USBBUS3` |
| CAN5 | XCP | 1 Mbit/s | `PCAN_USBBUS4` |

Edit `hil/regression/can_config.json` to match the bench, or override one bus for a single
run: `--can1 pcan:PCAN_USBBUS1:1000000`.

The report always records the physical channel behind each logical bus.

### What happens when a bus is missing

| Situation | Classification |
| --- | --- |
| bus not named with `--bus` | **SKIP** - "not selected for this run" |
| channel is not present on the machine | **SKIP** - the command lists the channels the driver does report |
| channel exists but will not open (occupied by PCAN-View, driver error) | **INFRA ERROR** |
| another transmitter already drives ids the framework must own | **SKIP**, with the conflicting ids named |

The last row matters: two nodes putting different payloads under `0x401` produce bus
errors and an ambiguous verdict, so the framework sniffs CAN1/CAN2 before it takes them
over and refuses to transmit if the bus is already occupied by an unknown source.

---

## 5. Expected topology

```
        +-------------------+                      +------------------+
        |  HIL PC           |                      |  S32K344 board   |
        |                   |                      |                  |
        |  PCAN_USBBUS1 ----+---- CAN0 500k --------+ CAN0 PTA6/7      |
        |  PCAN_USBBUS2 ----+---- CAN1 1M ---------+ CAN1 PTC8/9      |
        |  PCAN_USBBUS3 ----+---- CAN2 1M ---------+ CAN2 PTE24/25    |
        |  PCAN_USBBUS4 ----+---- CAN5 1M ---------+ CAN5 PTC26/27    |
        |                   |                      |                  |
        |  J-Link  ---------+---- SWD 4000 kHz ----+ SWD              |
        +-------------------+                      +------------------+
```

The framework owns CAN1 and CAN2 while a regression is running. Any external vAFE/vPACK
simulator must be stopped, or moved to a bus the framework is not using.

### J-Link caveat

Connecting with device name `S32K344` fills the application RAM with `0xDEADBEEF`. Every
session therefore resets the MCU immediately after connecting (`hil_common.Bench.
reset_and_run`). A board that is already running cannot be observed without losing its
state; the runner is built around short connect/reset/observe windows for that reason.

---

## 6. Reporting

Both files are written on every run:

```
hil/regression/reports/regression_latest.md
hil/regression/reports/regression_latest.json
```

They are **runtime artifacts** and are gitignored. The report contains:

* `host_artifact_identity` - ELF path, SHA256, size, mtime, git commit and dirty state.
  This describes the file on the host. **It is not evidence of what the MCU holds.**
* `target_flash_verification` - the `.pflash` byte compare read back through the probe.
  This is the only claim about the MCU contents.
* a summary table per area with PASS / FAIL / XFAIL / SKIP / INFRA ERROR
* one section per test with Status, Expected, Observed, Duration, Evidence, observation
  method, details and diagnostics
* `setup_steps` - prerequisite actions that ran to establish state but are not results
* the CAN bus mapping and the effective configuration

When `--test` is used, only the requested ids appear as results. Prerequisite steps still
run, and they are listed under `Setup` instead of inflating the SKIP count.

### Evidence labels

Every case carries one of:

| Label | Meaning |
| --- | --- |
| `VERIFIED ON HARDWARE` | the criterion ran against the board |
| `HOST-ONLY VERIFIED` | build / SIL - no board involved |
| `NOT EXECUTED - SKIPPED` | did not run |

### Observation methods

Each case records how it looked at the DUT: `CAN`, `GPIO`, `XCP`,
`debugger/exported symbol`, `debugger/file-static`. File-static symbols
(`g_BatteryData`, `g_Contactor[]`, `g_SystemFaults`, ...) resolve by plain name through
the S32DS gdb - the same mechanism `fault_decode.gdb` and `debug_live.bat` already use.

---

## 7. What is automated

| ID | Area | What it proves |
| --- | --- | --- |
| `REG-BUILD-001` | Build | `build.bat` succeeds and produces a non-empty `BMS_demo.elf` |
| `REG-SIL-001` | SIL | the existing SIL suite builds and pytest reports no failures, preserving strict xfail |
| `REG-FLASH-001` | Flash | `flash.bat` succeeds, the target reconnects, `.pflash` matches the ELF, firmware runs |
| `REG-BOOT-001` | Boot | reset reaches STANDBY under nominal stimulus without entering FAULT |
| `REG-SCHED-001` | Scheduler | `g_BmsSchedulerMissedTickCount` stays 0 and the pending-tick peak stays small |
| `REG-MEAS-001` | Measurement | `CellVoltageValid` with min = max = 3700 mV, delta 0 (decoder + CAN0 0x305/0x310-0x313) |
| `REG-MEAS-002` | Measurement | pack current/voltage valid flags and decoded values match the stimulus (plus CAN0 0x306/0x301) |
| `REG-CAN-001` | CAN | all 17 periodic CAN0 ids are present |
| `REG-CAN-002` | CAN | per-id inter-arrival statistics, burst detection, missing-frame detection |
| `REG-SM-001` | State Machine | STANDBY is stable with no critical fault |
| `REG-SM-002` | State Machine | CAN0 `0x201 = 0x01` moves STANDBY → ACTIVE and Pack 1 reaches RUN |
| `REG-CONTACTOR-001` | Contactor | the observed Pack 1 sequence is OFF → NEG_ON → PRECHARGE → POS_ON → RUN |
| `REG-FAULT-001` | Fault | one cell at 4260 mV sets `FAULT_CELL_OV`, reaches FAULT, opens all outputs, latches history |
| `REG-FAULT-002` | Fault | 4200 mV holds, 4140 mV clears the live fault, FAULT stays latched, `0x03` clears it |
| `REG-FAULT-003` | Fault | `0x201 = 0x04` clears the latched history without touching live masks or state |
| `REG-SM-003` | State Machine | CAN0 `0x201 = 0x02` moves ACTIVE → STANDBY with all Pack 1 outputs at 0 |
| `REG-XCP-001` | XCP | CONNECT on CAN5 `0x600` yields a positive `0x601` response |
| `REG-XCP-002` | XCP | SHORT_UPLOAD of `g_BmsXcpConnectCount` matches the same address read through the probe |

Faults are provoked through the measurement path (a real vAFE frame), never by calling
`FaultManager_SetSystem` from Python.

---

## 8. What is NOT yet validated

1. **Pack 1 physical relay GPIO is not measured externally.** `REG-CONTACTOR-001` reads
   the firmware's own output shadow state and the CAN0 `0x302` frame. Nothing confirms
   that PTC23/24/25 actually moved. That needs external instrumentation the runner does
   not have.
2. **Precharge is simulation/timer based.** `BMS_CONTACTOR_SIMULATION_MODE = 1`
   (a `#define` in `src/control/Bms_Contactor.c`), so precharge completes on a fixed
   100 ms timer and `FAULT_PRECHARGE_TIMEOUT` cannot be raised.
3. **Real Vbus/Vpack precharge is not validated.** `Bms_Contactor_SetBusVoltage()` has no
   caller in this configuration.
4. **TLF35584 regression is not implemented.** Extension point for Phase 2+: PMIC SPI
   transaction checks, window watchdog (WWD) and functional watchdog (FWD) behaviour,
   reset behaviour, and the PMIC's part in sleep/wake.
5. **Real AFE regression is not implemented.** Only the CAN-based virtual AFE is
   exercised; there is no SPI AFE path in this firmware.
6. **Sleep/wake is not part of Phase 1.** The runner does not assume a debugger stays
   connected, so a disconnect → external wake → reconnect sequence can be added without
   restructuring the framework.
7. **PEmicro snapshot reliability is outside Phase 1.** The `*_pemicro.bat` capture path
   is documented in `README.md` §10 as not yet trustworthy, and nothing here depends on
   it.
8. **The existing documented strict xfail remains a known defect.** The SIL suite keeps
   one strict `xfail` for the stuck vPACK alive counter
   (`src/battery/SOC_DESIGN.md` §5.11). It is reported as `XFAIL`, not silently
   reclassified, and it will turn into a failure the moment the defect is fixed - which
   is the intended behaviour.
9. **No DOWNLOAD regression.** `XCP DOWNLOAD` is not exercised in Phase 1.
10. **Temperature is not injected.** Pack temperatures come from the on-board NTCs on
    ADC1; there is no CAN path for them, so `nominal_config.json` carries
    `temperature_c` for future use only.

---

## 9. Layout

```
hil/regression/
    run_regression.py       CLI, orchestration, staged buses, safe teardown
    regression_result.py    result contract, suite aggregation, Markdown + JSON reports
    regression_common.py    frame codecs, CAN0 decoders, timing statistics, build/SIL stages
    regression_can.py       CAN transport abstraction over python-can (no vendor coupling)
    regression_target.py    J-Link target, ELF symbol layout, simulators, bus sniffer
    test_boot.py            REG-FLASH-001, REG-BOOT-001
    test_measurements.py    REG-MEAS-001/002, REG-CAN-001/002, REG-SCHED-001
    test_state_machine.py   REG-SM-001/002/003, REG-CONTACTOR-001
    test_fault_recovery.py  REG-FAULT-001/002/003
    test_xcp.py             REG-XCP-001/002
    tests/                  host-side unit tests (no probe, no CAN adapter)
    reports/                runtime artifacts (gitignored)
    FIRMWARE_FINDINGS.md    defects and inconsistencies found while building this framework
```

The five `test_*.py` scenario modules are **not** pytest modules: they need hardware.
`conftest.py` excludes them from pytest collection so `python -m pytest hil` cannot
accidentally execute them.

`hil/hil_common.py` gained two purely additive helpers (`Bench.read_until`,
`Bench.sample`, `elf_identity`). `hil/sop_hil.py` and `hil/sop_init_hil.py` are unchanged
and still work.

### Extending the framework

* Add a normal value: edit `nominal_config.json`. Test modules never hard-code cell or
  pack values.
* Add a timing policy: edit the `timing` / `timeouts` sections of `nominal_config.json`.
* Add a CAN backend (SocketCAN, Vector): add an entry to `can_config.json` with the
  matching `interface`. No test logic changes.
* Add a symbol to observe: extend `CORE_FIELDS` or `OPTIONAL_FIELDS` in
  `regression_target.py`. `tests/test_symbol_layout.py` validates every entry against the
  real ELF and fails if a struct code drifts.
