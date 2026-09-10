# Changelog

All notable changes to BMS_demo, newest first. Dates are commit dates; hashes refer to
`git log --oneline` on `master`/`bugfix/CAN_cyc_time` unless noted. This project doesn't use version
tags, so entries are grouped by date/topic instead of a version number.

## [Unreleased]

- **`src/battery/SOP_DESIGN.md` (draft, not yet implemented).** Design proposal for two new modules,
  pending review: `Bms_Sop` (Pack 1 discharge/regen/charge current limits — smaller of a static
  SOC-by-temperature table and a dynamic 2-RC equivalent-circuit-model prediction, trimmed by a
  feedback derating layer as cell voltage/temperature approaches the fault trip, with an asymmetric
  output rate limiter) and `Bms_BattCfg` (shared cell/pack config module that SOC's OCV table/
  capacity would migrate into). Nine open questions listed before coding starts. (`fb0d9ce`)

## 2026-09-06 — Defer SOC init until inputs are ready; misc

- **`Bms_Soc`: OCV-reset tier deferred instead of evaluated too early.** `Bms_Soc_Init()` used to run
  before the scheduler started and before any vAFE frame had been decoded, so `CellVoltageValid` was
  always `FALSE` and the OCV tier could never fire. Init is now a two-stage deferred start resolved
  from `Bms_Soc_MainFunctionPack()`: wait for elapsed sleep time to be readable, then (if the sleep
  was long enough) wait for `CellVoltageValid` before seeding from the OCV table — bounded by one
  shared, calibratable budget `g_BmsSocOcvWaitTimeout_ms` (500 ms default), falling through to NVM
  then the blind default on timeout. A new `InitSource = Pending (3)` is reported on `0x308` while
  waiting; estimators report capacity 0 / `Valid = FALSE` and integrate no charge until resolved.
  (`src/battery/Bms_Soc.c`, `Bms_Soc.h`, `DBC/BMS_demo.dbc`, `b56325d`)
- **New module `Bms_SleepTime`.** Elapsed power-off time moves out of `Bms_Soc.c` into its own
  module, matching the `Bms_Adc`/`Bms_Ntc` pattern for hardware-backed inputs (currently hardcoded to
  0 s, no RTC on target yet); `Bms_Soc`'s sleep-time API is unchanged and just delegates to it.
  (`src/battery/Bms_SleepTime.*`)
- SIL suite: `IT-04` (previously a skipped placeholder) now runs; `IT-09`–`IT-14` added, covering the
  pending hold, stage-2 timeout to NVM, stage-1 gating, and the no-integration-while-pending
  invariant. 48 passed, 1 xfailed (pre-existing 5.11 alive-counter defect).
- README documented SOC estimation (§13) and the SIL test platform (§14). (`fd479f9`)
- `.gitignore`: ignore `.tmp.driveupload/` (Google Drive desktop sync scratch folder). (`5c730b8`)

## 2026-09-05 — Three-estimator SOC, SIL test platform, CAN cycle-time fix

- **`Bms_Soc`: replaced the single Coulomb counter with three cell-based estimators.** `Min`/`Max`/
  `Avg` estimators are seeded from the weakest/strongest/average cell voltage and reported as one
  blended pack SOC — a weighted average of `Min`/`Max`, weighted by where `Avg` sits in the 0–100 %
  range (converges to the weak cell near empty, the strong cell near full). Startup is a strict
  3-tier chain (OCV reset → NVM restore → compile-time default), latched as `InitSource`. All
  estimator state changes go through one commit point, `Bms_Soc_SetEstimatorCapacity()`.
  (`src/battery/Bms_Soc.*`, `2914919`)
- **`Bms_Nvm`: SOC persistence reworked for the three estimators.** Now persists a single 24-byte
  record (magic `"SOC1"`→`"SOC2"`) and erases-and-wraps the sector when full (~341 saves) instead of
  returning `FALSE` forever; also fixes a sector-full boundary check that tested pointer position
  instead of whether the next record actually fits. (`src/storage/Bms_Nvm.*`)
- **New module `src/common/Lib_Interp`.** `Lib_Interp_Lookup_1D_uint16()`: shared 1-D table lookup +
  linear interpolation over a sorted uint16 table, clamped at both ends (no extrapolation); Y need
  not be monotonic in X, so derating/NTC-style falling curves work too. Wired into both build
  configs. (`ec99263`)
- **CAN: new frame `0x30B BMS_CellSoc`** carries `Pack1SOCMin`/`Max`/`Avg` (0.1 %/bit) with a
  per-estimator validity bit, sent from the 100 ms task. `0x308 BMS_SocStatus` gains
  `Pack1SOCInitSource` (bits 3:1 of byte 2: 0 default, 1 OCV, 2 NVM, 3 pending) — existing signal bit
  positions unchanged. `DBC/BMS_demo.dbc` updated with both frames and an init-source value table.
  (`src/communication/Bms_Can.*`, `4776ac8`)
- Battery monitor: `Bms_Vafe_UpdateStatistics()` now also computes `AverageCellVoltage_mV` in the
  same pass as min/max, propagated through `Battery_Monitor` as `AverageCellVoltage` (needed by the
  `Avg` SOC estimator). Also fixed a stale doc comment on `PackCurrent_mA` (said "positive =
  discharge", contradicting the actual over-current thresholds and SOC sign convention — no code
  change). (`8b22039`)
- Added `src/battery/SOC_DESIGN.md`: requirements, functional architecture, per-subfunction detailed
  design, SIL validation results, and known limitations (including one open defect, 5.11 — a stuck
  vPACK alive counter does not invalidate pack current). (`01ba962`)
- **New `sil/` software-in-the-loop test platform.** Compiles the BMS application layer natively
  (MinGW-w64 GCC) and drives it from Python via `pytest`/ctypes — no hardware needed. Only the RTD
  driver layer is faked (a NOR-accurate `C40_Ip` data-flash model + settable `Bms_Adc`/`Bms_Ntc`
  doubles); `Bms_Nvm`, `Bms_Vafe`, `Bms_Vpack`, `Battery_Monitor`, `Fault_Manager` and `Lib_Interp`
  compile unmodified, stimulated via real CAN frames through the production decoders. 49 tests across
  3 suites (SOC, persistence, Lib_Interp): 41 passed, 1 skipped, 1 xfail. Per-feature reports under
  `sil/reports/`, rolled up in `sil/TEST_REPORT.md`. (`b3f9991`)
- **Scheduler: fixed CAN cycle-time bursts after the main loop falls behind** (merged from
  `bugfix/CAN_cyc_time`, PR #5). `Bms_Scheduler_MainFunction` used to run every elapsed base tick
  through the full task table in a tight catch-up loop, which could burst several periods' worth of
  CAN sends back-to-back. It now advances each task's counter by the elapsed tick count but executes
  a due task **at most once per call** (counter reduced modulo its period, preserving phase). Added
  diagnostics: `g_BmsSchedulerPendingTicks`, `g_BmsSchedulerPendingTicksMax`,
  `g_BmsSchedulerMissedTickCount`. (`src/app/Bms_Scheduler.c`, `9f8cb0b`/`8ed4fb0`)

## 2026-09-03 — `dbdf372` packV bug fix

- Pack 1 voltage now sourced from the decoded `g_BmsVpackData.VoltageValid` /
  `PackVoltage_mV` (vPACK/CAN2 0x411) instead of the old standalone `g_CanPack1Voltage*` globals, in
  `Battery_Monitor.c` (fault timeout check, power calculation, and voltage assignment all updated
  consistently).
- `DBC/BMS_demo.dbc`: `vPack_Voltage` (0x1041 / CAN 0x411) signals `Pack1VoltageSim` and
  `BusVoltage_mV` changed from a raw-mV integer factor to a `0.001` factor so DBC tools display volts
  instead of millivolts; underlying byte encoding unchanged.

## 2026-09-02 — XCP Phase 1 (PR #4 `feature/XCP`, `e7a2ca2`)

- New calibration/measurement channel: `src/communication/xcp/` (`Xcp.c` protocol state machine,
  `Xcp_Can.c` CAN5 transport, `Xcp_Cfg.h`). Supports `CONNECT` / `GET_STATUS` / `SET_MTA` / `UPLOAD` /
  `SHORT_UPLOAD` / `DOWNLOAD` with RAM read/write against a configured whitelist.
  CAN5 (PTC26/PTC27), 1 Mbit/s, request ID `0x600` / response ID `0x601`, own FlexCAN instance
  separate from CAN0-2.
  - Polled from the 10 ms task (`Xcp_Can_MainFunction`), not 100 ms, since a real XCP master/DAQ
    tool expects lower latency than the other CAN traffic.
  - Initialized in `main()` alongside CAN0/1/2 (`Xcp_Can_Init`).

## 2026-09-01 — 3-pack vAFE/vPACK over CAN + build tooling (PR #3, PR #2)

- `Bms_Can.c`: CAN0 TX extended with 16 individual cell voltages (`0x310`–`0x313`) alongside the
  existing status/fault frames.
- `build.bat` reworked to auto-detect the installed S32DS version and Arm GCC toolchain instead of
  hardcoding paths/versions; restored `BMS_NTC_COUNT` macro lost in a prior refactor; removed an
  obsolete standalone `make/` build folder in favor of the Eclipse-generated `Debug_FLASH`/
  `Release_RAM` configs.

## 2026-08-30 — 2026-08-31 — vAFE measurement cycle, SOC→NVM, 1 Mbit/s CAN1/2

- `Bms_Vafe.c`: cell voltages now gated by a measurement-cycle header frame (`0x405`, rolling
  counter) — only a complete set of 4 voltage frames (`0x401`–`0x404`) belonging to one cycle marks
  `DataValid`; an interrupted cycle is discarded rather than mixed with the next one.
- CAN1 (vAFE) and CAN2 (vPACK) bitrate raised from the initial rate to 1 Mbit/s.
- `Bms_Soc.c` / `Bms_Nvm.c`: Pack 1 state-of-charge Coulomb counter now persists to data flash
  (`C40_Ip`), restored at boot, saved periodically or on sufficient change.
- Contactor precharge/close sequencing wired up per pack (`Pack1 contactor work`); latched fault
  history added to CAN0 (`0x309`/`0x30A`).
- Standalone CLI `make/` build folder added, later removed (see 2026-09-01) in favor of the IDE
  makefiles.

## 2026-08-26 — 2026-08-29 — vPACK (virtual ADBMS2950) over CAN2, SOC estimator

- `Bms_Vpack.c` added: decodes pack current/shunt voltage (`0x410`) and pack/bus voltage (`0x411`)
  from a simulated ADBMS2950 pack monitor on CAN2, with an alive-counter comm-health check.
  Pack current/power routed into `Battery_Monitor` and out over CAN0 (`0x306`/`0x307`).
- `Bms_Soc.c` added: Coulomb-counting state-of-charge estimator for Pack 1.
- Temperature fault thresholds widened after initial bring-up testing ("all test pass").

## 2026-08-20 — 2026-08-25 — Fault manager, contactor state machine, vAFE over CAN1

- `Fault_Manager.c` added: per-pack + system 32-bit fault masks, critical-fault mask, precharge
  timeout → fault → all-contactors-off interlock.
- Contactor/precharge state machine implemented per pack (NEG → PRECHARGE → POS → RUN), driven by
  ADC bus-voltage readings (ADC0 ×4 channels).
- `Bms_Vafe.c` added: 16 cell voltages decoded from CAN1 (`0x401`–`0x404`); cell OV/UV hysteresis.
- NTC resistance→temperature (Beta equation) and pack-voltage decoding added to `Battery_Monitor.c`.
- CAN0 status/fault frames and `DBC/BMS_demo.dbc` introduced.

## 2026-08-13 — 2026-08-19 — Initial bring-up

- Project init; PIT0-driven 10 ms base tick and the table-driven `Bms_Scheduler` (pending-tick
  counter pattern to avoid losing ticks under ISR/main-loop jitter).
- LED heartbeat, ADC1 (NTC + pack voltage channels), NTC reading.
- CAN0 500 kbit/s classic CAN TX/RX; `0x200` debug command and `0x201` control command frames.
- BMS supervisor state machine (INIT/STANDBY/ACTIVE/FAULT) V1.0.
- LPSPI1 configuration in preparation for a physical AFE (later superseded by the CAN-based vAFE/
  vPACK simulation approach used throughout the rest of the project).
