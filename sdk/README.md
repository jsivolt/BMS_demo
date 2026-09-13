# sdk/ — vendored NXP RTD base headers

The portable `make/` build resolves the four SDK include roots that the S32DS project references via
`${BASE_PLATFORMSDK_S32K3}` / `${PLATFORM_PLATFORMSDK_S32K3}` from this directory instead of from an
S32DS installation, so a fresh clone can build with just a toolchain.

## Provenance

| Item | Value |
| --- | --- |
| Source packages | `BaseNXP_TS_T40D34M70I1R0`, `Platform_TS_T40D34M70I1R0` |
| Package revision | `TS_T40D34M70I1R0` (RTD 7.0.1) |
| Source root | `C:\NXP\S32DS.3.6.10\S32DS\software\PlatformSDK_S32K3\RTD\` |
| Copied from | S32 Design Studio for S32 Platform 3.6.10 |
| Copy date | 2026-09-13 |
| License | `LA_OPT_NXP_SOFTWARE_LICENSE` — see `base/header/SCR.txt` |

## Contents

| Directory | Files | Size | Maps to CDT variable |
| --- | --- | --- | --- |
| `base/header/` | 67 headers (+ `SCR.txt`) | 3.5 MB | `${BASE_PLATFORMSDK_S32K3}/header` |
| `base/include/` | 112 | 32.6 MB | `${BASE_PLATFORMSDK_S32K3}/include` |
| `platform/include/` | 13 | 0.13 MB | `${PLATFORM_PLATFORMSDK_S32K3}/include` |
| `platform/startup/include/` | 4 | 0.02 MB | `${PLATFORM_PLATFORMSDK_S32K3}/startup/include` |

## Scope rationale

`base/header/` holds only `S32K344*.h` (67 files). The complete SDK directory is 1125 files / 284 MB
because it also carries register headers and `.svd` description files for every other S32K3 derivative
(S32K31x/32x/34x/36x/37x/38x/39x, S32M27x) — this board is a fixed S32K344_172HDQFP package, so those
can never be included. Two checks were run before narrowing the set:

1. the current build's `.d` dependency closure references exactly these 67 files, all `S32K344*`;
2. the `S32K344*.h` quoted-include closure is self-contained — all 66 distinct `#include "…"` names it
   references resolve inside this tree, so there is no cross-derivative dependency.

Coverage of "a macro was off today, on tomorrow" is preserved: *all* S32K344 register headers are present,
not only the ones used today. The other three directories are vendored whole so that a module enabled
later (e.g. its `*_MemMap.h`) cannot go missing.

## Deliberately not vendored

These are `#include`d by RTD sources but only inside always-false `#if` blocks for this configuration, so
they never enter the dependency closure and do not exist in the SDK either:

`Clock_Ip_Specific1.h`, `Clock_Ip_Specific2.h` (in `Clock_Ip_Private.h`), `Os.h`, `rtd.h`,
`FreeRTOS.h`, `task.h` (in `OsIf_Internal.h`), `Dma_Ip.h`, `Dma_Ip_Cfg.h`, `EUnit.h`
(DMA / AUTOSAR-OS / Zephyr / FreeRTOS integration paths that are not enabled here).

Enabling one of those features means re-vendoring with `make vendor-sdk` — the failure is a plain
missing-header error, not a silent miscompile.

## Integrity

The recursive copy was verified on 2026-09-13: 196/196 files byte-identical between source and
destination (SHA-256), with no extra files. To re-verify on Linux:

```bash
find sdk -type f -print0 | sort -z | xargs -0 sha256sum
```

## License note

Every file is an unmodified NXP file with its original copyright/license header intact.
`base/header/SCR.txt` is NXP's Software Content Register for the header package and names the outgoing
license as `LA_OPT_NXP_SOFTWARE_LICENSE`; the binding terms are those of the RTD installation this copy
came from. Committing this directory was an explicit project decision, consistent with the repository
already vendoring `RTD/src` and `RTD/include` the same way.

## Refreshing from a local SDK install

```bash
make vendor-sdk S32DS_SDK_ROOT=/path/to/sdk/PlatformSDK_S32K3/RTD
```

This re-runs the same recursive copy (whole `include`/`startup` trees plus `S32K344*.h`) and overwrites
this directory, so re-check `git diff` and the file counts above afterwards.
