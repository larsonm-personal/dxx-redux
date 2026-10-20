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
                        f"{phase}_action_to_ready",
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
        '<text x="20" y="50" font-size="12">Blue = controller upper bound (ADB/poll included); green = native local duration. Missing phases omitted.</text>',
    ]
    for index, ((game, peer, phase, basis), values) in enumerate(sorted(groups.items())):
        y = 85 + index * 30
        mean = sum(values) / len(values)
        color = "#207b54" if basis.startswith("native_") else "#386cb0"
        label = html.escape(f"{game} {peer} {phase}")
        parts.append(f'<text x="20" y="{y + 5}" font-size="12">{label}</text>')
        parts.append(f'<rect x="460" y="{y - 8}" width="{mean / maximum * 600:.2f}" height="16" fill="{color}"/>')
        for value in values:
            parts.append(f'<circle cx="{460 + value / maximum * 600:.2f}" cy="{y}" r="3" fill="#111"/>')
        parts.append(f'<text x="1080" y="{y + 5}" font-size="12">{mean / 1000:.3f}s n={len(values)}</text>')
    parts.append("</g></svg>")
    output.write_text("\n".join(parts) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("evidence", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    manifest, rows = extract(args.evidence)
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
    print(f"Wrote {len(rows)} observed intervals to {args.output}")


if __name__ == "__main__":
    main()
