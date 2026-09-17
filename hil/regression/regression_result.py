"""Result contract, aggregation and report rendering for the BMS_demo regression framework.

This module is deliberately free of hardware imports (no pylink, no python-can) so the
host-only test suite can exercise it anywhere.

Three outcomes are kept strictly apart:

    PASS / XFAIL   the test ran and the device under test behaved as expected
    FAIL           the capability was available and the DUT behaved differently
    SKIP           the test could not legitimately execute (capability unavailable
                   or not selected by the user)
    ERROR          an expected/configured capability failed to open, or the
                   regression infrastructure itself broke.  Rendered as
                   "INFRA ERROR" - it is never a DUT failure.
"""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from datetime import datetime, timezone
from enum import Enum
from pathlib import Path
from typing import Any, Iterable, Sequence

# --------------------------------------------------------------------------------------------------
# Enumerations
# --------------------------------------------------------------------------------------------------


class Status(str, Enum):
    """Outcome of one regression case."""

    PASS = "PASS"
    FAIL = "FAIL"
    SKIP = "SKIP"
    XFAIL = "XFAIL"
    ERROR = "ERROR"

    @property
    def label(self) -> str:
        """How the status is written in reports."""
        return "INFRA ERROR" if self is Status.ERROR else self.value


class Evidence(str, Enum):
    """How much of the result rests on real hardware."""

    HARDWARE = "VERIFIED ON HARDWARE"
    HOST = "HOST-ONLY VERIFIED"
    NOT_EXECUTED = "NOT EXECUTED - SKIPPED"


class Observation(str, Enum):
    """Which interface a case used to observe the DUT."""

    CAN = "CAN"
    GPIO = "GPIO"
    DEBUGGER_EXPORTED = "debugger/exported symbol"
    DEBUGGER_STATIC = "debugger/file-static"
    XCP = "XCP"


#: Stable area order for the summary table.
AREA_ORDER: tuple[str, ...] = (
    "Build",
    "SIL",
    "Flash",
    "Boot",
    "Scheduler",
    "Measurement",
    "CAN",
    "State Machine",
    "Contactor",
    "Fault",
    "XCP",
)

#: Capability names a case may require.
CAP_JLINK = "JLINK"
CAP_CAN0 = "CAN0"
CAP_CAN1 = "CAN1"
CAP_CAN2 = "CAN2"
CAP_CAN5 = "CAN5"


# --------------------------------------------------------------------------------------------------
# Result
# --------------------------------------------------------------------------------------------------


@dataclass
class RegressionResult:
    """Structured outcome of a single regression case."""

    test_id: str
    name: str
    status: Status
    duration_s: float
    expected: str
    observed: str
    details: str = ""

    area: str = "Uncategorised"
    caps: tuple[str, ...] = ()
    observations: tuple[Observation, ...] = ()
    evidence: Evidence = Evidence.NOT_EXECUTED

    skip_reason: str = ""
    infra_error: str = ""

    #: Free-form structured evidence (fault masks, timing statistics, ...).
    diagnostics: dict[str, Any] = field(default_factory=dict)

    # -- construction helpers ----------------------------------------------------------------------

    @classmethod
    def skip(
        cls,
        test_id: str,
        name: str,
        reason: str,
        *,
        area: str = "Uncategorised",
        caps: Sequence[str] = (),
        expected: str = "",
        observed: str = "not executed",
        duration_s: float = 0.0,
    ) -> "RegressionResult":
        return cls(
            test_id=test_id,
            name=name,
            status=Status.SKIP,
            duration_s=duration_s,
            expected=expected,
            observed=observed,
            details=f"SKIP: {reason}",
            area=area,
            caps=tuple(caps),
            skip_reason=reason,
            evidence=Evidence.NOT_EXECUTED,
        )

    @classmethod
    def infra(
        cls,
        test_id: str,
        name: str,
        reason: str,
        *,
        area: str = "Uncategorised",
        caps: Sequence[str] = (),
        expected: str = "",
        observed: str = "not executed",
        duration_s: float = 0.0,
    ) -> "RegressionResult":
        return cls(
            test_id=test_id,
            name=name,
            status=Status.ERROR,
            duration_s=duration_s,
            expected=expected,
            observed=observed,
            details=f"INFRA ERROR: {reason}",
            area=area,
            caps=tuple(caps),
            infra_error=reason,
            evidence=Evidence.NOT_EXECUTED,
        )

    # -- predicates --------------------------------------------------------------------------------

    @property
    def executed(self) -> bool:
        """True when the case actually produced a DUT verdict."""
        return self.status in (Status.PASS, Status.FAIL, Status.XFAIL)

    @property
    def satisfied(self) -> bool:
        """True when the case does not block its dependants."""
        return self.status in (Status.PASS, Status.XFAIL)

    # -- serialisation -----------------------------------------------------------------------------

    def to_dict(self) -> dict[str, Any]:
        return {
            "test_id": self.test_id,
            "name": self.name,
            "status": self.status.value,
            "status_label": self.status.label,
            "duration_s": round(self.duration_s, 3),
            "expected": self.expected,
            "observed": self.observed,
            "details": self.details,
            "area": self.area,
            "caps": list(self.caps),
            "observations": [o.value for o in self.observations],
            "evidence": self.evidence.value,
            "skip_reason": self.skip_reason,
            "infra_error": self.infra_error,
            "diagnostics": self.diagnostics,
        }

    @classmethod
    def from_dict(cls, raw: dict[str, Any]) -> "RegressionResult":
        return cls(
            test_id=raw["test_id"],
            name=raw["name"],
            status=Status(raw["status"]),
            duration_s=float(raw.get("duration_s", 0.0)),
            expected=raw.get("expected", ""),
            observed=raw.get("observed", ""),
            details=raw.get("details", ""),
            area=raw.get("area", "Uncategorised"),
            caps=tuple(raw.get("caps", ())),
            observations=tuple(Observation(o) for o in raw.get("observations", ())),
            evidence=Evidence(raw.get("evidence", Evidence.NOT_EXECUTED.value)),
            skip_reason=raw.get("skip_reason", ""),
            infra_error=raw.get("infra_error", ""),
            diagnostics=raw.get("diagnostics", {}),
        )


# --------------------------------------------------------------------------------------------------
# Aggregation
# --------------------------------------------------------------------------------------------------


@dataclass
class ArtifactIdentity:
    """Identity of an artifact on the host.  Does NOT prove what is inside the MCU."""

    path: str = ""
    size: int = 0
    sha256: str = ""
    mtime_utc: str = ""
    exists: bool = False

    def to_dict(self) -> dict[str, Any]:
        return {
            "path": self.path,
            "size": self.size,
            "sha256": self.sha256,
            "mtime_utc": self.mtime_utc,
            "exists": self.exists,
        }

    @classmethod
    def from_path(cls, path: Path) -> "ArtifactIdentity":
        import hashlib

        if not path.is_file():
            return cls(path=str(path), exists=False)
        blob = path.read_bytes()
        stamp = datetime.fromtimestamp(path.stat().st_mtime, tz=timezone.utc)
        return cls(
            path=str(path),
            size=len(blob),
            sha256=hashlib.sha256(blob).hexdigest(),
            mtime_utc=stamp.strftime("%Y-%m-%d %H:%M:%S"),
            exists=True,
        )


@dataclass
class Suite:
    """Everything one regression run has to say."""

    results: list[RegressionResult] = field(default_factory=list)

    requested: list[str] = field(default_factory=list)
    selection_active: bool = False

    firmware: dict[str, Any] = field(default_factory=dict)
    hardware: dict[str, Any] = field(default_factory=dict)
    environment: dict[str, Any] = field(default_factory=dict)
    configuration: dict[str, Any] = field(default_factory=dict)

    #: Prerequisite steps that ran to establish state but are not requested results.
    setup_steps: list[dict[str, Any]] = field(default_factory=list)

    #: Run-wide infrastructure failure (before any result could be produced).
    infra_failure: str = ""

    started_utc: str = ""
    duration_s: float = 0.0

    # -- collection --------------------------------------------------------------------------------

    def add(self, result: RegressionResult) -> RegressionResult:
        self.results.append(result)
        return result

    def extend(self, results: Iterable[RegressionResult]) -> None:
        for result in results:
            self.add(result)

    def record_setup(self, step: str, outcome: str, *, ok: bool | None = None) -> None:
        """Record a prerequisite step as setup/evidence, never as a skipped result."""
        entry: dict[str, Any] = {"step": step, "outcome": outcome}
        if ok is not None:
            entry["ok"] = ok
        self.setup_steps.append(entry)

    def get(self, test_id: str) -> RegressionResult | None:
        for result in self.results:
            if result.test_id == test_id:
                return result
        return None

    # -- counts ------------------------------------------------------------------------------------

    def counts(self) -> dict[str, int]:
        tally = {status.value: 0 for status in Status}
        for result in self.results:
            tally[result.status.value] += 1
        return tally

    def areas(self) -> tuple[str, ...]:
        seen = {result.area for result in self.results}
        ordered = [area for area in AREA_ORDER if area in seen]
        ordered += sorted(seen - set(ordered))
        return tuple(ordered)

    def area_counts(self, area: str) -> dict[str, int]:
        tally = {status.value: 0 for status in Status}
        for result in self.results:
            if result.area == area:
                tally[result.status.value] += 1
        return tally

    @property
    def verdict(self) -> str:
        if self.infra_failure:
            return "INFRA ERROR"
        tally = self.counts()
        if tally[Status.FAIL.value]:
            return "FAIL"
        if tally[Status.ERROR.value] or tally[Status.SKIP.value]:
            return "INCOMPLETE"
        return "PASS"

    def exit_code(self) -> int:
        """0 all requested executed tests pass, 1 a DUT failure, 2 incomplete run.

        A DUT failure outranks an infrastructure problem.
        """
        tally = self.counts()
        if tally[Status.FAIL.value]:
            return 1
        if tally[Status.ERROR.value] or tally[Status.SKIP.value]:
            return 2
        if self.infra_failure:
            return 2
        return 0

    # -- serialisation -----------------------------------------------------------------------------

    def to_dict(self) -> dict[str, Any]:
        return {
            "schema": "bms-demo-regression/1",
            "started_utc": self.started_utc,
            "duration_s": round(self.duration_s, 3),
            "verdict": self.verdict,
            "exit_code": self.exit_code(),
            "selection": {
                "active": self.selection_active,
                "requested_tests": list(self.requested),
            },
            "firmware": self.firmware,
            "hardware": self.hardware,
            "environment": self.environment,
            "configuration": self.configuration,
            "summary": {
                "counts": self.counts(),
                "by_area": {area: self.area_counts(area) for area in self.areas()},
                "areas_in_order": list(self.areas()),
            },
            "setup_steps": self.setup_steps,
            "infra_failure": self.infra_failure,
            "tests": [result.to_dict() for result in self.results],
        }

    def write_json(self, path: Path) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(self.to_dict(), indent=2) + "\n", encoding="utf-8")

    def write_markdown(self, path: Path) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(self.to_markdown(), encoding="utf-8")

    # -- markdown ----------------------------------------------------------------------------------

    def to_markdown(self) -> str:
        lines: list[str] = []
        add = lines.append

        add("# BMS_demo Regression Report")
        add("")
        add("> Generated by `hil/regression/run_regression.py`. Runtime artifact - do not hand edit.")
        add("")

        elf = self.firmware.get("elf", {}) or {}
        add("Firmware:")
        add(f"    ELF: {elf.get('path', 'unknown')}")
        add(f"    SHA256: {elf.get('sha256', 'unknown')}")
        add(f"    Size: {elf.get('size', 0)} bytes")
        add(f"    ELF mtime (UTC): {elf.get('mtime_utc', 'unknown')}")
        add("")

        git_info = self.firmware.get("git", {}) or {}
        add("Repository:")
        add(f"    Commit: {git_info.get('sha', 'unknown')}")
        add(f"    Dirty: {git_info.get('dirty', 'unknown')}")
        add(f"    Build config: {self.firmware.get('build_config', 'unknown')}")
        add("")

        add("Timestamp:")
        add(f"    {self.started_utc} (run duration {self.duration_s:.1f} s)")
        add("")

        add("Hardware:")
        add(f"    MCU: {self.hardware.get('mcu', 'S32K344')}")
        add(f"    Probe: {self.hardware.get('probe', 'not connected')}")
        add(f"    J-Link: {self.hardware.get('jlink', 'n/a')}")
        add("")

        add("### host_artifact_identity")
        add("")
        add("Identity of the ELF file **on this host**. It does not prove what is inside the MCU.")
        add("")
        add("| Field | Value |")
        add("|---|---|")
        add(f"| Path | `{elf.get('path', 'unknown')}` |")
        add(f"| SHA256 | `{elf.get('sha256', 'unknown')}` |")
        add(f"| Size (bytes) | {elf.get('size', 0)} |")
        add(f"| mtime (UTC) | {elf.get('mtime_utc', 'unknown')} |")
        add(f"| Git commit | `{git_info.get('sha', 'unknown')}` |")
        add(f"| Git dirty | {git_info.get('dirty', 'unknown')} |")
        add("")
        dirty_files = git_info.get("dirty_files") or []
        if dirty_files:
            add("Modified files at run time:")
            add("")
            for name in dirty_files:
                add(f"- `{name}`")
            add("")

        flash = self.firmware.get("target_flash", {}) or {}
        add("### target_flash_verification")
        add("")
        add("Byte compare of the target `.pflash` section against the same section in the ELF,")
        add("read back through the debug probe. This is the only claim about what the MCU holds.")
        add("")
        add("| Field | Value |")
        add("|---|---|")
        add(f"| Section | `{flash.get('section', '.pflash')}` |")
        add(f"| Result | {flash.get('verification', 'not performed')} |")
        if flash.get("detail"):
            add(f"| Detail | {flash['detail']} |")
        add("")

        add("## Summary")
        add("")
        areas = self.areas()
        add("| Area | PASS | FAIL | XFAIL | SKIP | INFRA ERROR |")
        add("|------|------|------|-------|------|-------------|")
        for area in areas:
            tally = self.area_counts(area)
            add(
                f"| {area} | {tally['PASS']} | {tally['FAIL']} | {tally['XFAIL']} "
                f"| {tally['SKIP']} | {tally['ERROR']} |"
            )
        total = self.counts()
        add(
            f"| **Total** | **{total['PASS']}** | **{total['FAIL']}** | **{total['XFAIL']}** "
            f"| **{total['SKIP']}** | **{total['ERROR']}** |"
        )
        add("")
        add(f"Verdict: **{self.verdict}**")
        add("")

        if self.selection_active:
            add("Requested tests:")
            add("")
            if self.requested:
                for test_id in self.requested:
                    add(f"- `{test_id}`")
            else:
                add("- (none matched)")
            add("")
            add("Only the requested cases appear below. Prerequisite steps that ran to establish")
            add("state are listed under *Setup* and are not reported as results.")
            add("")

        if self.infra_failure:
            add("> **INFRA ERROR:** " + self.infra_failure)
            add("")

        if self.setup_steps:
            add("## Setup")
            add("")
            add("| Step | Outcome |")
            add("|---|---|")
            for entry in self.setup_steps:
                outcome = str(entry.get("outcome", "")).replace("|", "\\|")
                add(f"| {entry.get('step', '')} | {outcome} |")
            add("")

        add("## Tests")
        add("")
        if not self.results:
            add("_No results were produced._")
            add("")
        for result in self.results:
            add(f"### {result.test_id} - {result.name}")
            add("")
            add(f"Status: **{result.status.label}**")
            add("")
            add("Expected:")
            add(result.expected or "n/a")
            add("")
            add("Observed:")
            add(result.observed or "n/a")
            add("")
            add(f"Duration: {result.duration_s:.3f} s")
            add("")
            add(f"Area: {result.area}")
            add("")
            add(f"Evidence: {result.evidence.value}")
            add("")
            if result.caps:
                add(f"Required capabilities: {', '.join(result.caps)}")
                add("")
            if result.observations:
                add("Observation method: " + ", ".join(o.value for o in result.observations))
                add("")
            if result.details:
                add("Details:")
                add("")
                add("```text")
                add(result.details.rstrip())
                add("```")
                add("")
            if result.diagnostics:
                add("Diagnostics:")
                add("")
                add("```json")
                add(json.dumps(result.diagnostics, indent=2))
                add("```")
                add("")

        add("## Configuration")
        add("")
        add("```json")
        add(json.dumps(self.configuration, indent=2))
        add("```")
        add("")

        add("## Environment")
        add("")
        add("| Field | Value |")
        add("|---|---|")
        for key, value in self.environment.items():
            add(f"| {key} | {value} |")
        add("")

        can_map = (self.hardware.get("can") or {})
        if can_map:
            add("## CAN bus mapping")
            add("")
            add("| Logical bus | Interface | Physical channel | Bitrate | Available | Note |")
            add("|---|---|---|---|---|---|")
            for bus, info in can_map.items():
                add(
                    f"| {bus} | {info.get('interface', '')} | {info.get('channel', '')} "
                    f"| {info.get('bitrate', '')} | {info.get('available', '')} "
                    f"| {info.get('note', '')} |"
                )
            add("")

        return "\n".join(lines).rstrip() + "\n"


# --------------------------------------------------------------------------------------------------
# Console summary
# --------------------------------------------------------------------------------------------------


def console_summary(suite: Suite, report_path: Path | None = None) -> str:
    """The block printed at the end of a run."""
    width = 62
    lines = ["=" * width, "BMS_demo SMOKE REGRESSION", "=" * width, ""]

    for area in suite.areas():
        tally = suite.area_counts(area)
        if tally[Status.FAIL.value]:
            outcome = "FAIL"
        elif tally[Status.ERROR.value]:
            outcome = "INFRA ERROR"
        elif tally[Status.SKIP.value] and not (tally[Status.PASS.value] or tally[Status.XFAIL.value]):
            outcome = "SKIP"
        elif tally[Status.SKIP.value]:
            outcome = "PARTIAL"
        elif tally[Status.XFAIL.value] and not tally[Status.PASS.value]:
            outcome = "XFAIL"
        else:
            outcome = "PASS"
        lines.append(f"{area:<17}{outcome}")

    total = suite.counts()
    lines += [
        "",
        f"PASS:  {total['PASS']}",
        f"FAIL:  {total['FAIL']}",
        f"XFAIL: {total['XFAIL']}",
        f"SKIP:  {total['SKIP']}",
        f"INFRA ERROR: {total['ERROR']}",
        "",
        f"Verdict: {suite.verdict}   (exit code {suite.exit_code()})",
    ]
    if report_path is not None:
        lines += ["", f"Report:", str(report_path)]
    lines.append("=" * width)
    return "\n".join(lines)
