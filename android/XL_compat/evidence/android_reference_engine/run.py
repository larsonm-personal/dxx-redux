"""Run an original XL Android reference session with already provisioned private data."""

import argparse
import hashlib
import json
import re
import shlex
import subprocess
import time
from pathlib import Path, PurePosixPath

from PIL import Image

SCENES = {
    "standoff": ("darkflamewolf", "vignette-xl.mn2", "standoff.rl2"),
    "pumo": ("single", "pmines.mn2", "pm-hub.rl2"),
}


def digest(path):
    with path.open("rb") as fp:
        return hashlib.file_digest(fp, "sha256").hexdigest()


def measure(a):
    prefix = [str(a.adb), "-s", a.serial]

    def shell(*args):
        return subprocess.check_output([*prefix, "shell", shlex.join(map(str, args))], text=True).strip()

    def transfer(direction, source, target):
        subprocess.run([*prefix, direction, str(source), str(target)], check=True)

    def device_sha(path):
        value = shell("sha256sum", path).split()[0]
        if not re.fullmatch("[0-9a-f]{64}", value):
            raise ValueError(f"Invalid digest from device for {path}")
        return value

    root = PurePosixPath(a.device_root)
    if not root.is_relative_to("/data/local/tmp") or root == PurePosixPath("/data/local/tmp") or ".." in root.parts:
        raise ValueError("Supply the provisioned test directory under /data/local/tmp")
    folder, descriptor, member = SCENES[a.scene]
    a.output.mkdir(parents=True, exist_ok=False)
    remote = PurePosixPath(shell("mktemp", "-d", root / "session.XXXXXX"))
    if remote.parent != root:
        raise ValueError("Unexpected device scratch path")
    binaries = {"xl_original": a.engine, "libSDL12.so": a.sdl, "libGL.so.1": a.gl}
    hashes = {}
    for name, local in binaries.items():
        transfer("push", local, remote / name)
        hashes[name] = digest(local)
        if device_sha(remote / name) != hashes[name]:
            raise RuntimeError(f"Transferred binary mismatch for {name}")
    shell("chmod", "755", remote / "xl_original")
    mission = root / "game/missions" / folder / descriptor
    assets = {str(path.relative_to(root)): device_sha(path) for path in (mission, mission.with_suffix(".hog"))}
    environment = {
        "HOME": root / "user",
        "SDL_VIDEODRIVER": "dummy",
        "SDL_AUDIODRIVER": "dummy",
        "XL_REFERENCE_DESCRIPTOR": descriptor,
        "XL_REFERENCE_FOLDER": folder + "/",
        "XL_REFERENCE_LEVEL": 1,
        "XL_REFERENCE_OUT": remote / "reference.tsv",
        "XL_REFERENCE_FRAME_PATH": remote / "frame.ppm",
    }
    args = [
        "env",
        *(f"{key}={value}" for key, value in environment.items()),
        "timeout",
        "-k",
        "4",
        "150",
        remote / "xl_original",
        "-800x600",
        "-datadir",
        root / "game",
        "-userdir",
        root / "user",
        "-cachedir",
        root / "cache",
        "-fullscreen",
        "0",
        "-nosound",
        "-nomovies",
        "1",
        "-briefings",
        "0",
        "-progress_bars",
        "0",
    ]
    command = "cd " + shlex.quote(str(remote)) + " && " + shlex.join(map(str, args))
    started = time.monotonic()
    with (a.output / "engine.log").open("wb") as log:
        result = subprocess.run([*prefix, "shell", command], stdout=log, stderr=subprocess.STDOUT, timeout=180)
    receipt = {
        "scene": a.scene,
        "device": a.serial,
        "device_abi": shell("getprop", "ro.product.cpu.abi"),
        "api": shell("getprop", "ro.build.version.sdk"),
        "binary_sha256": hashes,
        "mission_sha256": assets,
        "seconds": round(time.monotonic() - started, 3),
        "returncode": result.returncode,
        "device_directory": str(remote),
    }
    for name in ("reference.tsv", "frame.ppm"):
        if shell("sh", "-c", "test -f " + shlex.quote(str(remote / name)) + " && echo yes || echo no") == "yes":
            transfer("pull", remote / name, a.output / name)
    reference = a.output / "reference.tsv"
    records = reference.read_text().splitlines() if reference.exists() else []
    passed = all(
        expected in records
        for expected in (
            "listed\t1",
            "descriptor\t" + str(mission),
            "starting\t1\t" + member,
            "started\t1\t1",
            "frames\t120\t1\t0",
        )
    )
    receipt["level_and_frames_confirmed"] = passed
    image = a.output / "frame.ppm"
    if image.exists():
        with Image.open(image) as frame:
            colors = len(frame.getcolors(frame.width * frame.height))
            receipt["frame"] = {"width": frame.width, "height": frame.height, "colors": colors, "sha256": digest(image)}
            passed = passed and frame.size == (800, 600) and colors > 1
    else:
        passed = False
    receipt["passed"] = passed and result.returncode == 0
    (a.output / "result.json").write_text(json.dumps(receipt, indent=2) + "\n")
    return int(not receipt["passed"])


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("adb", "engine", "sdl", "gl", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--serial", required=True)
    parser.add_argument("--device-root", required=True)
    parser.add_argument("--scene", choices=SCENES, required=True)
    raise SystemExit(measure(parser.parse_args()))
