#!/usr/bin/env python3
"""Bounded seeded PCM configuration transitions through the actual shared loader.

No game data is fabricated or downloaded. Public mod PCM is independently extracted
with Python zipfile; synthetic stock slots isolate replacement ownership semantics.
"""

import argparse
import hashlib
import itertools
import json
import os
from pathlib import Path
import random
import subprocess
import zipfile


def dump(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")


def run_logged(command, log, failure, stage, env, timeout):
    try:
        # Direct native child stays in the owning campaign process group
        result = subprocess.run(command, capture_output=True, text=True, env=env, timeout=timeout)
    except subprocess.TimeoutExpired as error:

        def decoded(value):
            return value.decode(errors="replace") if isinstance(value, bytes) else value or ""

        log.write_text(decoded(error.stdout) + decoded(error.stderr))
        dump(
            failure,
            {
                "status": "timeout_incomplete",
                "stage": stage,
                "timeout_seconds": timeout,
                "command": command,
                "cleanup": "subprocess.run killed and waited for its direct native child",
            },
        )
        print(f"TIMEOUT {stage}; evidence retained at {failure}", flush=True)
        raise SystemExit(124) from error
    log.write_text(result.stdout + result.stderr)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assets", type=Path, required=True)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--seed", type=int, default=20261014)
    parser.add_argument("--steps", type=int, default=100)
    parser.add_argument(
        "--exhaustive-payloads",
        action="store_true",
        help="Cover every public PCM at every backend/output-rate setting first",
    )
    parser.add_argument(
        "--enumerate-configurations",
        action="store_true",
        help="Visit the complete bounded launcher-compatible mount/backend/rate/teardown state domain",
    )
    parser.add_argument("--game", choices=["d1", "d2", "both"], default="both")
    args = parser.parse_args()
    if not 1 <= args.steps <= 2000:
        parser.error("--steps must be 1..2000")
    args.assets = args.assets.resolve()
    args.build = args.build.resolve()
    args.out = args.out.resolve()
    if (args.out.parent / ".pause-requested").exists():
        print("PAUSED: campaign coordinator requested a between-batch stop", flush=True)
        return 75
    args.out.mkdir(parents=True, exist_ok=True)
    payload_dir = args.out / "payloads"
    payload_dir.mkdir(exist_ok=True)
    packs = {}
    hashes = {}
    for game in ("d1", "d2"):
        path = args.assets / f"{game}-hires-sounds.dxa"
        entries = {}
        hashes[path.name] = hashlib.sha256(path.read_bytes()).hexdigest()
        with zipfile.ZipFile(path) as archive:
            for entry in archive.infolist():
                if not entry.filename.lower().startswith("sounds/") or not entry.filename.lower().endswith(
                    (".r22", ".raw")
                ):
                    continue
                data = archive.read(entry)
                digest = hashlib.sha256(data).hexdigest()
                expected = payload_dir / (digest + ".pcm")
                if not expected.exists():
                    expected.write_bytes(data)
                entries[entry.filename.lower()] = {
                    "expected": str(expected),
                    "sha256": digest,
                    "source_rate": 22050 if entry.filename.lower().endswith(".r22") else 11025,
                }
        packs[game] = {"path": str(path), "entries": entries}
    # Authored valid PCM variants test cross-format/archive priority, not malformed parsers
    for kind, ext, data in (("raw", "raw", bytes(range(251)) * 5), ("r22", "r22", bytes(reversed(range(251))) * 7)):
        path = args.out / f"authored-{kind}.dxa"
        with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as archive:
            archive.writestr(f"Sounds/beep02.{ext}", data)
        digest = hashlib.sha256(data).hexdigest()
        expected = payload_dir / (digest + ".pcm")
        expected.write_bytes(data)
        packs[kind] = {
            "path": str(path),
            "entries": {
                f"sounds/beep02.{ext}": {
                    "expected": str(expected),
                    "sha256": digest,
                    "source_rate": 22050 if ext == "r22" else 11025,
                }
            },
        }
    names = sorted({Path(name).stem for pack in packs.values() for name in pack["entries"]})
    summary = {
        "seed": args.seed,
        "namespace_scope": "launcher-compatible public pack game identities; authored PCM priority controls",
        "pack_sha256": hashes,
        "games": {},
        "unsupported": [
            "actual mixer playback",
            "GPU texture pixels/dimensions",
            "menu-to-runtime wiring",
            "retail UUD banks",
            "actual presentation rendering",
            "save/reload game state",
        ],
    }
    for game in ("d1", "d2") if args.game == "both" else (args.game,):
        rng = random.Random(args.seed + (game == "d2"))

        def step(label, mounts, content, legacy=False, rate=22050, sample_names=None):
            samples = []
            for name in (
                sample_names
                if sample_names is not None
                else ["beep02", "__absent__"] + rng.sample([n for n in names if n != "beep02"], 10)
            ):
                expected = {"name": name, "expected": None}
                wanted_point = "d1-mods" if game == "d2" and content == 1 else ""
                for key, point in mounts:
                    if point != wanted_point:
                        continue
                    for extension in ("r22", "raw"):
                        entry = packs[key]["entries"].get(f"sounds/{name}.{extension}")
                        if entry:
                            expected.update(entry)
                            expected["owner"] = packs[key]["path"]
                            break
                    if expected["expected"]:
                        break
                samples.append(expected)
            return {
                "label": label,
                "mounts": [{"path": packs[key]["path"], "point": point} for key, point in mounts],
                "content_game": content,
                "legacy": legacy,
                "rate": rate,
                "samples": samples,
                "unmount_before_reset": bool(rng.randrange(2)),
            }

        identity = 1 if game == "d1" else 2
        steps = [step("stock", [], identity)]
        # Directed minimal A -> B -> A and disable/re-enable, with exact archive order
        for key in (game, "r22", game):
            steps.append(step("A_B_A_" + key, [(key, "")], identity))
        steps.extend(
            [
                step("disabled", [], identity),
                step("reenabled", [(game, "")], identity),
                step("raw_before_r22", [("raw", ""), ("r22", "")], identity),
                step("r22_before_raw", [("r22", ""), ("raw", "")], identity),
            ]
        )
        if game == "d2":
            for content in (1, 2, 1):
                steps.append(step("imported_D1_D2_D1", [("d1", "d1-mods"), ("d2", "")], content))
            steps.append(step("wrong_game_path", [("d1", "")], 1))
        if args.exhaustive_payloads:
            for key, content, point in [("d1", 1, "")] if game == "d1" else [("d2", 2, ""), ("d1", 1, "d1-mods")]:
                public_names = sorted({Path(n).stem for n in packs[key]["entries"]})
                for legacy in (False, True):
                    for rate in (11025, 22050, 44100, 48000):
                        for offset in range(0, len(public_names), 12):
                            steps.append(
                                step(
                                    "all_public_payloads",
                                    [(key, point)],
                                    content,
                                    legacy,
                                    rate,
                                    public_names[offset : offset + 12],
                                )
                            )
            if len(steps) > args.steps:
                parser.error(f"--steps must be at least {len(steps)} for --exhaustive-payloads ({game})")
        root_keys = [game, "raw", "r22"]
        domain = []
        for count in range(len(root_keys) + 1):
            for ordered in itertools.permutations(root_keys, count):
                for imported in (False, True) if game == "d2" else (False,):
                    mounts = [(key, "") for key in ordered] + ([("d1", "d1-mods")] if imported else [])
                    for content in (1, 2) if game == "d2" else (1,):
                        for legacy in (False, True):
                            for rate in (11025, 22050, 44100, 48000):
                                for unmount_first in (False, True):
                                    domain.append((mounts, content, legacy, rate, unmount_first))
        if args.enumerate_configurations:
            rng.shuffle(domain)
            for mounts, content, legacy, rate, unmount_first in domain:
                item = step("all_configurations", mounts, content, legacy, rate)
                item["unmount_before_reset"] = unmount_first
                steps.append(item)
            if len(steps) > args.steps:
                parser.error(f"--steps must be at least {len(steps)} for requested exhaustive phases ({game})")
        while len(steps) < args.steps:
            mounts, content, legacy, rate, unmount_first = rng.choice(domain)
            item = step("seeded_transition", mounts, content, legacy, rate)
            item["unmount_before_reset"] = unmount_first
            steps.append(item)
        scenario = {"schema": 1, "seed": args.seed, "game": game, "steps": steps[: args.steps]}
        scenario_path = args.out / f"{game}-scenario.json"
        result_path = args.out / f"{game}-result.json"
        dump(scenario_path, scenario)
        binary = args.build / f"host_mod_transitions_{game}"
        env = dict(
            os.environ,
            ASAN_OPTIONS=os.environ.get("ASAN_OPTIONS", "detect_leaks=0:halt_on_error=1"),
            UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1",
        )
        result_path.unlink(missing_ok=True)
        run = run_logged(
            [str(binary), str(scenario_path), str(result_path)],
            args.out / f"{game}.log",
            args.out / "failure.json",
            game,
            env,
            max(120, args.steps),
        )
        if run.returncode:
            dump(
                args.out / "failure.json",
                {
                    "status": "native_failure",
                    "game": game,
                    "seed": args.seed,
                    "exit_code": run.returncode,
                    "scenario": str(scenario_path),
                },
            )
            print(run.stdout + run.stderr)
            raise SystemExit(f"{game} failed ({run.returncode}); replay {scenario_path}")
        result = json.loads(result_path.read_text())
        if (
            result.get("status") != "pass"
            or result.get("seed") != args.seed
            or result.get("executed_steps") != len(scenario["steps"])
        ):
            dump(args.out / "failure.json", {"status": "incomplete_or_stale_result", "game": game, "seed": args.seed})
            raise SystemExit(f"{game}: result does not match the current complete scenario")
        # Demonstrate that the independent owner and byte oracle reject wrong expectations
        loaded_step = next(s for s in steps if any(v["expected"] for v in s["samples"]))
        for mutation in ("owner", "pcm"):
            bad = json.loads(json.dumps({"seed": args.seed, "steps": [loaded_step]}))
            sample = next(s for s in bad["steps"][0]["samples"] if s["expected"])
            if mutation == "owner":
                sample["owner"] = "wrong-owner-negative-control.dxa"
            else:
                wrong = bytearray(Path(sample["expected"]).read_bytes())
                wrong[0] ^= 1
                path = args.out / f"{game}-wrong.pcm"
                path.write_bytes(wrong)
                sample["expected"] = str(path)
            path = args.out / f"{game}-negative-{mutation}.json"
            dump(path, bad)
            negative = run_logged(
                [str(binary), str(path), str(args.out / "must-not-pass.json")],
                args.out / f"{game}-negative-{mutation}.log",
                args.out / "failure.json",
                f"{game}-negative-{mutation}",
                env,
                30,
            )
            marker = "loaded owner" if mutation == "owner" else "resident PCM equals"
            if negative.returncode != 1 or marker not in negative.stderr:
                raise SystemExit(f"{game} {mutation} negative control did not fail the intended assertion")
        state_keys = []
        selection_keys = set()
        for state in scenario["steps"]:
            state_key = json.dumps(
                [
                    [(Path(m["path"]).name, m["point"]) for m in state["mounts"]],
                    state["content_game"],
                    state["legacy"],
                    state["rate"],
                    state["unmount_before_reset"],
                ],
                separators=(",", ":"),
            )
            state_keys.append(state_key)
            for sample in state["samples"]:
                selection_keys.add(
                    json.dumps(
                        [
                            sample["name"],
                            Path(sample["owner"]).name if "owner" in sample else None,
                            state["content_game"],
                            state["legacy"],
                            state["rate"],
                            state["unmount_before_reset"],
                        ],
                        separators=(",", ":"),
                    )
                )
        coverage = {
            "states": sorted(set(state_keys)),
            "transitions": sorted(
                set(json.dumps(pair, separators=(",", ":")) for pair in zip(state_keys, state_keys[1:]))
            ),
            "selections": sorted(selection_keys),
        }
        dump(args.out / f"{game}-coverage.json", coverage)
        summary["games"][game] = {
            "steps": len(scenario["steps"]),
            "unique_states": len(coverage["states"]),
            "unique_transitions": len(coverage["transitions"]),
            "unique_selections": len(coverage["selections"]),
            "loaded_samples": result["loaded_samples"],
            "resident_negative_controls": result["negative_controls"],
            "oracle_negative_controls": 2,
            "physfs_runtime": result["physfs_runtime"],
            "configuration_domain_size": len(domain),
            "enumerated_configuration_states": len(domain) if args.enumerate_configurations else 0,
            "scenario": str(scenario_path),
            "status": "pass",
        }
        print(f"{game}: {run.stdout.strip()}, owner/PCM oracle negative controls passed", flush=True)
    dump(args.out / "summary.json", summary)


if __name__ == "__main__":
    raise SystemExit(main())
