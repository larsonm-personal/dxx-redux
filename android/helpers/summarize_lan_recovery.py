#!/usr/bin/env python3
"""Summarize observed LAN fault evidence without inventing timings or passes.

Controller recovery intervals include adb/polling overhead and deliberate holds.
Native wait intervals use one device's monotonic log only. Absent events remain
absent; a failing harness is not classified as an engine failure.
"""

import argparse
import csv
import io
import json
import re
from pathlib import Path

from chart_lan_startup import write_if_changed


def native_events(path):
    result = []
    if not path.exists():
        return result
    for number, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), 1):
        stamp = re.match(r"\s*(\d+\.\d+)\s", line)
        if not stamp or "[STARTUP-" not in line:
            continue
        fields = dict(re.findall(r"(\w+)=([^ ]+)", line))
        result.append({"time": float(stamp[1]), "source": f"{path.name}:{number}", "line": line.strip(), **fields})
    return result


def named(events, name):
    return [event for event in events if event.get("event") == name]


def local_interval(events, begin, end):
    ends = named(events, end)
    if not ends:
        return None
    final = ends[-1]
    starts = [event for event in named(events, begin) if event["time"] <= final["time"]]
    if not starts:
        return None
    start = starts[-1]
    return {"seconds": round(final["time"] - start["time"], 3), "begin": start["source"], "end": final["source"]}


def summarize(root):
    cases = []
    summary_path = root / "recovery-summary.json"
    summary = json.loads(summary_path.read_text(encoding="utf-8-sig")) if summary_path.exists() else None
    for directory in sorted((root / "parallel-startup-checks").glob("*")):
        if not directory.is_dir():
            continue
        game, case = directory.name.split("-", 1)
        events_path = directory / "events.jsonl"
        controller = (
            [json.loads(line) for line in events_path.read_text(encoding="utf-8-sig").splitlines()]
            if events_path.exists()
            else []
        )
        host = native_events(directory / f"{directory.name}-emulator-5554-logcat.txt")
        client = native_events(directory / f"{directory.name}-emulator-5556-logcat.txt")
        third = native_events(directory / f"{directory.name}-emulator-5558-logcat.txt")
        captures = [event for event in client if "[STARTUP-FAULT] capture" in event["line"]]
        releases = [event for event in client if "[STARTUP-FAULT] release" in event["line"]]
        kill = named(controller, "host-loss.request")
        recovered = named(controller, "host-loss.recovered")
        terminal = (
            named(controller, "host-loss.recovered")
            + named(controller, "three.loss.terminal")
            + named(controller, "host-failure.terminal")
        )
        row = {
            "game": game,
            "case": case,
            "case_pass_recorded": bool(named(controller, "case.pass")),
            "host_kill_recorded": bool(kill),
            "controller_host_loss_to_observed_menu_s": round(
                (recovered[-1]["controller_ms"] - kill[-1]["controller_ms"]) / 1000, 3
            )
            if kill and recovered
            else None,
            "client_local_sync_wait": local_interval(client, "level_sync_flush", "initial_sync_timeout"),
            "host_local_loading_wait": local_interval(host, "wait_requests_flush", "initial_load_peer_lost"),
            "third_local_sync_wait": local_interval(third, "level_sync_flush", "initial_sync_timeout"),
            "positive_terminal_menu_observations": sum(
                bool(event.get("data"))
                and event["data"].get("screen_mode") == "menu"
                and not event["data"].get("in_game")
                for event in terminal
            ),
            "host_sync_count": len(named(host, "sync_send_begin")),
            "host_ready_acceptances": len(named(host, "request_ready_accepted")),
            "host_initial_reconnect_routes": [
                event for event in named(host, "initial_route_proven") if event.get("changed") == "1"
            ],
            "host_signature_rejections": sum("[STARTUP-AUTH] signature_rejected" in event["line"] for event in host),
            "host_admissions_received": len(named(host, "admission_received")),
            "late_admissions_ignored": [
                event for event in named(host, "admission_ignored") if event.get("status") in ("1", "3")
            ],
            "proofs_received_while_playing": [
                event for event in named(host, "proof_received") if event.get("status") == "1"
            ],
            "wire_captures": captures,
            "wire_releases": releases,
            "wire_hashes_match_captures": all(
                any(
                    release.get("hash") == capture.get("hash") and release.get("bytes") == capture.get("bytes")
                    for capture in captures
                )
                for release in releases
            )
            if releases
            else None,
            "configured_faults": {
                path.name: json.loads(path.read_text(encoding="utf-8-sig"))
                for path in sorted(directory.glob("*-fault.json"))
            },
        }
        cases.append(row)
    return {"source_summary": summary, "cases": cases, "timing_note": __doc__}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    result = summarize(args.root)
    args.output.mkdir(parents=True, exist_ok=True)
    write_if_changed(
        args.output / "recovery-evidence.json", (json.dumps(result, indent=2, sort_keys=True) + "\n").encode()
    )
    output = io.StringIO()
    fields = [
        "game",
        "case",
        "case_pass_recorded",
        "host_kill_recorded",
        "controller_host_loss_to_observed_menu_s",
        "client_wait_s",
        "host_wait_s",
        "third_wait_s",
        "positive_terminal_menu_observations",
        "host_sync_count",
        "host_ready_acceptances",
        "host_signature_rejections",
        "host_admissions_received",
        "wire_hashes_match_captures",
    ]
    writer = csv.DictWriter(output, fieldnames=fields, lineterminator="\n")
    writer.writeheader()
    for case in result["cases"]:
        row = {key: case.get(key) for key in fields}
        for column, key in (
            ("client_wait_s", "client_local_sync_wait"),
            ("host_wait_s", "host_local_loading_wait"),
            ("third_wait_s", "third_local_sync_wait"),
        ):
            row[column] = (case[key] or {}).get("seconds")
        writer.writerow(row)
    write_if_changed(args.output / "recovery-evidence.csv", output.getvalue().encode())
    print(
        f"Summarized {len(result['cases'])} observed case directories; {sum(case['case_pass_recorded'] for case in result['cases'])} contain explicit pass records"
    )


if __name__ == "__main__":
    main()
