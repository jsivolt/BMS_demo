# Changelog

All notable changes to BMS_demo, newest first. Dates are commit dates; hashes refer to
`git log --oneline` on `master`/`bugfix/CAN_cyc_time` unless noted. This project doesn't use version
tags, so entries are grouped by date/topic instead of a version number.

## [Unreleased] — branch `bugfix/CAN_cyc_time`

- **Scheduler: fix CAN cycle-time bursts after the main loop falls behind.**
  `Bms_Scheduler_MainFunction` used to run every elapsed base tick through the full task table in a
  tight catch-up loop, which could burst several periods' worth of CAN sends back-to-back. It now
  advances each task's counter by the elapsed tick count but executes a due task **at most once per
  call** (counter reduced modulo its period, preserving phase). Added diagnostics:
  `g_BmsSchedulerPendingTicks`, `g_BmsSchedulerPendingTicksMax`, `g_BmsSchedulerMissedTickCount`.
  (`src/app/Bms_Scheduler.c`, uncommitted)

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
