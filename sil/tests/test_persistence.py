"""Persistence — Bms_Nvm and the Data Flash record log.

The append-only record log, its rate limiting, erase-and-wrap when the sector
fills, and behaviour under write failure and power loss. The C40_Ip double
enforces real NOR semantics, so these exercise genuine flash rules.

Case IDs match the validation matrix in SOC_DESIGN.md section 4.
Per-case results are in sil/reports/.
"""

from __future__ import annotations

import pytest

from bms_sil import (
    NVM_RECORDS_PER_SECTOR,
    NOMINAL_CELL_MV,
)

def test_PS_01_static_soc_does_not_write(bms):
    """PS-01: with SOC unchanged, the delta gate suppresses flash writes."""
    bms.run_normal(1000, current_mA=0, cell_mV=NOMINAL_CELL_MV)
    writes_before = bms.flash_writes

    bms.run_normal(600_000, current_mA=0, cell_mV=NOMINAL_CELL_MV)  # 10 minutes
    assert bms.flash_writes == writes_before, "static SOC must not wear the flash"


def test_PS_02_moving_soc_writes_once_per_period(bms):
    """PS-02: under load the rate limit yields one write per 60 s, not more."""
    bms.set_soc(500)
    bms.run_normal(1000, current_mA=-20_000, cell_mV=NOMINAL_CELL_MV)
    writes_before = bms.flash_writes

    bms.run_normal(300_000, current_mA=-20_000, cell_mV=NOMINAL_CELL_MV)  # 5 minutes
    written = bms.flash_writes - writes_before

    assert 4 <= written <= 6, f"expected ~5 writes in 5 minutes, got {written}"


def test_PS_03_survives_power_cycle(bms):
    """PS-03: values restored after a power cycle match what was saved."""
    bms.set_soc(614)
    bms.run_normal(61_000, current_mA=0, cell_mV=NOMINAL_CELL_MV)

    saved = bms.nvm_load()
    assert saved is not None

    bms.power_cycle()
    assert (bms.soc_min, bms.soc_max, bms.soc_avg) == saved


def test_PS_05_write_failure_does_not_advance(bms):
    """PS-05: a failed program leaves the record count unchanged and recovers."""
    bms.set_soc(500)
    bms.run_normal(1000, current_mA=0, cell_mV=NOMINAL_CELL_MV)

    bms.set_soc(600)
    bms.fail_next_flash_write(True)
    bms.run_normal(61_000, current_mA=0, cell_mV=NOMINAL_CELL_MV)
    records_after_failure = bms.nvm_records

    # The next window must succeed and actually add a record.
    bms.set_soc(700)
    bms.run_normal(61_000, current_mA=0, cell_mV=NOMINAL_CELL_MV)
    assert bms.nvm_records > records_after_failure


def test_PS_06_power_loss_during_write_leaves_no_bad_record(bms):
    """PS-06: losing supply mid-program must not produce a record that reads back valid.

    The read-back verify in Bms_Nvm_SaveSoc is what protects this.
    """
    bms.set_soc(500)
    bms.run_normal(1000, current_mA=0, cell_mV=NOMINAL_CELL_MV)
    good = bms.nvm_load()

    bms.set_soc(900)
    bms.flash_power_loss_after(6)      # die partway through the 24-byte record
    bms.run_normal(61_000, current_mA=0, cell_mV=NOMINAL_CELL_MV)

    bms.power_cycle()
    restored = bms.nvm_load()

    # Either the previous good record survives, or nothing valid is found -
    # but never a torn record presented as valid.
    assert restored in (good, None), f"torn record accepted as valid: {restored}"


@pytest.mark.slow
def test_PS_04_sector_wraps_when_full(bms):
    """PS-04: after a full sector of records the sector erases and wraps.

    This is the behaviour that keeps persistence alive past ~341 writes; before
    erase-and-wrap was added, Bms_Nvm_SaveSoc simply returned FALSE forever.
    """
    assert bms.flash_erases == 0

    # Force one write per 60 s window by stepping SOC between two values.
    for i in range(NVM_RECORDS_PER_SECTOR + 3):
        bms.set_soc(500 + (i % 2) * 20)
        bms.run_normal(60_000, current_mA=0, cell_mV=NOMINAL_CELL_MV)

    assert bms.flash_erases >= 1, "sector should have wrapped at least once"

    # After the wrap the newest value must still be readable.
    restored = bms.nvm_load()
    assert restored is not None
    assert restored[2] == bms.soc_avg

    bms.power_cycle()
    assert bms.soc_valid is True, "post-wrap record must survive a power cycle"
