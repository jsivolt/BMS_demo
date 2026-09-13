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


# ---------------------------------------------------------------------------
# 2-D lookup — Lib_Interp_Lookup_2D_uint16
#
# Added for the SOP static limit tables (SOP_DESIGN.md section 7.2). The map
# under test is deliberately non-separable: it is not f(x) * g(y), so a pair of
# chained 1-D lookups could not reproduce it.
# ---------------------------------------------------------------------------

# X = SOC 0.1 %, Y = temperature 0.1 degC, values = current limit 0.1 A.
MAP_X = [0, 500, 1000]
MAP_Y = [-200, 250, 600]
MAP_V = [
    [100, 200, 150],   # Y = -20.0 degC
    [300, 900, 400],   # Y =  25.0 degC
    [200, 500, 250],   # Y =  60.0 degC
]


def test_LI2_01_returns_exact_value_at_every_breakpoint(bms):
    """LI2-01: every grid intersection returns its stored value unchanged."""
    for iy, y in enumerate(MAP_Y):
        for ix, x in enumerate(MAP_X):
            assert bms.interp2d(MAP_X, MAP_Y, MAP_V, x, y) == MAP_V[iy][ix]


def test_LI2_02_interpolates_along_x_on_a_breakpoint_row(bms):
    """LI2-02: on a Y breakpoint the result is the plain 1-D X interpolation."""
    # Y = 25.0 degC row, midway between SOC 0 and 50 %: (300 + 900) / 2.
    assert bms.interp2d(MAP_X, MAP_Y, MAP_V, 250, 250) == 600


def test_LI2_03_interpolates_along_y_on_a_breakpoint_column(bms):
    """LI2-03: on an X breakpoint the result is the plain 1-D Y interpolation."""
    # SOC 50 % column, midway between -20.0 and 25.0 degC: (200 + 900) / 2.
    assert bms.interp2d(MAP_X, MAP_Y, MAP_V, 500, 25) == 550


def test_LI2_04_bilinear_in_the_interior(bms):
    """LI2-04: an interior point blends all four surrounding corners.

    Centre of the lower-left cell: corners 100, 200, 300, 900 average to 375.
    """
    assert bms.interp2d(MAP_X, MAP_Y, MAP_V, 250, 25) == 375


def test_LI2_05_clamps_at_all_four_edges(bms):
    """LI2-05: past an edge, that axis clamps; the other still interpolates."""
    # Past the low X edge, on a Y breakpoint -> first column of that row.
    assert bms.interp2d(MAP_X, MAP_Y, MAP_V, -9999, 250) == 300
    # Past the high X edge -> last column.
    assert bms.interp2d(MAP_X, MAP_Y, MAP_V, 9999, 250) == 400
    # Past the low Y edge -> first row, still interpolated along X.
    assert bms.interp2d(MAP_X, MAP_Y, MAP_V, 250, -9999) == 150
    # Past the high Y edge -> last row.
    assert bms.interp2d(MAP_X, MAP_Y, MAP_V, 250, 9999) == 350


def test_LI2_06_clamps_at_all_four_corners(bms):
    """LI2-06: outside on both axes returns the nearest corner value."""
    assert bms.interp2d(MAP_X, MAP_Y, MAP_V, -9999, -9999) == MAP_V[0][0]
    assert bms.interp2d(MAP_X, MAP_Y, MAP_V, 9999, -9999) == MAP_V[0][-1]
    assert bms.interp2d(MAP_X, MAP_Y, MAP_V, -9999, 9999) == MAP_V[-1][0]
    assert bms.interp2d(MAP_X, MAP_Y, MAP_V, 9999, 9999) == MAP_V[-1][-1]


def test_LI2_07_map_is_not_separable(bms):
    """LI2-07: the map cannot be reproduced by chained 1-D lookups.

    This is the argument for a real 2-D lookup over f(SOC) * g(T)
    (SOP_DESIGN.md section 7.2). If the map were separable, the ratio between
    two rows would be constant along X. Here it is not.
    """
    assert MAP_V[1][0] / MAP_V[0][0] != MAP_V[1][1] / MAP_V[0][1]


def test_LI2_08_negative_axis_needs_no_offset(bms):
    """LI2-08: a wholly negative Y axis interpolates without an offset."""
    y_axis = [-400, -200]
    values = [[1000], [2000]]
    assert bms.interp2d([0], y_axis, values, 0, -300) == 1500


def test_LI2_09_single_breakpoint_axis_is_constant(bms):
    """LI2-09: a one-breakpoint axis makes the map constant along it."""
    # One X breakpoint, two Y: X is ignored, Y still interpolates.
    assert bms.interp2d([0], [0, 100], [[10], [20]], 12345, 50) == 15
    # Both axes single -> the one value, whatever the inputs.
    assert bms.interp2d([0], [0], [[42]], -5, 5) == 42


def test_LI2_10_duplicate_breakpoint_takes_the_lower(bms):
    """LI2-10: a zero-width cell does not divide by zero; the lower row wins."""
    assert bms.interp2d([0, 0], [0], [[11, 22]], 0, 0) == 11
    assert bms.interp2d([0], [5, 5], [[33], [44]], 0, 5) == 33


def test_LI2_11_null_or_empty_map_returns_zero(bms):
    """LI2-11: NULL pointers or a zero count return 0 rather than faulting."""
    assert bms.interp2d_raw([], [0], [1], 0, 1, 0, 0) == 0
    assert bms.interp2d_raw([0], [], [1], 1, 0, 0, 0) == 0
    assert bms.interp2d_raw([0], [0], [], 1, 1, 0, 0) == 0


def test_LI2_12_descending_values_use_magnitude_arithmetic(bms):
    """LI2-12: a falling map interpolates correctly in both directions.

    A derate or limit map falls with temperature, so the value delta is
    negative while the arithmetic stays unsigned.
    """
    values = [[1000, 800], [600, 200]]
    assert bms.interp2d([0, 100], [0, 100], values, 50, 0) == 900
    assert bms.interp2d([0, 100], [0, 100], values, 50, 100) == 400
    assert bms.interp2d([0, 100], [0, 100], values, 50, 50) == 650
