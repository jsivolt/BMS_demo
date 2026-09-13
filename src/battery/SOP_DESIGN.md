# SOP Estimation: Design Document

Modules: `Bms_Sop` (Pack 1 state of power, meaning the current limits), `Bms_BattCfg` (shared battery data configuration).
Target: NXP S32K344, bare metal, S32K3 RTD 7.0.1.
Status: **Implemented and published.** `Bms_Sop`, `Bms_BattCfg`, the shared 2-D lookup and CAN frame `0x30C` are all built, and 33 SIL cases cover the module. The limit maps are **placeholder calibration**, not datasheet ratings, so no published limit is trustworthy yet. One decision still open, section 7.6. See section 7.
Last updated: 2026-09-12.

State of power (SOP) is the largest current the pack can carry right now without breaking a cell limit.

This document follows the `simple-english` writing rules.

---

## Contents

1. [Functional requirements](#1-functional-requirements)
2. [Functional architecture](#2-functional-architecture)
3. [Detailed design](#3-detailed-design)
4. [Validation plan](#4-validation-plan)
5. [Known limitations and future improvements](#5-known-limitations-and-future-improvements)
6. [Change log](#6-change-log)
7. [Open questions for review](#7-open-questions-for-review)

---

## 1. Functional requirements

### 1.1 Scope

This design has two deliverables. The second one exists to serve the first.

`Bms_Sop` computes the current limits for Pack 1. It produces a discharge limit, a regenerative braking limit, and a charge limit. Regenerative braking is the mode where the motor pushes current back into the pack. Each limit comes from one static lookup table, indexed by SOC and temperature, then trimmed by a feedback derate. Packs 2 and 3 are out of scope. They have no per-pack cell-voltage split, so there is no Min or Max cell SOC to index a table with and no cell voltage to derate against. Section 5.3 gives the full reason. `Bms_Soc` is limited to Pack 1 as well, for a related sensor reason.

`Bms_BattCfg` is a new battery data configuration module. It is the single owner of every cell constant and pack constant in the firmware. That set is the capacity, the open-circuit-voltage curve, the cell safety envelope, and the static SOP limit tables. Open-circuit voltage (OCV) is the cell voltage after a long rest. The cell safety envelope is the voltage band and temperature band the cell must stay inside. `Bms_BattCfg` holds data and read functions only. It has no algorithm, no state, and no runtime input.

The OCV table and the pack capacity live inside `Bms_Soc.c` today. They move into `Bms_BattCfg`. This move is a refactor that keeps behavior the same. The numbers do not change. The SOC results do not change. The current SIL test suite passes with no test edits. SIL means software in the loop, the host build that runs the firmware logic on a PC.

Out of scope:

- State of health.
- Capacity fade.
- Cell balancing.
- Power limits in watts.
- Any limit published in a unit other than current.
- Any limit for Pack 2 or Pack 3.
- A dynamic, model-based prediction of the limit. Section 6 records why this was cut.
- Rate-limiting the output and checking whether the inputs are valid. Section 6 records why these were cut too.

### 1.2 Requirements: `Bms_Sop`

| ID | Requirement | Rationale |
|---|---|---|
| SOP-FR-01 | The module shall compute current limits in two operating modes: Discharge (not charging) and Charge. | A charger attached and a load drawing current do not share one safe current band. |
| SOP-FR-02 | The module shall compute all three limits in every mode. In Discharge mode the discharge limit and the regenerative braking limit shall be the ones that apply. In Charge mode the charge limit shall be the one that applies. | These are the three limits the consumer needs. Computing all three unconditionally keeps the calibration fields meaningful in both modes (SOP-FR-03) and keeps the pipeline branch-free. |
| SOP-FR-03 | The module shall publish all three limits in every mode. For a limit that does not apply to the active mode it shall force the published value (`Final_dA`) to zero. The calibration fields of that limit (`Table_dA`, `DerateFactor`) shall keep their computed values. | A consumer must never guess which limits are live. A zero is clear. A stale value is not. The calibration fields never reach `0x30C`, so forcing them buys the consumer nothing. Keeping them lets a calibrator see the inactive direction, over `0x30D` or over XCP, whichever section 7.3 settles on. |
| SOP-FR-04 | The module shall compute each limit from one static lookup table indexed by SOC and temperature. The discharge table shall be indexed by the weakest cell's SOC. The regen and charge tables shall be indexed by the strongest cell's SOC. | The table holds datasheet, contactor, and fuse ratings. The weak cell reaches the low cutoff first under load. The strong cell reaches the high cutoff first under charge. The table must follow the cell that runs out first. |
| SOP-FR-05 | A feedback derating layer shall sit on top of SOP-FR-04. It shall watch the minimum cell voltage, the maximum cell voltage, and the maximum temperature. It shall reduce the limits as any of those three approaches its safety threshold. Derating means cutting the limit by a factor below one. | The table is feed-forward and open loop. Without a term that reads the measured state, a stale calibration walks the cell into the fault trip instead of backing off first. |
| SOP-FR-06 | Derating shall be continuous. It shall reach its floor at a threshold that sits inside the matching fault-trip threshold. | A step to zero at the trip point is a sudden loss of output and still trips the fault. Derating must finish before protection starts. |
| SOP-FR-07 | The derate factor for a limit shall be the minimum of the factors that apply to that limit direction. | Any one approaching limit governs. A product of two mildly active factors over-derates, so the factors must not multiply. |
| SOP-FR-08 | The module shall hold no battery characterization data of its own. All tables and constants shall come from `Bms_BattCfg`. | This is the reason the configuration module exists. |
| SOP-FR-09 | The module shall publish limits as `uint16` magnitudes in units of 0.1 A. The field name shall imply the direction. | This removes sign confusion from the CAN interface. The sign convention lives once, inside the computation, not in the published signal. |
| SOP-FR-10 | The module shall use no Kalman filter, no observer, and no online parameter identification. The static tables shall be a fixed calibration. | Same complexity budget as `Bms_Soc` (SOC-FR-05). A lookup table is the entire model, so there is nothing left to identify online. |
| SOP-FR-11 | The module shall take the operating mode as an input from a separate mode-provider component. It shall not derive the mode and shall not publish a mode signal of its own. | One component owns the mode. SOP is a consumer of it, like every other limit consumer. |

There is no requirement here for rate-limiting the output or for handling invalid inputs. Both were cut at your request. Section 5.6 records the cost of that.

### 1.3 Requirements: `Bms_BattCfg`

| ID | Requirement | Rationale |
|---|---|---|
| CFG-FR-01 | The module shall be the one place that defines the battery constants: capacity, OCV curve, cell safety envelope, and static SOP tables. | One number has one home. A constant defined twice will drift apart. |
| CFG-FR-02 | The module shall expose read-only functions that return `const` pointers to its tables. It shall not copy table data into a caller buffer. | The tables live in flash. A copy into RAM on every call of a 100 ms task is waste. |
| CFG-FR-03 | The module shall hold no state, no runtime input, and no algorithm. It shall hold constant data and the functions that return it. | This keeps the module easy to test. Any software component can call it from any task at any time. |
| CFG-FR-04 | The OCV table and the pack capacity shall move here from `Bms_Soc`. `Bms_Soc` shall then read them through the new functions. | Both are battery characterization data, not estimator logic. They belong with the pack's other configuration data, not inside an estimator. |
| CFG-FR-05 | The move in CFG-FR-04 shall keep behavior the same: the same values, the same SOC results, and the current SIL suite passing with no edits. | A refactor that changes behavior is not a refactor. Reviewing the two changes together is harder than reviewing them apart. |
| CFG-FR-06 | The cell safety envelope shall be defined here once. Both the SOP derating layer and, as a later step, the `Battery_Monitor` fault thresholds shall read it. | SOP-FR-06 needs the derate window to sit inside the fault trip. One place must hold both numbers, so that a reviewer can check the relation. See section 7.4. |

### 1.4 Interface requirements

| ID | Requirement |
|---|---|
| SOP-IR-01 | Cell voltage extremes come from `BatteryMonitor_GetData()`: `MinCellVoltage` and `MaxCellVoltage`, in volts as `float`. |
| SOP-IR-02 | Temperature comes from `BatteryMonitor_GetData()->MaxPackTemperature_dC`, in units of 0.1 degC. This is a thermistor reading on the pack, not a per-cell reading. See section 5.2. |
| SOP-IR-03 | SOC comes from `Bms_Soc_GetPackData()`. `Min.Soc_pct_x10` seeds the discharge lookup. `Max.Soc_pct_x10` seeds the charge and regen lookups. |
| SOP-IR-04 | The module publishes the limits on a new CAN frame, `0x30C SOP_Limits`, through `Bms_Can_SendSopLimits()`, which reads `Bms_Sop_GetData()`. |
| SOP-IR-05 | The module publishes calibration detail on an optional frame, `0x30D SOP_Debug`. That detail is the raw table value and the derate factor for one limit. See section 7.3. |
| SOP-IR-06 | The operating mode comes from a separate mode-provider component. The module consumes it and does not decide it. That component does not exist yet. Until it does, the module uses the compile-time default `BMS_SOP_DEFAULT_MODE`. See section 7.1. |
| CFG-IR-01 | `Bms_Soc` calls `Bms_BattCfg_GetOcvTable()` and `Bms_BattCfg_GetOcvTableSize()` inside `Bms_Soc_OcvToSoc()`. It calls `Bms_BattCfg_GetNominalCapacity_mAh()` wherever `BMS_SOC_PACK1_CAPACITY_MAH` is used today. |
| CFG-IR-02 | The OCV lookup still runs through `Lib_Interp_Lookup_1D_uint16()`. Only the owner of the table changes. |

Pack current is not an input to this module. The static table needs only SOC and temperature, and the derate layer needs only cell voltage and temperature. Section 5.5 explains what removing pack current is worth. The module also does not read the validity flag that comes with any of these inputs. Section 5.6 explains what that costs.

### 1.5 Timing requirements

| ID | Requirement |
|---|---|
| SOP-TR-01 | `Bms_Sop_MainFunction()` shall run every 100 ms from the 100 ms scheduler slot. It shall run after `BatteryMonitor_MainFunction()` and after `Bms_Soc_MainFunction()`. It reads the outputs of both. |
| SOP-TR-02 | `Bms_Sop_Init()` shall run after `Bms_Soc_Init()` in the startup sequence. All limits shall start at zero. |
| CFG-TR-01 | `Bms_BattCfg` has no task. If it has an init function at all, that function shall run first in the startup sequence, before any consumer. See section 7.5. |

---

## 2. Functional architecture

### 2.1 Context: external interfaces

```mermaid
flowchart TD
    CAN1(["CAN1 bus"]) -->|"0x401-0x405"| VAFE
    CAN2(["CAN2 bus"]) -->|"0x410-0x411"| VPACK
    ADC1(["ADC1"]) --> NTC

    subgraph sensing["Signal acquisition"]
        VAFE["Bms_Vafe<br/>16 cell voltages<br/>Min / Max / Avg / Delta"]
        VPACK["Bms_Vpack<br/>pack current, pack voltage"]
        NTC["Bms_Ntc<br/>3 thermistors"]
    end

    VAFE --> BM
    VPACK --> BM
    NTC --> BM

    BM["Battery_Monitor<br/>aggregation, unit conversion, validity"]
    SOC["Bms_Soc<br/>Min / Max / Avg SOC"]

    BM -->|"pack current + valid"| SOC
    BM -->|"Min / Max cell voltage<br/>Max temperature"| SOP
    SOC -->|"Min / Max SOC"| SOP

    subgraph sop["Bms_Sop - this module"]
        SOP["Init - once at startup<br/>MainFunction - 100 ms"]
    end

    CFG["Bms_BattCfg<br/>capacity, OCV curve,<br/>cell safety envelope,<br/>static SOP tables"]

    CFG -->|"OCV curve, capacity"| SOC
    CFG -->|"static tables + envelope"| SOP
    SOP -->|"table lookups"| LIB["Lib_Interp<br/>1-D today, 2-D needed"]
    SOC -->|"OCV lookup"| LIB

    SOP -->|"Bms_Sop_GetData"| CANMOD["Bms_Can<br/>0x30C SOP_Limits<br/>0x30D SOP_Debug optional"]
    CANMOD -->|"CAN0"| HOST(["Host / HIL"])

    MODE(["Mode-provider SWC<br/>separate component<br/>not built yet - 7.1"]) -.->|"Discharge / Charge"| SOP

    classDef this fill:transparent,stroke:#3b6fd4,stroke-width:2.5px
    classDef newcfg fill:transparent,stroke:#d48806,stroke-width:2.5px
    classDef undecided fill:transparent,stroke:#cf1322,stroke-width:2.5px,stroke-dasharray:5
    class SOP this
    class CFG newcfg
    class MODE undecided
```

### 2.2 Internal decomposition: `Bms_Sop`

`Bms_Sop` has two function blocks in a fixed pipeline. Section 3 gives the internals of each block. This level shows the blocks and what passes between them.

```mermaid
flowchart TD
    BM["Battery_Monitor<br/>external"]
    SOC["Bms_Soc<br/>external"]
    CFG["Bms_BattCfg<br/>external"]
    CANM["Bms_Can<br/>external"]

    STATIC["Static Limit Lookup<br/>table vs SOC and temperature<br/>detail: 3.5"]
    DERATE["Feedback Derating<br/>V-low / V-high / T-high factors<br/>detail: 3.6"]

    OUT["Published outputs<br/>Discharge / Regen / Charge limit<br/>derate flags"]

    BM -->|"max T"| STATIC
    BM -->|"cell V extremes, max T"| DERATE
    SOC -->|"Min / Max SOC"| STATIC
    CFG -->|"static tables"| STATIC
    CFG -->|"derate windows"| DERATE

    STATIC --> DERATE
    DERATE --> OUT
    OUT --> CANM

    classDef ext fill:transparent,stroke:#9a9a9a,stroke-width:1.5px
    class BM,SOC,CFG,CANM ext
```

This is a two-block pipeline with no held state of its own. An earlier draft of this design added a third block: a dynamic equivalent-circuit model with its own polarization state. That block ran in parallel with the static lookup and fed an arbitration step. A later draft also dropped a rate limiter and an explicit invalid-input fallback that once followed this pipeline. Section 6 records why both changes were made.

### 2.3 Internal decomposition: `Bms_BattCfg`

`Bms_BattCfg` is not a pipeline. It is a set of tables behind read functions. The diagram shows ownership, because that is the whole design.

```mermaid
flowchart LR
    subgraph cfg["Bms_BattCfg"]
        CAP["Nominal capacity<br/>MIGRATED from Bms_Soc"]
        OCV["OCV curve<br/>MIGRATED from Bms_Soc"]
        ENV["Cell safety envelope<br/>V min/max, T min/max<br/>+ derate windows"]
        SOPT["Static SOP tables<br/>discharge / regen / charge<br/>vs SOC x temperature"]
    end

    CAP --> SOCM["Bms_Soc"]
    OCV --> SOCM
    ENV --> SOPM["Bms_Sop"]
    SOPT --> SOPM
    ENV -.->|"follow-up, see 7.4"| BMM["Battery_Monitor<br/>fault thresholds"]

    classDef migrated fill:transparent,stroke:#d48806,stroke-width:2.5px
    classDef later fill:transparent,stroke:#9a9a9a,stroke-width:1.5px,stroke-dasharray:5
    class CAP,OCV migrated
    class BMM later
```

### 2.4 Interface summary

| Producer | Consumer | Data | Gate |
|---|---|---|---|
| `Battery_Monitor` | `Bms_Sop` | `MinCellVoltage`, `MaxCellVoltage` (V) | none, see section 5.6 |
| `Battery_Monitor` | `Bms_Sop` | `MaxPackTemperature_dC` (0.1 degC) | none, see section 5.6 |
| `Bms_Soc` | `Bms_Sop` | `Min.Soc_pct_x10`, `Max.Soc_pct_x10` | none, see section 5.6 |
| `Bms_BattCfg` | `Bms_Sop` | cell envelope, static SOP tables | none (constant) |
| `Bms_BattCfg` | `Bms_Soc` | OCV curve, nominal capacity | none (constant) |
| `Bms_Sop` | `Bms_Can` | limits, derate flags | `Bms_Sop_GetData()` |
| Mode-provider SWC (not built yet) | `Bms_Sop` | operating mode | section 7.1 |

### 2.5 Data ownership

| Datum | Owner today | Owner after this change |
|---|---|---|
| Pack nominal capacity | `Bms_Soc.h` (`BMS_SOC_PACK1_CAPACITY_MAH`) | `Bms_BattCfg` |
| OCV curve | `Bms_Soc.c` (`g_BmsSocOcvTable`, file-static) | `Bms_BattCfg` |
| Cell over-voltage, under-voltage, over-temperature, under-temperature thresholds | `Battery_Monitor.c` (private macros) | `Bms_BattCfg`, as a follow-up. See section 7.4. |
| Static SOP limit tables | none | `Bms_BattCfg` (new) |
| Derate windows | none | `Bms_BattCfg` (new) |
| Published limits | none | `Bms_Sop` |
| Operating mode | none | a separate mode-provider component (not built yet). `Bms_Sop` consumes it. |

---

## 3. Detailed design

### 3.1 Sign and unit conventions

This table states the conventions once. Everything below follows it.

| Quantity | Convention |
|---|---|
| Published limits | `uint16` magnitude, 0.1 A per bit. The field name gives the direction. There is no sign. |
| Cell voltage (internal) | `uint16` mV, converted from the `float` volts of `Battery_Monitor` on entry. |
| Temperature | `sint16`, 0.1 degC. This matches `Battery_Monitor`. |
| SOC | `uint16`, 0.1 percent, range 0 to 1000. This matches `Bms_Soc`. |
| Derate factor | `uint16`, 0.001 per bit, range 0 to 1000. A value of 1000 means no derate. It is an integer to keep `float` out of the pipeline where it does not add accuracy. |

A discharge limit of `750` means the pack can draw up to 75.0 A. That is a current no more negative than `-75000 mA`.

### 3.2 Data structures: `Bms_Sop`

```c
/** @brief Operating mode. Determines which limits are active. */
typedef enum
{
    BMS_SOP_MODE_DISCHARGE  = 0U,   /**< Not charging: discharge + regen limits active. */
    BMS_SOP_MODE_CHARGE = 1U    /**< Charger attached: charge limit active. */
} Bms_Sop_ModeType;

/** @brief Intermediate result for one limit, kept for calibration visibility. */
typedef struct
{
    uint16 Table_dA;       /**< Raw static-table result. Unit: 0.1 A. */
    uint16 DerateFactor;   /**< Applied factor, 0-1000. */
    uint16 Final_dA;       /**< After derate. Published value. */
} Bms_Sop_LimitType;

/** @brief Published SOP snapshot. */
typedef struct
{
    Bms_Sop_LimitType Discharge;   /**< Active in Discharge mode, else Final_dA forced to 0. */
    Bms_Sop_LimitType Regen;       /**< Active in Discharge mode, else Final_dA forced to 0. */
    Bms_Sop_LimitType Charge;      /**< Active in Charge mode, else Final_dA forced to 0. */

    Bms_Sop_ModeType  Mode;        /**< The mode taken from the provider this cycle. Not published. */

    /** @brief Which feedback term is currently governing. Diagnostic. */
    boolean DerateActiveVLow;
    boolean DerateActiveVHigh;
    boolean DerateActiveTHigh;
} Bms_Sop_DataType;
```

`g_BmsSopData` persists between calls, because `Bms_Sop_GetData()` has to return something. What the module does not do is carry state forward: every field above is overwritten each cycle from the present inputs alone, and no field's new value depends on its old one.

### 3.3 Data structures: `Bms_BattCfg`

```c
/** @brief Which static limit table to read. Owned here, not by Bms_Sop. */
typedef enum
{
    BMS_BATTCFG_LIMIT_DISCHARGE = 0U,
    BMS_BATTCFG_LIMIT_REGEN     = 1U,
    BMS_BATTCFG_LIMIT_CHARGE    = 2U
} Bms_BattCfg_LimitIdType;

/**
 * @brief Cell safety envelope. Single source for the fault thresholds and,
 *        once Bms_Sop exists, for the derate windows.
 *
 * Built 2026-09-11 (section 7.4). Every threshold is hysteretic, so each limit
 * carries the value that raises the fault and the value that clears it.
 */
typedef struct
{
    uint16 CellVoltageMax_mV;        /**< Cell OV set.  Fault trips at or above. */
    uint16 CellVoltageMaxClear_mV;   /**< Cell OV clear. */
    uint16 CellVoltageMin_mV;        /**< Cell UV set.  Fault trips at or below. */
    uint16 CellVoltageMinClear_mV;   /**< Cell UV clear. */

    uint16 CellImbalanceMax_mV;      /**< Max minus min cell. */
    uint16 CellImbalanceMaxClear_mV;

    sint16 TemperatureMax_dC;        /**< Over-temperature set. */
    sint16 TemperatureMaxClear_dC;
    sint16 TemperatureMin_dC;        /**< Under-temperature set. */
    sint16 TemperatureMinClear_dC;

    sint16 TemperatureDeltaMax_dC;   /**< Pack-to-pack spread. */
    sint16 TemperatureDeltaMaxClear_dC;

} Bms_BattCfg_CellLimitsType;
```

The derate windows landed with the module on 2026-09-12 (`104d24f`) and sit in the same struct: `DerateVHighStart/End_mV`, `DerateVLowStart/End_mV`, `DerateTHighStart/End_dC` and `DerateFloor`. As built they are 4100 to 4200 mV rising, 2900 to 2600 mV falling, 45.0 to 60.0 degC, floor 0.100. Every End sits on the safe side of its matching fault threshold, which is what SP-12 now checks. These are placeholder calibration too.

Read functions:

```c
uint32 Bms_BattCfg_GetNominalCapacity_mAh(void);

const uint16 (*Bms_BattCfg_GetOcvTable(void))[2];
uint16       Bms_BattCfg_GetOcvTableSize(void);

const Bms_BattCfg_CellLimitsType *Bms_BattCfg_GetCellLimits(void);

uint16 Bms_BattCfg_GetStaticLimit_dA(Bms_BattCfg_LimitIdType limitId,
                                     uint16                  soc_pct_x10,
                                     sint16                  temp_dC);
```

`Bms_BattCfg_LimitIdType` belongs to `Bms_BattCfg`, not to `Bms_Sop`. `Bms_BattCfg` is a foundation module: `Bms_Soc` already depends on it, and `Battery_Monitor` may later (section 7.4). If its header named a `Bms_Sop` type, `Bms_BattCfg.h` would have to include `Bms_Sop.h` while `Bms_Sop.h` includes `Bms_BattCfg.h`. That is an include cycle, and it would drag `Bms_Soc` into a dependency on `Bms_Sop` for no reason.

The identifier alone picks the table. The operating mode is not a parameter, because it adds nothing: the mode decides which limits `Bms_Sop` publishes (SOP-FR-03), not which table each limit reads.

The last function runs a table lookup. That sits against the CFG-FR-03 rule of no algorithm. The other option is to expose the raw table and put the two-dimensional lookup in `Bms_Sop`. See section 7.2.

### 3.4 Move of OCV and capacity out of `Bms_Soc`

This move keeps behavior the same. The table values do not change.

| Before (`Bms_Soc.c` and `Bms_Soc.h`) | After |
|---|---|
| `static const uint16 g_BmsSocOcvTable[][2]` | `Bms_BattCfg.c`, same values, read through `Bms_BattCfg_GetOcvTable()` |
| `BMS_SOC_OCV_TABLE_SIZE` (sizeof macro) | `Bms_BattCfg_GetOcvTableSize()` |
| `BMS_SOC_PACK1_CAPACITY_MAH` (`Bms_Soc.h`) | `Bms_BattCfg_GetNominalCapacity_mAh()` |

`Bms_Soc_OcvToSoc()` becomes:

```c
uint16 Bms_Soc_OcvToSoc(uint16 voltage_mV)
{
    return Lib_Interp_Lookup_1D_uint16(
        Bms_BattCfg_GetOcvTable(),
        Bms_BattCfg_GetOcvTableSize(),
        voltage_mV);
}
```

`Bms_Soc_SocX10ToCapacity_mAh()` and `Bms_Soc_SetEstimatorCapacity()` take the capacity from the read function instead of the macro. Nothing else in `Bms_Soc` changes.

Acceptance test: the current SIL suite (48 pass, 1 xfail) passes with no test edits. If a test needs an edit, that is proof the refactor changed behavior.

Order of work: land this move as its own commit, before any SOP code. A pure refactor with a green suite is a fast review. Folded into the SOP feature, it is not.

### 3.5 Subfunction: static limit lookup

The table holds the limits that come from the cell datasheet ratings, the contactor and fuse ratings, and the validation test results. It is the entire model: there is no second, model-based path to check it against.

Each limit is a function of SOC and temperature:

```
tableLimit_dA = Table[limitId](soc_pct_x10, temp_dC)
```

The discharge table is indexed by the SOC of the Min estimator. The regen table and the charge table are indexed by the SOC of the Max estimator. This is the same weak-cell and strong-cell logic as SOP-FR-04. The temperature axis is `MaxPackTemperature_dC` for the high-temperature roll-off. Cold behavior is on the same axis, so one two-dimensional table covers both ends. The lookup does a bilinear interpolation between breakpoints and clamps at all four edges.

This uses `Lib_Interp_Lookup_2D_uint16()`, added to `Lib_Interp` on 2026-09-11 for exactly this table. See section 7.2 for the signature.

Breakpoints as built (`104d24f`): SOC at 0, 10, 20, 40, 60, 80, 90, 100 percent. Temperature at -20, -10, 0, 10, 25, 40, 50, 60 degC. Three 8-by-8 maps, one per limit, in `Bms_BattCfg.c`.

**The map values are placeholders.** They are not from the cell, contactor or fuse datasheets. Nobody measured them. They are shaped to be plausible, to exercise the lookup, and to be wrong in safe directions: discharge falls to zero at empty and rolls off at both temperature ends; charge and regen fall to zero at full; charge and regen are zero at -20 degC and tiny at -10 degC; and every value stays inside the over-current trips, discharge peaking at 90.0 A against a 100.0 A trip, charge at 70.0 A and regen at 75.0 A against an 80.0 A trip. A SIL case asserts that last property, so a future calibration cannot quietly publish a limit that trips a fault. Replace all three maps before any published limit is trusted. Same status as the OCV curve.

### 3.6 Subfunction: feedback derating

The static table looks forward. It predicts a limit from SOC and temperature against a fixed calibration. It does not react to the pack getting closer to a limit than the table said. Three things can walk the pack toward the fault trip while the limit still reads healthy. They are a stale calibration, a weak cell that the pack-level SOC does not see, and a heat gradient across the pack. The derating layer closes that loop.

#### 3.6.1 Factor shape

Each watched signal makes one factor in the range `DerateFloor` to 1000 by a straight-line ramp:

```mermaid
flowchart LR
    A["Signal in safe region<br/>factor = 1000"] --> B["Between Start and End<br/>linear ramp"]
    B --> C["Past End<br/>factor = DerateFloor"]
    C --> D["Fault threshold<br/>protection acts"]

    classDef ok fill:transparent,stroke:#52c41a
    classDef warn fill:transparent,stroke:#faad14
    classDef bad fill:transparent,stroke:#cf1322
    class A ok
    class B,C warn
    class D bad
```

The ramp is `Lib_Interp_Lookup_1D_uint16()` over a two-point table. That is the existing one-dimensional function, so there is no new code. The SOP-FR-06 rule is that `End` sits inside the fault threshold. A reviewer can check that because both numbers live in `Bms_BattCfg` (CFG-FR-06).

#### 3.6.2 Which factor applies to which limit

| Factor | Driven by | Applies to |
|---|---|---|
| `k_vLow` | `MinCellVoltage` falling | Discharge |
| `k_vHigh` | `MaxCellVoltage` rising | Regen, Charge |
| `k_tHigh` | `MaxPackTemperature_dC` rising | All three |

```c
discharge.DerateFactor = MIN(k_vLow,  k_tHigh);
regen.DerateFactor     = MIN(k_vHigh, k_tHigh);
charge.DerateFactor    = MIN(k_vHigh, k_tHigh);
```

The rule is minimum, not product (SOP-FR-07). Two factors of 0.8 multiply to 0.64. With two factors mildly active, the product over-derates, and neither input asked for that. The minimum applies whichever one constraint is tightest.

There is no low-temperature factor here. Charging a lithium cell below about 0 degC plates metal lithium on the anode. That is a capacity loss and a safety problem, and it does not reverse. This design still guards against it. The static charge and regen tables in section 3.5 are already indexed by temperature. A breakpoint near 0 degC can roll the table limit down to a low value, or to zero, with no separate feedback factor needed. The feedback layer only needs to cover what the table cannot see coming, and cold is not one of those cases.

#### 3.6.3 Application

```c
final_dA = (uint16)(((uint32)table_dA * derateFactor) / 1000U);
```

`final_dA` is the value the module publishes. Nothing runs after it.

### 3.7 Flow: the 100 ms update

```mermaid
flowchart TD
    START(["Bms_Sop_MainFunction<br/>100 ms, after Bms_Soc"]) --> INPUTS["Read Battery_Monitor + Bms_Soc"]
    INPUTS --> STATIC["Static lookup<br/>3.5"]
    STATIC --> DER["Feedback derate<br/>3.6"]
    DER --> MODE{"Mode?"}
    MODE -->|"Discharge"| DISCH["Charge.Final_dA = 0"]
    MODE -->|"Charge"| CHG["Discharge.Final_dA = 0<br/>Regen.Final_dA = 0"]
    DISCH --> PUB["Publish g_BmsSopData"]
    CHG --> PUB
    PUB --> DONE(["Return"])
```

Every step in this pipeline is a function of the present inputs alone. The module carries no memory of an earlier cycle at all.

### 3.8 Flow: initialization

```mermaid
flowchart TD
    START(["Bms_Sop_Init<br/>after Bms_Soc_Init"]) --> ZERO["All limits = 0"]
    ZERO --> MODE["Mode = configured default<br/>see 7.1"]
    MODE --> DONE(["Return"])
```

The module starts at zero, not at a table value. This is on purpose. The first `MainFunction` call sets real limits 100 ms later. A 100 ms window of zero limit is harmless. A plausible limit published before any measurement was checked is not harmless.

### 3.9 Configuration constants: `Bms_Sop`

| Constant | Proposed | Note |
|---|---|---|
| `BMS_SOP_DEFAULT_MODE` | `DISCHARGE` | The mode `Bms_Sop_Init()` restores. Section 7.1. |

The mode itself is `volatile uint8 g_BmsSopMode`, not a constant. It is initialized to `BMS_SOP_DEFAULT_MODE` and left non-static so a debugger or an XCP master can overwrite it live; it is on the XCP write whitelist in `Xcp_IsWritableRange()`. Anything that is not a valid `Bms_Sop_ModeType` reads as Discharge, which publishes no charge limit. `Bms_Sop_Init()` restores the default, so a value written earlier does not survive a restart. This is the stand-in for the mode-provider component, not the final interface (section 7.1).

There is no `BMS_SOP_SAMPLE_PERIOD_MS`. Nothing in this design uses a time step. The module is stateless (section 3.2), so no term is integrated and no output is rate-limited. The 100 ms period is a scheduler fact (SOP-TR-01), not a module constant.

### 3.10 Published CAN signals

`0x30C SOP_Limits`. **Built and published** (`dc68326`). 100 ms. CAN0.

| Bytes | Signal | Encoding |
|---|---|---|
| 0-1 | `Pack1DischargeLimit` | `uint16` LE, 0.1 A per bit, magnitude |
| 2-3 | `Pack1RegenLimit` | `uint16` LE, 0.1 A per bit, magnitude |
| 4-5 | `Pack1ChargeLimit` | `uint16` LE, 0.1 A per bit, magnitude |
| 6 bits 0-2 | `Pack1SOPDerateActive` | one bit per factor: vLow, vHigh, tHigh |
| 6 bits 3-7 | reserved | send as 0 |
| 7 bits 0-3 | `SOPAliveCounter` | 4-bit rolling counter, same style as `0x308` and `0x30B` |

The frame carries no mode signal and no validity signal. The mode-provider component publishes the mode, see SOP-IR-06. Nothing publishes validity, see section 5.6.

That last point matters more now than it did on paper. During the pending-init window `Bms_Soc` reports SOC as 0, and the charge map at SOC 0 returns its full value, so `0x30C` briefly publishes a healthy-looking charge limit computed from a guess. While nothing transmitted, that was theoretical. It is on the wire now. Byte 6 bits 3-7 are reserved and are where a validity flag would go if section 5.6 is ever closed.

Publishing takes the 100 ms task from 17 blocking sends to 18. That was the argument against it, but the per-frame timeout dropped from 100 ms to 2 ms in `a90746b`, so the 18th send costs about 2 ms of worst case rather than 100 ms, and 0.26 ms of bus time. CAN0 carries roughly 4.7 percent load.

`0x30D SOP_Debug` was **dropped**, see section 7.3. The calibration detail it would have carried is read over XCP instead.

`0x30C` has an entry in `DBC/BMS_demo.dbc`. The file `DBC/BMS_demo.sym` is already out of date with the SOC frames. This design does not maintain it either.

---

## 4. Validation plan

**Run as of 2026-09-12.** 33 cases in `sil/tests/test_sop.py` cover SP-01 to SP-05 and SP-12. The suite is 93 pass and 1 xfail overall. SP-06 to SP-09 are **not** written, and SP-10 and SP-11 were covered by the `Bms_BattCfg` move. Each case below says where it stands.

SOP is a good fit for SIL. It is computation over the outputs of `Battery_Monitor` and `Bms_Soc`. The current SIL setup drives both: cell voltages and pack current arrive as real CAN frames on CAN1 and CAN2, and the pack temperatures come from the faked ADC.

### 4.1 Unit level: static table and derating

| ID | Case |
|---|---|
| SP-01 | The table returns the exact value at a breakpoint and interpolates between breakpoints. It clamps at all four edges. |
| SP-02 | The discharge lookup uses the Min SOC branch and the charge and regen lookups use the Max SOC branch. To test, make the two SOCs differ and watch which limit moves. The two cannot be made to differ through the normal CAN signal path (section 5.7), so this case has to set `Min.Soc_pct_x10` and `Max.Soc_pct_x10` directly. |
| SP-03 | The derate factor is 1000 in the safe region, `DerateFloor` past the end point, and linear between the two. |
| SP-04 | The combined derate is the minimum of the applicable factors, never the product. |

### 4.2 Integration: mode and signal chain

| ID | Case |
|---|---|
| SP-05 | Discharge mode forces `Charge.Final_dA` to zero. Charge mode forces `Discharge.Final_dA` and `Regen.Final_dA` to zero. The `Table_dA` and `DerateFactor` fields of the forced limits stay at their computed values (SOP-FR-03). |
| SP-06 | A cell-voltage set sent over CAN1 that drives `MinCellVoltage` into the derate window reduces the discharge limit within one task cycle. **Not written.** The equivalent is covered at module level by the SP-03 cases, which drive cell voltage through the SIL doubles rather than over CAN1. |
| SP-07 | A rising thermistor reading sent through the ADC double reduces every limit that the active mode publishes, and reduces the `DerateFactor` field of all three. The published value of an inactive limit stays zero, so `k_tHigh` has to be checked on the calibration fields (SOP-FR-03), not on `Final_dA`. |
| SP-08 | The `0x30C` frame encodes the limits and the derate flags correctly. The alive counter rolls 0 to 15 to 0. **Not written, and not writable yet.** SIL does not compile `Bms_Can.c`, so no CAN frame has test coverage. Closing this needs a `FlexCAN_Ip` fake in `sil/fakes/`; the driver surface `Bms_Can.c` uses is nine functions, of which only `SendBlocking` and `Receive` need real behavior. The frame ships verified by inspection and a bench capture only. |

### 4.3 Regression: the `Bms_BattCfg` move

| ID | Case |
|---|---|
| SP-10 | The full current SOC suite (48 pass, 1 xfail) passes with no edits after the OCV curve and the capacity move to `Bms_BattCfg`. No test edits are allowed. |
| SP-11 | `Bms_BattCfg_GetOcvTable()` returns the same curve `Bms_Soc` used before the move, point for point. |

### 4.4 Static configuration checks

These run over the `Bms_BattCfg` constants. They are not runtime tests.

| ID | Case |
|---|---|
| SP-12 | Every derate `End` threshold sits inside its matching fault threshold (SOP-FR-06). **Runnable and passing** since the derate windows landed. Extended to check the ramp directions, the floor range, and that each fault threshold's clear value sits on the safe side of its set value - which is the only coverage the envelope move from `47b18d8` has. |

### 4.5 Not coverable in SIL

- Whether the static table's current ratings match the cell, contactor, and fuse datasheets. SIL proves the lookup arithmetic, not the calibration data.
- Thermal behavior under a sustained load. There is no thermal model.
- Whether the derate windows are calibrated well enough to keep a real pack off its fault thresholds across the full temperature range.
- Anything in `Bms_Can`, including the `0x30C` encoding (SP-08) and the transmit-failure handling added in `a90746b`. `Bms_Can.c` is not in the SIL source list.
- The end-to-end property behind SOP-FR-06: that a sustained discharge at the published limit never trips `FAULT_CELL_UV`. This was test case SP-09 in an earlier draft of this plan. It needs a cell model whose voltage falls in response to the current drawn. SIL has no such model: cell voltages arrive as injected CAN1 frames and do not react to the pack current injected on CAN2. SP-12 is the static stand-in, and it only proves the thresholds are ordered, not that the ramp is fast enough.
- Any case that needs the Min and Max SOC estimators to hold different values. See section 5.7.

---

## 5. Known limitations and future improvements

### 5.1 No thermal prediction

The limits react to the measured temperature. They do not predict it. A sustained current can overheat the pack even at a temperature that is safe now. A full SOP predicts the temperature rise ahead of time instead of only reacting to the present reading. That is out of scope here. The temperature derating is a reaction, not a substitute.

### 5.2 "Max cell temperature" is really a pack thermistor reading

The requirement says maximum cell temperature. The signal on hand is `MaxPackTemperature_dC`, the maximum across three thermistors on the ADC. Those thermistors sit on the pack, not on single cells, and there are three of them for sixteen cells. The hottest cell is hotter than the hottest thermistor by an unknown margin. The derate windows must be calibrated with enough margin to absorb that gap. This document cannot say how large the gap is.

This is worth reading next to the existing F2 finding in `PROJECT_PLAN.md`. The over-temperature fault threshold is 200 degC. The thermistor path clamps and invalidates at 125 degC. So `FAULT_OVER_TEMP` cannot fire today. If the SOP derating is calibrated against that 200 degC number, it will never derate either.

### 5.3 One pack, one string

Pack 1 only, for a sensor reason related to the one that limits `Bms_Soc` (`SOC_DESIGN.md` section 5.5). Packs 2 and 3 have no per-pack cell-voltage split. Without one there is no Min or Max cell SOC to index a table with, and no cell voltage extreme to derate against. The missing current sensor on those packs is what limits `Bms_Soc`; it is not what limits this module, because this module never reads pack current (section 1.4).

### 5.4 There is no SOP accuracy requirement

SOC has the same gap (`SOC_DESIGN.md` section 5.10). There is no requirement that names a tolerance, such as plus or minus X percent of the true sustainable current. Validation can prove the code matches this document. It cannot prove the document is right.

### 5.5 The F12 defect does not reach this module

`PROJECT_PLAN.md` F12 and `SOC_DESIGN.md` section 5.11 describe a stuck vPACK alive counter that leaves `PackCurrentValid[0]` at `TRUE` while the current value freezes. An earlier draft of this design read pack current directly, so that earlier draft was exposed to the same defect. The static-table design in this document does not read pack current at all. SOC, cell voltage, and temperature are its only inputs. F12 stays a real defect elsewhere in the firmware. It does not affect the limits this module publishes.

### 5.6 No handling for invalid inputs

The module does not check whether its inputs are valid. `Bms_Soc` can report `Min.Valid` or `Max.Valid` as `FALSE`. `Battery_Monitor` can report `CellVoltageValid` or `TemperatureSummaryValid` as `FALSE`. Either way, this module still runs the table lookup and the derate on whatever value it currently holds. There is no fallback, and no validity bit on the published frame.

Two concrete cases come out of the `Bms_Soc` startup path in `SOC_DESIGN.md` section 3.8.

During the pending-init window, `Bms_Soc_MarkUnseeded()` commits a capacity of zero to all three estimators, so `Min.Soc_pct_x10` and `Max.Soc_pct_x10` both read **0** while `Valid` is `FALSE`. This module cannot tell that apart from a genuinely empty pack. The discharge limit it publishes is therefore near zero, which is harmless. The **charge** limit is not: a table indexed at 0 percent SOC returns the full charge allowance, so the module would invite a charger to push current into a pack whose real SOC nobody has measured yet. The window is up to `g_BmsSocOcvWaitTimeout_ms`, 500 ms by default.

If the wait then resolves to tier 3, all three estimators are set to `BMS_SOC_INITIAL_PCT_X10`, that is `500`. That number is a compile-time guess and `Valid` stays `FALSE`, but this module has no way to tell it apart from a real, measured 50 percent SOC. It publishes a limit computed from the guess, with no signal to the consumer that anything is uncertain.

This was cut at your request, along with the rate limiter (old SOP-FR-08) and the invalid-input fallback (old SOP-FR-09). It must be revisited before this design ships to real hardware.

### 5.7 The weak-cell and strong-cell split is inert today

SOP-FR-04 indexes the discharge table by the Min SOC and the regen and charge tables by the Max SOC. That split only does work when the two estimators hold different values. Today they cannot.

`SOC_DESIGN.md` section 5.1 states the reason: all three estimators integrate the same `PackCurrent_mA[0]` against the same pack capacity, so they receive identical increments every cycle. The only thing that could separate them is the OCV reset seeding them from different cell voltages at boot, and that path does not fire (`SOC_DESIGN.md` section 5.2). In the present firmware `soc_min == soc_avg == soc_max` at all times.

The effect on this module is that all three tables are indexed by the same number. The design is still correct and still worth building this way, because the split costs nothing and starts working on its own the day the estimators diverge. But it buys no protection now, and nobody should read SOP-FR-04 as evidence that a weak cell is being tracked. Making the estimators diverge needs per-estimator capacity constants and per-cell characterization data this project does not have.

This also limits validation. SP-02 cannot drive the two branches apart through the normal signal chain and has to set the two SOC fields directly.

### 5.8 The limit maps are not calibration data

Every number in the three static maps, and every derate window, is a placeholder written to be plausible. None of it comes from the cell, contactor or fuse datasheets, and none of it was measured. The lookup arithmetic is tested; the values are evidence of nothing.

The shapes are chosen to fail safe rather than to be right: zero discharge at empty, zero charge and regen at full, no charge below freezing, and every value inside the over-current trips. That makes the module safe to run on a bench. It does not make a published limit meaningful, and section 4.4 already says SIL cannot close this. Replace the maps before the pack is real.

This is the same status the OCV curve has carried since `Bms_Soc` was written, and it should be read the same way.

### 5.9 The published frame has no test

`0x30C` is transmitted by `Bms_Can`, and `Bms_Can.c` is not in the SIL source list, so no CAN frame in this firmware has automated coverage. SP-08 records what the case would assert. Until a `FlexCAN_Ip` fake exists, the encoding, the byte order, the derate-flag bits and the alive-counter rollover rest on inspection and on a bus capture at the bench.

---

## 6. Change log

| Date | Change | Rationale |
|---|---|---|
| 2026-09-12 | Published the limits on CAN frame `0x30C` (`dc68326`), with a DBC entry. Recorded in section 3.10 that this makes the section 5.6 validity gap reachable by a real consumer, and that the 18th blocking send now costs about 2 ms rather than 100 ms. | The module computed limits that no CAN consumer could see. The timeout fix removed the objection that had made this a real decision. |
| 2026-09-12 | Answered section 7.3: **dropped `0x30D`**, calibrate over XCP. `g_BmsSopData` links inside the existing XCP read window, so the data is reachable with no firmware change. Recorded the two costs: a RAM address is not self-describing and moves between builds, and no A2L generation exists. | XCP runs on its own FlexCAN instance and cannot add to the F7 blocking total. |
| 2026-09-12 | Answered section 7.1: **deferred**. No mode-provider component yet. The mode is `g_BmsSopMode`, a volatile calibratable variable defaulting to Discharge and on the XCP write whitelist. `Bms_Sop_Init()` restores the default. | Keeps the bench demo usable without committing to a mode interface. The stand-in is explicitly not the final interface. |
| 2026-09-12 | Built `Bms_Sop` (`3770dc2`) and the placeholder limit maps and derate windows in `Bms_BattCfg` (`104d24f`). 33 SIL cases, SP-01 to SP-05 and SP-12; suite now 93 pass / 1 xfail. Section 7.5 settled as built: no `Init()`. Added sections 5.8 and 5.9 to record that the calibration is invented and that the published frame has no test. | The design was agreed; this is the implementation. The maps are placeholders so that the module can run before datasheet ratings exist. |
| 2026-09-11 | Answered section 7.4 **yes** and moved the cell safety envelope out of `Battery_Monitor.c` into `Bms_BattCfg_CellLimitsType`: cell OV/UV, imbalance, over/under-temperature and pack-to-pack delta, each as a set and clear pair. `Battery_Monitor` keeps the old macro names as forwards to the struct. Pack over-current thresholds stayed behind. Updated the section 3.3 struct to match what was built. | The doc's single-value fields could not express hysteresis, which every threshold in the fault path uses. Forwarding macros keep the diff on a safety-critical path to one block. |
| 2026-09-11 | Answered section 7.2 with `Lib_Interp_Lookup_2D_uint16()` and built it: explicit `sint32` axes, row-major `uint16` values, independent clamping on each axis, 12 SIL cases over a non-separable map. Section 3.5 no longer says the interpolation is missing. | The static SOP table is two-dimensional and `Lib_Interp` only did one. Putting it in the shared library keeps `Bms_BattCfg` free of algorithm (CFG-FR-03) and stops the next module copying it. |
| 2026-09-11 | Implemented section 3.4: `Bms_BattCfg` now owns the OCV curve and the nominal capacity, `Bms_Soc` reads them through accessors. No `Init()`, per section 7.5. | Ordered first by section 3.4 — a pure refactor with a green suite reviews fast. |
| 2026-09-10 | Review fixes. Corrected section 5.6: `Bms_Soc` reports SOC as **0**, not `500`, during the pending-init window, and the resulting hazard is an over-permissive *charge* limit, not a plausible mid-pack discharge limit. Replaced the undefined `Bms_Sop_LimitIdType` with `Bms_BattCfg_LimitIdType`, owned by `Bms_BattCfg`, and dropped the mode parameter from `Bms_BattCfg_GetStaticLimit_dA()`, which removes an include cycle between the foundation module and its consumer. Clarified SOP-FR-03: only `Final_dA` is forced to zero. Dropped the vacuous saturation clause from SOP-FR-09 and the dead `BMS_SOP_SAMPLE_PERIOD_MS` constant. Replaced the stale "no current sensor" rationale for Packs 2 and 3. Restated SP-02 and SP-07, moved SP-12 into a new section 4.4, and retired SP-09 into section 4.5. Added section 5.7. | Corrections from a review of this document against the code. The `BMS_SOP_SAMPLE_PERIOD_MS` constant and the saturation clause were both left behind by the rate-limiter removal below. |
| 2026-09-10 | Removed `k_tLow`, the low-temperature derate factor, from section 3.6.2. Also removed its remaining traces: the `DerateActiveTLow` flag, the `DerateTLowStart_dC`/`DerateTLowEnd_dC` fields, and the `tLow` bit on `0x30C`. Resolved and removed the open question about it (old section 7.3). | The static charge and regen tables are already indexed by temperature, so a cold breakpoint can roll the table limit down on its own. A separate feedback factor for the same condition was not needed. |
| 2026-09-10 | Removed the rate limiter and the invalid-input fallback (old SOP-FR-08 and SOP-FR-09). The module now recomputes every field from scratch each cycle, with no state and no `Valid` flag. Dropped `Pack1SOPValid` from `0x30C`, the `FALL_RATE`/`RISE_RATE`/`FALLBACK_*` constants, the rate-limiting subsection, and the fallback-related test cases. Removed the fallback open question (old section 7.2). Added section 5.6 to record that invalid inputs are no longer handled at all. | Requested simplification, at your direction. |
| 2026-09-10 | Removed the dynamic equivalent-circuit-model path. `Bms_Sop` now computes each limit from the static table alone, then applies the same feedback derate. Deleted the ECM data structures, the resistor-capacitor state, the arbitration step, and the horizon requirements. Removed the ECM tables and the topology accessors from `Bms_BattCfg`. Pack current is no longer a required input, because the static table needs only SOC and temperature. | Requested simplification: a static-table-only design, with the dynamic path cut entirely. |
| 2026-09-09 | The operating mode is now an input from a separate mode-provider component. SOP does not publish a mode signal. Removed `Pack1SOPMode` from `0x30C`. Added a requirement for it. | One component owns the mode. Every limit consumer, SOP included, reads it. |
| 2026-09-08 | First draft. `Bms_Sop` runs the limit computation. `Bms_BattCfg` holds the shared battery data and takes the OCV table and capacity from `Bms_Soc`. | New feature. |

---

## 7. Open questions for review

Five of the six are settled. Only 7.6 is still open. The answered ones are kept, with their answers, because the reasoning is what a later reader needs.

### 7.1 How will the mode-provider component decide the mode? — DEFERRED 2026-09-12

The operating mode is an input (SOP-IR-06). A separate component owns it. That component does not exist yet, which is fine for this design. `Bms_Sop` only consumes the result, so none of the options below change the SOP design. The open question is the decision rule for that component. Candidates:

| Option | Note |
|---|---|
| CAN command. Add a mode sub-command to `0x201`. | This matches how Enable, Disable, and ClearFault already arrive. It is small. It trusts the host. |
| Infer from the current sign. Sustained positive current means charging. | No new interface. It is circular, because the mode sets the limit that shapes the current that sets the mode. It needs hysteresis and a dwell time, and it is wrong during regen. |
| Derive from `Bms_StateMachine`. | This needs a charging state that does not exist today. |
| A dedicated charger-detect input. | This is the right answer on real hardware. No pin is assigned. |

**Answered: deferred, at your direction.** No mode-provider component is being built yet. Until one exists the mode is `g_BmsSopMode`, a volatile calibratable variable defaulting to Discharge and writable over XCP (section 3.9). That keeps the bench demo usable — a calibration tool flips the mode by hand — without committing to any of the options above.

The table stands as the menu for whoever builds the component. My suggestion is still the CAN command for a bench demo: it matches the existing control interface and keeps the mode explicit and easy to watch. Note the XCP stand-in is deliberately not that interface. It is a RAM write by a calibration tool, invisible to every other consumer, so it should not be mistaken for the real thing or shipped as one.

### 7.2 Who owns the two-dimensional lookup? — ANSWERED 2026-09-11: `Lib_Interp`

The static table is two-dimensional, on SOC and temperature. `Lib_Interp` only does one dimension.

| Option | Note |
|---|---|
| Add `Lib_Interp_Lookup_2D_uint16()`. | This matches the naming rule and the SOC-FR-13 precedent. It is new shared code and needs its own tests. |
| Two chained one-dimensional lookups, `f(SOC)` times `g(T)`. | No new library code. It assumes the two axes are separable. For cell current ratings against SOC and temperature, they are usually not. |
| Keep the two-dimensional interpolation private to `Bms_Sop`. | Smallest surface. The next module that needs it will copy it, which is the thing `Lib_Interp` exists to stop. |

**Answered: `Lib_Interp_Lookup_2D_uint16()`.** Built and tested on 2026-09-11. It also settles the tension in section 3.3 about whether `Bms_BattCfg` is allowed to hold a lookup: with the interpolation in `Lib_Interp`, the configuration read function becomes a thin wrapper and CFG-FR-03 holds cleanly.

The signature takes explicit axes so the map is a plain calibration table:

```c
uint16 Lib_Interp_Lookup_2D_uint16(
    const sint32 *xAxis, uint16 xCount,
    const sint32 *yAxis, uint16 yCount,
    const uint16 *values,            /* row-major: values[(iy * xCount) + ix] */
    sint32 x, sint32 y);
```

Both axes are `sint32` so the temperature axis needs no offset to hold negative breakpoints. Values stay `uint16`, which is what the name records. It clamps on each axis independently, so all four edges and all four corners clamp rather than extrapolate, and it tolerates a one-breakpoint axis and duplicate breakpoints. Arithmetic is 32-bit, which caps the span between neighbouring breakpoints at 65535 — documented in the header as a caller responsibility, the same way the 1-D function documents its own.

Twelve SIL cases cover it (LI2-01 to LI2-12), deliberately over a non-separable map so a pair of chained 1-D lookups could not reproduce the result.

### 7.3 Is the `0x30D` diagnostic frame worth a 19th blocking transmit? — ANSWERED 2026-09-12: dropped

The 100 ms task already makes 17 calls to `FlexCAN_Ip_SendBlocking()` on one mailbox. The `PROJECT_PLAN.md` F7 finding puts the worst case near 1.7 s of blocking on a bus-off. `0x30C` makes it 18. `0x30D` makes it 19.

The debug frame is useful for calibrating the derate windows. Without it, you cannot see the raw table value or how far into the derate ramp the pack is. Options:

- Ship it and accept the cost.
- Ship it behind a compile-time flag.
- Ship it at 1 Hz instead of 10 Hz.
- Drop it and read the values over XCP. XCP is the calibration protocol already running on CAN5.

**Answered: dropped. Use XCP.** The whole `Bms_Sop_DataType` snapshot already sits inside the XCP read window: `g_BmsSopData` links at `0x204001e0` against a whitelist of `0x20400000` to `0x2047FFFF`, so a master can read all three limits, each with `Table_dA`, `DerateFactor` and `Final_dA`, plus the derate flags, with no firmware change at all. CAN5 is a separate FlexCAN instance on its own mailbox, so it cannot add to the F7 blocking total by construction, and it polls at 10 ms rather than 100 ms.

Two things this choice costs, worth stating plainly. A RAM address is not self-describing, and it moves when any earlier-linked module gains a static, so calibrating this way means reading `BMS_demo.map` or generating an A2L file, and neither is a build step today. And the 1 Hz option above would not have helped anyway: the timeout is per call, not per second, so a frame sent once a second still contributes its full timeout on the cycle it fires.

This also vindicates keeping `Table_dA` and `DerateFactor` live for the inactive direction under SOP-FR-03. That was done for `0x30D`, and it pays off identically over XCP.

### 7.4 Does `Battery_Monitor` move too? — ANSWERED 2026-09-11: yes

CFG-FR-06 wants the cell safety envelope in `Bms_BattCfg` so a reviewer can check the SOP-FR-06 rule that the derate window sits inside the fault threshold. That means the private threshold macros in `Battery_Monitor` move as well. `BMS_CELL_OV_FAULT_SET_MV` and the rest.

Doing the move makes the rule checkable. Not doing it leaves two sets of thresholds that can drift apart. F2 already shows what that looks like: a 200 degC over-temperature threshold against a 125 degC thermistor ceiling.

But this touches the fault path, which is the safety-critical part of the firmware, and SOP does not need it to work. Same commit, a separate follow-up commit, or not at all?

**Answered: yes, done 2026-09-11, as its own change.** `Bms_BattCfg_CellLimitsType` now holds the cell over- and under-voltage pairs, the imbalance pair, the over- and under-temperature pairs and the pack-to-pack delta pair — each as a set value and its clear value, because every threshold in `Battery_Monitor` is hysteretic and the doc's original single-value fields could not express that. `Battery_Monitor.c` keeps the old macro names as one-line forwards to the struct, so every comparison in the fault evaluators is character-identical to before and the reviewable diff is the definition block alone.

The pack over-current thresholds (`BMS_PACK_CHARGE_OC_SET_MA` and the discharge pair) stayed in `Battery_Monitor`. They are pack-current limits, not part of the cell safety envelope CFG-FR-06 is about, and nothing in SOP derates against pack current — this module never reads it.

Two things this move did **not** do. It did not fix F2: the 200 degC over-temperature set point is still unreachable behind the 125 degC thermistor ceiling. It only puts both numbers where a reviewer can see the mismatch, and the struct carries a comment saying so. And the derate windows are still absent — see section 3.3.

### 7.5 Does `Bms_BattCfg` need an init at all? — ANSWERED 2026-09-12: no

If it is only `const` tables in flash behind read functions (CFG-FR-03), there is nothing to initialize and `Bms_BattCfg_Init()` must not exist. Every other `Bms_*` module has one. Adding it here for symmetry is tempting. It is also dead code.

**Answered: no init function.** Built that way. `Bms_BattCfg` is the one `Bms_*` module without an `Init()`, and the header says why so the asymmetry does not read as an oversight.

### 7.6 One document or two?

`Bms_BattCfg` is a foundation module. `Bms_Soc`, `Bms_Sop`, and later `Battery_Monitor` all depend on it. Writing its spec inside the SOP document is convenient now. In six months the module serves three or four consumers and its spec still lives inside one of them. That is wrong.

Do you want it split into `src/battery/BATTCFG_DESIGN.md` now? The other option is to leave it here and split it once a third consumer appears.
