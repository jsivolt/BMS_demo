"""Dependency propagation and the SKIP / FAIL / INFRA ERROR contract.

These are the rules that stop the report from lying: a missing or unconfigured
capability SKIPs, a broken-but-configured capability is an infrastructure error, and a
failed prerequisite never turns into a cascade of meaningless failures.
"""

from __future__ import annotations

import pytest

import regression_can as rcan
import regression_common as rc
import regression_result as rr
import regression_target as rt

CAN1_SPEC = rcan.BusSpec("CAN1", "pcan", "PCAN_USBBUS2", 1000000)
CAN2_SPEC = rcan.BusSpec("CAN2", "pcan", "PCAN_USBBUS3", 1000000)


def make_session(buses=(), sniff=None, jlink=True, jlink_infra=False):
    session = rt.Session(args=type("A", (), {"bus": list(buses)})(), config=rc.NominalConfig.load())
    session.sniff = sniff or {}
    session.extra["jlink_infra"] = jlink_infra
    if not jlink:
        session.extra["jlink_reason"] = "no J-Link probe is connected"
    return session


# --------------------------------------------------------------------------------------------------
# Preconditions: a case never runs on unsatisfied requirements
# --------------------------------------------------------------------------------------------------


def test_a_missing_prerequisite_blocks_the_dependant():
    session = make_session()
    session.executed["REG-MEAS-001"] = rr.RegressionResult(
        "REG-MEAS-001", "m", rr.Status.FAIL, 0.1, "e", "o"
    )
    assert session.prerequisite_problem(["REG-MEAS-001"]) == "Prerequisite REG-MEAS-001 failed"


def test_a_skipped_prerequisite_blocks_the_dependant_with_a_clear_reason():
    session = make_session()
    session.executed["REG-BOOT-001"] = rr.RegressionResult(
        "REG-BOOT-001", "b", rr.Status.SKIP, 0.0, "e", "o"
    )
    assert session.prerequisite_problem(["REG-BOOT-001"]) == "Prerequisite REG-BOOT-001 was skipped"


def test_an_infrastructure_error_upstream_blocks_the_dependant_with_a_clear_reason():
    session = make_session()
    session.executed["REG-SIL-001"] = rr.RegressionResult(
        "REG-SIL-001", "s", rr.Status.ERROR, 0.0, "e", "o"
    )
    assert (
        session.prerequisite_problem(["REG-SIL-001"])
        == "Prerequisite REG-SIL-001 hit an infrastructure error"
    )


def test_a_passing_prerequisite_does_not_block():
    session = make_session()
    session.executed["REG-SM-001"] = rr.RegressionResult(
        "REG-SM-001", "s", rr.Status.PASS, 0.1, "e", "o"
    )
    assert session.prerequisite_problem(["REG-SM-001"]) is None


def test_an_xfail_prerequisite_counts_as_satisfied():
    session = make_session()
    session.executed["REG-X-001"] = rr.RegressionResult(
        "REG-X-001", "x", rr.Status.XFAIL, 0.1, "e", "o"
    )
    assert session.prerequisite_problem(["REG-X-001"]) is None


def test_an_unrun_prerequisite_does_not_block():
    """A case that was never attempted must not silently skip everything downstream."""
    assert make_session().prerequisite_problem(["REG-NEVER-RAN"]) is None


def test_a_prerequisite_is_found_even_when_it_was_not_requested_for_the_report():
    """--test must not break dependency tracking."""
    session = make_session()
    session.executed["REG-SM-002"] = rr.RegressionResult(
        "REG-SM-002", "s", rr.Status.FAIL, 0.1, "e", "o"
    )
    assert session.prerequisite_problem(["REG-SM-002"]) == "Prerequisite REG-SM-002 failed"


# --------------------------------------------------------------------------------------------------
# Capability classification
# --------------------------------------------------------------------------------------------------


def test_without_a_probe_the_jlink_capability_is_a_skip_not_an_error():
    problem = make_session(jlink=False).capability_problem(("JLINK",))
    assert problem is not None
    assert problem.infra is False
    assert "no J-Link probe" in problem.reason


def test_a_probe_that_fails_to_connect_is_an_infrastructure_error():
    problem = make_session(jlink=False, jlink_infra=True).capability_problem(("JLINK",))
    assert problem is not None
    assert problem.infra is True


def test_a_bus_that_is_not_configured_for_this_run_skips():
    session = make_session(buses=["CAN0"])
    session.transport = rcan.NullTransport({"CAN1": CAN1_SPEC}, reason="not selected")
    problem = session.capability_problem(("CAN1",))
    assert problem is not None
    assert problem.infra is False


def test_a_foreign_transmitter_on_an_owned_bus_skips_rather_than_failing():
    conflict = {
        "reason": "CAN1 already carries 0x401 from another transmitter; the regression must "
        "own these ids to produce deterministic stimulus"
    }
    session = make_session(buses=["CAN1"], sniff={"conflicts": {"CAN1": conflict}})
    session.transport = rcan.NullTransport({"CAN1": CAN1_SPEC})
    problem = session.capability_problem(("CAN1",))
    assert problem is not None
    assert problem.infra is False
    assert problem.reason == conflict["reason"]


def test_blocked_returns_a_skip_result_for_a_missing_prerequisite():
    session = make_session(jlink=False)
    result = session.blocked("REG-BOOT-001", "b", ("JLINK",), "expected", "Boot")
    assert result is not None
    assert result.status is rr.Status.SKIP
    assert result.caps == ("JLINK",)
    assert result.executed is False


def test_blocked_returns_an_infra_result_when_the_capability_is_broken():
    session = make_session(jlink=False, jlink_infra=True)
    result = session.blocked("REG-BOOT-001", "b", ("JLINK",), "expected", "Boot")
    assert result is not None
    assert result.status is rr.Status.ERROR
    assert result.infra_error


def test_blocked_returns_none_when_everything_is_present():
    session = make_session()
    session.target = _FakeTarget()
    assert session.blocked("REG-X-001", "x", ("JLINK",), "expected", "Boot") is None


def test_a_context_string_is_prepended_to_the_skip_reason():
    session = make_session(jlink=False)
    result = session.blocked(
        "REG-MEAS-001", "m", ("JLINK",), "expected", "Measurement", context="the vAFE needs it"
    )
    assert result is not None
    assert result.skip_reason.startswith("the vAFE needs it:")


class _FakeTarget:
    def __init__(self):
        self.layout = type("L", (), {"fields": {"state": (0, 1, "B")}})()


def test_a_target_without_a_resolved_layout_is_not_a_usable_jlink_capability():
    session = make_session()
    session.target = type("T", (), {"layout": None})()
    problem = session.capability_problem(("JLINK",))
    assert problem is not None
    assert "resolved symbol layout" in problem.reason


# --------------------------------------------------------------------------------------------------
# The three outcomes stay distinct in the report
# --------------------------------------------------------------------------------------------------


def test_skip_and_infra_are_never_reported_as_a_dut_failure():
    suite = rr.Suite(
        results=[
            rr.RegressionResult.skip("REG-A-001", "a", "no probe", area="Boot"),
            rr.RegressionResult.infra("REG-A-002", "b", "channel occupied", area="CAN"),
        ]
    )
    assert suite.counts()["FAIL"] == 0
    assert suite.counts()["SKIP"] == 1
    assert suite.counts()["ERROR"] == 1
    assert suite.exit_code() == 2
    assert "INFRA ERROR" in suite.to_markdown()


def test_a_dut_failure_is_reported_as_one_even_when_the_dut_merely_skipped_earlier():
    suite = rr.Suite(
        results=[
            rr.RegressionResult.skip("REG-A-001", "a", "no probe"),
            rr.RegressionResult("REG-A-002", "b", rr.Status.FAIL, 0.1, "e", "o"),
        ]
    )
    assert suite.exit_code() == 1


def test_setup_steps_record_prerequisites_without_inventing_results():
    suite = rr.Suite()
    suite.record_setup("Enable command", "reached ACTIVE in 0.31 s", ok=True)
    assert suite.results == []
    assert suite.to_dict()["setup_steps"][0]["ok"] is True
    assert "ACTIVE in 0.31 s" in suite.to_markdown()


@pytest.mark.parametrize(
    "status,expected_code",
    [
        (rr.Status.PASS, 0),
        (rr.Status.XFAIL, 0),
        (rr.Status.FAIL, 1),
        (rr.Status.SKIP, 2),
        (rr.Status.ERROR, 2),
    ],
)
def test_exit_code_matrix(status, expected_code):
    assert rr.Suite(results=[rr.RegressionResult("REG-A-001", "a", status, 0.1, "e", "o")]).exit_code() == expected_code


# --------------------------------------------------------------------------------------------------
# --bus selection
# --------------------------------------------------------------------------------------------------


def _orchestrator_args(bus, can_config):
    return type(
        "A",
        (),
        {
            "bus": list(bus),
            "can_config": can_config,
            "can0": None,
            "can1": None,
            "can2": None,
            "can5": None,
        },
    )()


def test_an_unknown_bus_name_is_an_infrastructure_error(tmp_path):
    import json

    import run_regression as runner

    config = tmp_path / "can_config.json"
    config.write_text(
        json.dumps({"buses": {"CAN0": {"interface": "pcan", "channel": "X", "bitrate": 500000}}}),
        encoding="utf-8",
    )
    session = rt.Session(args=_orchestrator_args(["CAN9"], config), config=rc.NominalConfig.load())
    suite = rr.Suite()
    session.suite = suite

    runner.open_buses(session, suite)

    assert "CAN9" in suite.infra_failure
    assert session.transport is None


def test_naming_a_bus_that_is_in_the_file_loads_the_config_first(tmp_path):
    """Regression guard: bus names must be validated after the config is read, not before."""
    import json

    import run_regression as runner

    config = tmp_path / "can_config.json"
    config.write_text(
        json.dumps(
            {"buses": {"CAN1": {"interface": "no-such-backend", "channel": "NOPE", "bitrate": 1000000}}}
        ),
        encoding="utf-8",
    )
    session = rt.Session(args=_orchestrator_args(["CAN1"], config), config=rc.NominalConfig.load())
    suite = rr.Suite()
    session.suite = suite

    runner.open_buses(session, suite)

    assert suite.infra_failure == "", "a known bus name must not be reported as unknown"
    assert "CAN1" in session.can_specs
    assert session.transport is not None
