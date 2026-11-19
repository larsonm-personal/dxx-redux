"""Replay one private XL trace on a specified Android device and compare the selected swap."""

import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path

from PIL import Image, ImageChops, ImageStat


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def executable_abi(path):
    with path.open("rb") as stream:
        header = stream.read(20)
    if len(header) != 20 or header[:6] != b"\x7fELF\x02\x01":
        raise ValueError(f"Expected a 64-bit little-endian Android ELF in {path}")
    machine = int.from_bytes(header[18:20], "little")
    return {62: "x86_64", 183: "arm64-v8a"}[machine]


def compare(actual, expected):
    if actual.size != expected.size:
        raise ValueError(f"Frame sizes differ: {actual.size} and {expected.size}")
    r, g, b = ImageChops.difference(actual, expected).split()
    histogram = ImageChops.lighter(r, ImageChops.lighter(g, b)).histogram()
    return {
        "dimensions": list(actual.size),
        "android_rgb_sha256": hashlib.sha256(actual.tobytes()).hexdigest(),
        "desktop_rgb_sha256": hashlib.sha256(expected.tobytes()).hexdigest(),
        "pixels": sum(histogram),
        "pixels_identical": histogram[0],
        "pixels_within_8_per_channel": sum(histogram[:9]),
        "mean_absolute_channel_error": [ImageStat.Stat(channel).mean[0] for channel in (r, g, b)],
    }


def replay(args):
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    adb = [str(args.adb), "-s", args.serial]

    def shell(command):
        return subprocess.check_output([*adb, "shell", command], text=True).strip()

    remote = shell("mktemp -d /data/local/tmp/xl-trace.XXXXXX")
    if not re.fullmatch(r"/data/local/tmp/xl-trace\.[A-Za-z0-9]+", remote):
        raise ValueError("Android did not return an isolated trace directory")
    binaries = {}
    for name in ("xl_trace_replay", "libGL4ES.so", "xl_known_draw"):
        path = args.build / name
        subprocess.run([*adb, "push", str(path), f"{remote}/{name}"], check=True)
        binaries[name] = digest(path)
    trace_sha = digest(args.trace)
    if args.device_trace:
        trace = args.device_trace
        if not re.fullmatch(r"/data/local/tmp/[-A-Za-z0-9_./]+", trace) or ".." in trace.split("/"):
            raise ValueError("Expected a file under /data/local/tmp")
    else:
        trace = f"{remote}/game.trace"
        subprocess.run([*adb, "push", str(args.trace), trace], check=True)
    if shell(f"sha256sum {trace}").split()[0] != trace_sha:
        raise ValueError("Host and device traces differ")
    shell(f"chmod 755 {remote}/xl_trace_replay {remote}/xl_known_draw")
    env = f"cd {remote} && LD_LIBRARY_PATH=. LIBGL_ES=2"
    with (out / "control.log").open("wb") as log:
        subprocess.run(
            [*adb, "shell", f"{env} ./xl_known_draw"], stdout=log, stderr=subprocess.STDOUT, check=True, timeout=60
        )
    with (out / "replay.log").open("wb") as log:
        subprocess.run(
            [*adb, "shell", f"{env} XL_CAPTURE_CALL={args.call} ./xl_trace_replay --headless {trace}"],
            stdout=log,
            stderr=subprocess.STDOUT,
            check=True,
            timeout=900,
        )
    frame = out / "android.ppm"
    subprocess.run([*adb, "pull", f"{remote}/frame.ppm", str(frame)], check=True)
    with Image.open(frame) as actual, Image.open(args.desktop) as expected:
        metrics = compare(actual.convert("RGB"), expected.convert("RGB"))
    receipt = {
        "trace_sha256": trace_sha,
        "desktop_file_sha256": digest(args.desktop),
        "android_file_sha256": digest(frame),
        "swap_call": args.call,
        "binaries": binaries,
        "android_api": shell("getprop ro.build.version.sdk"),
        "android_abi": shell("getprop ro.product.cpu.abi"),
        "executable_abi": executable_abi(args.build / "xl_trace_replay"),
        "native_bridge": shell("getprop ro.dalvik.vm.native.bridge"),
        "known_draw_passed": True,
        "comparison": metrics,
    }
    (out / "comparison.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for field in ("adb", "build", "trace", "desktop", "output"):
        parser.add_argument("--" + field, type=Path, required=True)
    parser.add_argument("--serial", required=True)
    parser.add_argument("--call", type=int, required=True)
    parser.add_argument("--device-trace", help="Reuse an existing device copy after SHA-256 verification")
    replay(parser.parse_args())
