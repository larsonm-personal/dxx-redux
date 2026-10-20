#!/usr/bin/env python3
"""Extract observed, device-local startup stages and optional packet trace.

Input: recovered lan-launch-preparation-public-shareware artifact directory.
Output: CSV with source line references and standalone SVG/PNG timeline.
No cross-device clock subtraction; profile boundaries use recorded local durations.
"""

import argparse
import csv
import json
import io
import re
import statistics
from pathlib import Path


def extract(root):
    revision = json.loads((root / "experiment.json").read_text(encoding="utf-8-sig"))["revision"]
    rows, trace, checkpoints, assets, auth = [], [], [], [], []
    for path in sorted(root.glob("d?-*-emulator-*-logcat.txt")):
        game, sample, _, port, _ = path.name.split("-", 4)
        lines = path.read_text(encoding="utf-8-sig").splitlines()
        game_pids = {line.split()[1] for line in lines if "auto_net: starting auto-" in line}
        if len(game_pids) != 1:
            raise ValueError(f"Expected one native game process in {path.name}")
        game_pid = next(iter(game_pids))
        points = {}
        profile = None
        previous_checkpoint = None
        markers = {
            "launch": "MP_COMMAND: launch_game",
            "activity_intent": "START u0 {cmp=com.dxxredux.app/.MainActivity",
            "library_loaded": "Loaded native library:",
            "native_entry": "Starting D2X-Redux game engine...",
            "activity_created": "DXX-Lifecycle: create activity=",
            "auto_net": "auto_net: starting auto-",
            "briefing_ready": "phase=6 armed=1",
        }
        for number, line in enumerate(lines, 1):
            clock = re.match(r"\s*(\d+\.\d+)\s", line)
            if not clock:
                continue
            stamp = float(clock[1])
            is_game = line.split()[1] == game_pid
            for key, marker in markers.items():
                if marker in line and key not in points:
                    points[key] = (stamp, number)
            if is_game and "CHECKPOINT:" in line and "launch" in points:
                checkpoint = line.split("CHECKPOINT:", 1)[1].strip()
                if previous_checkpoint:
                    before, title, source_line = previous_checkpoint
                    checkpoints.append(
                        dict(
                            revision=revision,
                            game=game,
                            sample=sample,
                            peer=port,
                            start_event=title,
                            end_event=checkpoint,
                            start_s=before,
                            end_s=stamp,
                            duration_s=round(stamp - before, 6),
                            source_start=f"{path.name}:{source_line}",
                            source_end=f"{path.name}:{number}",
                        )
                    )
                previous_checkpoint = (stamp, checkpoint, number)
            if is_game and "[STARTUP-AUTH]" in line and "launch" in points:
                fields = dict(re.findall(r"(\w+)=([^ ]+)", line.split("[STARTUP-AUTH]", 1)[1]))
                auth.append(
                    dict(
                        revision=revision,
                        game=game,
                        sample=sample,
                        peer=port,
                        timestamp_s=stamp,
                        source=f"{path.name}:{number}",
                        **fields,
                    )
                )
            if is_game and "[STARTUP-ASSET]" in line and "launch" in points:
                fields = dict(re.findall(r"(\w+)=([^ ]+)", line.split("[STARTUP-ASSET]", 1)[1]))
                assets.append(
                    dict(
                        revision=revision,
                        game=game,
                        sample=sample,
                        peer=port,
                        timestamp_s=stamp,
                        source=f"{path.name}:{number}",
                        **fields,
                    )
                )
            if is_game and "type=level_init" in line and "requested=1 " in line and profile is None:
                fields = {k: int(v) / 1e6 for k, v in re.findall(r"(\w+)_us=(\d+)", line)}
                start = stamp - fields["total"]
                points["level_begin"] = (start, number)
                points["sync_begin"] = (start + fields["pre_load"] + fields["load"], number)
                points["sync_end"] = (points["sync_begin"][0] + fields["network_sync"], number)
                profile = fields
            if is_game and "[STARTUP-SYNC]" in line:
                trace.append(
                    dict(
                        revision=revision,
                        game=game,
                        sample=sample,
                        peer=port,
                        timestamp_s=stamp,
                        source=f"{path.name}:{number}",
                        event=line.split("[STARTUP-SYNC]", 1)[1].strip(),
                    )
                )
        stages = [
            ("launcher preparation", "launch", "activity_intent"),
            ("Activity/process startup", "activity_intent", "activity_created"),
            ("Activity setup + library", "activity_created", "library_loaded"),
            ("library to JNI entry", "library_loaded", "native_entry"),
            ("native initialization", "native_entry", "auto_net"),
            ("LAN admission / selection", "auto_net", "level_begin"),
            ("level loading", "level_begin", "sync_begin"),
            ("native synchronization", "sync_begin", "sync_end"),
            ("post-sync to briefing ready", "sync_end", "briefing_ready"),
        ]
        required = {point for _, first, last in stages for point in (first, last)}
        if missing := required.difference(points):
            raise ValueError(f"Incomplete startup evidence: {path.name}: {sorted(missing)}")
        for stage, first, last in stages:
            if first not in points or last not in points or "launch" not in points:
                continue
            start, a = points[first]
            end, b = points[last]
            if end < start:
                raise ValueError(f"Clock reversal: {path} {stage}")
            rows.append(
                dict(
                    revision=revision,
                    game=game,
                    sample=sample,
                    peer=port,
                    stage=stage,
                    start_s=round(start, 6),
                    end_s=round(end, 6),
                    duration_s=round(end - start, 6),
                    offset_s=round(start - points["launch"][0], 6),
                    source_start=f"{path.name}:{a}",
                    source_end=f"{path.name}:{b}",
                )
            )
    return rows, trace, checkpoints, assets, auth


def network_waits(trace):
    rows = []
    groups = list(dict.fromkeys((r["game"], r["sample"], r["peer"]) for r in trace if r["peer"] == "5554"))
    for game, sample, peer in groups:
        events = [r for r in trace if (r["game"], r["sample"], r["peer"]) == (game, sample, peer)]
        waiting = False
        points = {}
        drops = {"UPID_REQUEST": 0, "UPID_RECONNECT_PROOF": 0}
        for row in events:
            fields = dict(re.findall(r"(\w+)=([^ ]+)", row["event"]))
            event = fields.get("event")
            if "flush_drop" in row["event"] and fields.get("name") in drops:
                drops[fields["name"]] += int(fields["count"])
            if event == "wait_requests_flush":
                waiting = True
            if not waiting:
                continue
            if event in ("flush_end", "request_received", "request_ready_accepted", "sync_send_begin"):
                points.setdefault(event, row)
        if not points:
            continue
        if len(points) != 4:
            raise ValueError(f"Incomplete sync trace for {game} {sample}")
        flush = points["flush_end"]
        request = points["request_received"]
        accepted = points["request_ready_accepted"]
        sync = points["sync_send_begin"]
        rows.append(
            dict(
                revision=flush["revision"],
                game=game,
                sample=sample,
                peer=peer,
                idle_after_flush_s=round(request["timestamp_s"] - flush["timestamp_s"], 6),
                authentication_s=round(accepted["timestamp_s"] - request["timestamp_s"], 6),
                flush_to_sync_s=round(sync["timestamp_s"] - flush["timestamp_s"], 6),
                dropped_requests=drops["UPID_REQUEST"],
                dropped_proofs=drops["UPID_RECONNECT_PROOF"],
                flush_source=flush["source"],
                request_source=request["source"],
                accepted_source=accepted["source"],
                sync_source=sync["source"],
            )
        )
    return rows


def write_if_changed(path, content):
    if not path.exists() or path.read_bytes() != content:
        path.write_bytes(content)


def save_figure(fig, output, name):
    for extension in ("svg", "png"):
        buffer = io.BytesIO()
        metadata = {"Date": None} if extension == "svg" else {"Software": "dxx-redux startup chart"}
        fig.savefig(buffer, format=extension, dpi=160, bbox_inches="tight", metadata=metadata)
        write_if_changed(output / f"{name}.{extension}", buffer.getvalue())


def compare(control, candidate, output, plt):
    def values(root, game, metric):
        filename = "network_waits" if metric == "flush_to_sync_s" else "stages"
        with (root / f"{filename}.csv").open() as file:
            rows = [r for r in csv.DictReader(file) if r["game"] == game and r["peer"] == "5554"]
        if metric == "flush_to_sync_s":
            return [float(r[metric]) for r in rows]
        if metric == "network_sync":
            return [float(r["duration_s"]) for r in rows if r["stage"] == "native synchronization"]
        return [
            sum(float(r["duration_s"]) for r in rows if r["sample"] == sample)
            for sample in dict.fromkeys(r["sample"] for r in rows)
        ]

    metrics = [
        ("flush_to_sync_s", "Host final flush to SYNC"),
        ("network_sync", "Host native synchronization"),
        ("launch_ready", "Host launch to briefing protocol ready"),
    ]
    fig, axes = plt.subplots(1, 3, figsize=(13, 4))
    summary = []
    for ax, (metric, title) in zip(axes, metrics):
        for index, game in enumerate(("d1", "d2")):
            before = values(control, game, metric)
            after = values(candidate, game, metric)
            summary.append(
                dict(
                    game=game,
                    metric=metric,
                    control_n=len(before),
                    candidate_n=len(after),
                    control_mean_s=statistics.mean(before),
                    candidate_mean_s=statistics.mean(after),
                    difference_s=statistics.mean(after) - statistics.mean(before),
                    control_min_s=min(before),
                    control_max_s=max(before),
                    candidate_min_s=min(after),
                    candidate_max_s=max(after),
                )
            )
            for offset, data, color, label in (
                (-0.18, before, "#1f77b4", "Control"),
                (0.18, after, "#ff7f0e", "Candidate"),
            ):
                ax.bar(
                    index + offset, statistics.mean(data), width=0.32, color=color, label=label if index == 0 else None
                )
                ax.scatter([index + offset] * len(data), data, color="black", s=15, zorder=4)
        ax.set_xticks([0, 1], ["D1", "D2"])
        ax.set_title(title, fontsize=10)
        ax.set_ylabel("Seconds")
        ax.grid(axis="y", alpha=0.2)
    axes[0].legend(fontsize=8)
    fig.suptitle(
        "Matched emulator experiment: means and individual samples\nSame probes and harness; configuration-reset launches; n=3 per game"
    )
    fig.tight_layout()
    save_figure(fig, output, "comparison")
    buffer = io.StringIO(newline="")
    writer = csv.DictWriter(buffer, fieldnames=list(summary[0]), lineterminator="\n")
    writer.writeheader()
    writer.writerows(summary)
    write_if_changed(output / "comparison.csv", buffer.getvalue().encode())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("evidence", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--compare", type=Path, help="Previously generated matched control result directory")
    args = parser.parse_args()
    rows, trace, checkpoints, assets, auth = extract(args.evidence)
    args.output.mkdir(parents=True, exist_ok=True)
    for name, data in [
        ("stages", rows),
        ("packets", trace),
        ("checkpoints", checkpoints),
        ("assets", assets),
        ("auth", auth),
        ("network_waits", network_waits(trace)),
    ]:
        if data:
            f = io.StringIO(newline="")
            writer = csv.DictWriter(f, fieldnames=list(data[0]), lineterminator="\n")
            writer.writeheader()
            writer.writerows(data)
            write_if_changed(args.output / f"{name}.csv", f.getvalue().encode())
    import matplotlib

    matplotlib.use("Agg")
    matplotlib.rcParams["svg.hashsalt"] = "dxx-redux-startup-v1"
    import matplotlib.pyplot as plt

    labels = list(dict.fromkeys((r["game"], r["sample"], r["peer"]) for r in rows))
    stages = list(dict.fromkeys(r["stage"] for r in rows))
    fig, ax = plt.subplots(figsize=(12, 7))
    for stage, color in zip(stages, plt.get_cmap("tab10").colors):
        selected = [r for r in rows if r["stage"] == stage]
        ax.barh(
            [labels.index((r["game"], r["sample"], r["peer"])) for r in selected],
            [r["duration_s"] for r in selected],
            left=[r["offset_s"] for r in selected],
            color=color,
            label=stage,
        )
    ax.set_yticks(
        range(len(labels)), [f"{g.upper()} sample {s} {'host' if p == '5554' else 'client'}" for g, s, p in labels]
    )
    ax.invert_yaxis()
    ax.set_xlabel("Seconds since this device received launch_game")
    ax.set_title(
        "Observed startup critical path on each device\nAll samples reset configs/process; sample 1 clears app data; 2-3 retain assets/pilots"
    )
    ax.legend(loc="upper left", bbox_to_anchor=(0, -0.13), ncol=3, fontsize=8)
    ax.grid(axis="x", alpha=0.2)
    fig.tight_layout()
    save_figure(fig, args.output, "startup")
    if args.compare:
        compare(args.compare, args.output, args.output, plt)
    print(f"{len(rows)} stages; {len(trace)} packet markers")


if __name__ == "__main__":
    main()
