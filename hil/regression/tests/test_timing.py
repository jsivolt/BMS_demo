"""CAN0 inter-arrival statistics - the check that has to catch cycle-time collapse."""

from __future__ import annotations

import regression_common as rc

EXPECTED = 100.0
TOLERANCE = 50.0
BURST = 40.0


def series(period_ms: float, count: int, start: float = 0.0):
    return [start + index * period_ms / 1000.0 for index in range(count)]


def analyse(timestamps):
    return rc.inter_arrival_stats(timestamps, EXPECTED, TOLERANCE, BURST)


def test_a_clean_hundred_millisecond_series_has_no_problems():
    stats = analyse(series(100.0, 40))
    assert stats["frame_count"] == 40
    assert stats["interval_count"] == 39
    assert stats["mean_period_ms"] == 100.0
    assert stats["min_period_ms"] == 100.0
    assert stats["max_period_ms"] == 100.0
    assert stats["burst_detected"] is False
    assert stats["missing_frames"] == 0
    assert rc.timing_verdict(stats) == []


def test_jitter_inside_the_tolerance_is_accepted():
    timestamps = [0.0]
    for index in range(30):
        timestamps.append(timestamps[-1] + (100.0 + (5 if index % 2 else -5)) / 1000.0)
    stats = analyse(timestamps)
    assert stats["over_tolerance_count"] == 0
    assert rc.timing_verdict(stats) == []


def test_the_historic_burst_collapse_is_detected_and_named():
    """The regression: after a scheduler backlog the 100 ms frames burst at ~4 ms."""
    timestamps = series(100.0, 15)
    burst_start = timestamps[-1]
    timestamps.extend(burst_start + index * 0.004 for index in range(1, 12))
    stats = analyse(timestamps)
    assert stats["burst_detected"] is True
    assert stats["short_interval_count"] >= 10
    assert stats["min_period_ms"] <= 5.0
    problems = rc.timing_verdict(stats)
    assert problems, "a burst run must never be reported as acceptable"
    assert any("collapse" in problem for problem in problems)


def test_the_average_alone_would_have_hidden_the_burst():
    """Mean stays plausible while individual intervals collapse - hence the explicit flag."""
    timestamps = series(100.0, 60)
    timestamps.extend(timestamps[-1] + index * 0.004 for index in range(1, 6))
    stats = analyse(timestamps)
    assert stats["mean_period_ms"] > 40.0
    assert stats["burst_detected"] is True
    assert stats["short_interval_count"] == 5


def test_a_long_gap_is_reported_as_reduced_coverage():
    timestamps = series(100.0, 10)
    timestamps.extend(timestamps[-1] + 0.1 + index * 0.1 for index in range(1, 11))
    stats = analyse(timestamps)
    assert stats["coverage_ratio"] < 1.0
    assert stats["missing_frames"] > 0


def test_short_interval_samples_are_carried_in_the_result():
    timestamps = series(100.0, 5)
    timestamps.extend(timestamps[-1] + index * 0.004 for index in range(1, 4))
    stats = analyse(timestamps)
    assert stats["short_intervals_ms"]
    assert max(stats["short_intervals_ms"]) < BURST


def test_fewer_than_two_frames_is_reported_as_a_problem():
    stats = analyse([1.0])
    assert stats["frame_count"] == 1
    assert stats["interval_count"] == 0
    assert rc.timing_verdict(stats) == ["fewer than two frames captured"]


def test_no_frames_at_all_is_handled():
    stats = analyse([])
    assert stats["frame_count"] == 0
    assert rc.timing_verdict(stats) == ["fewer than two frames captured"]


def test_p95_is_computed_from_the_sorted_intervals():
    timestamps = [0.0]
    for index in range(99):
        timestamps.append(timestamps[-1] + (0.100 if index < 95 else 0.200))
    stats = analyse(timestamps)
    assert stats["p95_period_ms"] >= 100.0
    assert stats["p95_period_ms"] < stats["max_period_ms"]


def test_percentile_edge_cases():
    assert rc.percentile([], 0.5) == 0.0
    assert rc.percentile([7.0], 0.95) == 7.0
    assert rc.percentile([1.0, 2.0, 3.0, 4.0, 5.0], 0.0) == 1.0
    assert rc.percentile([1.0, 2.0, 3.0, 4.0, 5.0], 1.0) == 5.0


def test_expected_period_is_echoed_so_the_report_states_its_own_tolerance():
    stats = analyse(series(100.0, 5))
    assert stats["expected_period_ms"] == EXPECTED
    assert stats["tolerance_ms"] == TOLERANCE
    assert stats["burst_threshold_ms"] == BURST
