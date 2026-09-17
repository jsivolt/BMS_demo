"""REG-XCP-001 and REG-XCP-002.

XCP runs on CAN5 (0x600 -> 0x601), polled from the 10 ms task.  SHORT_UPLOAD gives an
end-to-end proof of the whole chain:

    PC -> CAN5 -> FlexCAN -> Xcp_Can -> Xcp -> SRAM -> response

The value returned over the wire is compared against the same address read through the
debug probe, so a decoder or copy regression cannot pass unnoticed.
"""

from __future__ import annotations

import time
from typing import Any

import regression_common as rc
from regression_result import Evidence, Observation, RegressionResult, Status
from regression_target import Session

XCP1_ID = "REG-XCP-001"
XCP1_NAME = "XCP CONNECT"
XCP1_CAPS = ("CAN5",)

XCP2_ID = "REG-XCP-002"
XCP2_NAME = "XCP SHORT_UPLOAD against a known SRAM symbol"
XCP2_CAPS = ("CAN5", "JLINK")

#: Xcp_Cfg.h - read whitelist used by Xcp_IsValidRamRange.
RAM_START = 0x20400000
RAM_END = 0x2047FFFF


def _exchange(session: Session, request: bytes, timeout_s: float) -> tuple[bytes | None, float, list]:
    """Sends one CTO and waits for the first response on 0x601."""
    transport = session.transport
    assert transport is not None
    transport.flush("CAN5", 0.05)
    started = time.monotonic()
    transport.send("CAN5", rc.CAN5_XCP_RX_ID, request)
    seen: list[int] = []
    while time.monotonic() - started < timeout_s:
        frame = transport.recv("CAN5", 0.05)
        if frame is None:
            continue
        seen.append(frame.can_id)
        if frame.can_id == rc.CAN5_XCP_TX_ID:
            return frame.data, time.monotonic() - started, seen
    return None, time.monotonic() - started, seen


# --------------------------------------------------------------------------------------------------
# REG-XCP-001
# --------------------------------------------------------------------------------------------------


def xcp_connect_regression(session: Session) -> RegressionResult:
    started = time.monotonic()
    expected = (
        "CONNECT (0xFF, mode 0) on 0x600 produces a positive response on 0x601 with "
        "RESOURCE/COMM_MODE_BASIC/MAX_CTO 8/MAX_DTO 8/versions 0x10 0x10"
    )
    blocked = session.blocked(XCP1_ID, XCP1_NAME, XCP1_CAPS, expected, "XCP")
    if blocked:
        return blocked

    timeout = session.config.timeout("can5_s")
    try:
        response, elapsed, seen = _exchange(session, rc.xcp_connect_request(), timeout)
    except Exception as exc:  # noqa: BLE001
        return RegressionResult.infra(
            XCP1_ID, XCP1_NAME, f"the CONNECT request could not be sent: {exc}",
            area="XCP", caps=XCP1_CAPS, expected=expected,
        )

    diagnostics: dict[str, Any] = {
        "request": rc.xcp_connect_request().hex(" ").upper(),
        "response_timeout_s": timeout,
        "ids_seen_on_can5": [f"0x{can_id:03X}" for can_id in sorted(set(seen))],
        "elapsed_ms": round(elapsed * 1000.0, 1),
    }

    if response is None:
        return RegressionResult(
            test_id=XCP1_ID,
            name=XCP1_NAME,
            status=Status.FAIL,
            duration_s=time.monotonic() - started,
            expected=expected,
            observed=f"no response on 0x601 within {timeout:.1f} s",
            details=(
                "Xcp_ProcessCommand drops unsupported or malformed commands silently, so a "
                "timeout is not by itself proof that the command was rejected.\n"
                f"CAN ids seen while waiting: {diagnostics['ids_seen_on_can5'] or 'none'}"
            ),
            area="XCP",
            caps=XCP1_CAPS,
            observations=(Observation.XCP,),
            evidence=Evidence.HARDWARE,
            diagnostics=diagnostics,
        )

    diagnostics["response"] = response.hex(" ").upper()
    fields = rc.xcp_connect_fields(response)
    diagnostics["connect_fields"] = fields

    problems = []
    if fields is None:
        problems.append(
            f"the response is not a positive CONNECT: {response.hex(' ').upper()}"
        )
    else:
        if fields["max_cto"] != 8:
            problems.append(f"MAX_CTO is {fields['max_cto']}, expected 8")
        if fields["max_dto"] != 8:
            problems.append(f"MAX_DTO is {fields['max_dto']}, expected 8")
        if fields["comm_mode_basic"] != 0:
            problems.append(
                f"COMM_MODE_BASIC is {fields['comm_mode_basic']}, expected 0"
            )
        if fields["protocol_version"] != 0x10:
            problems.append(
                f"protocol version is 0x{fields['protocol_version']:02X}, expected 0x10"
            )
        if fields["transport_version"] != 0x10:
            problems.append(
                f"transport version is 0x{fields['transport_version']:02X}, expected 0x10"
            )

    # Symbol cross-check: only when a probe is available.  Absence is recorded, not failed.
    target = session.target
    if target is not None and target.has("xcp_connected") and target.has("xcp_connect_count"):
        values = target.read()
        connected = int(values.get("xcp_connected", 0)) == 1
        count = int(values.get("xcp_connect_count", 0))
        diagnostics["g_BmsXcpConnected"] = connected
        diagnostics["g_BmsXcpConnectCount"] = count
        if not connected:
            problems.append("g_BmsXcpConnected is still FALSE after a positive CONNECT")
        note = f"g_BmsXcpConnected={int(connected)}, g_BmsXcpConnectCount={count}"
    else:
        note = "g_BmsXcpConnected / g_BmsXcpConnectCount cross-check NOT PERFORMED (no J-Link)"

    return RegressionResult(
        test_id=XCP1_ID,
        name=XCP1_NAME,
        status=Status.FAIL if problems else Status.PASS,
        duration_s=time.monotonic() - started,
        expected=expected,
        observed=f"0x601 responded {response.hex(' ').upper()} in {elapsed * 1000.0:.1f} ms",
        details="\n".join(problems + [note]),
        area="XCP",
        caps=XCP1_CAPS,
        observations=(Observation.XCP,),
        evidence=Evidence.HARDWARE,
        diagnostics=diagnostics,
    )


# --------------------------------------------------------------------------------------------------
# REG-XCP-002
# --------------------------------------------------------------------------------------------------

#: Preferred SHORT_UPLOAD target.  It is a plain uint32 in SRAM and it moves on CONNECT,
#: so a stale or wrong answer is visible immediately.
UPLOAD_SYMBOL = "xcp_connect_count"


def xcp_short_upload_regression(session: Session) -> RegressionResult:
    started = time.monotonic()
    expected = (
        f"SHORT_UPLOAD of 4 bytes at &g_BmsXcpConnectCount returns those bytes, matching the "
        "value read from the same address through the debug probe"
    )
    blocked = session.blocked(XCP2_ID, XCP2_NAME, XCP2_CAPS, expected, "XCP")
    if blocked:
        return blocked

    target = session.target
    if target is None or not target.has(UPLOAD_SYMBOL):
        return RegressionResult.skip(
            XCP2_ID,
            XCP2_NAME,
            f"symbol {UPLOAD_SYMBOL} did not resolve against the ELF, so there is nothing to "
            "compare the response against",
            area="XCP",
            caps=XCP2_CAPS,
            expected=expected,
        )

    try:
        address = target.address_of(UPLOAD_SYMBOL)
    except Exception as exc:  # noqa: BLE001
        return RegressionResult.infra(
            XCP2_ID, XCP2_NAME, f"the symbol address is unavailable: {exc}",
            area="XCP", caps=XCP2_CAPS, expected=expected,
        )

    if not RAM_START <= address <= RAM_END:
        return RegressionResult.skip(
            XCP2_ID,
            XCP2_NAME,
            f"{UPLOAD_SYMBOL} is at 0x{address:08X}, outside the XCP read whitelist "
            f"0x{RAM_START:08X}-0x{RAM_END:08X}",
            area="XCP",
            caps=XCP2_CAPS,
            expected=expected,
        )

    request = rc.xcp_short_upload_request(4, address)
    timeout = session.config.timeout("can5_s")
    diagnostics: dict[str, Any] = {
        "symbol": "g_BmsXcpConnectCount",
        "address": f"0x{address:08X}",
        "request": request.hex(" ").upper(),
    }

    # Read the same address through the probe first.  Only a CONNECT changes this symbol,
    # and this case never sends one, so the value is stable across the exchange.
    try:
        probe_value = int(target.read().get(UPLOAD_SYMBOL, 0))
    except Exception as exc:  # noqa: BLE001
        return RegressionResult.infra(
            XCP2_ID, XCP2_NAME, f"reading {UPLOAD_SYMBOL} through the probe failed: {exc}",
            area="XCP", caps=XCP2_CAPS, expected=expected,
        )

    try:
        response, elapsed, seen = _exchange(session, request, timeout)
    except Exception as exc:  # noqa: BLE001
        return RegressionResult.infra(
            XCP2_ID, XCP2_NAME, f"the SHORT_UPLOAD request could not be sent: {exc}",
            area="XCP", caps=XCP2_CAPS, expected=expected,
        )

    diagnostics["ids_seen_on_can5"] = [f"0x{can_id:03X}" for can_id in sorted(set(seen))]
    diagnostics["elapsed_ms"] = round(elapsed * 1000.0, 1)
    diagnostics["probe_value"] = probe_value

    if response is None:
        return RegressionResult(
            test_id=XCP2_ID,
            name=XCP2_NAME,
            status=Status.FAIL,
            duration_s=time.monotonic() - started,
            expected=expected,
            observed=f"no response on 0x601 within {timeout:.1f} s",
            details=(
                "SHORT_UPLOAD is dropped silently when the frame is shorter than 8 bytes, the "
                "count is 0 or above 7, or the extension byte is non-zero."
            ),
            area="XCP",
            caps=XCP2_CAPS,
            observations=(Observation.XCP,),
            evidence=Evidence.HARDWARE,
            diagnostics=diagnostics,
        )

    diagnostics["response"] = response.hex(" ").upper()
    parsed = rc.xcp_parse_response(response)
    diagnostics["parsed"] = {
        "pid": f"0x{parsed['pid']:02X}" if parsed.get("pid") is not None else None,
        "is_error": parsed.get("is_error"),
    }

    problems = []
    returned = None
    if parsed.get("is_error"):
        code = parsed.get("error_code")
        suffix = f" (code 0x{code:02X})" if code is not None else ""
        problems.append(
            f"the slave answered with an error response: {response.hex(' ').upper()}{suffix}"
        )
    elif len(response) < 5:
        problems.append(f"the response is too short to carry 4 bytes: {response.hex(' ').upper()}")
    else:
        returned = int.from_bytes(response[1:5], "little")
        diagnostics["returned_value"] = returned
        if len(response) >= 8 and any(response[5:8]):
            problems.append(
                "the unused response bytes are not zero: " + response[5:8].hex(" ").upper()
            )
        if returned != probe_value:
            problems.append(
                f"the response carries {returned} (0x{returned:08X}) but the probe reads "
                f"{probe_value} (0x{probe_value:08X}) at 0x{address:08X}"
            )

    observed = (
        f"0x601 responded {response.hex(' ').upper()}"
        + (f"; value {returned} matches the probe read" if returned == probe_value else "")
    )

    return RegressionResult(
        test_id=XCP2_ID,
        name=XCP2_NAME,
        status=Status.FAIL if problems else Status.PASS,
        duration_s=time.monotonic() - started,
        expected=expected,
        observed=observed,
        details="\n".join(
            problems
            + [
                f"address 0x{address:08X} ({UPLOAD_SYMBOL}), probe value {probe_value} "
                f"(0x{probe_value:08X})",
                "Chain exercised: PC -> CAN5 -> FlexCAN -> Xcp_Can -> Xcp -> SRAM -> response.",
            ]
        ),
        area="XCP",
        caps=XCP2_CAPS,
        observations=(Observation.XCP, Observation.DEBUGGER_STATIC),
        evidence=Evidence.HARDWARE,
        diagnostics=diagnostics,
    )
