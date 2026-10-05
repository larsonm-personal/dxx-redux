"""Combine reviewed sound and filtered pictures without accelerating the mix."""

import hashlib
import html
import json
from pathlib import Path
import shutil
import subprocess

import cv2
import numpy as np

from store_asset_media import (
    ffmpeg,
    flyout_clip,
    imageio_ffmpeg,
    save_json,
    validate_graphics,
    video_info,
    validate_music_cancellation,
)


def digest(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def audio_digest(path):
    # Hash decoded audio from the delivered container, including codec padding
    pcm = subprocess.check_output(
        [
            imageio_ffmpeg.get_ffmpeg_exe(),
            "-v",
            "error",
            "-i",
            str(path),
            "-map",
            "0:a:0",
            "-f",
            "s16le",
            "-acodec",
            "pcm_s16le",
            "pipe:1",
        ]
    )
    return hashlib.sha256(pcm).hexdigest()


def camera_motion(path, start=24, seconds=6):
    """Track scene edges; moving explosions alone must not pass a camera check."""
    raw = subprocess.check_output(
        [
            imageio_ffmpeg.get_ffmpeg_exe(),
            "-v",
            "error",
            "-ss",
            str(start),
            "-i",
            str(path),
            "-t",
            str(seconds),
            "-vf",
            "scale=480:270,format=gray",
            "-fps_mode",
            "passthrough",
            "-f",
            "rawvideo",
            "pipe:1",
        ]
    )
    frames = np.frombuffer(raw, np.uint8).reshape(-1, 270, 480)
    mask = np.zeros((270, 480), np.uint8)
    # Tunnel walls outside the central ship/explosion and corner HUD regions
    mask[65:205, 30:175] = 255
    mask[65:205, 305:450] = 255
    motion = []
    for previous, current in zip(frames, frames[1:]):
        points = cv2.goodFeaturesToTrack(previous, 200, 0.02, 6, mask=mask)
        if points is None or len(points) < 8:
            motion.append(0.0)
            continue
        tracked, status, _ = cv2.calcOpticalFlowPyrLK(previous, current, points, None)
        shifts = np.linalg.norm(tracked.reshape(-1, 2) - points.reshape(-1, 2), axis=1)[status.ravel() == 1]
        motion.append(float(np.percentile(shifts, 75)) if len(shifts) >= 8 else 0.0)
    tunnel = np.array(motion[:36])
    median = float(np.median(tunnel))
    stalls = float(np.mean(tunnel < 0.5))
    jump_ratio = float(np.percentile(tunnel, 90) / max(0.01, median))
    return {
        "method": "P75 Lucas-Kanade displacement on side-wall edges at 480x270; central effects and HUD excluded",
        "frames": len(frames),
        "wall_motion_pixels": motion,
        "per_second_median": [float(np.median(motion[i : i + 30])) for i in range(0, len(motion), 30)],
        "initial_camera_stall_fraction": stalls,
        "initial_camera_jump_ratio": jump_ratio,
        "initial_camera_median_pixels": median,
        "smooth_tunnel": len(tunnel) == 36 and median > 0.5 and stalls <= 0.1 and jump_ratio <= 6,
    }


def prepare_combination(output, picture, audio, flyout):
    output, picture, audio, flyout = map(lambda p: Path(p).resolve(), (output, picture, audio, flyout))
    inputs = output / "inputs"
    inputs.mkdir(parents=True, exist_ok=True)
    picture_recipe = json.loads((picture / "edit.json").read_text())
    audio_recipe = json.loads((audio / "edit.json").read_text())

    def identity(recipe):
        return [
            {key: clip.get(key) for key in ("label", "group", "seconds", "demo_start", "demo_end")}
            for clip in recipe["clips"]
        ]

    if identity(picture_recipe) != identity(audio_recipe):
        raise ValueError("Picture and reference audio must use the same clip order, simulation intervals and durations")
    if sum(c["seconds"] for c in picture_recipe["clips"][:-1]) != 24:
        raise ValueError("Expected 24 seconds before the six-second fly-out")
    files = []
    for i, clip in enumerate(picture_recipe["clips"][:-1]):
        source = picture / "edit-clips" / f"{i:02d}.mp4"
        target = inputs / f"picture-{i:02d}.mp4"
        shutil.copyfile(source, target)
        files.append(
            {
                "file": target.relative_to(output).as_posix(),
                "sha256": digest(target),
                "label": clip["label"],
                "seconds": clip["seconds"],
            }
        )
    soundtrack = inputs / "reference-audio.m4a"
    ffmpeg(
        ["-i", audio / "store-preview-30s.mp4", "-map", "0:a:0", "-c:a", "copy", soundtrack], output / "copy-audio.log"
    )
    if audio_digest(soundtrack) != audio_digest(audio / "store-preview-30s.mp4"):
        raise ValueError("Extracted reference soundtrack differs from the reviewed source")
    clip = flyout_clip(flyout)
    if clip.get("capture_fps") != 60:
        raise ValueError("Combined video requires the fixed 60 Hz native fly-out capture")
    for state in sorted(flyout.glob("flyout-phase-*.json")):
        validate_graphics(json.loads(state.read_text()))
    source = flyout / clip["source"]
    target = inputs / "filtered-flyout.mp4"
    shutil.copyfile(source, target)
    clip["source"] = target.relative_to(output).as_posix()
    clip["sha256"] = digest(target)
    recipe = {
        "picture_run": str(picture),
        "audio_run": str(audio),
        "flyout_run": str(flyout),
        "reference_video_sha256": digest(audio / "store-preview-30s.mp4"),
        "picture_video_sha256": digest(picture / "store-preview-30s.mp4"),
        "picture_clips": files,
        "flyout": clip,
        "audio": {
            "file": soundtrack.relative_to(output).as_posix(),
            "sha256": digest(soundtrack),
            "decoded_sha256": audio_digest(soundtrack),
            "processing": "AAC stream copy; no tempo, gain or mix changes",
        },
    }
    save_json(output / "combined-edit.json", recipe)
    # Retain the editorial identity so this delivered run can be the next
    # generation's audio reference without depending on older temp folders
    save_json(output / "edit.json", picture_recipe)
    return compose_combination(output)


def compose_combination(output):
    output = Path(output).resolve()
    recipe = json.loads((output / "combined-edit.json").read_text())
    for entry in [*recipe["picture_clips"], recipe["audio"], recipe["flyout"]]:
        path = output / entry.get("file", entry.get("source", ""))
        if digest(path) != entry["sha256"]:
            raise ValueError(f"Combination source changed: {path}")
    clip = recipe["flyout"]
    encoded = output / "combined-flyout.mp4"
    ffmpeg(
        [
            "-ss",
            clip["start"],
            "-t",
            clip["end"] - clip["start"],
            "-i",
            output / clip["source"],
            "-vf",
            f"setpts=N/({clip['frame_rate']:.10f}*TB),fps=30,scale=2400:1350:force_original_aspect_ratio=decrease,pad=2400:1350:(ow-iw)/2:(oh-ih)/2,setsar=1",
            "-frames:v",
            180,
            "-an",
            "-c:v",
            "libx264",
            "-preset",
            "fast",
            "-crf",
            18,
            "-pix_fmt",
            "yuv420p",
            "-video_track_timescale",
            15360,
            encoded,
        ],
        output / "combined-flyout.log",
    )
    paths = [c["file"] for c in recipe["picture_clips"]] + [encoded.name]
    listing = output / "combined-concat.txt"
    listing.write_text("".join(f"file '{p}'\n" for p in paths))
    final = output / "store-preview-30s.mp4"
    ffmpeg(
        [
            "-f",
            "concat",
            "-safe",
            0,
            "-i",
            listing,
            "-i",
            output / recipe["audio"]["file"],
            "-map",
            "0:v:0",
            "-map",
            "1:a:0",
            "-c",
            "copy",
            "-t",
            30,
            "-movflags",
            "+faststart",
            final,
        ],
        output / "combine.log",
    )
    checks = validate_combination(output)
    (output / "index.html").write_text(
        '<!doctype html><meta charset="utf-8"><title>Combined store video</title>'
        "<style>body{background:#141420;color:#eee;font:18px system-ui;max-width:1300px;margin:30px auto}video{width:100%}a{color:#acddff}</style>"
        '<h1>Filtered picture, reviewed sound, smooth native fly-out</h1><video controls src="store-preview-30s.mp4"></video>'
        "<p>30 seconds at 2400x1350. Filtered launcher/action footage, unchanged reviewed soundtrack, "
        "and a fresh native fly-out captured at fixed simulation intervals with trilinear filtering, 4x MSAA and 16x AF.</p>"
        '<p><a href="combined-edit.json">Repeatable edit and source hashes</a> | '
        '<a href="combined-validation.json">Validation</a></p>'
        "<pre>"
        + html.escape(json.dumps({key: value for key, value in checks.items() if key != "motion"}, indent=2))
        + "</pre>",
        encoding="utf-8",
    )
    return final


def validate_combination(output):
    output = Path(output)
    recipe = json.loads((output / "combined-edit.json").read_text())
    final = output / "store-preview-30s.mp4"
    frames, seconds = imageio_ffmpeg.count_frames_and_secs(str(final))
    info = video_info(final)
    if frames != 900 or abs(seconds - 30) > 0.04 or info["size"] != (2400, 1350):
        raise ValueError("Combined video must be 900 frames, 30 seconds, 2400x1350")
    actual_audio = audio_digest(final)
    if actual_audio != recipe["audio"]["decoded_sha256"]:
        raise ValueError("Delivered audio differs from the reviewed reference")
    motion = camera_motion(final)
    checks = {
        "frames": frames,
        "seconds": seconds,
        "audio_identical_to_reference": True,
        "audio_decoded_sha256": actual_audio,
        "motion": motion,
    }
    checks["music_subtraction_regression"] = validate_music_cancellation(output / recipe["audio"]["file"], start=14)
    save_json(output / "combined-validation.json", checks)
    if not motion["smooth_tunnel"]:
        raise ValueError("Native fly-out camera still stalls/jumps; see combined-validation.json")
    return checks
