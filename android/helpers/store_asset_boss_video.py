"""Repeatable boss replacement and reordered Play Store preview."""

import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess
import wave

import numpy as np

from generate_store_assets import Capture, ROOT, PACKAGE
from store_asset_media import (
    save_json,
    demo_interval,
    ffmpeg,
    imageio_ffmpeg,
    render_audio,
    validate_graphics,
    video_info,
    contact_sheet,
    result_differences,
)
from store_asset_video import digest, camera_motion


def recipe():
    return json.loads((ROOT / "android/store-preview-boss.json").read_text())


def capture_boss(output, serial):
    capture = Capture(output, serial)
    setup = capture.launcher()
    if not setup.get("d1", {}).get("ready") or not setup.get("d2", {}).get("ready"):
        raise ValueError("Provision full D1/D2 data on the dedicated store emulator before capture")
    config = capture.private("cat", "files/d2x-redux/descent.cfg", check=False)
    if not config:
        config = capture.private("cat", "files/descent.cfg", check=False)
    saved = {}
    for key, default in (("ResolutionX", 2400), ("ResolutionY", 1080), ("AspectX", 9), ("AspectY", 20)):
        match = re.search(rf"(?m)^{key}=(\d+)", config)
        saved[key] = int(match.group(1)) if match else default
    previous = re.search(r"Override size: (\d+x\d+)", capture.shell("wm", "size"))
    item = recipe()
    save_json(Path(output) / "boss-recipe.json", item)
    try:
        capture.shell("am", "force-stop", PACKAGE)
        capture.shell("wm", "size", "1080x2400")
        capture.launcher()
        capture.command(
            "write_graphics_settings",
            settings=json.dumps(
                {
                    "ResolutionX": 2400,
                    "ResolutionY": 1080,
                    "AspectX": 9,
                    "AspectY": 20,
                    "TexFilt": 2,
                    "AnisoLevel": 16,
                    "MsaaLevel": 4,
                }
            ),
        )
        capture.replay(
            ROOT / "android/regression_demos" / (item["demo"] + ".dximdemo"),
            presentation=item["presentation"],
            presentation_video=True,
        )
    finally:
        capture.shell("am", "force-stop", PACKAGE)
        capture.shell("wm", "size", previous.group(1) if previous else "reset")
        capture.launcher()
        capture.command("write_graphics_settings", settings=json.dumps(saved))


def prepare(output, reference, audio_run):
    output, reference, audio_run = map(Path, (output, reference, audio_run))
    item = recipe()
    captured = json.loads((output / "boss-recipe.json").read_text())
    if any(captured[key] != item[key] for key in ("demo", "presentation")):
        raise ValueError("Boss presentation changed; recapture before preparing the edit")
    manifest_path = Path("demos") / item["demo"] / "capture.json"
    manifest = json.loads((output / manifest_path).read_text())
    audio_manifest = json.loads((audio_run / manifest_path).read_text())
    for captured_demo in (manifest, audio_manifest):
        if captured_demo["sha256"] != digest(ROOT / captured_demo["source"]):
            raise ValueError("Demo changed since capture")
        if not captured_demo.get("replay_result"):
            raise ValueError("Replay did not finish")
    demo = ROOT / manifest["source"]
    expected = next(
        json.loads(line)["result"] for line in demo.read_text().splitlines() if json.loads(line)["type"] == "result"
    )
    differences = result_differences(expected, manifest["replay_result"])
    if set(differences) - {"/game", "/mission", "/level_summary/endlevel_completed"}:
        raise ValueError(f"Boss simulation changed unexpectedly: {differences}")
    boss = demo_interval(manifest, item["demo_start"], item["demo_end"])
    boss_audio = demo_interval(audio_manifest, item["demo_start"], item["demo_end"])
    seconds = item["demo_end"] - item["demo_start"]
    boss_audio.update(label="D1 level 7 boss", group="demo", seconds=seconds)
    inputs = output / "inputs"
    inputs.mkdir(exist_ok=True)
    sources = {}

    def archive(source, name):
        target = inputs / name
        shutil.copyfile(source, target)
        relative = target.relative_to(output).as_posix()
        sources[relative] = digest(target)
        return relative

    first = archive(reference / "inputs/picture-09.mp4", "first.mp4")
    third = archive(reference / "inputs/picture-11.mp4", "third.mp4")
    flyout = archive(reference / "combined-flyout.mp4", "flyout.mp4")
    opening = archive(reference / "store-preview-30s.mp4", "reference.mp4")
    boss["source"] = archive(output / boss["source"], "boss-native.mp4")
    audio_path = audio_run / boss_audio["source"]
    meta = json.loads(audio_path.with_suffix(".audio.json").read_text())
    meta["source"] = archive(audio_run / meta["source"], "boss.wav")
    meta["music_source"] = archive(audio_run / meta["music_source"], "boss.music.wav")
    save_json(inputs / "boss.audio.json", meta)
    sources["inputs/boss.audio.json"] = digest(inputs / "boss.audio.json")
    # render_audio resolves the native PCM sidecars from this virtual video path
    boss_audio["source"] = "inputs/boss.mp4"
    state_path = Path("demos") / item["demo"] / "start-state.json"
    state = json.loads((output / state_path).read_text())
    validate_graphics(state)
    if (state["cockpit_mode"], state["hud_mode"], state["cockpit_views"]) != (3, 3, [0, 0]):
        raise ValueError("Boss presentation differs from the full-screen featured view")
    if not state["hud_layout"]["show_robot_hostage_counts"]:
        raise ValueError("Boss progress rows are disabled")
    sources[state_path.as_posix()] = digest(output / state_path)
    sources[manifest_path.as_posix()] = digest(output / manifest_path)
    sources["boss-recipe.json"] = digest(output / "boss-recipe.json")
    for suffix in (".json", "-result.json", "-log.jsonl"):
        proof = Path("scripts") / (item["demo"] + "-fov" + suffix)
        sources[proof.as_posix()] = digest(output / proof)
    fov_result = json.loads((output / "scripts" / (item["demo"] + "-fov-result.json")).read_text())
    if fov_result.get("result") != "PASS":
        raise ValueError("Native 110-degree FoV override failed")
    edit = {
        "recipe": item,
        "replay_differences": differences,
        "sources": sources,
        "boss_audio": boss_audio,
        "clips": [
            {"label": "D2 level 9", "source": first, "seconds": 5, "audio_start": 9},
            {"label": "D1 level 7 boss, 110 FoV", **boss, "seconds": seconds},
            {
                "label": "Launcher through guidebot briefing (muted)",
                "source": opening,
                "start": 0,
                "end": item["opening_end"],
                "seconds": item["opening_seconds"],
                "silent": True,
            },
            {"label": "D1 level 5", "source": third, "seconds": 5, "audio_start": 19},
            {"label": "D1 fly-out", "source": flyout, "seconds": 6, "audio_start": 24},
        ],
        "reference": opening,
    }
    save_json(output / "boss-edit.json", edit)
    return compose(output)


def pcm(path):
    return subprocess.check_output(
        [
            imageio_ffmpeg.get_ffmpeg_exe(),
            "-v",
            "error",
            "-i",
            str(path),
            "-map",
            "0:a:0",
            "-ar",
            "48000",
            "-ac",
            "2",
            "-f",
            "s16le",
            "pipe:1",
        ]
    )


def verify_sources(output, edit):
    for name, expected in edit["sources"].items():
        if digest(output / name) != expected:
            raise ValueError(f"Archived source changed: {name}")


def picture_digest(path, start, frames):
    return subprocess.check_output(
        [
            imageio_ffmpeg.get_ffmpeg_exe(),
            "-v",
            "error",
            "-ss",
            str(start),
            "-i",
            str(path),
            "-map",
            "0:v:0",
            "-frames:v",
            str(frames),
            "-pix_fmt",
            "yuv420p",
            "-f",
            "hash",
            "-hash",
            "sha256",
            "pipe:1",
        ],
        text=True,
    ).strip()


def compose(output):
    output = Path(output)
    edit = json.loads((output / "boss-edit.json").read_text())
    verify_sources(output, edit)
    encoded = output / "edit-clips"
    encoded.mkdir(exist_ok=True)
    render_audio(output, [edit["boss_audio"]], encoded)
    reference_pcm = pcm(output / edit["reference"])
    with wave.open(str(encoded / "soundtrack.wav"), "rb") as stream:
        boss_pcm = stream.readframes(stream.getnframes())
    pictures, soundtrack, timeline = [], bytearray(), []
    offset = 0
    for index, clip in enumerate(edit["clips"]):
        seconds = clip["seconds"]
        source = output / clip["source"]
        frames = round(seconds * 30)
        if "start" in clip:
            target = encoded / f"picture-{index:02d}.mp4"
            trim = ["-ss", clip["start"], "-t", clip["end"] - clip["start"], "-i", source]
            if "demo_start" in clip:
                # Native replay draws one recorded simulation frame per update;
                # remove host scheduling jitter using the captured frame count
                report = subprocess.check_output(
                    [
                        imageio_ffmpeg.get_ffmpeg_exe(),
                        "-v",
                        "error",
                        *map(str, trim),
                        "-map",
                        "0:v:0",
                        "-fps_mode",
                        "passthrough",
                        "-progress",
                        "pipe:1",
                        "-f",
                        "null",
                        "-",
                    ],
                    text=True,
                )
                native_frames = int(re.findall(r"frame=(\d+)", report)[-1])
                if native_frames < seconds * 20:
                    raise ValueError("Boss capture has fewer than 20 native frames per second")
                timing = f"N/({native_frames / seconds:.10f}*TB)"
                clip["native_frames"] = native_frames
            else:
                timing = f"(PTS-STARTPTS)*{seconds / (clip['end'] - clip['start']):.10f}"
            ffmpeg(
                [
                    *trim,
                    "-vf",
                    f"setpts={timing},fps=30,"
                    "scale=2400:1350:force_original_aspect_ratio=decrease,"
                    "pad=2400:1350:(ow-iw)/2:(oh-ih)/2,setsar=1,tpad=stop_mode=clone:stop_duration=0.1",
                    "-frames:v",
                    frames,
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
                    target,
                ],
                encoded / f"picture-{index:02d}.log",
            )
        else:
            target = source
        pictures.append(target.relative_to(output).as_posix())
        if clip.get("silent"):
            audio = bytes(round(seconds * 48000) * 4)
        elif "audio_start" in clip:
            start = round(clip["audio_start"] * 48000) * 4
            audio = reference_pcm[start : start + round(seconds * 48000) * 4]
        else:
            audio = boss_pcm
        if len(audio) != round(seconds * 48000) * 4:
            raise ValueError("Audio section has the wrong sample count")
        soundtrack.extend(audio)
        timeline.append({**clip, "timeline_start": offset, "timeline_end": offset + seconds, "frames": frames})
        offset += seconds
    with wave.open(str(output / "soundtrack.wav"), "wb") as stream:
        stream.setparams((2, 2, 48000, 0, "NONE", "not compressed"))
        stream.writeframes(soundtrack)
    listing = output / "boss-concat.txt"
    listing.write_text("".join(f"file '{path}'\n" for path in pictures))
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
            output / "soundtrack.wav",
            "-map",
            "0:v:0",
            "-map",
            "1:a:0",
            "-c:v",
            "copy",
            "-c:a",
            "aac",
            "-b:a",
            "320k",
            "-t",
            offset,
            "-movflags",
            "+faststart",
            final,
        ],
        output / "encode.log",
    )
    save_json(output / "boss-timeline.json", timeline)
    validate(output)
    stills = []
    for index, time in enumerate((2, 6, 8, 10, 12, 14, 18.9, 21, 25, 28)):
        image = output / f"review-{index:02d}.png"
        ffmpeg(["-ss", time, "-i", final, "-frames:v", 1, image], output / "review.log")
        stills.append(image)
    contact_sheet(stills, output / "video-contact.jpg")
    (output / "index.html").write_text(
        '<!doctype html><meta charset="utf-8"><title>Boss preview</title>'
        "<style>body{background:#141420;color:#eee;font:18px system-ui;max-width:1300px;margin:30px auto}video,img{width:100%}a{color:#acddff}</style>"
        '<h1>Revised store preview</h1><video controls src="store-preview-30s.mp4"></video>'
        "<p>D2 level 9 (5s), D1 boss at 110 FoV (10s), muted launcher/guidebot (4s), D1 level 5 (5s), fly-out (6s).</p>"
        '<p><a href="boss-edit.json">Repeatable edit</a> | <a href="boss-validation.json">Validation</a></p>'
        '<img src="video-contact.jpg">',
        encoding="utf-8",
    )
    return final


def validate(output):
    output = Path(output)
    edit = json.loads((output / "boss-edit.json").read_text())
    verify_sources(output, edit)
    timeline = json.loads((output / "boss-timeline.json").read_text())
    final = output / "store-preview-30s.mp4"
    frames, seconds = imageio_ffmpeg.count_frames_and_secs(str(final))
    if frames != 900 or abs(seconds - 30) > 0.04 or video_info(final)["size"] != (2400, 1350):
        raise ValueError("Preview must be 900 frames, 30 seconds, 2400x1350")
    master = pcm(output / "soundtrack.wav")
    reference = pcm(output / edit["reference"])
    delivered = np.frombuffer(pcm(final), np.int16).astype(np.float64)
    expected = np.frombuffer(master, np.int16).astype(np.float64)
    audio_checks = []
    retained_pictures = []
    for clip in timeline:
        first, last = (round(clip[key] * 48000) * 4 for key in ("timeline_start", "timeline_end"))
        segment = master[first:last]
        if clip.get("silent"):
            if any(segment):
                raise ValueError("Launcher master is not digitally silent")
        elif "audio_start" in clip:
            start = round(clip["audio_start"] * 48000) * 4
            if segment != reference[start : start + len(segment)]:
                raise ValueError("Reviewed PCM changed during rearrangement")
            source_hash = picture_digest(output / clip["source"], 0, clip["frames"])
            if picture_digest(final, clip["timeline_start"], clip["frames"]) != source_hash:
                raise ValueError("Retained gameplay/fly-out picture changed")
            retained_pictures.append({"label": clip["label"], "decoded_sha256": source_hash})
        # Exclude AAC transform overlap at the cut boundaries
        a, b = first // 2 + 4800, last // 2 - 4800
        x, y = expected[a:b], delivered[a:b]
        rms = float(np.sqrt(np.mean(y * y)))
        correlation = float(np.corrcoef(x, y)[0, 1]) if not clip.get("silent") else None
        if clip.get("silent") and rms > 1:
            raise ValueError("Delivered launcher audio is not silent")
        if not clip.get("silent") and (rms < 10 or correlation < 0.97):
            raise ValueError("Delivered audio differs from its unretimed master")
        audio_checks.append({"label": clip["label"], "rms": rms, "master_correlation": correlation})
    music = json.loads((output / "audio-sources.json").read_text())[0]
    if music["music_tempo"] != 1 or not music["music_samples_match"]:
        raise ValueError("Boss MIDI tempo changed")
    motion = camera_motion(final, start=timeline[-1]["timeline_start"])
    if not motion["smooth_tunnel"]:
        raise ValueError("Fly-out camera motion regressed")
    checks = {
        "frames": frames,
        "seconds": seconds,
        "timeline": timeline,
        "audio": audio_checks,
        "boss_audio": music,
        "retained_pictures": retained_pictures,
        "motion": motion,
    }
    save_json(output / "boss-validation.json", checks)
    return checks


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["capture", "prepare", "compose", "validate"])
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--reference", type=Path)
    parser.add_argument("--audio-run", type=Path)
    parser.add_argument("--serial", default="emulator-5580")
    args = parser.parse_args()
    if args.action == "capture":
        capture_boss(args.output, args.serial)
    elif args.action == "prepare":
        if not args.reference or not args.audio_run:
            parser.error("prepare requires --reference and --audio-run")
        print(prepare(args.output, args.reference, args.audio_run))
    elif args.action == "compose":
        print(compose(args.output))
    else:
        print(json.dumps(validate(args.output), indent=2))


if __name__ == "__main__":
    main()
