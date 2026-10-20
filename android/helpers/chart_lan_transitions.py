#!/usr/bin/env python3
"""Chart observed LAN timing artifacts. Never fill missing events with estimates.

Usage: python3 android/helpers/chart_lan_transitions.py EVIDENCE --output OUTPUT
EVIDENCE is the lan-launch-preparation-public-shareware artifact directory.
"""

import argparse
import csv
import html
import json
import re
import statistics
from collections import defaultdict
from pathlib import Path


def read_jsonl(path):
    return [json.loads(line) for line in path.read_text(encoding="utf-8-sig").splitlines() if line.strip()]


def extract(root):
    manifest = json.loads((root / "experiment.json").read_text(encoding="utf-8-sig"))
    events = read_jsonl(root / "events.jsonl")
    groups = defaultdict(list)
    rows = []
    for event in events:
        groups[event["game"], event["sample"]].append(event)

    def row(game, sample, peer, phase, basis, start, end, source):
        if end < start:
            raise ValueError(f"Clock reversal in {source}")
        rows.append(
            dict(
                revision=manifest["revision"],
                game=game,
                sample=sample,
                peer=peer,
                phase=phase,
                basis=basis,
                start_ms=start,
                end_ms=end,
                duration_ms=round(end - start, 3),
                source=source,
            )
        )

    for (game, sample), group in groups.items():
        for peer in (manifest["host"], manifest["client"]):
            local = [e for e in group if e["peer"] == peer]
            pairs = [
                ("launcher", "launcher.request", "launcher.ready", "controller_upper_bound"),
                ("briefing1_dwell_and_harness", "briefing1.ready", "skip.request", "deliberate_and_harness"),
                ("first_level_entry", "skip.request", "level1.ready", "controller_upper_bound"),
                ("exit_to_scores", "exit.request", "scores.ready", "controller_upper_bound"),
                ("scores_dwell_and_harness", "scores.ready", "next.request", "deliberate_and_harness"),
                ("scores_to_level2", "next.request", "level2.arrived", "controller_upper_bound"),
                ("briefing2_dwell_and_harness", "briefing2.ready", "skip.request", "deliberate_and_harness"),
            ]
            for phase, start_name, end_name, basis in pairs:
                starts = [e for e in local if e["event"] == start_name]
                if not starts:
                    continue
                start = starts[0]["controller_ms"]
                ends = [e for e in local if e["event"] == end_name and e["controller_ms"] >= start]
                if ends:
                    row(game, sample, peer, phase, basis, start, ends[0]["controller_ms"], "events.jsonl")
            starts = [e for e in group if e["event"] == "lan_start_game.request"]
            ends = [e for e in local if e["event"] == "briefing1.ready"]
            if starts and ends:
                row(
                    game,
                    sample,
                    peer,
                    "start_to_briefing1",
                    "controller_upper_bound",
                    starts[0]["controller_ms"],
                    ends[0]["controller_ms"],
                    "events.jsonl",
                )
            arrived = [e for e in local if e["event"] == "level2.arrived"]
            ready = [e for e in local if e["event"] == "level2.ready"]
            if arrived and ready:
                skips = [
                    e
                    for e in local
                    if e["event"] == "skip.request" and e["controller_ms"] > arrived[0]["controller_ms"]
                ]
                advances = [e for e in local if e["event"] == "next.request"]
                inputs = skips or advances
                if inputs:
                    row(
                        game,
                        sample,
                        peer,
                        "second_level_entry",
                        "controller_upper_bound",
                        inputs[0]["controller_ms"],
                        ready[0]["controller_ms"],
                        "events.jsonl",
                    )
            for phase in ("skip1", "exit", "next", "skip2"):
                path = root / f"{game}-{sample}-{peer}-{phase}-automation_log.jsonl"
                if not path.exists():
                    continue
                log = read_jsonl(path)
                action_step = 2
                final_step = 5 if phase == "exit" else 3
                actions = [e for e in log if e["step"] == action_step and e["status"] == "start"]
                completions = [
                    e for e in log if e["step"] == final_step and e["action"] == "wait_for" and e["status"] == "done"
                ]
                if len(actions) == 1 and len(completions) == 1:
                    row(
                        game,
                        sample,
                        peer,
                        "next_action_to_level2_arrived" if phase == "next" else f"{phase}_action_to_ready",
                        "native_action_to_condition",
                        actions[0]["elapsed_ms"],
                        completions[0]["elapsed_ms"],
                        path.name,
                    )
                # Each step's start uses SDL_GetTicks local to this script run. A
                # completed wait is exact for the automation condition, not photons
                for entry in log:
                    if entry["action"] != "wait_for" or entry["status"] != "done":
                        continue
                    starts = [e for e in log if e["step"] == entry["step"] and e["status"] == "start"]
                    if len(starts) != 1:
                        raise ValueError(f"Ambiguous wait start in {path}")
                    row(
                        game,
                        sample,
                        peer,
                        f"{phase}_wait_step{entry['step']}",
                        "native_condition_wait",
                        starts[0]["elapsed_ms"],
                        entry["elapsed_ms"],
                        path.name,
                    )
    phase_pattern = re.compile(
        r"\[LAN-TIMING\] ms=(\d+) player=(\d+) level=(-?\d+) generation=(\d+) phase=(\d+) armed=(\d+)"
    )
    for path in sorted(root.glob("d?-*-emulator-*-logcat.txt")):
        game, sample, emulator, port, _ = path.name.split("-", 4)
        text = path.read_text(encoding="utf-8-sig")
        startup = {}
        lobby = {}
        lobby_markers = {
            "host_request": "MP_COMMAND: lan_host_lobby",
            "host_created": "Hosting LAN lobby ",
            "host_peer_joined": "Player joined: TimingJ ",
            "discover_request": "MP_COMMAND: lan_discover",
            "discovered": "recv: ANNOUNCE ",
            "join_request": "joinLobby: lobbyId=",
            "joined": "JOIN_ACK received for lobby ",
        }
        milestones = {
            "request": "MP_COMMAND: launch_game",
            "feedback_published": "Launcher game launch requested",
            "activity_displayed": "Displayed com.dxxredux.app/.MainActivity",
            "library_loaded": "Loaded native library:",
            "native_autonet_started": "auto_net: starting auto-",
            "host_selection_completed": "closing select_players",
            "briefing_protocol_ready": "phase=6 armed=1",
        }
        for line in text.splitlines():
            clock = re.match(r"\s*(\d+\.\d+)\s", line)
            if clock:
                for key, marker in lobby_markers.items():
                    if marker in line and key not in lobby:
                        lobby[key] = float(clock.group(1)) * 1000
                for key, marker in milestones.items():
                    if marker in line and key not in startup:
                        startup[key] = float(clock.group(1)) * 1000
        for label, start, end in (
            ("host_lobby_create", "host_request", "host_created"),
            ("host_lobby_to_peer_joined", "host_created", "host_peer_joined"),
            ("client_discovery", "discover_request", "discovered"),
            ("client_join_ack", "join_request", "joined"),
        ):
            if start in lobby and end in lobby:
                row(
                    game,
                    int(sample),
                    f"{emulator}-{port}",
                    label,
                    "device_local_lobby_milestone",
                    lobby[start],
                    lobby[end],
                    path.name,
                )
        if "request" in startup:
            for key, stamp in startup.items():
                if key != "request":
                    row(
                        game,
                        int(sample),
                        f"{emulator}-{port}",
                        f"launch_to_{key}",
                        "device_local_startup_milestone",
                        startup["request"],
                        stamp,
                        path.name,
                    )
        for line in text.splitlines():
            if "loadprof_v=1" not in line:
                continue
            fields = dict(re.findall(r"(\w+)=([^ ]+)", line))
            if fields.get("type") not in ("level_load", "level_init"):
                continue
            level = fields.get("level", fields.get("current"))
            for key, value in fields.items():
                if key.endswith("_us"):
                    row(
                        game,
                        int(sample),
                        f"{emulator}-{port}",
                        f"level{level}_{fields['type']}_{key[:-3]}",
                        "native_profile",
                        0,
                        int(value) / 1000,
                        path.name,
                    )
        previous = None
        for line in text.splitlines():
            match = phase_pattern.search(line)
            if not match:
                continue
            clock = re.match(r"\s*(\d+\.\d+)\s", line)
            if not clock:
                continue
            # timer_query in the message is frame-cached; logcat -v monotonic
            # records the actual logging instant on this one device
            tick = float(clock.group(1)) * 1000
            _, player, level, generation, phase, armed = map(int, match.groups())
            if previous:
                old_tick, old_level, old_gen, old_phase, old_armed = previous
                if generation == old_gen and level == old_level and old_armed and phase != old_phase:
                    basis = "presentation_dwell" if old_phase == 6 else "native_phase_residence"
                    row(
                        game,
                        int(sample),
                        f"{emulator}-{port}",
                        f"level{level}_gen{generation}_phase{old_phase}_to_{phase}",
                        basis,
                        old_tick,
                        tick,
                        path.name,
                    )
            previous = tick, level, generation, phase, armed
    return manifest, rows


def write_chart(rows, output):
    groups = defaultdict(list)
    for row in rows:
        if row["basis"] not in ("deliberate_and_harness", "presentation_dwell"):
            groups[row["game"], row["peer"], row["phase"], row["basis"]].append(row["duration_ms"])
    height = 110 + 30 * len(groups)
    maximum = max((max(values) for values in groups.values()), default=1) or 1
    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="1200" height="{height}" viewBox="0 0 1200 {height}">',
        '<rect width="100%" height="100%" fill="white"/>',
        '<g font-family="sans-serif" fill="#17233b">',
        '<text x="20" y="28" font-size="20">Observed LAN transitions: individual samples and mean</text>',
        '<text x="20" y="50" font-size="12">Blue = controller upper bound (ADB/poll included); green = device-local duration. Missing phases omitted.</text>',
    ]
    for index, ((game, peer, phase, basis), values) in enumerate(sorted(groups.items())):
        y = 85 + index * 30
        mean = sum(values) / len(values)
        color = "#386cb0" if basis == "controller_upper_bound" else "#207b54"
        label = html.escape(f"{game} {peer} {phase}")
        parts.append(f'<text x="20" y="{y + 5}" font-size="12">{label}</text>')
        parts.append(f'<rect x="460" y="{y - 8}" width="{mean / maximum * 600:.2f}" height="16" fill="{color}"/>')
        for value in values:
            parts.append(f'<circle cx="{460 + value / maximum * 600:.2f}" cy="{y}" r="3" fill="#111"/>')
        parts.append(f'<text x="1080" y="{y + 5}" font-size="12">{mean / 1000:.3f}s n={len(values)}</text>')
    parts.append("</g></svg>")
    output.write_text("\n".join(parts) + "\n", encoding="utf-8")


def comparison_groups(rows):
    groups = defaultdict(list)
    phases = defaultdict(list)
    for row in rows:
        key = row["game"], row["peer"]
        if row["basis"] in ("native_action_to_condition", "device_local_startup_milestone"):
            groups[(*key, row["phase"])].append(float(row["duration_ms"]))
        if row["basis"] == "native_phase_residence":
            match = re.fullmatch(r"level(\d+)_gen(\d+)_phase(\d+)_to_(\d+)", row["phase"])
            if match:
                level, generation, start, end = map(int, match.groups())
                phases[(*key, int(row["sample"]), level, generation)].append(
                    (start, end, float(row["start_ms"]), float(row["end_ms"]))
                )
    # Compare each operation separately. Generations are time-derived and cannot
    # be matched by numeric value across runs; their local start order is stable.
    operations = defaultdict(list)
    for (game, peer, sample, level, generation), values in phases.items():
        closing = sorted(v for v in values if v[0] in (7, 8, 9))
        duration = None
        if [(v[0], v[1]) for v in closing] == [(7, 8), (8, 9), (9, 0)]:
            duration = closing[-1][3] - closing[0][2]
        # Preserve the operation ordinal even if a diagnostic edge is missing.
        operations[game, peer, sample, level].append((min(v[2] for v in values), duration))
    for (game, peer, sample, level), values in operations.items():
        for index, (start, duration) in enumerate(sorted(values), 1):
            if duration is not None:
                groups[game, peer, f"level{level}_handshake{index}_closing_to_settled"].append(duration)
    return groups


def write_comparison(before_rows, after_rows, output):
    before, after = comparison_groups(before_rows), comparison_groups(after_rows)
    summary = []
    for key in sorted(before.keys() | after.keys()):
        old, new = before.get(key, []), after.get(key, [])
        row = dict(zip(("game", "peer", "metric"), key))
        for label, values in (("before", old), ("after", new)):
            row[f"{label}_n"] = len(values)
            for stat, function in (
                ("mean", statistics.mean),
                ("median", statistics.median),
                ("min", min),
                ("max", max),
            ):
                row[f"{label}_{stat}_ms"] = round(function(values), 3) if values else ""
            row[f"{label}_samples_ms"] = ";".join(str(round(v, 3)) for v in values)
        row["mean_delta_ms"] = round(statistics.mean(new) - statistics.mean(old), 3) if old and new else ""
        summary.append(row)
    with (output / "comparison.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(summary[0]), lineterminator="\n")
        writer.writeheader()
        writer.writerows(summary)
    selected = [r for r in summary if "handshake" in r["metric"] or "_action_to_" in r["metric"]]
    height = 125 + 45 * len(selected)
    maximum = max(float(r[f"{label}_max_ms"] or 0) for r in selected for label in ("before", "after")) or 1
    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="1250" height="{height}">',
        '<rect width="100%" height="100%" fill="white"/><g font-family="sans-serif" fill="#17233b">',
        '<text x="20" y="28" font-size="20">LAN co-op: matched before / after observations</text>',
        '<text x="20" y="51" font-size="13">Gray: before. Green: immediate phase publication. Bars = means; dots = samples; target n=3 per row/run, actual n labeled.</text>',
        '<text x="20" y="72" font-size="13">Device-local native action conditions or closing→settled log timestamps. No controller polling bounds in this chart.</text>',
        '<text x="20" y="91" font-size="13">Small sequential emulator samples: variation is not proof of a speedup. Host=5554, client=5556.</text>',
    ]
    for index, row in enumerate(selected):
        y = 120 + 45 * index
        label = html.escape(f"{row['game']} {row['peer']} {row['metric']}")
        parts.append(f'<text x="20" y="{y + 5}" font-size="11">{label}</text>')
        for offset, variant, color in ((0, "before", "#8290a3"), (16, "after", "#207b54")):
            if not row[f"{variant}_n"]:
                continue
            mean = row[f"{variant}_mean_ms"]
            parts.append(
                f'<rect x="505" y="{y + offset - 6}" width="{mean / maximum * 570:.2f}" height="11" fill="{color}"/>'
            )
            for value in row[f"{variant}_samples_ms"].split(";"):
                parts.append(
                    f'<circle cx="{505 + float(value) / maximum * 570:.2f}" cy="{y + offset}" r="2.5" fill="#111"/>'
                )
            parts.append(
                f'<text x="1090" y="{y + offset + 4}" font-size="11">{mean / 1000:.3f}s n={row[f"{variant}_n"]}</text>'
            )
    parts.append("</g></svg>")
    (output / "comparison.svg").write_text("\n".join(parts) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("evidence", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--compare", type=Path, help="Before timings.csv; compare against this evidence run")
    args = parser.parse_args()
    manifest, rows = extract(args.evidence)
    events = read_jsonl(args.evidence / "events.jsonl")
    passed = {(e["game"], e["sample"]) for e in events if e["event"] == "sample.pass"}
    expected = {(game, sample) for game in ("d1", "d2") for sample in range(1, manifest["samples"] + 1)}
    failed_scripts = [
        p.name
        for p in args.evidence.glob("*-automation_result.json")
        if json.loads(p.read_text(encoding="utf-8-sig")).get("result") != "PASS"
    ]
    if passed != expected or failed_scripts:
        raise SystemExit(
            f"Incomplete/failed experiment; retain raw evidence, not a success chart: passed={sorted(passed)}, failed_scripts={failed_scripts}"
        )
    args.output.mkdir(parents=True, exist_ok=True)
    if not rows:
        raise SystemExit("No observed intervals; refusing to create a timing chart")
    with (args.output / "timings.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]), lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
    (args.output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    write_chart(rows, args.output / "timings.svg")
    overview = [r for r in rows if r["basis"] == "native_action_to_condition" or r["phase"] == "start_to_briefing1"]
    if overview:
        write_chart(overview, args.output / "overview.svg")
    if args.compare:
        with args.compare.open(newline="", encoding="utf-8") as stream:
            write_comparison(list(csv.DictReader(stream)), rows, args.output)
    print(f"Wrote {len(rows)} observed intervals to {args.output}")


if __name__ == "__main__":
    main()
