"""Lib_Interp — generic 1-D table interpolation.

The shared lookup helper in src/common/Lib_Interp.c. Covers requirement
SOC-FR-13: the interpolation must be a reusable library function, correct for
ascending and descending curves, clamping rather than extrapolating.

Case IDs match the validation matrix in SOC_DESIGN.md section 4.
Per-case results are in sil/reports/.
"""

from __future__ import annotations

import pytest

from bms_sil import (
    OCV_TABLE,
)

def test_LI_01_clamps_below_first_row(bms):
    """LI-01: x below the first breakpoint returns the first row's Y."""
    assert bms.interp(OCV_TABLE, 2500) == OCV_TABLE[0][1]
    assert bms.interp(OCV_TABLE, 0) == OCV_TABLE[0][1]


def test_LI_02_clamps_above_last_row(bms):
    """LI-02: x above the last breakpoint returns the last row's Y."""
    assert bms.interp(OCV_TABLE, 4200) == OCV_TABLE[-1][1]
    assert bms.interp(OCV_TABLE, 65535) == OCV_TABLE[-1][1]


@pytest.mark.parametrize("x,y", OCV_TABLE)
def test_LI_03_exact_breakpoints(bms, x, y):
    """LI-03: x exactly on a breakpoint returns that row's Y exactly."""
    assert bms.interp(OCV_TABLE, x) == y


def test_LI_04_interpolates_midway(bms):
    """LI-04: linear interpolation between two rows, with correct rounding."""
    # 3300 mV -> 200, 3450 mV -> 500. Midpoint 3375 mV -> 350.
    assert bms.interp(OCV_TABLE, 3375) == 350
    # A third of the way: 3350 -> 200 + (50/150)*300 = 300
    assert bms.interp(OCV_TABLE, 3350) == 300


def test_LI_05_descending_y_does_not_underflow(bms):
    """LI-05: Y decreasing with X (derating / NTC curve) interpolates downward.

    Guards the uint32 span underflow that a naive (y1 - y0) would hit.
    """
    table = [(0, 1000), (100, 500), (200, 0)]
    assert bms.interp(table, 0) == 1000
    assert bms.interp(table, 50) == 750
    assert bms.interp(table, 100) == 500
    assert bms.interp(table, 150) == 250
    assert bms.interp(table, 200) == 0


def test_LI_06_single_row_table(bms):
    """LI-06: a one-row table returns that row's Y for any x."""
    table = [(1234, 777)]
    for x in (0, 1234, 65535):
        assert bms.interp(table, x) == 777


def test_LI_07_null_or_empty_table_returns_zero(bms):
    """LI-07: NULL table or zero size returns 0 rather than faulting."""
    assert bms.interp_raw([], 0, 100) == 0
    assert bms.interp_raw([10, 20], 0, 100) == 0


def test_LI_08_max_span_no_overflow(bms):
    """LI-08: full-range table does not overflow the uint32 intermediate.

    Worst case is 65535 * 65535, which is the reason the implementation uses
    magnitude arithmetic in uint32 rather than a signed intermediate.
    """
    table = [(0, 0), (65535, 65535)]
    assert bms.interp(table, 0) == 0
    assert bms.interp(table, 65535) == 65535
    mid = bms.interp(table, 32768)
    assert abs(mid - 32768) <= 1
