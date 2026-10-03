"""Compare Android FOV recordings with host replay results without changing evidence."""

import argparse
import json
from pathlib import Path


def records(path):
    return [json.loads(line) for line in path.read_text().splitlines() if line.strip()]


def source_path(value, repo):
    value = value.replace("\\", "/")
    prefix = repo.resolve().as_posix() + "/"
    if value.lower().startswith(prefix.lower()):
        return value[len(prefix) :]
    return value


def verify(directory, repo):
    recording = records(directory / "fov_cpu_compat.dximdemo")
    actual = json.loads((directory / "replay-result.json").read_text())
    assert recording[-1]["type"] == "result", "Recording has no final result"
    assert recording[-1]["result"] == actual, "Replay final state differs"
    frames = [row for row in recording if row["type"] == "frame"]
    assert any(row["input"].get("s", {}).get("f1s") for row in frames), "Primary fire was not recorded"
    assert any(row["input"].get("s", {}).get("x") for row in frames), "Movement was not recorded"
    expected_rng = records(directory / "fov_cpu_compat.dximdemo.rngtrace.jsonl")
    actual_rng = records(directory / "replay-rng.jsonl")
    assert not expected_rng[0]["truncated"] and not actual_rng[0]["truncated"]
    assert len(expected_rng) == len(actual_rng), "RNG event counts differ"
    for index, (expected, observed) in enumerate(zip(expected_rng, actual_rng)):
        for event in (expected, observed):
            if "file" in event:
                event["file"] = source_path(event["file"], repo)
        assert expected == observed, f"RNG event {index} differs"
    summary = {
        "game": recording[0]["game"],
        "input_frames": actual["frame_count"],
        "final_state_equal": True,
        "rng_events_equal": len(expected_rng) - 1,
        "rng_source_paths": "checkout prefix removed for comparison only",
    }
    classic = directory / "classic.jsonl"
    if classic.exists():
        decoded = records(classic)
        frames = [row for row in decoded if row["type"] == "frame"]
        assert decoded[-1]["type"] == "result", "Classic decode incomplete"
        assert not decoded[-1]["truncated"], "Classic demo truncated"
        assert len(frames) == decoded[-1]["frames_decoded"] > 0
        assert [frame["f"] for frame in frames] == list(range(len(frames)))
        assert any(frame["object_count"] > 1 for frame in frames), "No non-viewer objects recorded"
        summary["classic_frames_decoded"] = len(frames)
        summary["classic_max_objects"] = max(frame["object_count"] for frame in frames)
    return summary


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[2])
    args = parser.parse_args()
    print(json.dumps(verify(args.directory, args.repo), indent=2))
