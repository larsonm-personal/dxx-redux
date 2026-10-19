#!/usr/bin/env python3
"""Create synthetic preview media, recording reproducible inputs and tool identity."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


def generate(output: Path, ffmpeg: str = "ffmpeg", ffprobe: str = "ffprobe") -> dict:
    ffmpeg_path = shutil.which(ffmpeg)
    ffprobe_path = shutil.which(ffprobe)
    if not ffmpeg_path or not ffprobe_path:
        raise ValueError("Preview audio requires ffmpeg and ffprobe on PATH or configured with FFMPEG and FFPROBE")
    output.mkdir(parents=True, exist_ok=True)
    audio = output / "preview.mp3"
    if audio.exists():
        raise ValueError(f"Refusing to replace an existing fixture: {audio}")
    command = [
        ffmpeg_path,
        "-nostdin",
        "-hide_banner",
        "-loglevel",
        "error",
        "-n",
        "-f",
        "lavfi",
        "-i",
        "sine=frequency=440:sample_rate=44100:duration=30",
        "-map_metadata",
        "-1",
        "-ac",
        "1",
        "-c:a",
        "libmp3lame",
        "-b:a",
        "128k",
        "-fflags",
        "+bitexact",
        "-flags:a",
        "+bitexact",
        "-write_xing",
        "0",
        "-id3v2_version",
        "0",
        str(audio),
    ]
    version = subprocess.run([ffmpeg_path, "-version"], check=True, capture_output=True, text=True, timeout=15).stdout
    subprocess.run(command, check=True, timeout=60)
    probe = subprocess.run(
        [ffprobe_path, "-v", "error", "-show_streams", "-show_format", "-of", "json", str(audio)],
        check=True,
        capture_output=True,
        text=True,
        timeout=15,
    )
    metadata = json.loads(probe.stdout)
    stream = metadata["streams"][0]
    duration = float(metadata["format"]["duration"])
    if (
        len(metadata["streams"]) != 1
        or stream["codec_name"] != "mp3"
        or int(stream["sample_rate"]) != 44100
        or stream["channels"] != 1
        or not 30 <= duration < 30.2
    ):
        raise ValueError("Generated MP3 did not meet the preview fixture contract")
    result = {
        "schema": 1,
        "kind": "generated-sine-preview",
        "command": command,
        "ffmpeg_version": version,
        "sha256": hashlib.sha256(audio.read_bytes()).hexdigest(),
        "bytes": audio.stat().st_size,
        "probe": metadata,
        "scope": "Synthetic media transport fixture; no retail audio or audio-quality parity claim",
    }
    (output / "provenance.json").write_text(json.dumps(result, indent=2) + "\n")
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--ffmpeg", default=os.environ.get("FFMPEG", "ffmpeg"))
    parser.add_argument("--ffprobe", default=os.environ.get("FFPROBE", "ffprobe"))
    args = parser.parse_args()
    print(json.dumps(generate(args.output, args.ffmpeg, args.ffprobe), indent=2))
