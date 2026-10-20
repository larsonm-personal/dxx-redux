#!/usr/bin/env python3
"""Verify initial readiness and bound cross-device load overlap from causal packets.

Read actual monotonic logcat captures. Packet send/receive pairs bound the host
minus client clock offset; never presume emulator clocks share an epoch.
"""

import argparse
import csv
import io
import json
import re
from pathlib import Path

from chart_lan_startup import write_if_changed


def read_log(path):
    events = []
    loads = []
    lines = path.read_text(encoding="utf-8-sig").splitlines()
    pids = {line.split()[1] for line in lines if "auto_net: starting auto-" in line}
    assert len(pids) == 1, f"Expected one native game process in {path}"
    game_pid = pids.pop()
    for number, line in enumerate(lines, 1):
        clock = re.match(r"\s*(\d+\.\d+)\s", line)
        if not clock or line.split()[1] != game_pid:
            continue
        stamp = float(clock[1])
        fields = dict(re.findall(r"(\w+)=([^ ]+)", line))
        source = f"{path.name}:{number}"
        if "[STARTUP-SYNC]" in line:
            events.append(dict(time=stamp, source=source, **fields))
        if "type=level_load" in line and fields.get("level") == "1":
            loads.append(dict(start=stamp - int(fields["total_us"]) / 1e6, end=stamp, source=source))
        if "phase=6 armed=1" in line:
            break
    return events, loads


def first(events, name):
    return next(e for e in events if e.get("event") == name)


def overlap(h, c, offset):
    return max(0, min(h["end"], c["end"] + offset) - max(h["start"], c["start"] + offset))


def check_pair(host_path, client_path, case):
    he, hl = read_log(host_path)
    ce, cl = read_log(client_path)
    if case == "cancel-client":
        assert not cl, "Canceled client unexpectedly finished LoadLevel"
        assert not any(e.get("event") == "readiness_send" for e in ce), "Canceled client sent readiness"
        assert not any(e.get("event") == "request_ready_accepted" for e in he), (
            "Host accepted readiness from canceled client"
        )
        first(ce, "admission_send")
        first(he, "admission_received")
        first(ce, "load_hold_begin")
        assert not any(e.get("event") == "load_hold_end" for e in ce)
        return dict(case=case, verified="canceled before load/readiness; inspect case.pass for timeout recovery")
    h, c = hl[0], cl[0]
    ready = first(ce, "readiness_send")
    assert ready["time"] >= c["end"], "Client requested readiness before load completed"
    accepted = first(he, "request_ready_accepted")
    matching = [e for e in ce if e.get("event") == "readiness_send" and e.get("counter") == accepted.get("counter")]
    assert matching, "Host accepted a counter not sent as loaded readiness"
    send_sync = first(he, "sync_send_begin")
    receive_sync = first(ce, "sync_received")
    assert send_sync["time"] >= accepted["time"] >= h["end"]
    row = dict(
        case=case,
        verified="post-load readiness counter accepted",
        host_load_s=h["end"] - h["start"],
        client_load_s=c["end"] - c["start"],
        host_load_source=h["source"],
        client_load_source=c["source"],
        readiness_source=matching[0]["source"],
        accepted_source=accepted["source"],
    )
    if case == "dropped-admission":
        first(ce, "admission_test_drop")
        assert not any(e.get("event") == "admission_send" for e in ce)
        return row
    admission = first(ce, "admission_send")
    received = next(
        e for e in he if e.get("event") == "admission_received" and e.get("counter") == admission.get("counter")
    )
    assert admission["counter"] != accepted["counter"], "Admission satisfied readiness"
    # Send logs precede sendto; receive logs follow recvfrom. Add 1ms per
    # endpoint for logcat's millisecond rounding, retaining conservative bounds
    lower = send_sync["time"] - receive_sync["time"] - 0.002
    upper = received["time"] - admission["time"] + 0.002
    assert lower <= upper, "Causal clock bounds inconsistent"
    guaranteed = min(overlap(h, c, lower), overlap(h, c, upper))
    row.update(
        offset_lower_s=lower,
        offset_upper_s=upper,
        offset_width_s=upper - lower,
        guaranteed_overlap_s=guaranteed,
        host_start_s=h["start"],
        host_end_s=h["end"],
        client_start_s=c["start"],
        client_end_s=c["end"],
        admission_send_source=admission["source"],
        admission_receive_source=received["source"],
        sync_send_source=send_sync["source"],
        sync_receive_source=receive_sync["source"],
    )
    if case == "slow-client-duplicate":
        ignored = first(he, "admission_ignored")
        assert ignored.get("status") == "3", "Duplicate was not ignored in WAITING"
        assert ignored["time"] < accepted["time"]
        first(ce, "load_hold_end")
    if case == "long-client":
        hold = first(ce, "load_hold_begin")
        end = first(ce, "load_hold_end")
        assert end["time"] - hold["time"] >= 19.9
        wait = first(he, "wait_requests_flush")
        assert accepted["time"] - wait["time"] > 15, "Long load did not cross old timeout"
    if case == "slow-host":
        first(he, "load_hold_end")
    return row


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("evidence", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--allow-incomplete", action="store_true", help="Explicitly analyze a partial failed run")
    args = parser.parse_args()
    rows = []
    for host in sorted(args.evidence.rglob("d*-emulator-5554-logcat.txt")):
        client = host.with_name(host.name.replace("5554", "5556"))
        game, case = host.name.split("-", 1)
        case = case.removesuffix("-emulator-5554-logcat.txt")
        row = check_pair(host, client, case)
        if not case.isdigit():
            events = [
                json.loads(line) for line in (host.parent / "events.jsonl").read_text(encoding="utf-8-sig").splitlines()
            ]
            assert any(e["event"] == "case.pass" for e in events), f"Fault case {game}/{case} did not pass"
        row.update(game=game, directory=str(host.parent.relative_to(args.evidence)))
        rows.append(row)
    assert rows, "No peer captures found"
    if not args.allow_incomplete:
        cases = {
            "1",
            "2",
            "3",
            "slow-host",
            "slow-client-duplicate",
            "long-client",
            "dropped-admission",
            "cancel-client",
            "retry",
        }
        assert {(r["game"], r["case"]) for r in rows} == {(g, c) for g in ("d1", "d2") for c in cases}, (
            "Incomplete experiment"
        )
    args.output.mkdir(parents=True, exist_ok=True)
    columns = list(dict.fromkeys(k for row in rows for k in row))
    output = io.StringIO(newline="")
    writer = csv.DictWriter(output, fieldnames=columns, lineterminator="\n")
    writer.writeheader()
    writer.writerows(rows)
    write_if_changed(args.output / "overlap.csv", output.getvalue().encode())
    write_if_changed(args.output / "verification.json", (json.dumps(rows, indent=2, sort_keys=True) + "\n").encode())
    import matplotlib

    matplotlib.use("Agg")
    matplotlib.rcParams["svg.hashsalt"] = "dxx-parallel-load-v1"
    import matplotlib.pyplot as plt

    measured = [r for r in rows if r["case"].isdigit()]
    fig, axes = plt.subplots(len(measured), 1, figsize=(10, 2 * len(measured)), squeeze=False)
    for ax, row in zip(axes[:, 0], measured):
        origin = row["host_start_s"]
        middle = (row["offset_lower_s"] + row["offset_upper_s"]) / 2
        ax.barh(1, row["host_load_s"], left=0, color="#2878b5", label="Host LoadLevel")
        ax.barh(
            0,
            row["client_load_s"],
            left=row["client_start_s"] + middle - origin,
            color="#f28e2b",
            label="Client LoadLevel (bounded alignment)",
        )
        ax.errorbar(
            [row["client_start_s"] + middle - origin], [0], xerr=row["offset_width_s"] / 2, color="black", capsize=4
        )
        ax.set_yticks([0, 1], ["client", "host"])
        ax.set_title(
            f"{row['game'].upper()} sample {row['case']}: overlap at least {row['guaranteed_overlap_s']:.3f}s; clock bound width {row['offset_width_s']:.3f}s"
        )
        ax.set_xlabel("Seconds relative to host LoadLevel start")
        ax.grid(axis="x", alpha=0.2)
    fig.tight_layout()
    for extension in ("svg", "png"):
        buffer = io.BytesIO()
        fig.savefig(
            buffer,
            format=extension,
            dpi=140,
            metadata={"Date": None} if extension == "svg" else {"Software": "dxx-redux"},
        )
        write_if_changed(args.output / f"overlap.{extension}", buffer.getvalue())
    print(f"Verified {len(rows)} peer pairs; {len(measured)} matched timing samples")


if __name__ == "__main__":
    main()
