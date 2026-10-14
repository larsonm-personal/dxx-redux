#!/usr/bin/env python3
"""Music-only SDL_mixer oracles for the native headed configuration campaign.

This checks actual postmix PCM, not Android ymfm/TSF or device speaker output.
The session must be in a quiet menu with DigiVolume=0; each observation discards
250 ms after stop, or 1.5 s for a synthesizer release tail after mute,
then measures a one-second PCM window, allowing a bounded 5 s mute decay.
FluidSynth dither up to one int16
mean absolute unit is allowed only when muted; stopped output must be zero.
"""

import random
import time

if not __debug__:
    raise RuntimeError("Music oracles require Python assertions; do not use -O/PYTHONOPTIMIZE")


class MusicUnavailable(RuntimeError):
    """The requested source/backend could not play; never counted as a pass."""


def pcm_window(before, after):
    """Return rendered PCM deltas and reject absent/stalled/reset callbacks."""
    a, b = before["music"], after["music"]
    if not b["audio_open"]:
        raise MusicUnavailable("SDL_mixer audio device is not open")
    delta = {key: b[key] - a[key] for key in ("pcm_samples", "pcm_nonzero", "pcm_absolute")}
    if delta["pcm_samples"] <= 0:
        raise MusicUnavailable("Postmix callback produced no PCM samples")
    assert all(value >= 0 for value in delta.values()), "PCM counters reset within window"
    assert delta["pcm_nonzero"] <= delta["pcm_samples"], "Invalid PCM counters"
    if "pcm_positive_crossings" in b:
        delta["positive_crossings"] = b["pcm_positive_crossings"] - a["pcm_positive_crossings"]
        delta["positive_crossing_hz"] = (
            delta["positive_crossings"] * b["frequency"] * b["channels"] / delta["pcm_samples"]
        )
    delta["peak_last_second"] = b.get("pcm_peak_1s")
    delta["mean_absolute"] = delta["pcm_absolute"] / delta["pcm_samples"]
    return delta


def check_music_window(before, after, *, audible, stopped=False):
    """Validate a settled quiet-menu sample using runtime state and real PCM."""
    cfg, music = after["config"], after["music"]
    assert cfg["DigiVolume"] == 0, "Music PCM oracle requires muted sound effects"
    delta = pcm_window(before, after)
    assert music["volume"] == cfg["MusicVolume"] * 16, "Mixer/config volume mismatch"
    if stopped:
        assert not music["playing"], "Music still playing after no-music selection"
        assert music["song"] == -1, "Song bookkeeping not cleared after stop"
    if audible:
        if not music["playing"] or music["play_result"] != 0:
            raise MusicUnavailable(f"Track unavailable: {music['last_track']!r}, result={music['play_result']}")
        assert music["last_track"], "Playing music has no requested source"
        assert not music["paused"], "Menu music unexpectedly paused"
        assert delta["mean_absolute"] > 1, "Playing unmuted music produced only silence/dither"
    else:
        if stopped:
            assert delta["pcm_nonzero"] == 0, "Stopped music leaked nonzero PCM"
        else:
            assert delta["mean_absolute"] <= 1, "Muted music exceeded the FluidSynth dither floor"
    return delta


def observe(session, *, audible, stopped=False):
    started = time.monotonic()
    time.sleep(1.5 if not audible and not stopped else 0.25)
    before = session.wait(lambda state: "music" in state, "music sample start")
    while True:
        time.sleep(1.0)
        after = session.wait(
            lambda state: "music" in state and state["ticks"] - before["ticks"] >= 950, "music sample end"
        )
        pcm = pcm_window(before, after)
        assert pcm["pcm_samples"] >= after["music"]["frequency"] * after["music"]["channels"] / 2, (
            "PCM window shorter than half a second"
        )
        elapsed = time.monotonic() - started
        session.record(
            "music_window", before=before, after=after, audible=audible, stopped=stopped, elapsed_seconds=elapsed
        )
        # FluidSynth's reverb tail is score-dependent even after its gain is zero
        # Keep the measured floor fixed and require a full settled window by 5 s
        if not audible and not stopped and pcm["mean_absolute"] > 1 and elapsed + 1 < 5:
            session.record("music_tail_wait", mean_absolute=pcm["mean_absolute"], elapsed_seconds=elapsed)
            before = after
            continue
        if audible:
            try:
                check_music_window(before, after, audible=False)
            except AssertionError:
                session.record("negative_control", result="unmuted PCM rejected by mute oracle")
            else:
                raise AssertionError("Mute oracle accepted intentionally unmuted PCM")
        return {
            "before": before["music"],
            "after": after["music"],
            "settle_seconds": elapsed,
            "pcm": check_music_window(before, after, audible=audible, stopped=stopped),
        }


def exercise_menu_music(session, seed=0, rounds=1):
    """Drive the real sound menu. Caller owns launch, watchdog, logs and relaunch."""
    rng = random.Random(seed)
    results = []
    volume_order = [1, 2, 4, 7, 8]
    rng.shuffle(volume_order)
    session.select("FX Volume", exact=True)
    for _ in range(9):
        session.key("left")
    assert session.snapshot()["config"]["DigiVolume"] == 0
    for iteration in range(rounds):
        session.select("no music", exact=True)
        session.key("space")
        results.append({"case": "stop", "iteration": iteration, **observe(session, audible=False, stopped=True)})
        session.select("built-in/addon music", exact=True)
        session.key("space")
        session.select("music volume", exact=True)
        for _ in range(9):
            session.key("left")
        results.append({"case": "mute", "iteration": iteration, **observe(session, audible=False)})
        volume = volume_order[iteration % len(volume_order)]
        for _ in range(volume):
            session.key("right")
        results.append({"case": "builtin", "iteration": iteration, "volume": volume, **observe(session, audible=True)})
    return results


def make_jukebox_fixture(path, frequency=440, seconds=4):
    """Create a deterministic valid PCM WAV for native source switching tests."""
    import math
    import struct
    import wave

    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(22050)
        output.writeframes(
            b"".join(
                struct.pack("<h", round(8000 * math.sin(2 * math.pi * frequency * i / 22050)))
                for i in range(22050 * seconds)
            )
        )


def check_jukebox_waveform(observation, frequency=440):
    """Independent decoded-source and gain check for the known 440 Hz fixture."""
    import math

    pcm = observation["pcm"]
    if "positive_crossing_hz" not in pcm:
        raise MusicUnavailable("PCM zero-crossing diagnostic missing; rebuild test binary")
    assert abs(pcm["positive_crossing_hz"] - frequency) < 5, f"Decoded jukebox waveform is not {frequency} Hz"
    expected_mean = (8000 * 2 / math.pi) * observation["after"]["volume"] / 128
    assert abs(pcm["mean_absolute"] - expected_mean) < expected_mean * 0.05, "Jukebox PCM gain mismatch"


def exercise_jukebox_switch(session, expected_track):
    """Require caller to configure CMMiscMusic0 to the generated valid WAV."""
    session.select("jukebox", exact=True)
    session.key("space")
    result = observe(session, audible=True)
    assert result["after"]["last_track"] == str(expected_track), "Wrong jukebox source"
    check_jukebox_waveform(result)
    session.select("no music", exact=True)
    session.key("space")
    stopped = observe(session, audible=False, stopped=True)
    session.select("built-in/addon music", exact=True)
    session.key("space")
    builtin = observe(session, audible=True)
    assert builtin["after"]["last_track"] != str(expected_track), "Jukebox source stayed active"
    return [{"case": "jukebox", **result}, {"case": "jukebox-stop", **stopped}, {"case": "builtin-restore", **builtin}]


def run_case(executable, data, directory, game="d1", seed=0, rounds=1, timeout=180):
    from pathlib import Path
    from host_config_campaign import HostSession, freeze_binary

    directory = Path(directory).resolve()
    directory.mkdir(parents=True, exist_ok=True)
    # Freeze the complete executable so concurrent build links cannot race relaunch
    frozen_executable = freeze_binary(executable, directory)
    fixture = directory / "jukebox.wav"
    make_jukebox_fixture(fixture)
    session = HostSession(
        frozen_executable,
        data,
        directory,
        game,
        timeout,
        config={
            "ResolutionX": 640,
            "ResolutionY": 480,
            "WindowMode": 1,
            "MusicType": 0,
            "DigiVolume": 0,
            "MusicVolume": 8,
            "CMMiscMusic0": fixture,
        },
    )
    try:
        session.start()
        session.enter_options()
        session.open_menu("sound effects & music")
        results = exercise_menu_music(session, seed, rounds)
        results += exercise_jukebox_switch(session, fixture)
        expected = {key: session.snapshot()["config"][key] for key in ("MusicType", "MusicVolume", "DigiVolume")}
        session.back_to_options()
        session.key("escape")
        results.append({"case": "main-menu-restore", **observe(session, audible=True)})
        session.close()
        session.start()
        session.assert_config(expected)
        results.append({"case": "relaunch-persist", **observe(session, audible=True)})
        session.close()
        return results
    finally:
        session.stop()


def run_missing_source_case(executable, data, directory, game="d1", timeout=120):
    """Expected-negative source load, followed by real-menu builtin recovery."""
    from pathlib import Path
    from host_config_campaign import HostSession, freeze_binary

    directory = Path(directory).resolve()
    directory.mkdir(parents=True, exist_ok=True)
    frozen = freeze_binary(executable, directory)
    missing = directory / "intentionally-missing.wav"
    assert not missing.exists(), "Expected-negative source must not exist"
    session = HostSession(
        frozen,
        data,
        directory,
        game,
        timeout,
        config={
            "ResolutionX": 640,
            "ResolutionY": 480,
            "WindowMode": 1,
            "MusicType": 0,
            "DigiVolume": 0,
            "MusicVolume": 8,
            "CMMiscMusic0": missing,
        },
    )
    try:
        session.start()
        session.enter_options()
        session.open_menu("sound effects & music")
        session.set_item("jukebox", 1, exact=True)
        failed = observe(session, audible=False, stopped=True)
        assert failed["after"]["play_result"] == -1, "Missing source did not report load failure"
        assert failed["after"]["last_track"] == str(missing), "Wrong source tested"
        session.set_item("built-in/addon music", 1, exact=True)
        recovered = observe(session, audible=True)
        session.back_to_options()
        session.key("escape")
        session.close()
        return [{"case": "missing-source-rejected", **failed}, {"case": "builtin-after-missing-source", **recovered}]
    finally:
        session.stop()


def exercise_playback_shortcuts(session):
    baseline = session.wait(lambda state: state.get("kind") == "game", "game before music shortcuts")["music"]
    session.key("f10", modifiers=0x303)
    session.wait(lambda state: state["music"]["paused"] == 1, "native music paused")
    paused = observe(session, audible=False)
    assert paused["pcm"]["pcm_nonzero"] == 0, "Paused music leaked PCM"
    session.key("f10", modifiers=0x303)
    resumed = observe(session, audible=True)
    assert resumed["after"]["requests"] == baseline["requests"], "Resume restarted source"
    assert resumed["after"]["last_track"] == baseline["last_track"], "Resume changed source"
    # Native builtin mode ignores previous/next offsets; only jukebox/CD use them
    session.key("f11", modifiers=0x303)
    session.key("f12", modifiers=0x303)
    skipped = observe(session, audible=True)
    assert skipped["after"]["requests"] == baseline["requests"], "Builtin next/previous unexpectedly restarted source"
    assert skipped["after"]["last_track"] == baseline["last_track"], "Builtin next/previous changed source"
    return [
        {"case": "native-pause", **paused},
        {"case": "native-resume", **resumed},
        {"case": "builtin-offset-ignored", **skipped},
    ]


def check_midi_source(actual, reference):
    """Compare actual converted input bytes with an independently captured native reference."""
    if not actual.get("midi_bytes") or not reference.get("midi_bytes"):
        raise MusicUnavailable("Converted MIDI source fingerprint missing")
    assert actual["midi_bytes"] == reference["midi_bytes"] and actual["midi_fnv1a64"] == reference["midi_fnv1a64"], (
        "Wrong builtin MIDI source payload"
    )


def run_game_case(
    executable, data, directory, game="d1", mission=None, version=None, timeout=180, reference_report=None
):
    """Observe actual level music across options and native save/load transitions."""
    from pathlib import Path
    from host_config_campaign import HostSession, freeze_binary

    directory = Path(directory).resolve()
    directory.mkdir(parents=True, exist_ok=True)
    frozen = freeze_binary(executable, directory)
    session = HostSession(
        frozen,
        data,
        directory,
        game,
        timeout,
        config={
            "ResolutionX": 640,
            "ResolutionY": 480,
            "WindowMode": 1,
            "MusicType": 1,
            "DigiVolume": 0,
            "MusicVolume": 4,
        },
    )
    try:
        session.start()
        state = session.start_level(mission=mission, version=version)
        session.record(
            "music_mission_identity", engine=state["engine"], mission=state["mission"], level=state["game"]["level"]
        )
        initial = observe(session, audible=True)
        references = {}
        if reference_report:
            import json

            references = {
                case["case"]: case["after"] for case in json.loads(Path(reference_report).read_text())["cases"]
            }
            session.record("source_reference", actual=initial["after"], expected=references["level-entry"])
            check_midi_source(initial["after"], references["level-entry"])
        assert initial["after"]["song"] >= 5, "Level did not select a level song"
        results = [{"case": "level-entry", "engine": state["engine"], "mission": state["mission"], **initial}]
        results += exercise_playback_shortcuts(session)
        session.save_first_slot("musicqa")
        session.enter_game_options()
        session.open_menu("sound effects & music")
        session.set_item("no music", 1, exact=True)
        results.append({"case": "in-game-stop", **observe(session, audible=False, stopped=True)})
        session.back_to_options()
        session.key("escape")
        session.wait(lambda state: state["kind"] == "game", "game after music settings")
        session.load_first_slot("musicqa")
        # Native saves do not capture Android's per-save music source preferences
        session.assert_config({"MusicType": 0, "DigiVolume": 0, "MusicVolume": 4})
        results.append({"case": "native-load-keeps-global-music-off", **observe(session, audible=False, stopped=True)})
        session.enter_game_options()
        session.open_menu("sound effects & music")
        session.set_item("built-in/addon music", 1, exact=True)
        restored = observe(session, audible=True)
        assert restored["after"]["last_track"] == initial["after"]["last_track"], "Restored level used wrong song"
        assert restored["after"]["song"] == initial["after"]["song"], "Restored level song number changed"
        results.append({"case": "level-music-restored", **restored})
        session.back_to_options()
        session.key("escape")
        session.wait(lambda state: state["kind"] == "game", "game before leaving")
        session.leave_game()
        title = observe(session, audible=True)
        if references:
            check_midi_source(title["after"], references["title-music-after-level"])
        assert title["after"]["song"] == 0, "Leaving level did not restore title song"
        results.append({"case": "title-music-after-level", **title})
        session.close()
        return results
    finally:
        session.stop()


def run_playlist_case(executable, data, directory, game="d1", timeout=180):
    """Continuous, per-level and random playlist transitions with known PCM tones."""
    from pathlib import Path
    from host_config_campaign import HostSession, freeze_binary

    directory = Path(directory).resolve()
    directory.mkdir(parents=True, exist_ok=True)
    frozen = freeze_binary(executable, directory)
    playlist = directory / "playlist"
    playlist.mkdir(exist_ok=True)
    tracks = {str(playlist / "a440.wav"): 440, str(playlist / "b660.wav"): 660}
    for path, frequency in tracks.items():
        make_jukebox_fixture(path, frequency, seconds=4)
    session = HostSession(
        frozen,
        data,
        directory,
        game,
        timeout,
        config={
            "ResolutionX": 640,
            "ResolutionY": 480,
            "WindowMode": 1,
            "MusicType": 3,
            "DigiVolume": 0,
            "MusicVolume": 4,
            "CMLevelMusicPath": str(playlist),
            "CMLevelMusicPlayOrder": 0,
            "CMMiscMusic0": next(iter(tracks)),
        },
    )
    results = []

    def sample(label, expected=None):
        observation = observe(session, audible=True)
        path = observation["after"]["last_track"]
        assert path in tracks, "Playlist selected an unrelated source"
        if expected is not None:
            assert tracks[path] == expected, "Wrong playlist track selected"
        assert observation["before"]["requests"] == observation["after"]["requests"], (
            "Track changed during waveform sample"
        )
        check_jukebox_waveform(observation, tracks[path])
        results.append({"case": label, "expected_frequency": tracks[path], **observation})
        return observation

    try:
        session.start()
        session.start_level()
        sample("playlist-level-entry", 440)
        session.key("f12", modifiers=0x303)
        sample("playlist-next", 660)
        session.key("f11", modifiers=0x303)
        previous = sample("playlist-previous", 440)
        session.wait(
            lambda state: state["music"]["requests"] > previous["after"]["requests"],
            "continuous playlist auto-advance",
            timeout=12,
        )
        sample("playlist-completion-advance", 660)
        session.enter_game_options()
        session.open_menu("sound effects & music")
        session.set_item("one track per level", 1, exact=True)
        per_level = sample("playlist-per-level", 440)
        time.sleep(9)
        looped = sample("playlist-per-level-loop", 440)
        assert looped["after"]["requests"] == per_level["after"]["requests"], "Per-level loop advanced playlist"
        session.set_item("random", 1, exact=True)
        sample("playlist-random-member")
        session.set_item("no music", 1, exact=True)
        results.append({"case": "playlist-stop", **observe(session, audible=False, stopped=True)})
        session.set_item("built-in/addon music", 1, exact=True)
        results.append({"case": "builtin-after-playlist", **observe(session, audible=True)})
        session.back_to_options()
        session.key("escape")
        session.wait(lambda state: state["kind"] == "game", "game after playlist settings")
        session.leave_game()
        session.close()
        return results
    finally:
        session.stop()


def main():
    import argparse
    import hashlib
    import json
    from pathlib import Path

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True)
    parser.add_argument("--data", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--game", choices=("d1", "d2"), default="d1")
    parser.add_argument("--seed", type=int, default=1014)
    parser.add_argument("--rounds", type=int, default=1)
    parser.add_argument("--missing-source", action="store_true")
    parser.add_argument("--gameplay", action="store_true")
    parser.add_argument("--playlist", action="store_true")
    parser.add_argument("--mission")
    parser.add_argument("--mission-version", type=int, choices=(1, 2))
    parser.add_argument("--reference-music-report")
    parser.add_argument("--timeout", type=int, default=180)
    args = parser.parse_args()
    report = {
        "game": args.game,
        "seed": args.seed,
        "rounds": args.rounds,
        "executable_sha256": hashlib.sha256(Path(args.executable).read_bytes()).hexdigest(),
        "data_directory": str(Path(args.data).resolve()),
        "mute_minimum_drain_seconds": 1.5,
        "mute_settle_deadline_seconds": 5,
        "sample_seconds": 1.0,
        "mute_mean_absolute_limit_int16": 1,
        "coverage": "native SDL_mixer PCM with muted SFX; not Android renderer or speaker output",
    }
    try:
        if args.playlist:
            report["cases"] = run_playlist_case(args.executable, args.data, args.output, args.game, args.timeout)
        elif args.gameplay:
            report["cases"] = run_game_case(
                args.executable,
                args.data,
                args.output,
                args.game,
                args.mission,
                args.mission_version,
                args.timeout,
                args.reference_music_report,
            )
        elif args.missing_source:
            report["cases"] = run_missing_source_case(args.executable, args.data, args.output, args.game, args.timeout)
        else:
            report["cases"] = run_case(
                args.executable, args.data, args.output, args.game, args.seed, args.rounds, args.timeout
            )
        report["executable_sha256"] = json.loads((args.output / "binary.json").read_text())["sha256"]
        report["status"] = "passed"
    except MusicUnavailable as error:
        report.update(status="blocked", error=str(error))
    except Exception as error:
        report.update(status="failed", error=f"{type(error).__name__}: {error}")
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "music-report.json").write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0 if report["status"] == "passed" else 2 if report["status"] == "blocked" else 1


if __name__ == "__main__":
    raise SystemExit(main())
