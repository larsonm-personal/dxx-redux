#!/usr/bin/env python3
"""Bounded desktop filtering-change/render-capability proof, not exhaustive visual QA."""

import argparse
import json
import math
from pathlib import Path
import time

from host_config_campaign import HostSession, ProbeFailure, freeze_binary, write_json

ERROR_FIELDS = ("previous_gl_error", "buffer_gl_error", "read_gl_error", "restore_gl_error")
EXPECTED_SIZE = (640, 480)


def validate_capture(metadata, image, raw_bytes):
    """Reject failed GL calls, truncated RGB data and spatially empty frames."""
    if any(metadata.get(field) != 0 for field in ERROR_FIELDS):
        raise ProbeFailure(f"render GL diagnostics were not all zero: {metadata}")
    if metadata.get("written") is not True:
        raise ProbeFailure("render capture was not written successfully")
    if (metadata.get("width"), metadata.get("height")) != EXPECTED_SIZE:
        raise ProbeFailure("render metadata dimensions differ from expected 640x480")
    if image.size != EXPECTED_SIZE or image.mode != "RGB":
        raise ProbeFailure("render image dimensions or RGB mode invalid")
    pixels = image.tobytes()
    if raw_bytes != EXPECTED_SIZE[0] * EXPECTED_SIZE[1] * 3 or len(pixels) != raw_bytes:
        raise ProbeFailure("render RGB byte count invalid")
    colors = len(set(zip(pixels[0::3], pixels[1::3], pixels[2::3])))
    stddev = []
    for channel in range(3):
        values = pixels[channel::3]
        mean = sum(values) / len(values)
        variance = sum(value * value for value in values) / len(values) - mean * mean
        stddev.append(math.sqrt(max(0, variance)))
    stride = EXPECTED_SIZE[0] * 3
    tile_means = [
        sum(sum(pixels[row * stride + x * 3 : row * stride + (x + 80) * 3]) for row in range(y, y + 80)) / (80 * 80 * 3)
        for y in range(0, 480, 80)
        for x in range(0, 640, 80)
    ]
    tile_range = max(tile_means) - min(tile_means)
    if colors < 32 or min(stddev) < 5 or tile_range < 5:
        raise ProbeFailure("render image lacks nontrivial spatial content")
    return {"colors": colors, "rgb_stddev": stddev, "tile_mean_range": tile_range}


def validate_difference(baseline, changed):
    before, after = baseline.tobytes(), changed.tobytes()
    expected_bytes = EXPECTED_SIZE[0] * EXPECTED_SIZE[1] * 3
    if (
        baseline.size != EXPECTED_SIZE
        or changed.size != EXPECTED_SIZE
        or baseline.mode != "RGB"
        or changed.mode != "RGB"
        or len(before) != expected_bytes
        or len(after) != expected_bytes
    ):
        raise ProbeFailure("render difference requires complete 640x480 RGB frames")
    pixels = bytes(abs(a - b) for a, b in zip(before, after))
    changed_pixels = sum(any(pixels[i : i + 3]) for i in range(0, len(pixels), 3))
    mean_delta = sum(pixels) / len(pixels)
    if changed_pixels < EXPECTED_SIZE[0] * EXPECTED_SIZE[1] // 100 or mean_delta < 0.5:
        raise ProbeFailure("filter change did not produce a meaningful rendered difference")
    return {"changed_pixels": changed_pixels, "mean_absolute_channel_delta": mean_delta}


def capture(session, name):
    from PIL import Image

    time.sleep(0.5)
    (session.probe / "capture.json").unlink(missing_ok=True)
    (session.probe / "capture.ppm").unlink(missing_ok=True)
    (session.probe / "capture.request").write_text("capture\n")
    until = min(session.deadline, time.monotonic() + 5)
    while time.monotonic() < until:
        session.health()
        try:
            metadata = json.loads((session.probe / "capture.json").read_text())
            break
        except (OSError, ValueError):
            time.sleep(0.02)
    else:
        raise ProbeFailure("render capture timed out")
    payload = (session.probe / "capture.ppm").read_bytes()
    header = b"P6\n640 480\n255\n"
    if not payload.startswith(header):
        raise ProbeFailure("render PPM header invalid")
    with Image.open(session.probe / "capture.ppm") as opened:
        image = opened.copy()
    statistics = validate_capture(metadata, image, len(payload) - len(header))
    image.save(session.directory / f"{name}.png")
    write_json(session.directory / f"{name}.json", {**metadata, **statistics})
    return image, {**metadata, **statistics}


def main():
    from PIL import ImageChops

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True)
    parser.add_argument("--data", required=True)
    parser.add_argument("--game", choices=("d1", "d2"), required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    output = Path(args.output).resolve()
    output.mkdir(parents=True, exist_ok=False)
    executable = freeze_binary(args.executable, output)
    session = HostSession(
        executable,
        args.data,
        output,
        args.game,
        timeout=90,
        config={"ResolutionX": 640, "ResolutionY": 480, "WindowMode": 1, "MusicType": 0},
    )
    report = {
        "scope": "filtering-change/render-capability proof; not exhaustive visual correctness",
        "game": args.game,
        "status": "fail",
    }
    try:
        session.start()
        session.enter_options()
        session.open_menu("graphics options")
        session.set_item("None (Classical)", 1, exact=True)
        session.back_to_options()
        session.assert_config({"TexFilt": 0})
        baseline, report["baseline"] = capture(session, "baseline-nearest")
        session.open_menu("graphics options")
        session.set_item("Trilinear", 1, exact=True)
        session.back_to_options()
        session.assert_config({"TexFilt": 2})
        changed, report["changed"] = capture(session, "changed-trilinear")
        report["difference"] = validate_difference(baseline, changed)
        ImageChops.difference(baseline, changed).save(output / "difference.png")
        session.key("escape")
        session.close()
        report["status"] = "pass"
    except Exception as error:
        report["error"] = str(error)
    finally:
        session.stop()
        write_json(output / "render-report.json", report)
    print(json.dumps(report, sort_keys=True))
    return 0 if report["status"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
