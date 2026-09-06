"""Generates the SIL test reports from the actual pytest run.

One report per feature under sil/reports/, plus a roll-up index at
sil/TEST_REPORT.md.

The report is derived entirely from the executed tests - objective from the
docstring, procedure from the test source, result and duration from the run -
so it cannot drift from what the suite really does. It is regenerated on every
`pytest` invocation that collects the whole suite.
"""

from __future__ import annotations

import inspect
import os
import platform
import re
import shutil
import subprocess
import sys
import textwrap
from datetime import datetime, timezone
from pathlib import Path

SIL_DIR = Path(__file__).resolve().parent.parent
REPO = SIL_DIR.parent
REPORT = SIL_DIR / "TEST_REPORT.md"

# Requirement coverage, keyed by test-ID prefix. IDs refer to SOC_DESIGN.md §1.
_REQUIREMENTS = {
    "LI": "SOC-FR-13",
    "SA_01": "SOC-FR-03, SOC-FR-04",
    "SA_02": "SOC-FR-03, SOC-FR-04",
    "SA_03": "SOC-FR-03, SOC-FR-04",
    "SA_04": "SOC-FR-03, SOC-FR-04",
    "SA_05": "SOC-FR-11",
    "SA_06": "SOC-FR-11",
    "SA_07": "SOC-FR-11",
    "SA_08": "SOC-FR-01, SOC-IR-01",
    "SA_09": "SOC-FR-01, SOC-TR-01",
    "SA_blend": "SOC-FR-03, SOC-FR-04",
    "IT": "SOC-FR-06, SOC-FR-07",
    "PS_01": "SOC-TR-03",
    "PS_02": "SOC-TR-03",
    "PS_03": "SOC-FR-09",
    "PS_04": "SOC-FR-10",
    "PS_05": "SOC-FR-09",
    "PS_06": "SOC-FR-09",
    "CH_01": "SOC-FR-12",
    "CH_03": "SOC-IR-02",
    "CH_single": "SOC-FR-12, SOC-IR-01",
    "CH_stuck": "SOC-FR-12, SOC-IR-01",
    "blend_divergence": "SOC-FR-02",
    "coulomb": "SOC-FR-01",
}

# One report per feature. Keyed by test module stem.
_FEATURES = {
    "test_lib_interp": (
        "lib_interp",
        "Lib_Interp — generic 1-D table interpolation",
        "The shared lookup helper in `src/common/Lib_Interp.c` (SOC-FR-13).",
    ),
    "test_soc": (
        "soc",
        "SOC — estimation, initialization and upstream signal chain",
        "The SOC feature itself: `src/battery/Bms_Soc.c` and the paths feeding it "
        "through `Battery_Monitor`. Covers the integration law and clamping, the "
        "pack blend, the three-tier startup chain, and upstream faults whose "
        "consequence lands on the SOC output.",
    ),
    "test_persistence": (
        "persistence",
        "Persistence — Bms_Nvm and the Data Flash record log",
        "Append-only log, rate limiting, erase-and-wrap, write failure and power loss.",
    ),
}

REPORTS_DIR = SIL_DIR / "reports"

_OUTCOME_LABEL = {
    "passed": "PASS",
    "failed": "**FAIL**",
    "skipped": "SKIPPED",
    "xfailed": "XFAIL (known defect)",
    "xpassed": "**XPASS — defect appears fixed**",
    "error": "**ERROR**",
}


class Collector:
    """Accumulates one record per executed test."""

    def __init__(self) -> None:
        self.records: list[dict] = []
        self._seen: set[str] = set()

    def add(self, item, report) -> None:
        name = item.originalname or item.name
        # Parametrised cases collapse into one entry; record the worst outcome.
        key = name
        outcome = _classify(report)

        for existing in self.records:
            if existing["name"] == key:
                existing["duration"] += report.duration
                existing["runs"] += 1
                if _severity(outcome) > _severity(existing["outcome"]):
                    existing["outcome"] = outcome
                    existing["longrepr"] = _short_reason(report)
                return

        func = item.function
        self.records.append(
            {
                "name": key,
                "case_id": _case_id(key),
                "title": _title(key),
                "objective": _objective(func),
                "notes": _notes(func),
                "procedure": _procedure(func),
                "requirements": _requirement(key),
                "outcome": outcome,
                "longrepr": _short_reason(report),
                "duration": report.duration,
                "runs": 1,
                "file": Path(str(item.fspath)).name,
                "line": func.__code__.co_firstlineno,
            }
        )
        self._seen.add(key)


def _severity(outcome: str) -> int:
    return {
        "passed": 0,
        "xfailed": 1,
        "skipped": 2,
        "xpassed": 3,
        "failed": 4,
        "error": 5,
    }.get(outcome, 0)


def _classify(report) -> str:
    if hasattr(report, "wasxfail"):
        return "xpassed" if report.passed else "xfailed"
    if report.outcome == "skipped":
        return "skipped"
    return report.outcome


def _short_reason(report) -> str:
    if hasattr(report, "wasxfail"):
        return str(report.wasxfail)
    if report.outcome == "skipped" and isinstance(report.longrepr, tuple):
        return str(report.longrepr[2])
    if report.failed and report.longrepr is not None:
        text = str(report.longrepr)
        lines = [ln for ln in text.splitlines() if ln.startswith("E ")]
        return "\n".join(lines[:8]) if lines else text.splitlines()[-1]
    return ""


def _case_id(name: str) -> str:
    m = re.match(r"test_([A-Z]{2})_(\d{2})_", name)
    if m:
        return f"{m.group(1)}-{m.group(2)}"
    return "—"


def _title(name: str) -> str:
    stripped = re.sub(r"^test_(?:[A-Z]{2}_\d{2}_)?", "", name)
    return stripped.replace("_", " ")


def _objective(func) -> str:
    doc = inspect.getdoc(func) or ""
    return doc.split("\n")[0].strip() if doc else "(no docstring)"


def _notes(func) -> str:
    doc = inspect.getdoc(func) or ""
    parts = doc.split("\n", 1)
    return textwrap.dedent(parts[1]).strip() if len(parts) > 1 else ""


def _procedure(func) -> str:
    """The test body, with the docstring stripped — this is the real procedure."""
    try:
        src = inspect.getsource(func)
    except (OSError, TypeError):
        return ""

    lines = src.splitlines()
    body_start = next((i for i, ln in enumerate(lines) if ln.rstrip().endswith(":")), 0) + 1
    body = lines[body_start:]

    # Drop the docstring block.
    text = "\n".join(body)
    text = re.sub(r'^\s*"""(?:.|\n)*?"""\n', "", text, count=1)
    return textwrap.dedent(text).rstrip()


def _requirement(name: str) -> str:
    stripped = name[len("test_"):] if name.startswith("test_") else name
    for prefix in sorted(_REQUIREMENTS, key=len, reverse=True):
        if stripped.startswith(prefix):
            return _REQUIREMENTS[prefix]
    return "—"


def _feature_of(module_stem: str):
    """(slug, title, blurb) for a test module, or a generic entry if unknown."""
    if module_stem in _FEATURES:
        return _FEATURES[module_stem]
    return (module_stem, module_stem.replace("_", " "), "")


def _cmd(args: list[str]) -> str:
    exe = shutil.which(args[0])
    if exe is None:
        return "not found"
    try:
        out = subprocess.run([exe, *args[1:]], capture_output=True, text=True, timeout=20)
        return (out.stdout or out.stderr).strip().splitlines()[0]
    except Exception:
        return "unknown"


def _environment() -> list[tuple[str, str]]:
    from bms_sil import LIB_PATH  # local import: path set up by conftest

    cc = os.environ.get("SIL_CC") or shutil.which("gcc")
    if cc is None:
        winlibs = (
            Path(os.environ.get("LOCALAPPDATA", ""))
            / "Microsoft/WinGet/Packages"
            / "BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe"
            / "mingw64/bin/gcc.exe"
        )
        cc = str(winlibs) if winlibs.is_file() else None

    cc_version = "unknown"
    if cc:
        try:
            out = subprocess.run([cc, "--version"], capture_output=True, text=True, timeout=20)
            cc_version = out.stdout.strip().splitlines()[0]
        except Exception:
            pass

    lib_stamp = "not built"
    if LIB_PATH.is_file():
        mtime = datetime.fromtimestamp(LIB_PATH.stat().st_mtime, timezone.utc)
        lib_stamp = f"{LIB_PATH.name}, {LIB_PATH.stat().st_size} bytes, built {mtime:%Y-%m-%d %H:%M:%S} UTC"

    try:
        commit = subprocess.run(
            ["git", "-C", str(REPO), "rev-parse", "--short", "HEAD"],
            capture_output=True, text=True, timeout=20,
        ).stdout.strip() or "unknown"
        dirty = subprocess.run(
            ["git", "-C", str(REPO), "status", "--porcelain"],
            capture_output=True, text=True, timeout=20,
        ).stdout.strip()
        if dirty:
            commit += " (working tree has uncommitted changes)"
    except Exception:
        commit = "unknown"

    import pytest as _pytest

    return [
        ("Executed (UTC)", datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M:%S")),
        ("Host", f"{platform.system()} {platform.release()} ({platform.machine()})"),
        ("Host compiler", cc_version),
        ("Python", sys.version.split()[0]),
        ("pytest", _pytest.__version__),
        ("SIL library", lib_stamp),
        ("Repo commit", commit),
    ]


def _counts(records) -> dict:
    out: dict[str, int] = {}
    for r in records:
        out[r["outcome"]] = out.get(r["outcome"], 0) + 1
    return out


def _summary_table(counts: dict) -> list[str]:
    rows = ["| Outcome | Count |", "|---|---|"]
    for key in ("passed", "xfailed", "skipped", "xpassed", "failed", "error"):
        if counts.get(key):
            rows.append(f"| {_OUTCOME_LABEL[key]} | {counts[key]} |")
    return rows


def _case_detail(r: dict) -> list[str]:
    out = [f"### {r['name']}", "", "| | |", "|---|---|",
           f"| Case ID | {r['case_id']} |",
           f"| Requirement | {r['requirements']} |",
           f"| Result | {_OUTCOME_LABEL.get(r['outcome'], r['outcome'])} |",
           f"| Duration | {r['duration'] * 1000:.1f} ms"
           + (f" over {r['runs']} variants" if r["runs"] > 1 else "") + " |",
           f"| Source | `sil/tests/{r['file']}:{r['line']}` |", "",
           f"**Objective.** {r['objective']}", ""]

    if r["notes"]:
        for line in r["notes"].splitlines():
            out.append(f"> {line}" if line.strip() else ">")
        out.append("")

    out += ["**Procedure.**", "", "```python", r["procedure"], "```", ""]

    if r["longrepr"]:
        label = "Reason" if r["outcome"] in ("skipped", "xfailed") else "Evidence"
        out += [f"**{label}.**", "", "```", r["longrepr"], "```", ""]

    return out


def _write_feature_report(stem: str, records: list, env: list, exitstatus: int) -> dict:
    """Write one feature report. Returns a summary dict for the index."""
    slug, title, blurb = _feature_of(stem)
    records = sorted(records, key=lambda r: r["line"])
    counts = _counts(records)
    duration = sum(r["duration"] for r in records)
    failed = counts.get("failed", 0) + counts.get("error", 0) + counts.get("xpassed", 0)

    out = [f"# {title} — Test Report", "",
           "> Generated by `sil/tests/report.py` from the executed tests. Do not edit "
           "by hand: objective and procedure come from the test source, results from "
           "the run.", ""]
    if blurb:
        out += [blurb, ""]

    out += [f"Source: `sil/tests/{stem}.py`  ·  Index: [../TEST_REPORT.md](../TEST_REPORT.md)", ""]

    out += ["## Execution environment", "", "| | |", "|---|---|"]
    out += [f"| {k} | {v} |" for k, v in env]
    out += [""]

    out += ["## Summary", "",
            f"**{len(records)} test cases** "
            f"({sum(r['runs'] for r in records)} executions) in {duration:.2f} s.", ""]
    out += _summary_table(counts)
    out += ["", f"**Feature verdict: {'FAIL' if failed else 'PASS'}**", ""]

    out += ["## Cases", "", "| Case | Test | Requirement | Result |", "|---|---|---|---|"]
    for r in records:
        anchor = r["name"].replace("_", "-").lower()
        out.append(f"| {r['case_id']} | [{r['name']}](#{anchor}) | {r['requirements']} "
                   f"| {_OUTCOME_LABEL.get(r['outcome'], r['outcome'])} |")
    out += [""]

    out += ["## Case detail", ""]
    for r in records:
        out += _case_detail(r)

    # Machine-readable footer, so the index can be rebuilt from whatever
    # reports exist on disk rather than only from the modules this run touched.
    stamp = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M:%S")
    meta = {
        "slug": slug,
        "title": title,
        "cases": str(len(records)),
        "duration": f"{duration:.3f}",
        "verdict": "FAIL" if failed else "PASS",
        "executed": stamp,
    }
    for key in ("passed", "xfailed", "skipped", "xpassed", "failed", "error"):
        meta[key] = str(counts.get(key, 0))
    out += ["<!-- sil-report " + " ".join(f"{k}={v!r}" for k, v in meta.items()) + " -->"]

    REPORTS_DIR.mkdir(parents=True, exist_ok=True)
    (REPORTS_DIR / f"{slug}.md").write_text(
        "\n".join(out) + "\n", encoding="utf-8", newline="\n")

    return {"slug": slug, "title": title, "stem": stem, "counts": counts,
            "cases": len(records), "duration": duration, "failed": failed,
            "executed": stamp}


_META_RE = re.compile(r"<!-- sil-report (.*?) -->")


def _read_feature_reports() -> list[dict]:
    """Rebuild feature summaries from the reports currently on disk."""
    found = []
    if not REPORTS_DIR.is_dir():
        return found

    for path in sorted(REPORTS_DIR.glob("*.md")):
        m = _META_RE.search(path.read_text(encoding="utf-8"))
        if not m:
            continue
        meta = dict(re.findall(r"(\w+)='([^']*)'", m.group(1)))
        counts = {k: int(meta.get(k, 0)) for k in
                  ("passed", "xfailed", "skipped", "xpassed", "failed", "error")}
        found.append({
            "slug": meta.get("slug", path.stem),
            "title": meta.get("title", path.stem),
            "counts": {k: v for k, v in counts.items() if v},
            "cases": int(meta.get("cases", 0)),
            "duration": float(meta.get("duration", 0.0)),
            "failed": counts["failed"] + counts["error"] + counts["xpassed"],
            "executed": meta.get("executed", "unknown"),
        })
    return found


def _write_index(features: list, env: list, exitstatus: int, ran_slugs: set) -> None:
    total_cases = sum(f["cases"] for f in features)
    total_duration = sum(f["duration"] for f in features)
    combined: dict[str, int] = {}
    for f in features:
        for k, v in f["counts"].items():
            combined[k] = combined.get(k, 0) + v

    out = ["# SOC SIL — Test Report Index", "",
           "> Generated by `sil/tests/report.py`. One report per feature; this page "
           "is the roll-up.", "",
           "## Execution environment", "", "| | |", "|---|---|"]
    out += [f"| {k} | {v} |" for k, v in env]
    out += [""]

    out += ["## Overall", "",
            f"**{total_cases} test cases** across {len(features)} features "
            f"in {total_duration:.2f} s.", ""]
    out += _summary_table(combined)
    any_failed = any(f["failed"] for f in features)
    verdict = "FAIL" if (any_failed or exitstatus != 0) else "PASS"
    note = "" if len(ran_slugs) == len(features) else (
        f"  ·  this run exercised {len(ran_slugs)} of {len(features)} features")
    out += ["", f"**Overall: {verdict}** (pytest exit status {exitstatus}){note}", ""]

    out += ["## Features", "",
            "| Feature | Report | Cases | Result | Last executed (UTC) |",
            "|---|---|---|---|---|"]
    for f in sorted(features, key=lambda x: x["slug"]):
        bits = []
        for key in ("passed", "xfailed", "skipped", "xpassed", "failed", "error"):
            if f["counts"].get(key):
                bits.append(f"{f['counts'][key]} {key}")
        status = "**FAIL**" if f["failed"] else "PASS"
        out.append(f"| {f['title']} | [{f['slug']}.md](reports/{f['slug']}.md) "
                   f"| {f['cases']} | {status} — {', '.join(bits)} "
                   f"| {f.get('executed', 'unknown')} |")
    out += [""]

    stale = [f for f in features if f.get("executed") not in (None, "unknown")
             and f["slug"] not in ran_slugs]
    if stale:
        out += ["> Not exercised by the most recent run; figures above are carried "
                "over from each feature's own last execution: "
                + ", ".join(f"`{f['slug']}`" for f in stale), ""]

    REPORT.write_text("\n".join(out) + "\n", encoding="utf-8", newline="\n")


def write(collector: Collector, exitstatus: int) -> None:
    records = collector.records
    if not records:
        return

    env = _environment()

    by_module: dict[str, list] = {}
    for r in records:
        by_module.setdefault(Path(r["file"]).stem, []).append(r)

    written = [_write_feature_report(stem, recs, env, exitstatus)
               for stem, recs in by_module.items()]
    ran_slugs = {f["slug"] for f in written}

    # Drop reports for features that no longer exist (a test module renamed or
    # merged away). Without this the index, which is built from disk, would keep
    # showing results for a suite that is gone. Reports this run produced are
    # never pruned, so an unregistered new module still survives.
    keep = {slug for slug, _, _ in _FEATURES.values()} | ran_slugs
    if REPORTS_DIR.is_dir():
        for path in REPORTS_DIR.glob("*.md"):
            if path.stem not in keep:
                path.unlink()

    # The index is built from every report on disk, not just this run's, so a
    # single-file run cannot silently drop the other features from the roll-up.
    features = _read_feature_reports()
    if not features:
        features = written

    _write_index(features, env, exitstatus, ran_slugs)
