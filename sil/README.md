# BMS SIL platform

Compiles the BMS application layer natively on a PC and drives it from Python,
so behaviour can be tested without hardware.

## Quick start

```bash
python sil/build.py          # compile the shared library
cd sil && python -m pytest   # run the suite
```

Prerequisites: a host C compiler (MinGW-w64 GCC is what this was brought up on)
and `pytest`. The build auto-detects `gcc`/`clang`/`cc` on `PATH`, falls back to
the WinGet WinLibs install location, and honours `SIL_CC` as an override.

## What is under test

The **application layer is real** — the production `.c` files from `src/` are
compiled unmodified, with the same dialect and warning flags as the ARM build
(`-std=c99 -Wall -Wextra -Wdouble-promotion ...`). Only the RTD driver layer is
replaced.

| In scope (production source) | Replaced by a SIL double |
|---|---|
| `Bms_Soc` | `C40_Ip` — Data Flash model |
| `Battery_Monitor` | `Bms_Adc` — settable raw values |
| `Bms_Vafe`, `Bms_Vpack` | `Bms_Ntc` — settable temperatures |
| `Bms_Nvm` | |
| `Fault_Manager` | |
| `Lib_Interp` | |

Excluded: `src/drivers/`, `main.c`, and the pure hardware shims
(`Bms_Spi`, `Bms_Afe`, `Xcp_Can`). `Bms_Can` and `Bms_StateMachine` are outside
the current SOC slice and will need a `FlexCAN_Ip` and `Siul2_Dio_Ip` double
when they are added.

Note that `Bms_Nvm` is deliberately **inside** the test scope rather than
stubbed out: its record log, sequencing, checksum and erase-and-wrap are real
logic worth exercising. The `C40_Ip` double enforces genuine NOR semantics —
erased state is `0xFF`, programming can only clear bits, erase is per-sector,
and program length must be an 8-byte multiple within the sector. A misaligned
or out-of-bounds write fails in SIL the same way it would on target.

## Tests are integrated, not isolated

Stimulus is injected as **real CAN frames** through the production decoders:

```
Sil_InjectVpackCurrent()  --> 0x410 --> Bms_Vpack_ProcessFrame()
Sil_InjectVafeCycle()     --> 0x405, 0x401-0x404 --> Bms_Vafe_ProcessFrame()
                                   |
                                   v
                          Battery_Monitor  -->  Bms_Soc  -->  Bms_Nvm
```

So a test named for a SOC behaviour also fails if an upstream decoder or a
downstream flash write regresses. The alive-counter cases are the clearest
example: they are about vPACK comms integrity, but they are in the SOC suite
because that is where the consequence shows up.

Time is virtual and deterministic — Python drives the 100 ms and 1000 ms task
slots explicitly, in `main.c`'s order. There is no wall clock and no sleeping,
which is why a simulated hour of 1 C discharge runs in milliseconds.

## Test reports

One report per feature under [`reports/`](reports/), with [TEST_REPORT.md](TEST_REPORT.md)
as the roll-up index. Each per-case entry carries: objective, the full procedure
(extracted from the test source), result, duration, failure evidence, and the
requirement it covers — plus the execution environment (compiler version, library
build timestamp, repo commit, dirty-tree flag).

Suites are split by component, not by test count: `test_soc.py` holds every case
whose subject is the SOC feature (estimation, initialization, upstream signal
chain), while `Bms_Nvm` and `Lib_Interp` get their own because they stand on
their own.

Reports are generated, not written, so they cannot drift from the suite. Four
properties worth knowing:

- **Per-feature timestamps.** Running one file regenerates only that feature's
  report; the index keeps the others and states plainly which features the last
  run did *not* exercise.
- **The index is rebuilt from disk**, not from the current run, so a single-file
  run cannot silently drop the other features from the roll-up.
- **A failure recorded in any feature keeps the overall verdict at FAIL**, even
  if a later partial run of a different feature exits 0. A green subset cannot
  mask a known-failing feature.

Report generation is wrapped so a fault in it can never break the test run.

## Layout

```
sil/
  build.py              compile to build/bms_sil.dll
  fakes/                RTD driver doubles + host Std_Types.h
  harness/              sil_api.h, sil_main.c — control surface for Python
  python/bms_sil.py     ctypes binding + test helpers
  tests/                pytest suite, one file per component
    test_soc.py              SA-*, IT-*, CH-*  the SOC feature
    test_persistence.py      PS-*  Bms_Nvm / Data Flash
    test_lib_interp.py       LI-*  table interpolation
    report.py                report generator
  reports/              generated per-feature reports (committed)
  TEST_REPORT.md        generated roll-up index (committed)
```

`python sil/python/bms_sil.py` runs a standalone smoke check.

## Writing a test

```python
def test_something(bms):          # fixture: cold boot, erased flash
    bms.set_soc(500)                                  # 50.0 %
    bms.run_normal(600_000, current_mA=-20_000)       # 10 min at 20 A discharge
    assert bms.soc < 500
```

`run_normal()` keeps the whole chain healthy (current, voltage and cell frames
all fresh). Use `run_ms()` when a test needs to starve one input — omitting an
argument simply stops injecting that frame, which is how the timeout paths are
reached.

## Known gaps

- **IT-04 is skipped.** The OCV reset tier is unreachable — see
  `SOC_DESIGN.md` 5.2.
- **The blend is only covered degenerately.** `Min`/`Max`/`Avg` cannot be made
  to differ through any reachable API (`SOC_DESIGN.md` 5.1), so the
  non-degenerate blend is checked against a Python mirror of the formula rather
  than end-to-end. `test_blend_divergence_is_unreachable` will fail if that
  ever changes, as a prompt to add real coverage.
- **One xfail records a live defect** in `Battery_Monitor` — see
  `test_CH_stuck_alive_counter_must_invalidate_current`. It is `strict=True`,
  so it flips to a failure the moment the defect is fixed.
- Host floating point is IEEE-754 single precision as on the Cortex-M7 FPU, but
  the two are not bit-verified against each other. Numeric assertions use
  tolerances rather than exact equality where that matters.
