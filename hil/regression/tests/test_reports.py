"""Report rendering - Markdown and JSON must carry the same facts."""

from __future__ import annotations

import json

import regression_result as rr


def build_suite() -> rr.Suite:
    suite = rr.Suite()
    suite.started_utc = "2026-09-16 12:00:00"
    suite.duration_s = 42.5
    suite.firmware = {
        "elf": {
            "path": "Debug_FLASH/BMS_demo.elf",
            "size": 4045804,
            "sha256": "e02a9ed993cbbd4354ccc2d0417d1b57c2d91a6b0313ddcd3195c29df5efa836",
            "mtime_utc": "2026-09-16 06:48:57",
            "exists": True,
        },
        "git": {
            "sha": "91305e94c6655f3c92ef58368bcb901e78211748",
            "short_sha": "91305e9",
            "dirty": True,
            "dirty_files": ["README.md", "CHANGELOG.md"],
        },
        "target_flash": {"section": ".pflash", "verification": "matched"},
        "build_config": "Debug_FLASH",
    }
    suite.hardware = {
        "mcu": "S32K344",
        "probe": "pylink-square 2.0.1, probe S/N 150712146",
        "jlink": "J-Link DLL V7.90",
        "can": {
            "CAN0": {
                "interface": "pcan",
                "channel": "PCAN_USBBUS1",
                "bitrate": 500000,
                "available": True,
                "note": "open",
            },
            "CAN5": {
                "interface": "pcan",
                "channel": "PCAN_USBBUS4",
                "bitrate": 1000000,
                "available": False,
                "note": "not selected for this run",
            },
        },
    }
    suite.environment = {"Python": "3.13.15", "Host": "Windows 11"}
    suite.configuration = {"build_config": "Debug_FLASH"}
    suite.add(
        rr.RegressionResult(
            test_id="REG-BOOT-001",
            name="Boot to STANDBY",
            status=rr.Status.PASS,
            duration_s=2.4,
            expected="INIT -> STANDBY",
            observed="INIT -> STANDBY in 2100 ms",
            details="transitions: INIT@0ms STANDBY@2100ms",
            area="Boot",
            caps=("JLINK",),
            observations=(rr.Observation.DEBUGGER_EXPORTED,),
            evidence=rr.Evidence.HARDWARE,
            diagnostics={"final_state": 1},
        )
    )
    suite.add(
        rr.RegressionResult(
            test_id="REG-XCP-001",
            name="XCP CONNECT",
            status=rr.Status.SKIP,
            duration_s=0.0,
            expected="positive CONNECT response on 0x601",
            observed="not executed",
            details="SKIP: CAN5 not selected for this run",
            area="XCP",
            caps=("CAN5",),
            skip_reason="CAN5 not selected for this run",
        )
    )
    suite.add(
        rr.RegressionResult(
            test_id="REG-SIL-001",
            name="Existing SIL suite",
            status=rr.Status.ERROR,
            duration_s=0.2,
            expected="suite runs",
            observed="not executed",
            details="INFRA ERROR: no host C compiler",
            area="SIL",
            infra_error="no host C compiler",
        )
    )
    return suite


def test_markdown_has_every_required_section():
    text = build_suite().to_markdown()
    for heading in (
        "# BMS_demo Regression Report",
        "Firmware:",
        "Timestamp:",
        "Hardware:",
        "## Summary",
        "## Tests",
        "### host_artifact_identity",
        "### target_flash_verification",
    ):
        assert heading in text, heading


def test_markdown_summary_table_has_a_row_per_area_and_a_total():
    text = build_suite().to_markdown()
    assert "| Area | PASS | FAIL | XFAIL | SKIP | INFRA ERROR |" in text
    for area in ("Boot", "SIL", "XCP"):
        assert f"| {area} |" in text
    assert "| **Total** |" in text


def test_markdown_test_section_carries_the_contract_fields():
    text = build_suite().to_markdown()
    assert "### REG-BOOT-001 - Boot to STANDBY" in text
    assert "Status: **PASS**" in text
    assert "Expected:" in text
    assert "INIT -> STANDBY" in text
    assert "Observed:" in text
    assert "Duration: 2.400 s" in text


def test_markdown_labels_an_infra_error_clearly():
    text = build_suite().to_markdown()
    assert "Status: **INFRA ERROR**" in text


def test_markdown_records_the_can_bus_mapping():
    text = build_suite().to_markdown()
    assert "## CAN bus mapping" in text
    assert "PCAN_USBBUS1" in text
    assert "PCAN_USBBUS4" in text


def test_markdown_states_the_selection_and_the_setup_steps():
    suite = build_suite()
    suite.selection_active = True
    suite.requested = ["REG-BOOT-001", "REG-XCP-001"]
    suite.record_setup("Enable command", "reached ACTIVE", ok=True)
    text = suite.to_markdown()
    assert "Requested tests:" in text
    assert "`REG-BOOT-001`" in text
    assert "## Setup" in text
    assert "Enable command" in text


def test_host_and_target_identity_are_reported_separately():
    suite = build_suite()
    text = suite.to_markdown()
    assert "Identity of the ELF file **on this host**" in text
    assert "It does not prove what is inside the MCU" in text
    assert "This is the only claim about what the MCU holds" in text


def test_json_document_matches_the_object_graph():
    suite = build_suite()
    raw = suite.to_dict()
    assert raw["schema"] == "bms-demo-regression/1"
    assert raw["verdict"] == "INCOMPLETE"
    assert raw["exit_code"] == 2
    assert [entry["test_id"] for entry in raw["tests"]] == [
        "REG-BOOT-001",
        "REG-XCP-001",
        "REG-SIL-001",
    ]
    assert raw["summary"]["counts"]["PASS"] == 1
    assert raw["summary"]["counts"]["SKIP"] == 1
    assert raw["summary"]["counts"]["ERROR"] == 1


def test_json_document_retains_the_requested_test_list():
    suite = build_suite()
    suite.selection_active = True
    suite.requested = ["REG-BOOT-001"]
    assert suite.to_dict()["selection"] == {
        "active": True,
        "requested_tests": ["REG-BOOT-001"],
    }


def test_json_is_written_and_reloads_identically(tmp_path):
    suite = build_suite()
    target = tmp_path / "reports" / "regression_latest.json"
    suite.write_json(target)
    assert target.is_file()
    assert json.loads(target.read_text(encoding="utf-8")) == suite.to_dict()


def test_markdown_is_written_to_disk(tmp_path):
    suite = build_suite()
    target = tmp_path / "reports" / "regression_latest.md"
    suite.write_markdown(target)
    assert target.read_text(encoding="utf-8") == suite.to_markdown()


def test_console_summary_lists_areas_then_totals():
    suite = build_suite()
    text = rr.console_summary(suite, report_path="hil/regression/reports/regression_latest.md")
    assert "BMS_demo SMOKE REGRESSION" in text
    assert "PASS:  1" in text
    assert "FAIL:  0" in text
    assert "INFRA ERROR: 1" in text
    assert "exit code 2" in text


def test_console_summary_marks_a_partially_skipped_area_as_partial():
    suite = rr.Suite()
    suite.add(rr.RegressionResult("REG-A-1", "a", rr.Status.PASS, 0.1, "e", "o", area="CAN"))
    suite.add(rr.RegressionResult("REG-A-2", "b", rr.Status.SKIP, 0.0, "e", "o", area="CAN"))
    assert "PARTIAL" in rr.console_summary(suite)


def test_console_summary_marks_a_fully_skipped_area_as_skip():
    suite = rr.Suite()
    suite.add(rr.RegressionResult("REG-A-2", "b", rr.Status.SKIP, 0.0, "e", "o", area="CAN"))
    assert "SKIP" in rr.console_summary(suite)


def test_an_empty_report_still_renders():
    suite = rr.Suite()
    text = suite.to_markdown()
    assert "_No results were produced._" in text


def test_details_and_diagnostics_are_fenced_so_they_cannot_break_the_table():
    suite = rr.Suite()
    result = rr.RegressionResult(
        "REG-A-1",
        "a",
        rr.Status.FAIL,
        0.1,
        "e",
        "o",
        details="line one | with a pipe",
        area="CAN",
        diagnostics={"short_intervals_ms": [4.0, 4.1]},
    )
    suite.add(result)
    text = suite.to_markdown()
    assert "line one | with a pipe" in text
    assert '"short_intervals_ms"' in text


# --------------------------------------------------------------------------------------------------
# Git metadata
# --------------------------------------------------------------------------------------------------


def test_every_reported_dirty_file_actually_exists():
    """Guards the porcelain parse: stripping the block shifts the first path by a column."""
    import pathlib

    import regression_common as rc

    info = rc.git_info()
    for name in info["dirty_files"]:
        assert (rc.REPO / name).exists(), f"{name!r} is not a real path - the parse is off"


# --------------------------------------------------------------------------------------------------
# SIL report snapshot / restore
# --------------------------------------------------------------------------------------------------


def test_snapshot_and_restore_leave_the_sil_reports_byte_identical(tmp_path):
    """Running SIL regenerates committed reports; the runner must put them back."""
    import regression_common as rc

    base = tmp_path / "sil"
    (base / "reports").mkdir(parents=True)
    originals = {
        "TEST_REPORT.md": b"original summary\n",
        "reports/soc.md": b"original soc report\n",
    }
    for relative, blob in originals.items():
        (base / relative).write_bytes(blob)
    # A report that does not exist yet must simply be absent from the snapshot.
    missing = ("reports/sop.md",)

    snapshot = rc._snapshot_files(base, tuple(originals) + missing)
    assert set(snapshot) == set(originals)

    for relative in originals:
        (base / relative).write_bytes(b"regenerated by pytest\n")
    (base / "reports" / "sop.md").write_bytes(b"brand new\n")

    rc._restore_files(base, snapshot)

    for relative, blob in originals.items():
        assert (base / relative).read_bytes() == blob
    assert (base / "reports" / "sop.md").read_bytes() == b"brand new\n"
