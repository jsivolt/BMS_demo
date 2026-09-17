# Firmware findings — discovered while building the Phase 1 regression framework

**Status of every entry: OPEN. Nothing in this file has been fixed.** The regression
framework tests the firmware as it exists today; no production behaviour was changed to
make a test pass.

## How to read this file

| Term | Meaning |
| --- | --- |
| **Confirmed** | The behaviour was read directly out of the current source; the quoted code is the whole story. Whether it is *wrong* is still a separate judgement. |
| **Inconsistency** | Source and documentation, or two parts of the source, disagree. One of them is wrong, but which is a design decision, not a reading of the code. |
| **Suspected defect** | The behaviour contradicts what the surrounding code and design notes say was intended, but there is no specification that makes it provably a bug. |

Nothing here is recorded as a *confirmed defect* unless the intended behaviour can be
demonstrated from the source or from a design document. Where that is not possible, the
entry says so.

## Index

| ID | Subsystem | One-line summary | Class |
| --- | --- | --- | --- |
| D1 | State machine | INIT settle window is cumulative, not continuous | Suspected defect |
| D2 | vAFE decoder | Cell data never expires; losing CAN1 raises no fault | Suspected defect |
| D3 | vAFE decoder | Measurement counter is latched, never validated | Inconsistency |
| D4 | vPACK decoder | Alive counter masked to 4 bits while the DBC declares 8 | Inconsistency |
| D5 | vPACK / Battery_Monitor | Alive validity has no timeout; latched copies keep a device fault set | Suspected defect |
| D6 | Battery_Monitor | Device status raises a fault on any non-zero byte; status bit macros unused | Inconsistency |
| D7 | Contactor | Contactor FAULT is not latched, unlike the supervisor FAULT | Inconsistency |
| D8 | Contactor | Simulated precharge reuses the post-POS settle constant | Inconsistency |
| D9 | LED driver | `Bms_Led` is dead code; two callers write the pins directly | Inconsistency |
| D10 | CAN TX | All CAN0 frames share one mailbox, hiding a failed non-final frame | Suspected defect |
| D11 | Fault manager | `FaultManager_PackHasCriticalFault()` returns TRUE for an out-of-range pack | Inconsistency |
| D12 | XCP transport | Instance 5 is paired with `FlexCAN_State3` | Inconsistency |
| D13 | Documentation | Stale pass counts and a stale "not yet implemented" entry | Documentation |
| D14 | Documentation | README §16 overstates what the HIL scripts verify about the image | Documentation |

---

## D1 — INIT settle window is cumulative, not continuous

* **Subsystem:** Supervisor state machine
* **Source:** `src/app/Bms_StateMachine.c` (`case BMS_STATE_INIT`, `g_BmsInitCycles`)
* **Observed behaviour (confirmed):** the counter is incremented only while a critical
  fault is present, and is not cleared when the fault goes away:

  ```c
  if (FaultManager_HasCriticalFault() == TRUE)
  {
      g_BmsInitCycles++;
      if (g_BmsInitCycles >= BMS_STATE_INIT_SETTLE_CYCLES) { g_BmsState = BMS_STATE_FAULT; }
  }
  else
  {
      g_BmsState = BMS_STATE_STANDBY;   /* counter keeps its value */
  }
  ```

  `g_BmsInitCycles` is reset only in `Bms_StateMachine_Init()`.
* **Expected/intended:** `README.md` §4 describes the rule as "a critical fault has to
  persist before it latches" and "INIT waits 2 s ... a fault that clears inside that
  window falls through to STANDBY". That reads as 2 s of *uninterrupted* fault. The code
  accumulates across separate episodes, so after boot has already accrued 20 samples the
  very first 100 ms sample with any critical fault transitions straight to FAULT.
* **Regression impact:** `REG-BOOT-001` only asserts INIT → STANDBY with no FAULT before
  it under nominal stimulus, so it does not depend on this. A future test that bounces a
  fault in and out of INIT would need to know which rule is intended before it can assert
  anything.
* **Safety/functional impact:** the practical effect is a *more* eager transition into
  FAULT than documented, which is fail-safe rather than unsafe. It does mean the
  documented 2 s debounce is not what actually protects a slow-starting AFE; what protects
  it is that the fault must clear before the counter saturates.
* **Proposed future test:** drive a critical fault for N < 20 cycles, clear it for a while,
  then re-assert it and confirm the transition happens on the first sample rather than
  after a fresh 2 s. Pin the *actual* rule, then decide whether the rule should change.
* **Status:** OPEN

---

## D2 — Cell data never expires, and losing CAN1 raises no fault

* **Subsystem:** vAFE decoder
* **Source:** `src/battery/vAFE/Bms_Vafe.c` (`HeaderValid`, `DataValid`, no timeout arm)
* **Observed behaviour (confirmed):** `g_BmsVafeData.DataValid` is set inside
  `Bms_Vafe_PublishMeasurement()` and cleared only by `Bms_Vafe_Init()`. `HeaderValid` is
  set on the first `0x405` and likewise cleared only by `Bms_Vafe_Init()`. There is no
  AFE-cycle timeout anywhere in the file.
* **Expected/intended:** not provable from the source. The vPACK decoder *does* have a
  1000 ms timeout that invalidates its data, and `Battery_Monitor` raises
  `FAULT_AFE_COMM` when `g_BmsVafeData.DataValid` is FALSE. A stuck-TRUE validity flag
  therefore makes that fault unreachable after the first good cycle. Because the two
  decoders in the same product behave differently and `FAULT_AFE_COMM` is in
  `FAULT_CRITICAL_MASK`, this is recorded as a **suspected defect** rather than a
  confirmed one - no document states the intended rule.
* **Regression impact:** directly limits `REG-MEAS-001`. That case proves the nominal
  values reach the decoder and the CAN0 frames; it cannot prove that a *lost* AFE is
  detected, because the firmware does not detect it.
* **Safety/functional impact:** the highest-impact entry in this file. If CAN1 goes silent
  while the packs are live, the last good cell voltages stay "valid" and no AFE
  communication fault is raised. Cell over/under-voltage protection continues to run
  against frozen data.
* **Proposed future test:** after valid cell data is established, stop the CAN1 stimulus
  and assert that `CellVoltageValid` goes FALSE within a bounded time and that
  `FAULT_AFE_COMM` is raised. This test is expected to FAIL against today's firmware,
  which is the point of writing it.
* **Status:** OPEN

---

## D3 — Measurement counter is latched, never validated

* **Subsystem:** vAFE decoder
* **Source:** `src/battery/vAFE/Bms_Vafe.c` (`case 0x405`, `g_BmsVafeActiveCounter`)
* **Observed behaviour (confirmed):** byte 0 of `0x405` is copied straight into
  `g_BmsVafeActiveCounter` and published as `MeasurementCounter` when the cycle completes.
  There is no expected-increment check, no rollover handling and no comparison with the
  previous value.
* **Expected/intended:** inconsistent with `Bms_Vpack`, which validates its alive counter
  as previous + 1. `README.md` §7 simply describes the counter as a rolling counter; no
  document says it must be checked.
* **Regression impact:** `REG-MEAS-001` records the counter but cannot assert a rule.
* **Safety/functional impact:** low on its own. A stuck or jumping AFE counter is not
  detected, but this counter is not used for any safety decision today - only published.
  It matters if it is ever used as a liveness signal.
* **Proposed future test:** replay a header with a frozen counter and with a counter that
  jumps, and assert whatever rule is decided (accept silently, or raise `FAULT_AFE_COMM`).
* **Status:** OPEN

---

## D4 — vPACK alive counter is masked to 4 bits while the DBC declares 8

* **Subsystem:** vPACK decoder
* **Source:** `src/battery/vPACK/Bms_Vpack.c` and `Bms_Vpack.h`
  (`BMS_VPACK_ALIVE_MAX_VALUE = 15U`); `DBC/BMS_demo.dbc` `vPack_Current.AliveCounter`
* **Observed behaviour (confirmed):** the expected value is
  `(previous + 1) & 0x0F`, so a producer rolling 0…255 fails the check on the 16 → 17
  transition and on every 16th frame afterwards.
* **Expected/intended:** the DBC declares the field as 8 bits with range `[0|255]`. Either
  the mask is wrong or the DBC is wrong; the source does not say which. Recorded as an
  inconsistency.
* **Regression impact:** high. The framework must roll its own `0x410` counter modulo 16
  to keep `AliveValid` TRUE. A simulator written from the DBC would fail intermittently,
  which is exactly the kind of false signal a regression framework must not produce.
  `nominal_config.json` documents this, and `REG-MEAS-002` asserts the modulo-16 chain.
* **Safety/functional impact:** a real pack monitor that rolls 0…255 would raise
  `FAULT_VPACK_ALIVE_ERROR` (critical) every 16 frames - or, read the other way, a real
  device that only ever counts to 15 would work fine with the mask but not with the DBC.
* **Proposed future test:** feed a counter rolling 0…255 and record whether
  `FAULT_VPACK_ALIVE_ERROR` appears every 16th frame; then decide the intended width.
* **Status:** OPEN

---

## D5 — Alive validity has no timeout, and latched vPACK copies keep a device fault set

* **Subsystem:** vPACK decoder / Battery_Monitor
* **Source:** `src/battery/vPACK/Bms_Vpack.c`, `src/battery/Battery_Monitor.c`
  (`BatteryMonitor_UpdatePackMonitor`)
* **Observed behaviour (confirmed):** two related things.
  1. `g_BmsVpackData.AliveValid` is recomputed only when a `0x410` frame arrives; it has
     no timeout arm of its own, unlike `CurrentValid` and `VoltageValid`.
  2. The six vPACK field copies in `g_BatteryData` (`VpackPackVoltage_mV`,
     `VpackBusVoltage_mV`, `VpackAliveCounter`, `VpackStatus`, ...) are refreshed only
     while `g_BmsVpackData.Valid == TRUE`. `BatteryMonitor_UpdateVpackFaults()` then does:

     ```c
     if (g_BatteryData.VpackStatus != 0U) { FaultManager_SetSystem(FAULT_VPACK_DEVICE_FAULT); }
     else                                 { FaultManager_ClearSystem(FAULT_VPACK_DEVICE_FAULT); }
     ```

     Because `VpackStatus` keeps its last value whenever `Valid` is FALSE, a non-zero
     status byte seen *once* keeps `FAULT_VPACK_DEVICE_FAULT` (critical) asserted. Once
     `Valid` goes FALSE the fault can no longer be cleared by the device reporting OK,
     because the reported status is never refreshed.
* **Expected/intended:** a device fault should track the device's current status. The
  latched-copy pattern is deliberate for the *measurement* fields (they should hold their
  last good value), but applying it to the status byte turns a transient report into a
  latched fault. Recorded as a **suspected defect**.
* **Regression impact:** none of the Phase 1 cases hit this, because the nominal stimulus
  always sends status `0`. It would make any future "device fault clears" test fail.
* **Safety/functional impact:** a critial fault that cannot self-clear will hold the
  contactors open until an operator clears it. Fail-safe, but it can strand a healthy
  pack, and it hides whether the device fault is still real.
* **Proposed future test:** send one `0x410` with status non-zero, then a stream with
  status `0`, and assert `FAULT_VPACK_DEVICE_FAULT` clears.
* **Status:** OPEN

---

## D6 — Device status raises a fault on any non-zero byte

* **Subsystem:** Battery_Monitor
* **Source:** `src/battery/Battery_Monitor.c` (`BatteryMonitor_UpdateVpackFaults`),
  `src/battery/vPACK/Bms_Vpack.h`
* **Observed behaviour (confirmed):** any non-zero `Status` byte raises
  `FAULT_VPACK_DEVICE_FAULT`. The bit constants intended for the job —
  `BMS_VPACK_STATUS_CURRENT_FAULT`, `BMS_VPACK_STATUS_SHUNT_FAULT`,
  `BMS_VPACK_STATUS_COMM_FAULT` — are defined in the header and referenced nowhere.
* **Expected/intended:** the presence of three named bit macros implies the status byte was
  meant to be decoded bit-wise. Recorded as an inconsistency.
* **Regression impact:** the framework must send status `0` in the nominal stimulus, and
  `nominal_config.json` / `REG-MEAS-002` both state why. Without that, every measurement
  case would fail with a critical fault.
* **Safety/functional impact:** a single benign status bit holds a critical fault. Also
  removes the diagnostic value of knowing *which* fault the device reported.
* **Proposed future test:** drive each documented status bit individually and record the
  resulting system mask, then decide whether one bit should be critical and another not.
* **Status:** OPEN

---

## D7 — Contactor FAULT is not latched, unlike the supervisor FAULT

* **Subsystem:** Contactor state machine
* **Source:** `src/control/Bms_Contactor.c` (`case BMS_CONTACTOR_FAULT`)
* **Observed behaviour (confirmed):** the contactor machine leaves `BMS_CONTACTOR_FAULT`
  as soon as no critical pack or system fault is present - no clear request and no
  `0x201 = 0x03` needed. The supervisor `FAULT` state *is* latched and needs an explicit
  ClearFault. So after the underlying fault clears, the packs sit in `OFF` while the
  supervisor still reports `FAULT`.
* **Expected/intended:** `README.md` §5 does describe this correctly ("It leaves FAULT as
  soon as no critical pack or system fault is present"), but §4's general statement that
  "FAULT is latched" reads as if it covers both. Recorded as an inconsistency between two
  sections rather than a code defect.
* **Regression impact:** `REG-FAULT-001` accepts the contactor state as `FAULT` **or**
  `OFF`, precisely because the two machines un-latch at different moments. Asserting only
  `FAULT` would be flaky. That is recorded in the case's diagnostics.
* **Safety/functional impact:** none identified. Both paths end with all outputs de-asserted.
* **Proposed future test:** assert the exact ordering of the two un-latch events so the
  behaviour is pinned rather than tolerated.
* **Status:** OPEN

---

## D8 — Simulated precharge reuses the post-POS settle constant

* **Subsystem:** Contactor state machine
* **Source:** `src/control/Bms_Contactor.c` (`case BMS_CONTACTOR_PRECHARGE`, `#if
  BMS_CONTACTOR_SIMULATION_MODE == 1U`), `src/control/Bms_Contactor_Cfg.h`
* **Observed behaviour (confirmed):** with simulation mode on, precharge completes after
  `BMS_CONTACTOR_POS_DELAY_MS` (100 ms). That same constant is documented in
  `Bms_Contactor_Cfg.h` as the settle delay *after* POS_ON before precharge is released.
  One constant therefore controls two different stages.
* **Expected/intended:** the `#else` branch uses a dedicated `BMS_PRECHARGE_TIMEOUT_MS`.
  The simulation branch reusing the post-POS constant looks like a shortcut. Recorded as
  an inconsistency; the resulting timing is what the tests expect.
* **Regression impact:** `REG-CONTACTOR-001` expects roughly 100 ms per phase and records
  the median sampling interval. Changing `BMS_CONTACTOR_POS_DELAY_MS` for one purpose
  would silently change the other and would be visible as a phase-timing change.
* **Safety/functional impact:** none in simulation mode. If simulation mode is ever turned
  off, this constant stops being involved in precharge completion, so the coupling is
  bench-only.
* **Proposed future test:** none needed beyond `REG-CONTACTOR-001`; the finding exists so
  the coupling is not discovered by surprise.
* **Status:** OPEN

---

## D9 — `Bms_Led` is dead code

* **Subsystem:** LED driver
* **Source:** `src/drivers/Bms_Led.c`/`.h`; callers in `src/app/Bms_StateMachine.c` and
  `src/communication/Bms_Can.c`
* **Observed behaviour (confirmed):** `Bms_Led_Init/On/Off/AllOff` have no caller anywhere
  in `src/`. The state machine writes PTA31 directly through `Siul2_Dio_Ip_WritePin`, and
  `Bms_Can` writes PTA30 the same way with its own port/pin defines.
* **Expected/intended:** `README.md` §4 and §9 attribute those LEDs to `Bms_Led.c` and to
  `BMS_LED_BLUE`. Recorded as a documentation/source inconsistency.
* **Regression impact:** none. Nothing in the framework depends on the LED abstraction;
  `g_LedCounter` (a `static uint32` in `main.c`) is used as a liveness signal and is
  independent of the LED driver.
* **Safety/functional impact:** none. Maintenance risk only: editing `Bms_Led.c` has no
  effect on any LED that is actually driven today.
* **Proposed future test:** none. Fix the documentation, or wire the callers through the
  abstraction.
* **Status:** OPEN

---

## D10 — All CAN0 frames share one mailbox, hiding a failed non-final frame

* **Subsystem:** CAN transmit
* **Source:** `src/communication/Bms_Can.c` (`Bms_Can_SendFrame`, the 17 senders)
* **Observed behaviour (confirmed):** every one of the 17 periodic CAN0 frames is sent
  through `BMS_CAN_CFG_TX_MB_INDEX` (MB0) and each send overwrites the single
  `g_BmsCanTxStatus`. Only the last frame of a 100 ms pass - `0x30C` - is therefore
  observable in that variable. Separately, `Bms_Can1_SendTest` passes `raiseFault = FALSE`,
  so a failed `0x400` send aborts the transfer and reports nothing at all.
* **Expected/intended:** probably intentional (a single TX mailbox is normal for a small
  application), but the loss of per-frame diagnostics looks unintended.
* **Regression impact:** `REG-CAN-001` detects a missing frame by absence, so a *failed*
  frame is still caught; what is lost is the reason. The case reports the observed id set
  rather than relying on `g_BmsCanTxStatus`.
* **Safety/functional impact:** low for CAN0, which is periodic status traffic. It matters
  if a CAN1 or CAN2 frame is ever sent with `raiseFault = FALSE`, because a silent failure
  there would look like a decoder problem.
* **Proposed future test:** make a CAN0 send fail deliberately (force a bus error) and
  assert that the failure is attributable to the right frame. Requires instrumentation
  that does not exist yet.
* **Status:** OPEN

---

## D11 — `FaultManager_PackHasCriticalFault()` returns TRUE for an out-of-range pack

* **Subsystem:** Fault manager
* **Source:** `src/safety/Fault_Manager.c` (`FaultManager_PackHasCriticalFault`)
* **Observed behaviour (confirmed):** every other pack API range-checks `packId` and
  returns `FALSE` or `FAULT_NONE` for an invalid pack. This one returns `TRUE`.
* **Expected/intended:** the asymmetry looks deliberate - `Bms_Contactor_ProcessPack`
  calls it before touching pack state, and "assume critical" is the fail-safe answer.
  Recorded as an inconsistency because the same function family behaves two ways.
* **Regression impact:** none. The framework never passes an invalid pack id.
* **Safety/functional impact:** fail-safe as written. The risk is the opposite one: a
  future caller that *depends* on FALSE-for-invalid would be surprised.
* **Proposed future test:** none. Clarify the contract in the header comment.
* **Status:** OPEN

---

## D12 — XCP transport pairs instance 5 with `FlexCAN_State3`

* **Subsystem:** XCP CAN transport
* **Source:** `src/communication/xcp/Xcp_Can.c` (`Xcp_Can_Init`)
* **Observed behaviour (confirmed):** `FlexCAN_Ip_Init(XCP_CAN_CFG_INSTANCE /* 5 */,
  &FlexCAN_State3, &FlexCAN_Config5)`. The generated configuration only defines
  `FlexCAN_State0..3` and `FlexCAN_Config0/1/2/5`, so the init/link state object for
  instance 5 is a different instance's state struct.
* **Expected/intended:** not provable from the source. The pattern is common when the
  state struct is only used to hold a function-pointer table and its contents are
  instance-independent, which is plausible here. Recorded as an inconsistency, to be
  checked against the RTD documentation rather than asserted as a bug.
* **Regression impact:** `REG-XCP-001/002` would detect it indirectly - if the state
  struct were wrong, CONNECT would fail. Both cases currently SKIP for lack of a CAN5
  channel, so this is unverified.
* **Safety/functional impact:** none if it works (XCP is a calibration interface, not a
  safety path); if it does not, XCP silently does not work.
* **Proposed future test:** `REG-XCP-001` on a connected CAN5 channel is the test.
* **Status:** OPEN

---

## D13 — Stale documentation: pass counts and an implemented module called a draft

* **Subsystem:** Documentation
* **Source:** `src/battery/SOC_DESIGN.md`, `src/battery/SOP_DESIGN.md`,
  `sil/README.md`, `CHANGELOG.md` `[Unreleased]`
* **Observed behaviour (confirmed):**
  * `SOC_DESIGN.md` states "48 passed, 1 xfail"; the suite is 93 cases.
  * `SOP_DESIGN.md` states "43 SIL cases ... 103 pass and 1 xfail overall"; the suite is
    93 cases across four features.
  * `sil/README.md`'s layout section omits `test_sop.py`, `conftest.py` and `report.py`.
  * `CHANGELOG.md` `[Unreleased]` still describes `SOP_DESIGN.md` as "draft, not yet
    implemented", while `Bms_Sop.c` exists, `SOP_DESIGN.md` says "Implemented and
    published", and 40 SIL cases plus 21 HIL cases cover it.
* **Expected/intended:** the true counts, as reported by `sil/TEST_REPORT.md` and the
  root `README.md` §15 (93 cases, 92 passed, 1 strict xfail).
* **Regression impact:** none mechanically, but the root `README.md` §15 was corrected on
  2026-09-16 while the design documents were not, so three documents now disagree with
  each other. `REG-SIL-001` reports the counts it actually parses from pytest, which is
  the authoritative answer.
* **Safety/functional impact:** none.
* **Proposed future test:** none. `REG-SIL-001` parses the live pytest summary rather than
  trusting any document, which is the mitigation.
* **Status:** OPEN

---

## D14 — README §16 overstates what the HIL scripts verify about the image

* **Subsystem:** Documentation
* **Source:** `README.md` §16
* **Observed behaviour (confirmed):** the section says both HIL scripts verify that
  `Debug_FLASH/BMS_demo.elf` "is on disk and match the image actually flashed". The check
  is `Bench.verify_flash()`, a byte compare of the target `.pflash` section against the
  same section of the ELF, performed *after* connecting. There is no hash or mtime
  comparison, and the ELF is not known to be the file the operator intended to flash.
* **Expected/intended:** state the check for what it is.
* **Regression impact:** the framework itself is explicit: `host_artifact_identity` and
  `target_flash_verification` are reported as separate sections, and the report states
  that the SHA256 describes the host file and does not prove what the MCU holds.
* **Safety/functional impact:** none. Reporting accuracy only - a reader could otherwise
  believe a provenance guarantee exists that does not.
* **Proposed future test:** none.
* **Status:** OPEN

---

## Things deliberately not changed

For the record, the following were considered and left alone, because changing them would
be a production change rather than a regression change:

* `BMS_CONTACTOR_SIMULATION_MODE` stays `1U`. `REG-CONTACTOR-001` states in its output
  that precharge is timer-based and that real Vbus/Vpk precharge is not validated.
* The over-voltage trip points (set 4250 mV / clear 4150 mV) are used as-is.
  `REG-FAULT-001/002` drive 4260 / 4200 / 4140 mV, which sit either side of those
  thresholds. Driving a single cell that far also sets `FAULT_CELL_IMBALANCE`
  (560 mV delta against a 300 mV set point). That is expected collateral, it is recorded
  in the case diagnostics, and it is not in `FAULT_CRITICAL_MASK`.
* No symbol was made non-static, and no debug mirror was added, to expose private state.
  Every value the framework reads was already reachable through the ELF.
