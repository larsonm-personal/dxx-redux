#!/usr/bin/env python3
"""Extract observed, device-local startup stages and optional packet trace.

Input: recovered lan-launch-preparation-public-shareware artifact directory.
Output: CSV with source line references and standalone SVG/PNG timeline.
No cross-device clock subtraction; profile boundaries use recorded local durations.
"""

import argparse
import csv
import json
import re
from pathlib import Path


def extract(root):
    revision = json.loads((root / "experiment.json").read_text(encoding="utf-8-sig"))["revision"]
    rows, trace, checkpoints, assets = [], [], [], []
    for path in sorted(root.glob("d?-*-emulator-*-logcat.txt")):
        game, sample, _, port, _ = path.name.split("-", 4)
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
        for number, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), 1):
            clock = re.match(r"\s*(\d+\.\d+)\s", line)
            if not clock:
                continue
            stamp = float(clock[1])
            for key, marker in markers.items():
                if marker in line and key not in points:
                    points[key] = (stamp, number)
            if "CHECKPOINT:" in line and "launch" in points:
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
            if "[STARTUP-ASSET]" in line and "launch" in points:
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
            if "type=level_init" in line and "requested=1 " in line and profile is None:
                fields = {k: int(v) / 1e6 for k, v in re.findall(r"(\w+)_us=(\d+)", line)}
                start = stamp - fields["total"]
                points["level_begin"] = (start, number)
                points["sync_begin"] = (start + fields["pre_load"] + fields["load"], number)
                points["sync_end"] = (points["sync_begin"][0] + fields["network_sync"], number)
                profile = fields
            if "[STARTUP-SYNC]" in line:
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
    return rows, trace, checkpoints, assets


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("evidence", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    rows, trace, checkpoints, assets = extract(args.evidence)
    args.output.mkdir(parents=True, exist_ok=True)
    for name, data in [("stages", rows), ("packets", trace), ("checkpoints", checkpoints), ("assets", assets)]:
        if data:
            with (args.output / f"{name}.csv").open("w", newline="") as f:
                writer = csv.DictWriter(f, fieldnames=list(data[0]))
                writer.writeheader()
                writer.writerows(data)
    import matplotlib

    matplotlib.use("Agg")
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
        "Observed startup critical path on each device\nSample 1: cleared app data; samples 2-3: retained data, restarted process"
    )
    ax.legend(loc="upper left", bbox_to_anchor=(0, -0.13), ncol=3, fontsize=8)
    ax.grid(axis="x", alpha=0.2)
    fig.tight_layout()
    for extension in ("svg", "png"):
        fig.savefig(args.output / f"startup.{extension}", dpi=160, bbox_inches="tight")
    print(f"{len(rows)} stages; {len(trace)} packet markers")


if __name__ == "__main__":
    main()
