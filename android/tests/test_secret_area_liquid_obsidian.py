"""Check Obsidian liquids; optionally exercise Vertigo/Obsidian loading and checkpoints."""

import argparse
import hashlib
import json
import os
import queue
import re
import subprocess
import threading
import time
from pathlib import Path


def validate(report):
    level = report["levels"][0]
    assert level["level_num"] == 13
    assert level["scanner_enabled"] and level["secret_areas_complete"]
    assert level["secret_count"] == 6, level["secret_count"]
    liquids = {tuple(secret["segments"]): secret for secret in level["secrets"] if secret["liquid_only"]}
    assert set(liquids) == {(154,), (282,)}, set(liquids)
    for segments, entrance, items in (((154,), (144, 3), {21, 30}), ((282,), (203, 3), {23})):
        secret = liquids[segments]
        assert {(entry["seg"], entry["side"]) for entry in secret["entrances"]} == {entrance}
        assert {item["id"] for item in secret["items"]} == items
    identities = [secret["identity"] for secret in level["secrets"]]
    assert len(set(identities)) == 6 and all(int(identity) for identity in identities)
    # Transparent water at 58/71 and 334/347 must never become liquid secrets
    assert not any(segment in {58, 71, 334, 347} for members in liquids for segment in members)



def mission_levels(path):
    """Read only the advertised level lists from the supplied installed descriptor."""
    lines = [line.split(";", 1)[0].strip() for line in path.read_text(
        encoding="ascii").rstrip("\x1a\r\n").splitlines()]
    sections = {}
    for index, line in enumerate(lines):
        key, separator, value = line.partition("=")
        key = key.strip().lower()
        if not separator or key not in {"num_levels", "num_secrets"}:
            continue
        assert key not in sections, "Duplicate descriptor level count"
        assert re.fullmatch(r"[0-9]+", value.strip()), "Invalid descriptor level count"
        count = int(value)
        assert (1 if key == "num_levels" else 0) <= count <= 100
        entries = lines[index + 1:index + 1 + count]
        assert len(entries) == count, "Truncated descriptor level list"
        sections[key] = entries
    assert set(sections) == {"num_levels", "num_secrets"}, "Missing descriptor level list"
    result = []
    for key, sign in (("num_levels", 1), ("num_secrets", -1)):
        for index, entry in enumerate(sections[key], 1):
            parts = [part.strip() for part in entry.split(",")]
            assert len(parts) == (1 if sign == 1 else 2), "Invalid descriptor level entry"
            assert re.fullmatch(r"[A-Za-z0-9_-]+\.rl2", parts[0], re.IGNORECASE)
            if sign == -1:
                assert re.fullmatch(r"[0-9]+", parts[1])
                assert 1 <= int(parts[1]) <= len(sections["num_levels"])
            result.append((sign * index, parts[0]))
    assert len({name.lower() for _, name in result}) == len(result), "Duplicate level file"
    return result


def validate_worker_result(report, request):
    assert report["schema"] == "dxx-level-metadata-v1"
    assert report["request_id"] == request["request_id"], "Foreign or duplicate worker result"
    assert report["status"] == "ok" and report["game"] == "d2"
    assert not report.get("worker_restart_required"), "Worker reuse was not preserved"
    assert report["source"] == request["source_name"]
    assert report["mission_filename"].lower() == request["mission_name"]
    assert len(report["levels"]) == 1, "Expected exactly one requested level"
    level = report["levels"][0]
    assert level["status"] == "ok" and level["level_num"] == request["level_num"]
    assert level["level_file"].lower() == request["level_file"].lower()
    for key in ("segment_count", "texture_count"):
        assert type(level[key]) is int and level[key] > 0
    return level


def base_identity(report):
    keys = ("level_name", "level_file", "segment_count", "wall_count", "trigger_count",
            "object_count", "texture_count", "robots", "hostages", "powerups", "reactors")
    return (report["mission_filename"], {key: report["levels"][0][key] for key in keys})


def check_loading(worker, hogdir, missions, work):
    """Keep one process alive for base -> Vertigo -> base -> Obsidian -> base."""
    lines = queue.Queue()
    with (work / "worker-stderr.log").open("w", encoding="utf-8") as errors:
        process = subprocess.Popen(
            [str(worker.resolve())], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=errors, text=True, encoding="utf-8", errors="replace", cwd=work,
            env={**os.environ, "SDL_AUDIODRIVER": "dummy", "SDL_VIDEODRIVER": "dummy"},
        )

        def read_stdout():
            try:
                for line in process.stdout:
                    lines.put(line)
            finally:
                lines.put(None)

        reader = threading.Thread(target=read_stdout, daemon=True)
        reader.start()
        requests = 0

        def receive(deadline):
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError("Metadata worker exceeded its request deadline")
            try:
                return lines.get(timeout=remaining)
            except queue.Empty as error:
                raise TimeoutError("Metadata worker exceeded its request deadline") from error

        def request(mission, number, filename, extra=None):
            nonlocal requests
            requests += 1
            message = {
                "schema": "dxx-level-metadata-request-v1", "request_id": f"loading-{requests}",
                "game": "d2", "source_type": "active_level", "source_name": f"{mission}-{number}-{requests}",
                "data_dir": str(hogdir.resolve()), "mission_name": mission, "level_file": filename,
                "level_num": number, "cpu_duty_percent": 100,
            }
            if extra is not None:
                message["extra_data_dir"] = str(extra.resolve())
            deadline = time.monotonic() + 60
            process.stdin.write(json.dumps(message) + "\n")
            process.stdin.flush()
            while True:
                line = receive(deadline)
                assert line is not None, "Metadata worker exited before returning a result"
                if line.startswith("DXXMETA\t"):
                    report = json.loads(line[8:])
                    validate_worker_result(report, message)
                    return report

        try:
            baseline = base_identity(request("d2", 1, "d2leva-1.rl2"))
            for mission, extra in missions:
                advertised = mission_levels(extra / (mission + ".mn2"))
                for number, filename in advertised:
                    request(mission, number, filename, extra)
                assert base_identity(request("d2", 1, "d2leva-1.rl2")) == baseline, (
                    "Base identity or resource counts changed after " + mission)
                print(f"{mission}: all {len(advertised)} advertised levels loaded; base counts restored")
            process.stdin.close()
            deadline = time.monotonic() + 10
            while True:
                line = receive(deadline)
                if line is None:
                    break
                assert not line.startswith("DXXMETA\t"), "Unsolicited or duplicate worker result"
            assert process.wait(timeout=10) == 0, "Metadata worker shutdown failed"
        finally:
            if process.poll() is None:
                process.kill()
            process.wait(timeout=10)
            reader.join(timeout=5)
            process.stdout.close()
            if not process.stdin.closed:
                process.stdin.close()
    print(f"PASS: {requests} metadata requests in one worker")


def validate_route(result, output, mission, *, restored=False, rejected=False):
    log = result.stdout + result.stderr
    if rejected:
        assert result.returncode == 1, "Checkpoint mismatch must fail at the restore guard"
        assert "ROUTE-CONFIRM FAIL checkpoint restore or mission/level mismatch" in log
        assert "ROUTE-CONFIRM OK" not in log and not output.exists()
        return
    assert result.returncode == 0, (result.returncode, log)
    report = json.loads(output.read_text(encoding="utf-8"))
    assert report["schema"] == "dxx-guidebot-route-confirmation-v1"
    assert report["mission"] == mission and report["level"] == 1
    assert report["status"] == "confirmed" and report["frames"] > 0
    assert report["seed"] == 1 and report["fixed_hz"] == 60 and report["difficulty"] == 2
    assert report["radius"]["player"] == report["radius"]["effective"] > 0
    assert report["objectives"][-1]["kind"] == "exit"
    assert report["objectives"][-1]["activation_kind"] == "enter_exit"
    assert report["objectives"][-1]["frame"] == report["frames"]
    if restored:
        state = report["start_state"]
        assert state["kind"] == "saved_world" and state["segment"] > 0
        # Both pinned L1 routes earn blue/red keys before saving at the exit
        assert state["key_flags"] == 6
        assert len(state["position_fixed"]) == 3
        assert all(type(value) is int for value in state["position_fixed"])
        assert "route_confirmation" not in report
    else:
        assert "start_state" not in report
        assert report["route_confirmation"]["status"] == "confirmed"


def check_routes(executable, hogdir, missions, work):
    for mission, extra in missions:
        assert extra.name == "missions", "Native discovery requires an installed missions/ directory"
        destination = work / mission
        destination.mkdir()
        saves = destination / "saves"
        saves.mkdir()
        checkpoint = saves / "level1.sav"
        checkpoint_digest = None
        for phase, target, level, save_flag in (
            ("start", mission, 1, "-route-confirm-checkpoint-out"),
            ("restore", mission, 1, "-route-confirm-checkpoint"),
            ("wrong-level", mission, 2, "-route-confirm-checkpoint"),
            ("wrong-mission", "d2", 1, "-route-confirm-checkpoint"),
        ):
            output = destination / (phase + ".json")
            result = subprocess.run(
                [str(executable.resolve()), "-hogdir", str(hogdir.resolve()),
                 # Native mission discovery scans missions/ beneath the mounted root
                 "-extra-dir", str(extra.parent.resolve()), "-mission", target, "-level", str(level),
                 "-route-confirm-user-dir", str(saves.resolve()), save_flag, checkpoint.name,
                 "-route-confirm-json-out", str(output.resolve())],
                capture_output=True, text=True, timeout=60, check=False, cwd=destination,
                env={**os.environ, "SDL_AUDIODRIVER": "dummy", "SDL_VIDEODRIVER": "dummy"},
            )
            (destination / (phase + ".log")).write_text(
                result.stdout + result.stderr, encoding="utf-8")
            validate_route(result, output, mission, restored=phase == "restore",
                           rejected=phase.startswith("wrong-"))
            assert checkpoint.is_file() and checkpoint.stat().st_size > 0, "Checkpoint missing"
            digest = hashlib.sha256(checkpoint.read_bytes()).hexdigest()
            if checkpoint_digest is not None:
                assert digest == checkpoint_digest, "Restore modified its input checkpoint"
            checkpoint_digest = digest
        print(f"PASS {mission} L1: native route, checkpoint save/restore, wrong-level/mission rejection")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", required=True, type=Path)
    parser.add_argument("--hogdir", required=True, type=Path)
    parser.add_argument("--mission-dir", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--worker", type=Path)
    parser.add_argument("--route-exe", type=Path)
    parser.add_argument("--vertigo-dir", type=Path, help="Installed Vertigo missions/ directory")
    args = parser.parse_args(argv)
    extended = (args.worker, args.route_exe, args.vertigo_dir)
    if any(value is not None for value in extended) and not all(extended):
        parser.error("--worker, --route-exe and --vertigo-dir must be supplied together")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.unlink(missing_ok=True)
    result = subprocess.run(
        [
            str(args.exe.resolve()),
            "-hogdir",
            str(args.hogdir.resolve()),
            "-extra-dir",
            str(args.mission_dir.resolve()),
            "-mission",
            "obsidian",
            "-level",
            "13",
            "-secretarea-json-out",
            str(args.output.resolve()),
        ],
        capture_output=True,
        text=True,
        timeout=60,
        check=False,
    )
    assert result.returncode == 0, (result.returncode, result.stdout, result.stderr)
    validate(json.loads(args.output.read_text(encoding="utf-8")))
    print("Obsidian 13: six secrets, both opaque reward pockets, transparent water excluded")
    if all(extended):
        work = args.output.with_name(args.output.stem + "-lifecycle")
        work.mkdir()
        missions = (("d2x", args.vertigo_dir), ("obsidian", args.mission_dir / "missions"))
        check_loading(args.worker, args.hogdir, missions, work)
        check_routes(args.route_exe, args.hogdir, missions, work)


if __name__ == "__main__":
    main()
