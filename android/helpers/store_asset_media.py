"""Offline editing and validation for generate_store_assets.py captures."""

import html
import json
from pathlib import Path
import subprocess
import wave
from array import array
import math

import imageio_ffmpeg
from PIL import Image, ImageChops, ImageDraw, ImageStat


def save_json(path, data):
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def ffmpeg(arguments, log):
    with log.open("w", encoding="utf-8") as stream:
        subprocess.run(
            [imageio_ffmpeg.get_ffmpeg_exe(), "-hide_banner", "-y", *map(str, arguments)],
            stdout=stream,
            stderr=subprocess.STDOUT,
            check=True,
        )


def video_info(path):
    reader = imageio_ffmpeg.read_frames(str(path))
    try:
        return next(reader)
    finally:
        reader.close()


def motion_cadence(path, start, seconds):
    """Measure visible updates, not just the container's nominal frame rate."""
    result = subprocess.run(
        [
            imageio_ffmpeg.get_ffmpeg_exe(),
            "-v",
            "error",
            "-ss",
            str(start),
            "-t",
            str(seconds),
            "-i",
            str(path),
            "-vf",
            "scale=320:144,format=gray",
            "-fps_mode",
            "passthrough",
            "-f",
            "rawvideo",
            "pipe:1",
        ],
        capture_output=True,
        check=True,
    )
    frame_bytes = 320 * 144
    previous = None
    updates = repeats = longest = held = 0
    count = len(result.stdout) // frame_bytes
    for offset in range(0, count * frame_bytes, frame_bytes):
        current = Image.frombytes("L", (320, 144), result.stdout[offset : offset + frame_bytes])
        if previous is not None:
            difference = ImageStat.Stat(ImageChops.difference(previous, current)).mean[0]
            if difference > 0.25:
                updates += 1
                held = 0
            else:
                repeats += 1
                held += 1
                longest = max(longest, held)
        previous = current
    fps = count / seconds
    return {
        "start": start,
        "seconds": seconds,
        "decoded_frames": count,
        "visible_updates_per_second": round(updates * fps / max(1, count - 1), 2),
        "near_duplicate_frames": repeats,
        "longest_hold_seconds": round((longest + 1) / fps, 3),
        "method": "Adjacent 320x144 grayscale frames; mean absolute pixel difference > 0.25",
    }


def demo_interval(capture, start, end):
    """Map simulation time to captured wall time, interpolating nearby samples."""
    samples = capture["samples"]

    def locate(value):
        for left, right in zip(samples, samples[1:]):
            if left["demo_seconds"] <= value <= right["demo_seconds"]:
                if left["part"] != right["part"]:
                    raise ValueError("Selected clip crosses a screenrecord restart; choose another interval")
                span = right["demo_seconds"] - left["demo_seconds"]
                fraction = (value - left["demo_seconds"]) / span if span else 0
                return left["part"], left["wall"] + fraction * (right["wall"] - left["wall"])
        raise ValueError(f"Demo time {value} is outside captured samples")

    part, first = locate(start)
    last_part, last = locate(end)
    if part != last_part:
        raise ValueError("Selected clip crosses recording parts")
    return {"source": capture["parts"][part], "start": first, "end": last, "demo_start": start, "demo_end": end}


def default_recipe(output):
    opening = json.loads((output / "opening.json").read_text())
    if not opening["beats"] or opening["beats"][-1]["label"] != "End":
        raise ValueError("Opening capture is incomplete")
    beats = {b["label"]: b["wall"] for b in opening["beats"]}
    # The encoder starts after adb launches screenrecord. The last observed
    # state may therefore be slightly past the media's final timestamp.
    beats["End"] = min(beats["End"], video_info(output / opening["source"])["duration"])
    if "File picker ready" in beats:
        shots = [
            ("Launcher and import", beats["Launcher"], beats["Pick game data"], 1),
            ("Pick game data", beats["File picker ready"], beats["Imported game data"], 1.2),
            ("Scroll launcher", beats["Imported game data"], beats["Launcher bottom"] + 0.3, 0.8),
            ("Launch D2", beats["Launcher bottom"], beats["Launch Descent 2"] + 0.3, 0.6),
            ("Create pilot", beats["Pilot name"], beats["Created pilot"], 1),
            (
                "Counterstrike campaign",
                beats["Created pilot"],
                beats.get("Choose difficulty", beats["Difficulty selected"] - 1.5) + 0.3,
                1,
            ),
            ("Briefing page 1", beats["Briefing page 1"], beats["Briefing page 1"] + 0.8, 0.8),
            ("Briefing page 2", beats["Briefing page 2"], beats["Briefing page 2"] + 0.8, 0.8),
            ("Briefing page 3", beats["Briefing page 3"], beats["Briefing page 3"] + 0.8, 0.8),
            ("Enter level", beats["End"] - 1, beats["End"], 1),
        ]
        clips = [
            {
                "label": label,
                "group": "opening",
                "source": opening["source"],
                "start": start,
                "end": end,
                "seconds": seconds,
            }
            for label, start, end, seconds in shots
        ]
    else:
        clips = [
            {
                "label": "Launcher, data, pilot, briefing",
                "group": "opening",
                "source": opening["source"],
                "start": opening["beats"][0]["wall"],
                "end": beats["End"],
                "seconds": 9,
            }
        ]
    demos = [json.loads(path.read_text()) for path in sorted((output / "demos").glob("*/capture.json"))]
    eligible = [c for c in demos if c["duration"] >= 50 and c.get("replay_result")]
    # Include both games in the default three-clip edit
    chosen = [next(c for c in eligible if c["game"] == "d2")]
    chosen += [c for c in eligible if c["game"] == "d1"][:2]
    if len(chosen) != 3:
        raise ValueError("The video requires one D2 and two D1 recordings >= 50 seconds")
    for capture in chosen:
        start = capture["duration"] * 0.9
        # Review-selected combat replaces this recording's stalled exit approach
        if Path(capture["source"]).stem == "d2_descent2_level9_20260511_192804":
            start = 48.0
        state = json.loads((output / "demos" / Path(capture["source"]).stem / "start-state.json").read_text())
        recorded_fps = capture.get("simulation_fps", state["input_demo"]["replay_frame_count"] / capture["duration"])
        clips.append(
            {
                "label": Path(capture["source"]).stem,
                "group": "demo",
                "seconds": 5,
                "frame_rate": recorded_fps,
                **demo_interval(capture, start, start + 5),
            }
        )
    flyout = json.loads((output / "flyout.json").read_text())
    active = [s for s in flyout["samples"] if s["phase"] > 0]
    if len(active) < 2 or flyout["samples"][-1]["phase"] != 0:
        raise ValueError("Fly-out capture is incomplete")
    # Introspection timestamps follow the frame they observe, while the video
    # encoder starts after its launch. Keep one observation interval of margin
    # so the score-screen transition cannot flash at the end of the edit.
    end = active[-2]["wall"]
    rate = flyout.get("playback_rate", 1)
    clips.append(
        {
            "label": "Descent 1 fly-out",
            "group": "flyout",
            "source": flyout["source"],
            "start": max(0, end - 6 * rate),
            "end": end,
            "seconds": 6,
            "transpose": flyout.get("transpose"),
        }
    )
    return {"width": 2400, "height": 1350, "fps": 30, "audio": "engine-pcm", "clips": clips}


def render_audio(output, clips, encoded):
    """Cut the engine's mixed MIDI/SFX; time compression preserves pitch."""
    waves = []
    evidence = []
    for index, clip in enumerate(clips):
        source = output / clip["source"]
        start, end = clip["start"], clip["end"]
        if clip["group"] == "flyout":
            normal = json.loads((output / "flyout-audio.json").read_text())
            source = output / normal["source"]
            active = [sample for sample in normal["samples"] if sample["phase"] > 0]
            end = active[-2]["wall"]
            start = max(0, end - clip["seconds"])
        meta = json.loads(source.with_suffix(".audio.json").read_text())
        if meta["overflow"] or meta["sample_rate"] != 48000 or meta["channels"] != 2:
            raise ValueError(f"Invalid engine PCM capture: {source}")
        if not meta["engine_audio"]["music"].get("active") or meta["engine_audio"]["music"].get("type") != 1:
            raise ValueError(f"Engine MIDI was not playing: {source}")
        audio_start = max(0, start - meta["video_offset"])
        audio_end = max(0, end - meta["video_offset"])
        speed = (end - start) / clip["seconds"]
        delay = max(0, meta["video_offset"] - start) / speed
        target = encoded / f"{index:02d}.wav"
        if delay >= clip["seconds"]:
            with wave.open(str(target), "wb") as stream:
                stream.setparams((2, 2, 48000, 0, "NONE", "not compressed"))
                stream.writeframes(bytes(round(clip["seconds"] * 48000) * 4))
        else:
            tempo = []
            while speed > 2:
                tempo.append("atempo=2")
                speed /= 2
            while speed < 0.5:
                tempo.append("atempo=0.5")
                speed *= 2
            tempo.append(f"atempo={speed:.10f}")
            filters = (
                f"atrim=start={audio_start:.6f}:end={audio_end:.6f},asetpts=PTS-STARTPTS,"
                + ",".join(tempo)
                + f",adelay={round(delay * 48000)}S:all=1,apad,atrim=duration={clip['seconds']}"
            )
            ffmpeg(
                ["-i", output / meta["source"], "-af", filters, "-ar", 48000, "-ac", 2, "-c:a", "pcm_s16le", target],
                encoded / f"{index:02d}-audio.log",
            )
        with wave.open(str(target), "rb") as stream:
            pcm = array("h", stream.readframes(stream.getnframes()))
        rms = math.sqrt(sum(value * value for value in pcm) / max(1, len(pcm)))
        peak = max(map(abs, pcm), default=0)
        gain = (
            min(10 ** (12 / 20), 32768 * 10 ** (-22 / 20) / max(1, rms), 32768 * 10 ** (-2 / 20) / max(1, peak))
            if peak
            else 1
        )
        # Preserve the engine's music/effects balance within each excerpt while
        # avoiding a large volume jump from quiet menus into combat
        frames = len(pcm) // 2
        fade_out = round(48000 * (0.15 if clip["group"] == "flyout" else 0.015))
        for frame in range(frames):
            factor = gain * min(1, frame / 720, (frames - 1 - frame) / fade_out)
            pcm[frame * 2] = round(pcm[frame * 2] * factor)
            pcm[frame * 2 + 1] = round(pcm[frame * 2 + 1] * factor)
        with wave.open(str(target), "wb") as stream:
            stream.setparams((2, 2, 48000, 0, "NONE", "not compressed"))
            stream.writeframes(pcm.tobytes())
        waves.append(target)
        evidence.append(
            {
                "label": clip["label"],
                "source": meta["source"],
                "start": audio_start,
                "end": audio_end,
                "leading_silence": delay,
                "engine_audio": meta["engine_audio"],
                "gain_db": round(20 * math.log10(gain), 2),
            }
        )
    soundtrack = encoded / "soundtrack.wav"
    with wave.open(str(soundtrack), "wb") as destination:
        destination.setparams((2, 2, 48000, 0, "NONE", "not compressed"))
        for path in waves:
            with wave.open(str(path), "rb") as source:
                destination.writeframes(source.readframes(source.getnframes()))
    save_json(output / "audio-sources.json", evidence)
    return soundtrack


def audit_audio(output, timeline):
    # Decode the final AAC, not an intermediate WAV, and inspect each section
    raw = subprocess.run(
        [
            imageio_ffmpeg.get_ffmpeg_exe(),
            "-v",
            "error",
            "-i",
            str(output / "store-preview-30s.mp4"),
            "-map",
            "0:a:0",
            "-f",
            "s16le",
            "-ar",
            "48000",
            "-ac",
            "2",
            "pipe:1",
        ],
        capture_output=True,
        check=True,
    ).stdout
    samples = array("h", raw)
    if abs(len(samples) / 96000 - 30) > 0.05:
        raise ValueError("Final audio must span 30 seconds")
    checks = []
    for clip in timeline:
        if clip["group"] == "opening" and clip["timeline_start"] < 3.6:
            continue  # Android launcher/file picker have no music
        segment = samples[round(clip["timeline_start"] * 96000) : round(clip["timeline_end"] * 96000)]
        rms = math.sqrt(sum(value * value for value in segment) / max(1, len(segment)))
        if rms < 10:
            raise ValueError(f"Missing game audio: {clip['label']}")
        checks.append(
            {
                "label": clip["label"],
                "rms_dbfs": round(20 * math.log10(rms / 32768), 2),
                "peak": max(map(abs, segment)),
                "clipped_fraction": sum(abs(value) >= 32767 for value in segment) / len(segment),
            }
        )
    save_json(output / "audio-validation.json", checks)
    return checks


def compose(output, recipe_path=None):
    output = Path(output)
    recipe_path = Path(recipe_path) if recipe_path else output / "edit.json"
    if not recipe_path.exists():
        save_json(recipe_path, default_recipe(output))
    recipe = json.loads(recipe_path.read_text())
    if abs(sum(c["seconds"] for c in recipe["clips"]) - 30) > 0.0001:
        raise ValueError("Edit must total exactly 30 seconds")
    encoded = output / "edit-clips"
    encoded.mkdir(exist_ok=True)
    paths = []
    timeline = []
    offset = 0
    for i, clip in enumerate(recipe["clips"]):
        source = output / clip["source"]
        info = video_info(source)
        if clip["start"] < 0 or clip["end"] <= clip["start"] or clip["end"] > info["duration"] + 0.05:
            raise ValueError(f"Invalid source interval: {clip}")
        count = round(clip["seconds"] * recipe["fps"])
        ratio = clip["seconds"] / (clip["end"] - clip["start"])
        w, h, fps = recipe["width"], recipe["height"], recipe["fps"]
        # Replays advance by recorded frames, not emulator wall time. The guest
        # recorder retains their VFR frames; restore their original cadence.
        timing = f"N/({clip['frame_rate']:.10f}*TB)" if "frame_rate" in clip else f"(PTS-STARTPTS)*{ratio:.10f}"
        filters = (
            (f"transpose={clip['transpose']}," if clip.get("transpose") else "") + f"setpts={timing},fps={fps},"
            f"scale={w}:{h}:force_original_aspect_ratio=decrease,"
            f"pad={w}:{h}:(ow-iw)/2:(oh-ih)/2,setsar=1,"
            "tpad=stop_mode=clone:stop_duration=1"
        )
        target = encoded / f"{i:02d}.mp4"
        ffmpeg(
            [
                "-ss",
                clip["start"],
                "-t",
                clip["end"] - clip["start"],
                "-i",
                source,
                "-vf",
                filters,
                "-frames:v",
                count,
                "-an",
                "-c:v",
                "libx264",
                "-preset",
                "fast",
                "-crf",
                "18",
                "-pix_fmt",
                "yuv420p",
                "-video_track_timescale",
                "15360",
                target,
            ],
            encoded / f"{i:02d}.log",
        )
        paths.append(target)
        timeline.append({**clip, "timeline_start": offset, "timeline_end": offset + clip["seconds"], "frames": count})
        offset += clip["seconds"]
    listing = encoded / "concat.txt"
    listing.write_text("".join(f"file '{p.name}'\n" for p in paths), encoding="utf-8")
    final = output / "store-preview-30s.mp4"
    soundtrack = render_audio(output, recipe["clips"], encoded)
    ffmpeg(
        [
            "-f",
            "concat",
            "-safe",
            "0",
            "-i",
            listing,
            "-i",
            soundtrack,
            "-map",
            "0:v:0",
            "-map",
            "1:a:0",
            "-c:v",
            "copy",
            "-c:a",
            "aac",
            "-b:a",
            "192k",
            "-t",
            "30",
            "-movflags",
            "+faststart",
            final,
        ],
        output / "encode.log",
    )
    info = video_info(final)
    count, seconds = imageio_ffmpeg.count_frames_and_secs(str(final))
    if count != 900 or abs(seconds - 30) > 0.04 or tuple(info["size"]) != (2400, 1350):
        raise ValueError(f"Unexpected final video: {count} frames, {seconds}s, {info}")
    save_json(
        output / "video-validation.json", {"frames": count, "seconds": seconds, "info": info, "timeline": timeline}
    )
    # The fly-out camera settles later, so audit its moving first two seconds.
    # Gameplay clips are audited in full, including their original 25 Hz cadence.
    audit_motion(output, timeline)
    audit_audio(output, timeline)
    # One thumbnail per final second makes all five sections easy to inspect
    video_stills = output / "video-stills"
    video_stills.mkdir(exist_ok=True)
    ffmpeg(["-i", final, "-vf", "fps=1", "-frames:v", "30", video_stills / "%02d.png"], output / "video-stills.log")
    contact_sheet(list(video_stills.glob("*.png")), output / "video-contact-sheet.jpg", columns=5)
    review(output)
    return final


def audit_motion(output, timeline):
    cadence = []
    for clip in timeline:
        if clip["group"] not in ("demo", "flyout"):
            continue
        duration = min(2, clip["seconds"]) if clip["group"] == "flyout" else clip["seconds"]
        cadence.append(
            {
                "label": clip["label"],
                "group": clip["group"],
                **motion_cadence(output / "store-preview-30s.mp4", clip["timeline_start"], duration),
            }
        )
    save_json(output / "motion-validation.json", cadence)
    return cadence


def contact_sheet(paths, destination, columns=4):
    width, height = 400, 225
    sheet = Image.new("RGB", (columns * width, ((len(paths) + columns - 1) // columns) * (height + 26)), "#141420")
    draw = ImageDraw.Draw(sheet)
    for i, path in enumerate(paths):
        with Image.open(path) as original:
            im = original.convert("RGB")
            im.thumbnail((width - 8, height))
            x = (i % columns) * width
            y = (i // columns) * (height + 26)
            sheet.paste(im, (x + (width - im.width) // 2, y + (height - im.height) // 2))
            draw.text((x + 6, y + height + 5), path.stem, fill="white")
    sheet.save(destination, quality=90)


def review(output):
    output = Path(output)
    sections = []
    launcher = list(sorted((output / "launcher").glob("*.png")))
    groups = [("Launcher", launcher)]
    briefing = sorted((output / "briefing").glob("*.png"))
    if briefing:
        groups.append(("In-engine D2 briefing / paired animation frames", briefing))
    for directory in sorted((output / "demos").glob("*")):
        if directory.is_dir():
            paths = list(sorted(directory.glob("*.png")))
            if paths:
                contact_sheet(paths, directory / "contact-sheet.jpg")
                groups.append((directory.name, paths))
    for title, paths in groups:
        cards = []
        for path in paths:
            with Image.open(path) as im:
                w, h = im.size
            url = path.relative_to(output).as_posix()
            cards.append(
                f'<a href="{html.escape(url)}"><img loading="lazy" src="{html.escape(url)}">'
                f"<span>{html.escape(path.stem)} &middot; {w} &times; {h}</span></a>"
            )
        sections.append(f'<h2>{html.escape(title)}</h2><div class="grid">{"".join(cards)}</div>')
    video = (
        '<video controls preload="metadata" src="store-preview-30s.mp4"></video>'
        if (output / "store-preview-30s.mp4").exists()
        else ""
    )
    document = (
        '<!doctype html><html lang="en"><meta charset="utf-8"><title>DXX-Revival store asset review</title>'
        "<style>body{background:#101018;color:#eee;font:16px system-ui;margin:40px auto;max-width:1500px;padding:0 24px}"
        "h1{color:#cfbaff}h2{margin-top:40px;font-size:20px}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(320px,1fr));gap:18px}"
        "a{color:#ddd;text-decoration:none;background:#242232;border-radius:8px;overflow:hidden}img{width:100%;height:220px;object-fit:contain}"
        "span{display:block;padding:12px}video{width:100%;max-height:760px}p{color:#bbb;line-height:1.6}</style>"
        "<h1>DXX-Revival / store asset review</h1><p>Real Android captures. Click any image for the original PNG. "
        "The 30-second video includes engine MIDI and sound effects: 9 seconds of setup, three 5-second demo excerpts, then 6 seconds of D1 fly-out. "
        "The game display is preserved in full with padding for a 16:9 video.</p>"
        + video
        + "".join(sections)
        + "</html>"
    )
    (output / "index.html").write_text(document, encoding="utf-8")


def validate(output):
    """End-to-end evidence check; no fabricated media or mocked app state."""
    output = Path(output)
    root = Path(__file__).resolve().parents[2]
    state = json.loads((output / "launcher/top-state.json").read_text())
    if not state.get("resume_candidate", {}).get("has_thumbnail"):
        raise ValueError("Launcher capture has no populated save")
    checks = {"launcher_images": 0, "demo_images": 0, "demos": [], "video": None}
    for name in ("01-top.png", "02-bottom.png", "03-save-explorer.png"):
        with Image.open(output / "launcher" / name) as im:
            im.verify()
            if im.size != (1080, 2400):
                raise ValueError(f"Unexpected launcher size: {im.size}")
        checks["launcher_images"] += 1
    games = set()
    for path in sorted((output / "demos").glob("*/capture.json")):
        capture = json.loads(path.read_text())
        source = root / capture["source"]
        expected = None
        with source.open(encoding="utf-8") as stream:
            for line in stream:
                record = json.loads(line)
                if record["type"] == "result":
                    expected = record["result"]
        if expected is None or capture.get("replay_result") != expected:
            raise ValueError(f"Replay did not match its recorded result: {source.name}")
        start_state = json.loads((path.parent / "start-state.json").read_text())
        if not start_state["hud_layout"]["show_robot_hostage_counts"]:
            raise ValueError("HUD counter defaults were not applied")
        for item in capture["screenshots"]:
            with Image.open(output / item["file"]) as im:
                im.verify()
                if im.size != (2400, 1080):
                    raise ValueError(f"Unexpected game size: {im.size}")
            checks["demo_images"] += 1
        # Polling may land just after a ten-second boundary; do not silently
        # accept missing screenshots or an aborted source recording
        expected_targets = list(range(10, int(capture["duration"]), 10))
        actual_targets = [round(s["target_seconds"]) for s in capture["screenshots"] if s["target_seconds"]]
        if expected_targets != actual_targets:
            raise ValueError(f"Missing ten-second samples: {source.name}")
        if not capture["samples"] or capture["samples"][-1]["demo_seconds"] < capture["duration"] - 1:
            raise ValueError(f"Incomplete video source: {source.name}")
        games.add(capture["game"])
        checks["demos"].append({"name": source.name, "result_match": True, "screenshots": len(capture["screenshots"])})
    if games != {"d1", "d2"} or len(checks["demos"]) < 3:
        raise ValueError("Review requires at least three recordings spanning D1 and D2")
    opening = json.loads((output / "opening.json").read_text())
    required = {
        "Launcher",
        "Pick game data",
        "Imported game data",
        "Launcher bottom",
        "Created pilot",
        "In level",
        "End",
    }
    if not required.issubset({b["label"] for b in opening["beats"]}):
        raise ValueError("Opening flow is incomplete")
    video = output / "store-preview-30s.mp4"
    info = video_info(video)
    frames, seconds = imageio_ffmpeg.count_frames_and_secs(str(video))
    if frames != 900 or abs(seconds - 30) > 0.04 or tuple(info["size"]) != (2400, 1350):
        raise ValueError("Final video must decode to 900 frames at 2400x1350 in 30 seconds")
    checks["video"] = {"frames": frames, "seconds": seconds, "width": 2400, "height": 1350, "audio": "engine-pcm"}
    timeline = json.loads((output / "video-validation.json").read_text())["timeline"]
    checks["audio"] = audit_audio(output, timeline)
    assets = json.loads((output / "game-data.json").read_text())
    if not any(asset["name"] == "robots-h.mvl" for asset in assets):
        raise ValueError("D2 robot briefing movies were not supplied")
    checks["briefing"] = []
    for page in range(1, 4):
        state = json.loads((output / f"briefing/page-{page}-state.json").read_text())
        if not state["music"].get("active") or state["music"].get("song_playing") != 1:
            raise ValueError(f"Engine briefing MIDI not playing on page {page}")
        with (
            Image.open(output / f"briefing/page-{page}.png") as first,
            Image.open(output / f"briefing/page-{page}-later.png") as later,
        ):
            # Fixed capture geometry: movie region excludes the text and buttons
            w, h = first.size
            region = (int(w * 0.52), int(h * 0.35), int(w * 0.8), int(h * 0.9))
            difference = ImageStat.Stat(ImageChops.difference(first.convert("RGB"), later.convert("RGB")).crop(region))
            motion = sum(difference.mean) / 3
            if motion < 0.05:
                raise ValueError(f"No robot animation visible on briefing page {page}")
        checks["briefing"].append({"page": page, "midi_song": "briefing.hmp", "movie_region_difference": motion})
    cadence = audit_motion(output, timeline)
    if len(cadence) != 4:
        raise ValueError("Expected three gameplay motion checks and one fly-out check")
    for item in cadence:
        minimum = 27 if item["group"] == "flyout" else 23
        if item["visible_updates_per_second"] < minimum or item["longest_hold_seconds"] > 0.14:
            raise ValueError(f"Visible frame pacing failed: {item}; inspect motion-validation.json")
    checks["motion"] = cadence
    save_json(output / "validation.json", checks)
    return checks
