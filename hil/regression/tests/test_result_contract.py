"""Result contract and the exit-code rule."""

from __future__ import annotations

import json

import regression_result as rr


def make(test_id="REG-X-001", status=rr.Status.PASS, area="Boot", **kwargs):
    base = dict(
        test_id=test_id,
        name="a case",
        status=status,
        duration_s=1.25,
        expected="expected text",
        observed="observed text",
        area=area,
    )
    base.update(kwargs)
    return rr.RegressionResult(**base)


def test_the_documented_field_set_survives_serialisation():
    result = make(details="some detail")
    raw = result.to_dict()
    for field in ("test_id", "name", "status", "duration_s", "expected", "observed", "details"):
        assert field in raw
    assert raw["status"] == "PASS"
    assert json.loads(json.dumps(raw))["test_id"] == "REG-X-001"


def test_round_trip_through_json_preserves_every_field():
    original = make(
        status=rr.Status.SKIP,
        area="XCP",
        caps=("CAN5", "JLINK"),
        observations=(rr.Observation.CAN, rr.Observation.XCP),
        evidence=rr.Evidence.NOT_EXECUTED,
        skip_reason="CAN5 not selected",
        diagnostics={"mask": 4},
    )
    restored = rr.RegressionResult.from_dict(json.loads(json.dumps(original.to_dict())))
    assert restored.to_dict() == original.to_dict()


def test_status_error_renders_as_infra_error():
    assert rr.Status.ERROR.label == "INFRA ERROR"
    assert rr.Status.PASS.label == "PASS"
    assert rr.Status.SKIP.label == "SKIP"


def test_only_the_four_contract_statuses_exist_plus_the_infra_marker():
    assert {status.value for status in rr.Status} == {"PASS", "FAIL", "SKIP", "XFAIL", "ERROR"}


def test_skip_factory_records_the_reason_and_no_evidence():
    result = rr.RegressionResult.skip("REG-A-001", "a", "CAN0 not selected", area="CAN")
    assert result.status is rr.Status.SKIP
    assert result.skip_reason == "CAN0 not selected"
    assert "SKIP: CAN0 not selected" in result.details
    assert result.evidence is rr.Evidence.NOT_EXECUTED
    assert result.executed is False
    assert result.satisfied is False


def test_infra_factory_records_the_reason():
    result = rr.RegressionResult.infra("REG-A-002", "b", "channel would not open", area="CAN")
    assert result.status is rr.Status.ERROR
    assert result.infra_error == "channel would not open"
    assert result.executed is False
    assert result.satisfied is False


def test_executed_and_satisfied_semantics():
    assert make(status=rr.Status.PASS).executed is True
    assert make(status=rr.Status.FAIL).executed is True
    assert make(status=rr.Status.XFAIL).executed is True
    assert make(status=rr.Status.PASS).satisfied is True
    assert make(status=rr.Status.XFAIL).satisfied is True
    assert make(status=rr.Status.FAIL).satisfied is False


# --------------------------------------------------------------------------------------------------
# Exit codes
# --------------------------------------------------------------------------------------------------


def test_exit_zero_when_everything_passes():
    suite = rr.Suite(results=[make(status=rr.Status.PASS), make(status=rr.Status.PASS)])
    assert suite.exit_code() == 0
    assert suite.verdict == "PASS"


def test_xfail_is_allowed_to_still_exit_zero():
    suite = rr.Suite(results=[make(status=rr.Status.PASS), make(status=rr.Status.XFAIL)])
    assert suite.exit_code() == 0


def test_exit_one_on_a_dut_failure():
    suite = rr.Suite(results=[make(status=rr.Status.PASS), make(status=rr.Status.FAIL)])
    assert suite.exit_code() == 1
    assert suite.verdict == "FAIL"


def test_exit_two_on_a_skip_alone():
    suite = rr.Suite(results=[make(status=rr.Status.PASS), make(status=rr.Status.SKIP)])
    assert suite.exit_code() == 2
    assert suite.verdict == "INCOMPLETE"


def test_exit_two_on_an_infra_error_alone():
    suite = rr.Suite(results=[make(status=rr.Status.PASS), make(status=rr.Status.ERROR)])
    assert suite.exit_code() == 2


def test_a_dut_failure_outranks_an_infra_error():
    suite = rr.Suite(
        results=[make(status=rr.Status.ERROR), make(status=rr.Status.FAIL), make(status=rr.Status.SKIP)]
    )
    assert suite.exit_code() == 1


def test_a_run_wide_infra_failure_exits_two():
    suite = rr.Suite()
    suite.infra_failure = "no J-Link probe"
    assert suite.exit_code() == 2
    assert suite.verdict == "INFRA ERROR"


def test_an_empty_suite_is_not_a_pass():
    assert rr.Suite().exit_code() == 0
    assert rr.Suite().verdict == "PASS"


# --------------------------------------------------------------------------------------------------
# Aggregation
# --------------------------------------------------------------------------------------------------


def test_counts_and_area_breakdown():
    suite = rr.Suite(
        results=[
            make("REG-A-001", rr.Status.PASS, area="Build"),
            make("REG-A-002", rr.Status.FAIL, area="Build"),
            make("REG-B-001", rr.Status.SKIP, area="CAN"),
        ]
    )
    assert suite.counts() == {
        "PASS": 1,
        "FAIL": 1,
        "SKIP": 1,
        "XFAIL": 0,
        "ERROR": 0,
    }
    assert suite.area_counts("Build")["PASS"] == 1
    assert suite.area_counts("Build")["FAIL"] == 1
    assert suite.area_counts("CAN")["SKIP"] == 1


def test_areas_come_back_in_the_canonical_order():
    suite = rr.Suite(
        results=[
            make("REG-X-001", area="XCP"),
            make("REG-B-001", area="Build"),
            make("REG-M-001", area="Measurement"),
        ]
    )
    assert suite.areas() == ("Build", "Measurement", "XCP")


def test_an_unknown_area_is_appended_rather_than_dropped():
    suite = rr.Suite(results=[make(area="Build"), make(area="Something New")])
    assert suite.areas() == ("Build", "Something New")


def test_get_returns_the_result_by_id():
    suite = rr.Suite(results=[make("REG-A-001"), make("REG-A-002")])
    assert suite.get("REG-A-002") is not None
    assert suite.get("REG-A-999") is None


def test_setup_steps_are_kept_out_of_the_result_list():
    suite = rr.Suite(results=[make("REG-SM-002")])
    suite.record_setup("Enable command to reach ACTIVE", "reached ACTIVE in 0.3 s", ok=True)
    raw = suite.to_dict()
    assert len(raw["tests"]) == 1
    assert raw["setup_steps"][0]["step"] == "Enable command to reach ACTIVE"
    assert raw["setup_steps"][0]["ok"] is True
