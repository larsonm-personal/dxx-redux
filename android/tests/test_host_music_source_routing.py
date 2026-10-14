#!/usr/bin/env python3
"""Public-shareware MIDI provenance and loose soundtrack override regression."""

import argparse
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).parents[1] / "helpers"))
from host_config_campaign import HostSession, freeze_binary
from host_music_campaign import check_jukebox_waveform, check_midi_source, make_jukebox_fixture, observe


def native_reference(path):
    return {case["case"]: case["after"] for case in json.loads(Path(path).read_text())["cases"]}


def run_cycle(executable, data, directory, references):
    directory.mkdir(parents=True, exist_ok=True)
    session = HostSession(
        freeze_binary(executable, directory),
        data,
        directory,
        "d2",
        150,
        config={
            "MusicType": 1,
            "MusicVolume": 4,
            "DigiVolume": 0,
            "WindowMode": 1,
            "ResolutionX": 640,
            "ResolutionY": 480,
        },
    )
    observations = []
    try:
        session.start()
        initial = observe(session, audible=True)
        check_midi_source(initial["after"], references[2]["title-music-after-level"])
        observations.append({"case": "initial-d2-title", **initial})
        for version in (1, 2, 1):
            mission = "Descent Demo" if version == 1 else "Descent 2 Demo"
            state = session.start_level(mission, version=version)
            level = observe(session, audible=True)
            session.record("source_reference", actual=level["after"], expected=references[version]["level-entry"])
            check_midi_source(level["after"], references[version]["level-entry"])
            observations.append({"case": "level", "mission": state["mission"], **level})
            session.leave_game()
            title = observe(session, audible=True)
            check_midi_source(title["after"], references[version]["title-music-after-level"])
            observations.append({"case": "title-return", "content_version": version, **title})
        session.close()
        return observations
    finally:
        session.stop()


def run_override(executable, data, directory):
    directory.mkdir(parents=True, exist_ok=True)
    session = HostSession(
        freeze_binary(executable, directory),
        data,
        directory,
        "d2",
        120,
        config={
            "MusicType": 1,
            "MusicVolume": 4,
            "DigiVolume": 0,
            "WindowMode": 1,
            "ResolutionX": 640,
            "ResolutionY": 480,
        },
    )
    make_jukebox_fixture(session.profile / "tone.wav")
    # A valid authored soundtrack overrides all five special songs and level one
    (session.profile / "dxx-r.sng").write_text("tone.wav\n" * 6)
    observations = []
    try:
        session.start()
        title = observe(session, audible=True)
        check_jukebox_waveform(title)
        observations.append({"case": "loose-title-override", **title})
        state = session.start_level("Descent Demo", version=1)
        level = observe(session, audible=True)
        check_jukebox_waveform(level)
        assert level["after"]["last_track"] == "tone.wav", "Loose soundtrack source lost precedence"
        observations.append({"case": "loose-d1-level-override", "mission": state["mission"], **level})
        session.leave_game()
        title = observe(session, audible=True)
        check_jukebox_waveform(title)
        observations.append({"case": "loose-title-return", **title})
        session.close()
        return observations
    finally:
        session.stop()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True)
    parser.add_argument("--data", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--native-d1-report", required=True)
    parser.add_argument("--native-d2-report", required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    report = {
        "status": "failed",
        "scope": "native reference MIDI payloads, D1-D2-D1 source restoration and loose overrides",
    }
    try:
        references = {1: native_reference(args.native_d1_report), 2: native_reference(args.native_d2_report)}
        assert references[1]["level-entry"]["midi_fnv1a64"] != references[2]["level-entry"]["midi_fnv1a64"], (
            "Independent native references must distinguish sources"
        )
        report["cycle"] = run_cycle(args.executable, args.data, args.output / "cycle", references)
        report["override"] = run_override(args.executable, args.data, args.output / "override")
        report["status"] = "passed"
    except Exception as error:
        report["error"] = f"{type(error).__name__}: {error}"
    (args.output / "source-routing-report.json").write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
