#!/usr/bin/env python3
"""Bounded public-asset transitions through production Android CPU texture code.

Python ZIP/header parsing and Pillow provide independent expected payloads. This
runner does not alter source assets, decompress ETC2 blocks, or run a GPU renderer.
"""

import argparse
import copy
import hashlib
import io
import itertools
import json
import os
from pathlib import Path
import random
import struct
import subprocess
import zipfile
from PIL import Image


def dump(path, value):
    content = json.dumps(value, indent=2, sort_keys=True) + "\n"
    if not path.exists() or path.read_text() != content:
        path.write_text(content)


def require(condition, message):
    if not condition:
        raise ValueError(message)


def decode_ktx(data):
    require(data[:12] == b"\xabKTX 20\xbb\r\n\x1a\n", "Invalid KTX2 identifier in public asset")
    header = struct.unpack_from("<9I4I2Q", data, 12)
    fmt, _, width, height, depth, layers, faces, count, compression = header[:9]
    require(
        fmt in (147, 151) and depth == layers == compression == 0 and faces == 1, "Unsupported KTX2 public asset layout"
    )
    values = {}
    cursor, end = header[11], header[11] + header[12]
    while cursor < end:
        (size,) = struct.unpack_from("<I", data, cursor)
        cursor += 4
        key, value = data[cursor : cursor + size].split(b"\0", 1)
        values[key] = value
        cursor += (size + 3) & ~3
    payload = bytearray()
    for level in range(count):
        offset, length, raw_length = struct.unpack_from("<QQQ", data, 80 + level * 24)
        level_width, level_height = max(1, width >> level), max(1, height >> level)
        require(
            length == raw_length == ((level_width + 3) // 4) * ((level_height + 3) // 4) * (16 if fmt == 151 else 8),
            "KTX2 public asset mip size mismatch",
        )
        payload.extend(struct.pack("<I", length))
        payload.extend(data[offset : offset + length])
    return {
        "kind": "ktx2",
        "width": width,
        "height": height,
        "format": int(fmt == 151),
        "orig_width": struct.unpack("<H", values[b"OrigWidth"][:2])[0] if b"OrigWidth" in values else width,
        "orig_height": struct.unpack("<H", values[b"OrigHeight"][:2])[0] if b"OrigHeight" in values else height,
        "mip_count": count,
    }, bytes(payload)


def decode_png(data):
    with Image.open(io.BytesIO(data)) as image:
        if image.mode in ("1", "L"):
            image = image.convert("LA" if "transparency" in image.info else "L")
        elif image.mode == "RGB" and "transparency" in image.info:
            image = image.convert("RGBA")
        elif image.mode == "P":
            image = image.convert("RGBA" if "transparency" in image.info else "RGB")
        require(image.mode in ("L", "LA", "RGB", "RGBA"), "Unsupported PNG mode")
        require(max(image.size) <= 2048, "Resampling not covered by this fixture")
        return {
            "kind": "png",
            "width": image.width,
            "height": image.height,
            "channels": len(image.getbands()),
        }, image.tobytes()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assets", type=Path, required=True)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--seed", type=int, default=20261014)
    parser.add_argument("--steps", type=int, default=80)
    parser.add_argument("--samples", type=int, default=32)
    parser.add_argument("--exhaustive-payloads", action="store_true")
    parser.add_argument("--exhaustive-mounts", action="store_true")
    parser.add_argument("--expected-physfs-version")
    args = parser.parse_args()
    if not 32 <= args.steps <= 500 or not 8 <= args.samples <= 256:
        parser.error("--steps must be 32..500 and --samples 8..256")
    args.assets, args.build, args.out = (p.resolve() for p in (args.assets, args.build, args.out))
    args.out.mkdir(parents=True, exist_ok=True)
    payload_dir = args.out / "payloads"
    payload_dir.mkdir(exist_ok=True)
    packs, hashes = {}, {}
    rng = random.Random(args.seed)
    for game in ("d1", "d2"):
        for resolution in (128, 256):
            key = f"{game}-{resolution}"
            path = args.assets / f"{game}-hires-{resolution}-textures-ktx2.dxa"
            hashes[path.name] = hashlib.sha256(path.read_bytes()).hexdigest()
            entries = {}
            with zipfile.ZipFile(path) as archive:
                names = sorted(n for n in archive.namelist() if n.lower().endswith((".ktx2", ".png")))
                # Preserve different dimensions/formats/mip counts before seeded fill
                metadata = {}
                for name in names:
                    raw = archive.read(name)
                    entry, payload = decode_ktx(raw) if name.endswith(".ktx2") else decode_png(raw)
                    digest = hashlib.sha256(payload).hexdigest()
                    expected = payload_dir / (digest + ".bin")
                    if not expected.exists():
                        expected.write_bytes(payload)
                    entry.update({"expected": str(expected), "sha256": digest})
                    entries[name.lower()] = entry
                    metadata[name] = (
                        entry["kind"],
                        entry["width"],
                        entry["height"],
                        entry.get("format"),
                        entry.get("mip_count"),
                        entry.get("orig_width"),
                        entry.get("orig_height"),
                    )
                seen, selected = set(), []
                # Explicit A/B dimensional comparison texture is present in all four packs
                for name in [f"textures/{game}/blown01.ktx2"] + names:
                    if metadata[name] not in seen:
                        selected.append(name.lower())
                        seen.add(metadata[name])
                rng.shuffle(names)
                for name in names:
                    if len(selected) >= args.samples:
                        break
                    if name.lower() not in selected:
                        selected.append(name.lower())
                selected = selected[: args.samples]
                if not any(n.endswith(".png") for n in selected):
                    selected[-1] = next(n.lower() for n in names if n.endswith(".png"))
            packs[key] = {"path": str(path), "entries": entries, "selected": selected}
    summary = {
        "schema": 1,
        "seed": args.seed,
        "pack_sha256": hashes,
        "games": {},
        "coverage": "production pngfile.c and libktx/stb under ASan/UBSan; real PhysFS; independent CPU payload and source oracle",
        "unsupported": [
            "ETC2 block-to-pixel GPU decompression",
            "GPU upload/residency/readback",
            "rendered presentation",
            "menu-to-runtime bindings",
            "automatic engine cache-clear callsites",
            "PCX/FNT presentation assets",
            "malformed binary input",
            "PNG downsampling above 2048",
        ],
    }
    for game in ("d1", "d2"):
        rng = random.Random(args.seed + (game == "d2"))
        small, large = f"{game}-128", f"{game}-256"
        other = "d2" if game == "d1" else "d1"
        default_queries = sorted(set(packs[small]["selected"] + packs[large]["selected"]))
        default_queries += [
            f"textures/{other}/blown01.ktx2",
            f"textures/{game}/__absent__.ktx2",
            f"textures/{game}/__absent__.png",
            default_queries[0].upper(),
            default_queries[0].replace("/", "\\"),
        ]

        def step(label, mounts, clear=True, queries=None, blocked=None):
            samples = []
            for query in default_queries if queries is None else queries:
                canonical = query.replace("\\", "/").lower()
                sample = {
                    "query": query,
                    "source": "",
                    "expected": None,
                    "kind": "ktx2" if canonical.endswith(".ktx2") else "png",
                }
                for key, point in mounts:
                    prefix = point + "/" if point else ""
                    if canonical.startswith(prefix):
                        found = packs[key]["entries"].get(canonical[len(prefix) :])
                        if found is not None:
                            sample.update(found)
                            sample["source"] = packs[key]["path"]
                            break
                if blocked:
                    sample["expected"] = None
                    if blocked == "stale_index":
                        sample["source"] = ""
                samples.append(sample)
            return {
                "label": label,
                "mounts": [{"path": packs[key]["path"], "point": point} for key, point in mounts],
                "clear_cache": clear,
                "samples": samples,
            }

        one = [
            f"textures/{game}/blown01.ktx2",
            next(n for n in default_queries if n.endswith(".png") and "__absent__" not in n),
        ]
        steps = [
            step("stock_empty_index", [], queries=one),
            step("negative_control_stale_index", [(small, "")], clear=False, queries=one, blocked="stale_index"),
            step("cache_clear_recovers_new_mount", [(small, "")], queries=one),
            step("negative_control_populate_miss_cache_after_unmount", [], clear=False, queries=one),
            step(
                "negative_control_stale_misses_after_remount",
                [(small, "")],
                clear=False,
                queries=one,
                blocked="stale_miss",
            ),
            step("cache_clear_recovers_stale_misses", [(small, "")], queries=one),
        ]
        for key in (small, large, small):
            steps.append(step("128_256_128_" + key, [(key, "")]))
        steps += [
            step("disabled", []),
            step("reenabled", [(small, "")]),
            step("128_priority", [(small, ""), (large, "")]),
            step("256_priority", [(large, ""), (small, "")]),
            step("256_priority_unchanged_index", [(large, ""), (small, "")], clear=False),
            step("128_priority_changed_without_clear", [(small, ""), (large, "")], clear=False),
        ]
        allowed_keys = [key for key in packs if game == "d2" or key.startswith("d1-")]

        def mount_point(key):
            return "d1-mods" if game == "d2" and key.startswith("d1-") else ""

        if game == "d2":
            namespaced = ["d1-mods/" + n for n in packs["d1-128"]["selected"]] + packs["d2-128"]["selected"]
            for resolution in (128, 256, 128):
                steps.append(
                    step(
                        "imported_D1_D2_namespace_" + str(resolution),
                        [(f"d1-{resolution}", "d1-mods"), (f"d2-{resolution}", "")],
                        queries=namespaced,
                    )
                )
            steps.append(
                step(
                    "all_four_launcher_namespaces",
                    [(key, mount_point(key)) for key in allowed_keys],
                    queries=namespaced,
                )
            )
        # Incorrect lookup paths against a valid mount configuration must miss
        steps.append(step("wrong_namespace_query", [(small, "")], queries=["d1-mods/" + q for q in default_queries]))
        if args.exhaustive_payloads:
            for key in allowed_keys:
                point = mount_point(key)
                prefix = point + "/" if point else ""
                steps.append(
                    step(
                        "all_public_CPU_payloads_" + key,
                        [(key, point)],
                        queries=[prefix + name for name in sorted(packs[key]["entries"])],
                    )
                )
        if args.exhaustive_mounts:
            for count in range(len(allowed_keys) + 1):
                for keys in itertools.permutations(allowed_keys, count):
                    mounts = [(key, mount_point(key)) for key in keys]
                    queries = list(default_queries)
                    for key, point in mounts:
                        queries.extend((point + "/" if point else "") + n for n in packs[key]["selected"][:8])
                    steps.append(step("all_ordered_namespace_compatible_mount_states", mounts, queries=queries))
            if len(steps) > args.steps:
                parser.error(f"--steps must be at least {len(steps)} for exhaustive coverage ({game})")
        while len(steps) < args.steps:
            keys = rng.sample(allowed_keys, rng.randrange(len(allowed_keys) + 1))
            mounts = [(key, mount_point(key)) for key in keys]
            queries = list(default_queries)
            for key, point in mounts:
                queries.extend((point + "/" if point else "") + name for name in packs[key]["selected"][:8])
            steps.append(step("seeded_transition", mounts, queries=queries))
        scenario = {"schema": 1, "seed": args.seed, "game_headers": game, "steps": steps}
        if args.expected_physfs_version:
            scenario["expected_physfs_version"] = args.expected_physfs_version
        scenario_path, result_path = args.out / f"{game}-scenario.json", args.out / f"{game}-result.json"
        dump(scenario_path, scenario)
        binary = args.build / f"host_texture_transitions_{game}"
        env = dict(
            os.environ,
            ASAN_OPTIONS=os.environ.get("ASAN_OPTIONS", "detect_leaks=0:halt_on_error=1"),
            UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1",
        )

        def fail(outcome, phase, detail):
            summary["status"] = "incomplete" if outcome == "timeout" else "fail"
            summary["failure"] = {"game": game, "phase": phase, "outcome": outcome, "detail": detail}
            dump(args.out / "summary.json", summary)
            raise SystemExit(f"{game} {outcome} in {phase}: {detail}")

        def execute(input_path, output_path, phase, timeout):
            log_path = args.out / f"{game}{'-' + phase if phase != 'transitions' else ''}.log"
            try:
                run = subprocess.run(
                    [str(binary), str(input_path), str(output_path)],
                    capture_output=True,
                    text=True,
                    env=env,
                    timeout=timeout,
                )
            except subprocess.TimeoutExpired as error:

                def as_text(value):
                    return value.decode(errors="replace") if isinstance(value, bytes) else value or ""

                log_path.write_text(as_text(error.stdout) + as_text(error.stderr))
                fail("timeout", phase, {"seconds": timeout, "scenario": str(input_path), "log": str(log_path)})
            log_path.write_text(run.stdout + run.stderr)
            return run

        completed = execute(scenario_path, result_path, "transitions", 600)
        if completed.returncode:
            print(completed.stdout + completed.stderr)
            fail("fixture_failure", "transitions", {"exit_code": completed.returncode, "scenario": str(scenario_path)})
        result = json.loads(result_path.read_text())
        # Corrupt only the oracle, never the valid source archive, to prove sensitivity
        for mutation, marker in (
            ("source", "source archive matches"),
            ("dimensions", "KTX dimensions"),
            ("payload", "KTX packed CPU mip payload"),
        ):
            bad = {"seed": args.seed, "steps": [copy.deepcopy(steps[2])]}
            if args.expected_physfs_version:
                bad["expected_physfs_version"] = args.expected_physfs_version
            sample = bad["steps"][0]["samples"][0]
            if mutation == "source":
                sample["source"] = "wrong-owner-negative-control.dxa"
            elif mutation == "dimensions":
                sample["width"] += 1
            else:
                wrong = bytearray(Path(sample["expected"]).read_bytes())
                wrong[-1] ^= 1
                bad_file = args.out / f"{game}-wrong-oracle.bin"
                bad_file.write_bytes(wrong)
                sample["expected"] = str(bad_file)
            bad_path = args.out / f"{game}-negative-{mutation}.json"
            dump(bad_path, bad)
            negative = execute(bad_path, args.out / "must-not-pass.json", "negative-" + mutation, 30)
            if negative.returncode != 1 or marker not in negative.stderr:
                fail(
                    "oracle_negative_control_failed",
                    mutation,
                    {"exit_code": negative.returncode, "expected_marker": marker, "scenario": str(bad_path)},
                )
        summary["games"][game] = {
            key: result[key] for key in ("status", "ktx_loaded", "png_loaded", "misses", "repeat_reads")
        }
        states = [json.dumps(s["mounts"], sort_keys=True) for s in steps]
        successful = [o for s in result["steps"] for o in s["observations"] if "payload_bytes" in o]
        unique_assets = {
            (o["source"], o["query"].replace("\\", "/").lower().removeprefix("d1-mods/")) for o in successful
        }
        summary["games"][game].update(
            {
                "physfs_compiled_version": result["physfs_compiled_version"],
                "physfs_runtime_version": result["physfs_runtime_version"],
                "unique_mount_states": len(set(states)),
                "unique_directed_mount_transitions": len(set(zip(states, states[1:]))),
                "unique_archive_members_loaded": len(unique_assets),
                "decoded_dimensions": sorted({(o["width"], o["height"]) for o in successful}),
                "ktx_mip_counts": sorted({o["mip_count"] for o in successful if "mip_count" in o}),
                "ktx_formats": sorted({o["format"] for o in successful if "format" in o}),
                "png_channels": sorted({o["channels"] for o in successful if "channels" in o}),
                "steps": len(steps),
                "oracle_negative_controls": 3,
                "cache_negative_controls": 2,
                "public_members_per_pack": {key: len(pack["entries"]) for key, pack in packs.items()},
                "exhaustive_payloads": args.exhaustive_payloads,
                "exhaustive_mounts": args.exhaustive_mounts,
            }
        )
        print(game, completed.stdout.strip())
    summary["status"] = "pass"
    dump(args.out / "summary.json", summary)
    print("PASS independent wrong-source/dimension/payload controls rejected")


if __name__ == "__main__":
    main()
