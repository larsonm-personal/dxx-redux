"""Verify original Mac media, GOG replacement, cleanup, Guide-Bot and audible D1 effects."""

import argparse
import array
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import wave

ROOT = Path(__file__).resolve().parents[2]
SET = "mac_edition_assets_test"
SCRATCH = "/data/local/tmp/dxx-mac-editions"


def command(action, **args):
    return {"action": "setup_command", "command": action, "args": args}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", required=True)
    parser.add_argument("--package", required=True)
    parser.add_argument("--adb", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    adb = [args.adb, "-s", args.serial]

    def device(*parts, timeout=240):
        return subprocess.check_output([*adb, *parts], timeout=timeout)

    def read_app(path):
        return device("exec-out", "run-as", args.package, "cat", path)

    def run(name, steps):
        script = output / f"{name}.json"
        script.write_text(
            json.dumps(
                [
                    {"_info": {"games": ["d2"], "_standalone": False, "_owner": "test_mac_edition_assets"}},
                    {"action": "enter_launcher"},
                    *steps,
                ],
                indent=2,
            )
            + "\n",
            encoding="utf-8",
        )
        with (output / f"{name}.log").open("w", encoding="utf-8") as log:
            result = subprocess.run(
                [
                    "pwsh",
                    "-NoProfile",
                    "-File",
                    str(ROOT / "android/helpers/run_test.ps1"),
                    "-ScriptName",
                    str(script),
                    "-Game",
                    "d2",
                    "-Serial",
                    args.serial,
                    "-LeaveRunning",
                    "-PreserveState",
                    "-TimeoutSeconds",
                    "480",
                ],
                cwd=ROOT,
                stdout=log,
                stderr=subprocess.STDOUT,
                timeout=540,
                check=False,
            )
        durable = read_app("files/automation_result.json")
        (output / f"{name}-result.json").write_bytes(durable)
        (output / f"{name}-logcat.txt").write_bytes(device("logcat", "-d", "-t", "4000"))
        if result.returncode:
            raise RuntimeError(f"{name} failed; see {output / (name + '.log')}")

    def stage_file(source, name):
        device("push", str(source), f"{SCRATCH}/{name}")
        return f"{SCRATCH}/{name}"

    def import_cd(label, source, stem):
        cue = source / f"{stem}.cue"
        binary = source / f"{stem}.bin"
        staged = output / f"{label}.cue"
        staged.write_text(re.sub(r'FILE "[^"]+"', f'FILE "{label}.bin"', cue.read_text()), encoding="ascii")
        stage_file(staged, f"{label}.cue")
        stage_file(binary, f"{label}.bin")
        run(
            label,
            [
                command(
                    "import_cd",
                    cue_path=f"{SCRATCH}/{label}.cue",
                    bin_path=f"{SCRATCH}/{label}.bin",
                    include_audio=False,
                ),
                {"action": "wait_for", "field": "import_state.status", "value": "complete", "timeout_ms": 240000},
            ],
        )
        device("shell", "rm", "-f", f"{SCRATCH}/{label}.cue", f"{SCRATCH}/{label}.bin")

    def put_asset(source, name):
        remote = stage_file(source, "asset")
        device("shell", "run-as", args.package, "cp", remote, f"files/imported/sets/{SET}/{name}")

    def audit(name, clear=False):
        run(name, [command("clear_base_assets" if clear else "audit_base_assets", game="d2", confirmed=clear)])
        data = read_app("files/base_asset_audit.json")
        (output / f"{name}-audit.json").write_bytes(data)
        return json.loads(data)

    config = json.loads(
        device(
            "exec-out",
            "run-as",
            args.package,
            "sh",
            "-c",
            "if [ -f files/file_sets.json ]; then cat files/file_sets.json; else echo '{}'; fi",
        )
    )
    previous = config.get("active", "default")
    if any(item["name"] == SET for item in config.get("sets", [])):
        raise RuntimeError(f"Remove the previous test-owned set {SET} before retrying")
    media = ROOT / "game_data/CD images"
    gog = ROOT / "game_data/gog installers/setup_descent_2_1.1_(16596).exe"
    if not gog.is_file():
        raise RuntimeError("The specified GOG installer is required")
    created = False
    try:
        device("shell", "mkdir", "-p", SCRATCH)
        device("logcat", "-c")
        run("create", [command("create_set", name=SET), command("switch_set", name=SET)])
        created = True
        for name in ("d2demo.hog", "d2demo.ham", "d2demo.pig"):
            put_asset(ROOT / "game_data/shareware/d2" / name, name)
        import_cd("d1-mac", media / "Descent - Mac macplay", "Descent - Mac macplay")
        import_cd("d2-mac", media / "d2 mac", "Descent_II")
        mixed = audit("demo-and-mac")
        if not mixed["reasons"]:
            raise RuntimeError("Demo/retail mixture was not detected")
        gameplay = [
            {"action": "enter_game", "game": "d1-in-d2"},
            {"action": "skip_intro", "timeout_ms": 30000},
            {"action": "select", "text": "Ok", "timeout_ms": 15000, "optional": True},
            {"action": "select", "text": "player", "timeout_ms": 5000, "optional": True, "post_delay_ms": 200},
            {"action": "wait_for", "field": "screen_mode", "value": "menu", "timeout_ms": 15000},
            {"action": "select", "text": "New game", "timeout_ms": 10000},
            {"action": "select_mission", "text": "first strike", "timeout_ms": 10000},
            {"action": "select", "text": "Ok", "timeout_ms": 10000},
            {"action": "select", "text": "Trainee", "timeout_ms": 10000},
            {"action": "skip_briefing", "timeout_ms": 60000},
            {"action": "wait_for", "field": "in_game", "value": "true", "timeout_ms": 30000},
            {"action": "assert", "expect": {"asset_trace.mode": "d1-in-d2", "current_level_num": 1}},
            {"action": "meta_action", "id": 1037, "post_delay_ms": 200},
            {"action": "meta_action", "id": 1037, "post_delay_ms": 200},
            {"action": "wait_for", "expect": {"guidebot.companion_count": 1}, "timeout_ms": 15000},
            {"action": "set_debug", "field": "audio_effects_volume", "value": "8"},
            {"action": "set_debug", "field": "audio_music_volume", "value": "0"},
            {"action": "set_debug", "field": "capture_audio", "value": "start"},
            {"action": "set_debug", "field": "audio_effects_probe", "value": "laser"},
            {"action": "wait_ms", "ms": 1200},
            {"action": "set_debug", "field": "audio_effects_probe", "value": "explosion"},
            {"action": "wait_ms", "ms": 2000},
            {"action": "set_debug", "field": "capture_audio", "value": "stop"},
            {"action": "introspect"},
        ]
        run("mac-gameplay", gameplay)
        for name in ("store-audio.wav", "store-music.wav", "introspect.json"):
            (output / name).write_bytes(read_app("files/" + name))
        with wave.open(str(output / "store-audio.wav")) as wav:
            if wav.getsampwidth() != 2 or wav.getframerate() != 48000:
                raise RuntimeError("Unexpected effects capture format")
            samples = array.array("h", wav.readframes(wav.getnframes()))
            if sys.byteorder != "little":
                samples.byteswap()
            if not samples or max(map(abs, samples)) < 100:
                raise RuntimeError("Mac D1 effects are silent")
        stage_file(gog, "gog.exe")
        run("gog-over-mac", [command("import_gog", path=f"{SCRATCH}/gog.exe", include_audio=False)])
        listing = device("shell", "run-as", args.package, "ls", f"files/imported/sets/{SET}").decode().split()
        for name in ("groupa.pig", "alien1.pig", "alien2.pig", "fire.pig", "ice.pig", "water.pig"):
            if [item for item in listing if item.lower() == name] != [name]:
                raise RuntimeError(f"Case variants survived GOG publication: {name}")
        pc_pig = read_app(f"files/imported/sets/{SET}/groupa.pig")
        if len(pc_pig) != 4931752:
            raise RuntimeError("GOG textures did not replace the Mac textures")
        # Recreate a pre-fix installation with both spellings on Android's filesystem
        device(
            "shell",
            "run-as",
            args.package,
            "cp",
            f"files/imported/sets/{SET}/groupa.pig",
            f"files/imported/sets/{SET}/GROUPA.PIG",
        )
        put_asset(media / "d2 mac/data_tracks/groupa.pig", "groupa.pig")
        sentinel = output / "sentinel.txt"
        sentinel.write_text("Preserve user content\n", encoding="utf-8")
        for name in ("keep.plr", "keep.sg0", "custom.hog", "custom.ogg"):
            put_asset(sentinel, name)
        cleared = audit("clear-mixed", clear=True)
        if cleared["after"] or not any("conflicting copies" in reason for reason in cleared["reasons"]):
            raise RuntimeError("Existing conflicting copies were not detected and cleared")
        for name in ("keep.plr", "keep.sg0", "custom.hog", "custom.ogg"):
            # The content manager may adopt added missions/music into its owned directory
            paths = (
                device(
                    "shell", "run-as", args.package, "find", f"files/imported/sets/{SET}", "-type", "f", "-name", name
                )
                .decode()
                .splitlines()
            )
            if not paths or not any(read_app(path) == sentinel.read_bytes() for path in paths):
                raise RuntimeError(f"Cleanup changed unrelated content: {name}")
        if len(read_app(f"files/imported/sets/{SET}/descent.pig")) != 3975533:
            raise RuntimeError("D2 cleanup changed the D1 installation")
        run("clean-gog-import", [command("import_gog", path=f"{SCRATCH}/gog.exe", include_audio=False)])
        clean = audit("clean-gog")
        if clean["reasons"]:
            raise RuntimeError("Clean GOG installation was incorrectly reported as mixed")
        if (
            hashlib.sha256(read_app(f"files/imported/sets/{SET}/groupa.pig")).digest()
            != hashlib.sha256(pc_pig).digest()
        ):
            raise RuntimeError("Clean reimport changed the GOG textures")
        (output / "result.json").write_text(
            json.dumps(
                {
                    "status": "PASS",
                    "mac_guidebot": True,
                    "mac_effects": True,
                    "case_replacement": True,
                    "reviewed_cleanup": True,
                    "clean_reimport": True,
                },
                indent=2,
            )
            + "\n"
        )
    finally:
        if created:
            run("restore-set", [command("switch_set", name=previous), command("delete_set", name=SET)])
        device("shell", "rm", "-rf", SCRATCH)


if __name__ == "__main__":
    main()
