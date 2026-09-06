"""pytest fixtures for the BMS SIL suite."""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "python"))

import bms_sil  # noqa: E402
from bms_sil import Bms  # noqa: E402


# The shared library holds process-global state, so it is loaded once and the
# simulated device is reset between tests rather than reloaded.
_INSTANCE: Bms | None = None


def _instance() -> Bms:
    global _INSTANCE
    if _INSTANCE is None:
        _INSTANCE = Bms()
    return _INSTANCE


@pytest.fixture
def bms() -> Bms:
    """A cold-booted BMS with erased Data Flash (virgin device)."""
    dev = _instance()
    dev.flash_wipe()
    dev.power_on()
    return dev


@pytest.fixture
def bms_dirty() -> Bms:
    """A BMS whose flash is *not* wiped, for power-cycle / persistence tests.

    The test is responsible for establishing the flash contents it wants.
    """
    dev = _instance()
    dev.flash_wipe()
    return dev


@pytest.fixture(scope="session")
def const():
    """Constants mirrored from the production headers."""
    return bms_sil


# ---------------------------------------------------------------------------
# Test report generation
#
# Every full run regenerates sil/TEST_REPORT.md from the executed tests, so the
# documented procedure and results can never drift from the suite. A filtered
# run (-k / -m) is skipped, so a partial run cannot overwrite a complete report.
# ---------------------------------------------------------------------------

import report as _report  # noqa: E402

_collector = _report.Collector()
_filtered = False


def pytest_configure(config):
    global _filtered
    _filtered = bool(config.option.keyword or config.option.markexpr)


@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_makereport(item, call):
    outcome = yield
    rep = outcome.get_result()
    if rep.when == "call" or (rep.when == "setup" and rep.outcome == "skipped"):
        _collector.add(item, rep)


def pytest_sessionfinish(session, exitstatus):
    if _filtered:
        return
    try:
        _report.write(_collector, exitstatus)
    except Exception as exc:  # never let reporting break the run
        print(f"\n[report] generation failed: {exc}")
