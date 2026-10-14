"""Fail-closed, test-only fan collision fault fingerprint, not parity admission.

Native live capture and its unmodified recording are the sole oracle sources.
An expected imported carried-RNG abort remains an abort, and raw identity,
metadata and effect differences remain failures in the separate raw comparison.
"""

import argparse
import copy
import json
import re
import sys
from pathlib import Path

import check_public_effect_phase as strict
import check_public_mine_blast as mine
import compact_public_replay as compact
import d1_replay_parity as parity
import ordered_public_replay as ordered

SCHEMA = "native-d1-fan-transparency-v1"
MAX_ORACLE_BYTES = 100000
MAX_REPORT_BYTES = 8 * 1024 * 1024
META = {
    "game",
    "scenario",
    "capture_role",
    "public_replay_admission",
    "pixel_observation",
    "armed_frame",
    "request_frame",
    "no_fire",
    "fire_delay",
    "frames",
    "flare_slot",
    "flare_signature",
    "flare_radius",
    "hold",
    "aim",
    "hit_center",
}
FLARE = {
    "slot",
    "signature",
    "flags",
    "lifeleft",
    "parent_num",
    "parent_signature",
    "physics_flags",
    "position",
    "segment",
    "size",
    "velocity",
}
PIXEL = {"point", "u", "v", "x", "y", "pixel", "native_crossable", "effect_frame", "wall_bitmap"}
require, keys, integer, vector, sha = strict.require, strict.keys, strict.integer, strict.vector, mine.sha
# A deliberately narrow imported fault fingerprint, never native oracle data
FAULT = {
    "frame": 300,
    "abort": 301,
    "slot": 77,
    "signature": 179,
    "position": [8208474, -3156161, 30042682],
    "velocity": [0, 0, 0],
    "native_position": [8216206, -2905300, 30439592],
    "native_velocity": [129480, 4201080, 6646920],
    "rng_before": {"s": 2301841099, "c": 345},
    "rng_after": {"s": 2296157864, "c": 346},
}


def read_demo(path, full=False):
    rows = list(strict.strict_records(path, strict.MAX_DEMO_BYTES if full else MAX_ORACLE_BYTES))
    require(len(rows) >= 4, "Incomplete recording")
    h = rows[0]
    require(h.get("type") == "header" and set(h) <= compact.HEADER_KEYS, "Invalid recording header")
    require((h.get("game"), h.get("mission"), h.get("level")) == ("d1", "d1", 1), "Expected native D1 level 1")
    require(h.get("start_mode") == "new_level" and h.get("rng_mode") == "lcg_state", "Expected ordinary LCG start")
    integer(h.get("frame_count"), "frames", 1, 1000)
    require(len(rows) == h["frame_count"] + 3, "Incomplete recording boundaries")
    keys(rows[-1], {"type", "result"}, "recording terminal")
    require(rows[-1]["type"] == "result", "Missing recording result")
    mine.validate_result(rows[-1]["result"], h, "d1")
    batches = ordered.validate(h, rows[1:-2], rows[-2])
    for row in rows:
        compact._no_save(row)
    for row in rows[1:-1]:
        require(
            all(e.get("kind") in compact.DIAGNOSTIC_EVENTS for e in row.get("events", [])), "Unknown recording event"
        )
        require(
            full or (not {"state", "diag"} & set(row) and not row.get("events")), "Compact input contains telemetry"
        )
    require(all(op["k"] == "sample" for b in batches for op in b["ops"]), "Fan recording allows sampled controls only")
    return {"header": h, "batches": batches, "result": rows[-1]["result"]}


def validate_capture(capture):
    require(isinstance(capture, dict) and META | {"observations"} <= set(capture), "Incomplete native fan capture")
    require(
        (capture["game"], capture["scenario"], capture["capture_role"])
        == ("d1", "fan-transparency", "native_d1_reference"),
        "Oracle source must be native live fan capture",
    )
    require(
        capture["public_replay_admission"] is False and capture["pixel_observation"] is True,
        "Missing native pixel observation or false admission",
    )
    integer(capture["frames"], "frames", 303, 1000)
    integer(capture["armed_frame"], "armed frame", 299, 299)
    integer(capture["fire_delay"], "fire delay", 0, 1)
    integer(capture["request_frame"], "request frame", 299 + capture["fire_delay"], 299 + capture["fire_delay"])
    require(type(capture["no_fire"]) is bool, "Invalid no-fire flag")
    require(not capture["no_fire"] or capture["fire_delay"] == 0, "No-fire cannot also delay")
    require(
        (capture["flare_slot"], capture["flare_signature"]) == ((-1, -1) if capture["no_fire"] else (77, 179)),
        "Unexpected flare identity",
    )
    integer(capture["flare_radius"], "flare radius", 26709, 26709)
    for field in ("hold", "aim", "hit_center"):
        vector(capture[field], field)
    rows = capture["observations"]
    require(isinstance(rows, list) and len(rows) == capture["frames"], "Incomplete native observations")
    for f, row in enumerate(rows):
        integer(row.get("f"), "observation frame", f, f)
        for field in ("fan_frame", "fan_timer", "fan_bitmap"):
            integer(row.get(field), field, 0)
        require(
            0 <= row["fan_frame"] <= 7 and row["fan_bitmap"] == 823 + row["fan_frame"], "Wrong native fan phase/bitmap"
        )
        flare = row.get("flare")
        if capture["no_fire"] or f < capture["request_frame"]:
            require(flare is None, "Unexpected flare before requested input")
        else:
            keys(flare, FLARE, "live flare")
            for k in FLARE - {"position", "velocity"}:
                integer(flare[k], "flare." + k)
            for k in ("position", "velocity"):
                vector(flare[k], "flare." + k)
            require(
                (flare["slot"], flare["signature"], flare["size"], flare["parent_num"], flare["parent_signature"])
                == (77, 179, 26709, 0, 1),
                "Native flare identity/reference changed",
            )
            require(not flare["flags"] & 2 and flare["lifeleft"] > 0, "Dead native flare")
        if f >= capture["armed_frame"]:
            pixel = row.get("pixel_probe")
            keys(pixel, PIXEL, "native target pixel")
            vector(pixel["point"], "pixel point")
            for k in PIXEL - {"point"}:
                integer(pixel[k], "pixel." + k)
            require(
                pixel["point"] == capture["hit_center"] and (pixel["x"], pixel["y"]) == (47, 34),
                "Wrong authored native target pixel",
            )
            require(
                (pixel["effect_frame"], pixel["wall_bitmap"]) == (row["fan_frame"], row["fan_bitmap"]),
                "Native pixel phase mismatch",
            )
            require(
                0 <= pixel["pixel"] <= 255 and pixel["native_crossable"] == int(pixel["pixel"] == 255),
                "Pixel/transparency observation mismatch",
            )
            collision_pixel = row.get("collision_pixel_probe")
            keys(collision_pixel, PIXEL, "native fixed collision pixel")
            vector(collision_pixel["point"], "collision pixel point")
            for k in PIXEL - {"point"}:
                integer(collision_pixel[k], "collision pixel." + k)
            require(
                collision_pixel["point"] == FAULT["position"]
                and (collision_pixel["u"], collision_pixel["v"], collision_pixel["x"], collision_pixel["y"])
                == (-17056, 35286, 47, 34),
                "Wrong native fixed collision geometry",
            )
            require(
                (collision_pixel["effect_frame"], collision_pixel["wall_bitmap"])
                == (row["fan_frame"], row["fan_bitmap"]),
                "Fixed collision pixel phase mismatch",
            )
            require(
                collision_pixel["pixel"] == pixel["pixel"]
                and collision_pixel["native_crossable"] == pixel["native_crossable"],
                "Authored/fixed target pixels differ",
            )
            if row["fan_frame"] in (0, 1):
                require(
                    collision_pixel["pixel"] == (255 if row["fan_frame"] == 0 else 59),
                    "Wrong native phase0/phase1 collision mask",
                )
    collision = capture["request_frame"] + 1
    row = rows[collision]
    if not capture["no_fire"]:
        passed = capture["fire_delay"] == 0
        require(row["flare"]["segment"] == (142 if passed else 135), "Native fan crossing/control failed")
        require(
            row["pixel_probe"]["native_crossable"] == int(passed),
            "Native target mask does not support crossing/control",
        )
        require(
            row["flare"]["velocity"] == (FAULT["native_velocity"] if passed else [0, 0, 0]),
            "Unexpected native collision velocity",
        )
        if passed:
            require(
                row["flare"]["position"] == FAULT["native_position"] and row["fan_frame"] == 0,
                "Unexpected native crossing magnitude/phase",
            )
    return capture


def check_actions(capture, demo):
    require(demo["header"]["frame_count"] == capture["frames"], "Capture/input length mismatch")
    shots = []
    for b in demo["batches"]:
        for op in b["ops"]:
            if op["p"]["fl"]:
                require(op["p"]["fl"] == 1, "Multiple flare pulses")
                shots.append(b["f"])
            if b["f"] >= capture["armed_frame"]:
                require(
                    not any(op["s"].values()) and not any(v for k, v in op["p"].items() if k != "fl"),
                    "Non-neutral controls after arming",
                )
        if b["f"] >= capture["armed_frame"]:
            require(
                not any(b["residual"]["s"].values()) and not any(b["residual"]["p"].values()),
                "Non-neutral residual after arming",
            )
    require(shots == ([] if capture["no_fire"] else [capture["request_frame"]]), "Flare input/capture disagreement")


def make_oracle(capture_path, full_path, compact_path, provenance_path, rng_path):
    capture = validate_capture(strict.load_json(capture_path))
    full, small = read_demo(full_path, True), read_demo(compact_path)
    require(full == small, "Compaction changed native recording")
    check_actions(capture, full)
    p = strict.load_json(provenance_path)
    require(p.get("engine_inputs_unchanged") is True, "Engine inputs changed")
    mine.digest_value(p.get("executable_sha256"), "Missing native executable digest")
    require(isinstance(p.get("engine_source_revision"), str) and p["engine_source_revision"], "Missing source revision")
    require(isinstance(p.get("status"), str) and p["status"], "Missing provenance qualification")
    oracle = {
        "schema": SCHEMA,
        "source": {
            key: parity.digest(path)
            for key, path in zip(
                (
                    "capture_sha256",
                    "full_recording_sha256",
                    "compact_sha256",
                    "capture_provenance_sha256",
                    "capture_rng_sha256",
                ),
                (capture_path, full_path, compact_path, provenance_path, rng_path),
            )
        },
        "provenance": {
            k: p[k] for k in ("status", "engine_inputs_unchanged", "engine_source_revision", "executable_sha256")
        },
        "metadata": {k: capture[k] for k in sorted(META)},
        "header": full["header"],
        "terminal": full["result"],
        "controls_sha256": sha(full["batches"]),
        "recording_checks": {
            **mine.recording_fingerprints(full_path),
            "simulation_rng": mine.analyze_rng(rng_path, full),
        },
        "observations": [
            [
                r["fan_frame"],
                r["fan_timer"],
                r["fan_bitmap"],
                r.get("flare"),
                r.get("pixel_probe"),
                r.get("collision_pixel_probe"),
            ]
            for r in capture["observations"]
        ],
    }
    validate_oracle(oracle)
    return oracle


def validate_oracle(oracle):
    keys(
        oracle,
        {
            "schema",
            "source",
            "provenance",
            "metadata",
            "header",
            "terminal",
            "controls_sha256",
            "recording_checks",
            "observations",
        },
        "oracle",
    )
    require(oracle["schema"] == SCHEMA, "Wrong fan oracle schema")
    require(len(strict.encoded(oracle)) <= MAX_ORACLE_BYTES, "Oracle exceeds 100000 bytes")
    keys(oracle["source"], mine.SOURCES, "oracle source hashes")
    for value in oracle["source"].values():
        mine.digest_value(value, "Invalid source digest")
    provenance = oracle["provenance"]
    keys(
        provenance,
        {"status", "engine_inputs_unchanged", "engine_source_revision", "executable_sha256"},
        "oracle provenance",
    )
    require(provenance["engine_inputs_unchanged"] is True, "Changed oracle engine inputs")
    require(
        isinstance(provenance["status"], str) and provenance["status"].strip(),
        "Missing oracle provenance qualification",
    )
    require(
        isinstance(provenance["engine_source_revision"], str)
        and re.fullmatch(r"[0-9a-f]{40}", provenance["engine_source_revision"]),
        "Invalid oracle source revision",
    )
    mine.digest_value(provenance["executable_sha256"], "Invalid oracle executable digest")
    keys(oracle["metadata"], META, "oracle metadata")
    rows = []
    for f, r in enumerate(oracle["observations"]):
        require(isinstance(r, list) and len(r) == 6, "Malformed compact observation")
        rows.append(
            dict(zip(("fan_frame", "fan_timer", "fan_bitmap", "flare", "pixel_probe", "collision_pixel_probe"), r), f=f)
        )
    capture = validate_capture({**oracle["metadata"], "observations": rows})
    require(oracle["header"]["frame_count"] == capture["frames"], "Oracle/header count mismatch")
    mine.validate_result(oracle["terminal"], oracle["header"], "d1")
    checks = oracle["recording_checks"]
    keys(checks, {"full_frame_sha256", "gameplay_frame_sha256", "simulation_rng"}, "recording checks")
    for k in ("full_frame_sha256", "gameplay_frame_sha256"):
        require(
            isinstance(checks[k], list) and len(checks[k]) == capture["frames"], "Incomplete recording fingerprints"
        )
        for d in checks[k]:
            mine.digest_value(d, "Invalid frame digest")
    mine.digest_value(oracle["controls_sha256"], "Invalid control digest")
    return capture


def validate_companion(oracle, path):
    capture = validate_oracle(oracle)
    require(parity.digest(path) == oracle["source"]["compact_sha256"], "Wrong compact recording bytes")
    demo = read_demo(path)
    require(
        demo["header"] == oracle["header"]
        and demo["result"] == oracle["terminal"]
        and sha(demo["batches"]) == oracle["controls_sha256"],
        "Wrong decoded oracle companion",
    )
    check_actions(capture, demo)
    return demo


def select(objects, capture, frame, restored=False):
    player = objects.get("0")
    require(player and player["type"] == 4 and not player["flags"] & 2, "Missing actual player reference")
    flares = [(s, o) for s, o in objects.items() if o["type"] == 5 and o["id"] == 9]
    active = not restored and not capture["no_fire"] and frame >= capture["request_frame"]
    require(len(flares) == int(active), "Missing or unexpected flare objects")
    flare = None
    if active:
        slot, flare = flares[0]
        require(
            (int(slot), flare["signature"], flare["control_type"], flare["movement_type"]) == (77, 179, 9, 1),
            "Flare slot/signature/type changed",
        )
        require(not flare["flags"] & 2 and flare["lifeleft"] > 0, "Dead flare")
        parent = flare["weapon"]
        require(
            (parent["parent_num"], parent["parent_type"], parent["parent_signature"])
            == (0, player["type"], player["signature"]),
            "Flare parent does not bind to actual player",
        )
    return {"flare": copy.deepcopy(flare), "player": copy.deepcopy(player)}


def read_trace(path, oracle, engine, aborted=False):
    """Decode lossless deltas with the existing schemas, preserving aborted phase.

    The full-parity decoder intentionally rejects aborted traces. This separate
    diagnostic accepts only the declared 301-frame envelope, never rewrites it.
    """
    capture = validate_oracle(oracle)
    expected = FAULT["abort"] if aborted else capture["frames"]
    terminal_phase = "aborted" if aborted else "terminal"
    boundaries, counts, objects, world = {}, {"frame_state": 0, "object_state": 0, "world_state": 0}, {}, {}
    samples, effects, full, gameplay = [], [], [], []
    ft, meta, ended = None, None, False
    for index, row in enumerate(strict.strict_records(path, strict.MAX_TRACE_BYTES)):
        kind = row.get("type")
        require(kind in mine.TRACE_KINDS, "Unsupported raw trace record")
        require(index != 0 or kind == "meta", "Missing first trace metadata")
        if kind == "meta":
            require(meta is None and index == 0, "Duplicate trace metadata")
            parity.validate_identity(row, oracle["header"], "d1")
            require(
                all(row.get(k) == oracle["header"][k] for k in ("level", "difficulty", "start_mode", "frame_count")),
                "Wrong raw trace identity/start/count",
            )
            meta = row
        elif kind in counts:
            require(
                not ended and all((k, "restored") in boundaries for k in ("object_boundary", "world_boundary")),
                "SIM frame outside boundaries",
            )
            f = counts[kind]
            integer(row.get("f"), "trace frame", f, f)
            require(f < expected, "Unexpected simulation after expected stop")
            counts[kind] += 1
            if kind == "frame_state":
                ft, a, b = mine.frame_fingerprint(row, f, ft)
                full.append(a)
                gameplay.append(b)
            else:
                require(row.get("reset") is (f == 0), "Wrong delta reset")
                require(
                    type(row.get("version")) is int and row["version"] == (2 if kind == "object_state" else 10),
                    "Wrong raw delta version",
                )
                if kind == "object_state":
                    parity.validate_object_storage(row, engine)
                    for slot, obj in row["slots"].items():
                        if obj is None:
                            require(slot in objects, "Deleting absent object slot")
                            del objects[slot]
                        else:
                            objects[slot] = obj
                    parity.validate_object_cardinality(row, objects)
                    samples.append({**select(objects, capture, f), "clock": row["clock"], "rng": row["rng"]})
                else:
                    world.update(row["state"])
                    parity.validate_world_state(world, engine)
                    require(len(world["effects"]) > 3, "Missing fan effect")
                    effects.append(copy.deepcopy(world["effects"][3]))
        elif kind in ("object_boundary", "world_boundary"):
            phase, f = row.get("phase"), row.get("f")
            require(
                phase in ("restored", terminal_phase) and (kind, phase) not in boundaries,
                "Wrong/duplicate raw boundary phase",
            )
            require(
                row.get("reset") is True and row.get("version") == (2 if kind == "object_boundary" else 10),
                "Wrong boundary schema",
            )
            integer(f, "boundary frame", 0 if phase == "restored" else expected, 0 if phase == "restored" else expected)
            require(
                all(n == (0 if phase == "restored" else expected) for n in counts.values()),
                "Incomplete boundary evidence",
            )
            if phase != "restored":
                ended = True
            if kind == "object_boundary":
                parity.validate_object_storage(row, engine)
                boundaries[kind, phase] = {
                    **select(row["slots"], capture, f, phase == "restored"),
                    "clock": row["clock"],
                    "rng": row["rng"],
                }
            else:
                parity.validate_world_state(row["state"], engine)
                require(len(row["state"]["effects"]) > 3, "Missing boundary fan effect")
                boundaries[kind, phase] = copy.deepcopy(row["state"]["effects"][3])
    require(meta is not None and all(n == expected for n in counts.values()), "Missing raw SIM evidence")
    require(
        set(boundaries)
        == {(k, p) for k in ("object_boundary", "world_boundary") for p in ("restored", terminal_phase)},
        "Missing complete raw boundaries",
    )
    samples.append(boundaries["object_boundary", terminal_phase])
    effects.append(boundaries["world_boundary", terminal_phase])
    return {
        "samples": samples,
        "effects": effects,
        "full_frame_sha256": full,
        "gameplay_frame_sha256": gameplay,
        "phase": terminal_phase,
        "frames": expected,
        "sha256": parity.digest(path),
    }


def live_flare(sample):
    obj = sample["flare"]
    if obj is None:
        return None
    return {
        "slot": 77,
        **{k: obj[k] for k in ("signature", "flags", "lifeleft", "size")},
        "segment": obj["segnum"],
        "position": obj["pos"],
        "velocity": obj["physics"]["velocity"],
        "physics_flags": obj["physics"]["flags"],
        **{k: obj["weapon"][k] for k in ("parent_num", "parent_signature")},
    }


def rng_trace(path, demo, terminal):
    for row in strict.strict_records(path, strict.MAX_TRACE_BYTES):
        if row.get("type") != "meta":
            for k in ("seq", "frame", "gt", "call_count", "state_before", "state_after"):
                integer(row.get(k), "RNG." + k, 0)
            require(type(row.get("stream", 0)) is int, "Invalid RNG stream")
    events = list(parity.rng_events(path))
    current = demo["batches"][0]["pre_rng"]
    for e in events:
        require(e["type"] == "rand", "SIM RNG reseeded")
        value = (current["s"] * 1103515245 + 12345) & 0xFFFFFFFF
        require(
            e["state_before"] == current["s"]
            and e["state_after"] == value
            and e["call_count"] == current["c"] + 1
            and e["result"] == (value >> 16) & 32767,
            "Invalid carried SIM RNG chain",
        )
        integer(e["frame"], "SIM RNG frame", 0, terminal)
        current = {"s": value, "c": current["c"] + 1}
    return events, current


def probe_evidence(path):
    require(Path(path).stat().st_size <= strict.MAX_TRACE_BYTES, "Probe exceeds byte budget")
    selected = {}
    wanted = {"trans_wall_pixel", "physics_fvi_fate", "awareness_rng_gate"}
    for line in Path(path).read_text(encoding="utf-8").splitlines():
        if not re.search(r"(?:^| )frame=300(?: |$)", line) or not re.search(r"(?:^| )obj=77(?: |$)", line):
            continue
        if not any("kind=" + kind + " " in line for kind in wanted):
            continue
        values = {}
        for k, v in re.findall(r"([A-Za-z_][A-Za-z0-9_]*)=([^\s]+)", line):
            require(k not in values or values[k] == v, "Conflicting probe keys")
            values[k] = v
        if values.get("frame") == "300" and values.get("obj") == "77" and values.get("kind") in wanted:
            k = values["kind"]
            require(k not in selected, "Duplicate target collision probe")
            require(values.get("sig") == "179" and values.get("id") == "9", "Wrong probe object identity")
            selected[k] = values
    require(set(selected) == wanted, "Missing target collision probes")
    expected = {
        "trans_wall_pixel": {
            "gt": "1232896",
            "seg": "135",
            "side": "4",
            "face": "0",
            "tmap": "83",
            "tmap2": "329",
            "wall": "5",
            "u": "-17056",
            "v": "35286",
            "bmx": "47",
            "bmy": "34",
            "bm_w": "64",
            "bm_h": "64",
            "direct": "59",
            "gpixel": "59",
            "transparent": "0",
            "p": "(8208474,-3156161,30042682)",
        },
        "physics_fvi_fate": {
            "gt": "1232896",
            "fate": "wall",
            "start_seg": "135",
            "hit_side": "4",
            "hit_side_seg": "135",
            "child": "142",
            "wall": "5",
            "size": "26709",
            "parent": "0",
            "parent_sig": "0",
            "target": "(8216206,-2905300,30439592)",
            "vel": "(129480,4201080,6646920)",
        },
        "awareness_rng_gate": {
            "gt": "1232896",
            "source": "collide_weapon_wall",
            "source_obj": "77",
            "aux_obj": "135",
            "calls": "345->346",
            "state": "2301841099->2296157864",
            "added": "1",
        },
    }
    for kind, fields in expected.items():
        require(all(selected[kind].get(k) == v for k, v in fields.items()), "Unexpected " + kind + " fault fingerprint")
    return {"sha256": parity.digest(path), "target_rows": selected}


def rng_value_prefix(native, imported):
    """Check values/context, while retaining the one already-bound raw reference fault."""
    require(len(native) == len(imported), "SIM RNG prefix length differs")
    for left, right in zip(native, imported):
        differences = list(parity.field_differences(left, right))
        require(
            not differences
            or (
                differences == [{"path": "$.ctx_sig", "expected": 1, "actual": 0}]
                and left.get("ctx_obj") == right.get("ctx_obj") == 0
                and left.get("ctx_id") == right.get("ctx_id") == 0
            ),
            "SIM RNG values/context differ beyond bound raw player signature",
        )


def compare_evidence(oracle, demo, native, imported, native_rng, imported_rng, control=False):
    capture = validate_oracle(oracle)
    recorded = oracle["recording_checks"]
    require(native["frames"] == capture["frames"] and native["phase"] == "terminal", "Native run did not finish")
    require(
        native["full_frame_sha256"] == recorded["full_frame_sha256"], "Native diagnostic/gameplay recording mismatch"
    )
    count = capture["frames"] if control else FAULT["abort"]
    require(
        imported["frames"] == count and imported["phase"] == ("terminal" if control else "aborted"),
        "Wrong imported terminal envelope",
    )
    require(
        imported["gameplay_frame_sha256"] == recorded["gameplay_frame_sha256"][:count],
        "Imported pre-collision gameplay/RNG prefix mismatch",
    )
    raw = []
    for f in range(count + 1):
        left, right = native["samples"][f], imported["samples"][f]
        require(
            left["player"]["signature"] == 1 and right["player"]["signature"] == 0, "Unexpected raw player signatures"
        )
        for field in ("flare", "player"):
            raw.extend(
                {"trace_frame": f, **d} for d in parity.field_differences(left[field], right[field], "$." + field)
            )
        raw.extend(
            {"trace_frame": f, **d}
            for d in parity.field_differences(native["effects"][f], imported["effects"][f], "$.fan_effect")
        )
    for f, row in enumerate(capture["observations"]):
        actual = live_flare(native["samples"][f + 1])
        # Shot input f+1 creates the flare before SIM-entry f+1. That one
        # entry cannot stand in for the preceding no-flare post-SIM sample
        if capture["no_fire"] or f != capture["request_frame"] - 1:
            require(actual == row.get("flare"), "Native live flare trajectory/reference mismatch at " + str(f))
        e = native["effects"][f + 1]
        require(
            (e["frame_count"], e["time_left"], e["wall_bitmap"])
            == (row["fan_frame"], row["fan_timer"], row["fan_bitmap"]),
            "Native live fan effect mismatch",
        )
    # At SIM entry input has already run. Post-SIM f is visible in entry f+1;
    # all those later inputs are independently decoded and required neutral
    for f in range(capture["request_frame"], count + 1 if control else FAULT["frame"] + 1):
        a, b = live_flare(native["samples"][f]), live_flare(imported["samples"][f])
        if a is not None:
            raw_differences = list(
                parity.field_differences(native["samples"][f]["flare"], imported["samples"][f]["flare"])
            )
            require(
                raw_differences == [{"path": "$.weapon.parent_signature", "expected": 1, "actual": 0}],
                "Pre-collision/control raw flare differs beyond bound parent signature",
            )
    ne, ns = native_rng
    ie, state = imported_rng
    require(
        sha(ne) == recorded["simulation_rng"]["sha256"] and ns == demo["batches"][-1]["post_rng"],
        "Native full SIM RNG recording mismatch",
    )
    raw_rng = list(parity.field_differences(ne, ie, "$.simulation_rng"))
    if control:
        require(capture["no_fire"] or capture["fire_delay"] == 1, "Expected no-fire/delayed control")
        rng_value_prefix(ne, ie)
        require(ns == state, "Control SIM RNG terminal state differs")
        if not capture["no_fire"]:
            a, b = live_flare(native["samples"][-1]), live_flare(imported["samples"][-1])
            require(
                {k: v for k, v in a.items() if k != "parent_signature"}
                == {k: v for k, v in b.items() if k != "parent_signature"},
                "Terminal control physical mismatch",
            )
    else:
        require(not capture["no_fire"] and capture["fire_delay"] == 0, "Positive fault requires ordinary flare timing")
        after = imported["samples"][FAULT["abort"]]
        before_clock = {"game_time": 1232896, "frame_time": 4096, "d_tick_count": 301}
        before_rng = {"available": 1, "state": 2301841099, "calls": 345}
        for trace in (native, imported):
            require(
                trace["samples"][300]["clock"] == before_clock and trace["samples"][300]["rng"][0] == before_rng,
                "Wrong pre-collision object clock/RNG",
            )
        require(
            native["samples"][301]["clock"] == {"game_time": 1236992, "frame_time": 4096, "d_tick_count": 302}
            and native["samples"][301]["rng"][0] == before_rng,
            "Wrong native post-collision SIM-entry clock/RNG",
        )
        fl = live_flare(after)
        require(
            fl["segment"] == 135 and fl["position"] == FAULT["position"] and fl["velocity"] == FAULT["velocity"],
            "Wrong imported collision magnitude/trajectory",
        )
        expected_raw_flare = copy.deepcopy(native["samples"][301]["flare"])
        expected_raw_flare.update(next=75, segnum=135, pos=FAULT["position"])
        expected_raw_flare["physics"]["velocity"] = [0, 0, 0]
        expected_raw_flare["weapon"]["parent_signature"] = 0
        require(after["flare"] == expected_raw_flare, "Unexpected raw aborted flare fields")
        native_fl = live_flare(native["samples"][301])
        require(
            native_fl["segment"] == 142
            and native_fl["position"] == FAULT["native_position"]
            and native_fl["velocity"] == FAULT["native_velocity"],
            "Native did not pass through fan",
        )
        e = imported["effects"][301]
        require(
            (e["frame_count"], e["wall_bitmap"], e["time_left"]) == (1, 824, 2156), "Wrong imported collision phase"
        )
        prefix = [e for e in ne if e["frame"] < 300]
        require(len(ie) == len(prefix) + 1, "Pre-collision SIM RNG event count differs")
        rng_value_prefix(prefix, ie[:-1])
        require(
            ie[-1]
            == {
                "type": "rand",
                "frame": 300,
                "gt": 1232896,
                "call_count": 346,
                "ctx_obj": 77,
                "ctx_sig": 179,
                "ctx_id": 9,
                "state_before": 2301841099,
                "state_after": 2296157864,
                "result": 2268,
            },
            "Wrong collision awareness RNG event",
        )
        require(
            state == FAULT["rng_after"] and demo["batches"][301]["pre_rng"] == FAULT["rng_before"],
            "Wrong carried-RNG abort cause",
        )
        require(
            after["clock"] == {"game_time": 1232896, "frame_time": 4096, "d_tick_count": 301},
            "Wrong aborted simulation clock",
        )
        require(
            after["rng"][0] == {"available": 1, "state": state["s"], "calls": state["c"]}, "Abort boundary RNG mismatch"
        )
    raw_frames = list(
        parity.field_differences(
            recorded["full_frame_sha256"][:count], imported["full_frame_sha256"], "$.full_frame_sha256"
        )
    )
    require(raw, "No raw imported mismatch; native/native is not a fault")
    return {
        "schema": SCHEMA + "-report",
        "status": "control_reproduced" if control else "known_fault_reproduced",
        "public_replay_admission": False,
        "oracle_sha256": sha(oracle),
        "source": oracle["source"],
        "provenance": oracle["provenance"],
        "metadata": capture | {"observations": None},
        "trace_sha256": {"native": native["sha256"], "imported": imported["sha256"]},
        "native_live": "pass",
        "gameplay_rng_prefix": {"status": "pass", "frames": count},
        "simulation_rng_value_prefix": {
            "status": "pass",
            "events": len(ie) if control else len(ie) - 1,
            "permitted_reference_difference": "Only ctx_obj0/ctx_id0 player ctx_sig1->0, bound to each raw player object; raw SIM RNG equality remains separately reported",
        },
        "imported_completion": {"phase": imported["phase"], "frames": count, "expected_frames": capture["frames"]},
        "alignment": "SIM-entry f follows ordered input f; post-SIM 300 native entry301 follows neutral input301, imported aborted301 is before input301; clocks are not equated",
        "scope": "Focused flare, parent reference, native target pixel, fan phase, gameplay and SIM RNG; not full parity. The one-frame timing control also changes muzzle position and hits pixel46,34, unlike positive47,34; same-frame paired shot and fixed-impact-point native masks isolate the phase consequence.",
        "raw_comparison": {
            "status": "fail",
            "objects_and_fan": mine.result(raw),
            "full_diagnostics": mine.result(raw_frames),
            "simulation_rng": mine.result(raw_rng),
        },
    }


def compare_pair(
    oracle,
    compact_path,
    native_path,
    imported_path,
    native_rng_path,
    imported_rng_path,
    native_result_path,
    imported_log_path,
    imported_exit_code,
    imported_probe_path=None,
    imported_result_path=None,
    control=False,
):
    demo = validate_companion(oracle, compact_path)
    require(imported_exit_code == (0 if control else 1), "Unexpected imported process exit")
    log = Path(imported_log_path)
    require(log.stat().st_size <= strict.MAX_TRACE_BYTES, "Run log exceeds byte budget")
    stops = re.findall(r"Input demo replay stopped: ([^\r\n]+)", log.read_text(encoding="utf-8"))
    require(
        stops == ([] if control else ["carried SIM RNG differs before input at frame 301"]),
        "Wrong or missing strict carried-RNG abort",
    )
    native = read_trace(native_path, oracle, "d1")
    imported = read_trace(imported_path, oracle, "d2", not control)
    nr = rng_trace(native_rng_path, demo, oracle["metadata"]["frames"])
    ir = rng_trace(imported_rng_path, demo, imported["frames"])
    value = compare_evidence(oracle, demo, native, imported, nr, ir, control)
    summary = strict.load_json(native_result_path)
    mine.validate_result(summary, oracle["header"], "d1")
    require(summary == oracle["terminal"], "Native terminal result mismatch")
    if control:
        require(imported_result_path is not None, "Missing complete control result")
        summary = strict.load_json(imported_result_path)
        mine.validate_result(summary, oracle["header"], "d2")
        require(
            parity.canonical_result(summary, oracle["header"], "d2") == oracle["terminal"],
            "Control terminal result mismatch",
        )
    else:
        require(
            imported_result_path is None or not Path(imported_result_path).exists(),
            "Aborted run unexpectedly has terminal success result",
        )
        require(imported_probe_path is not None, "Missing actual collision probe")
        value["collision_probe"] = probe_evidence(imported_probe_path)
    value["run_log_sha256"] = parity.digest(log)
    value["imported_exit_code"] = imported_exit_code
    return value


def causal_gate(positive_oracle, positive_demo, negative_oracle, negative_demo, delayed_oracle, delayed_demo):
    oracles = (positive_oracle, negative_oracle, delayed_oracle)
    demos = [validate_companion(o, p) for o, p in zip(oracles, (positive_demo, negative_demo, delayed_demo))]
    require(
        [(o["metadata"]["no_fire"], o["metadata"]["fire_delay"]) for o in oracles]
        == [(False, 0), (True, 0), (False, 1)],
        "Wrong causal variants",
    )
    require(demos[0]["header"] == demos[1]["header"] == demos[2]["header"], "Causal headers differ")
    # RNG snapshots and outcomes are deliberately not controls and can diverge
    # after the pulse. Every decoded sample/residual/timestep must otherwise match
    controls = []
    for demo in demos:
        seq = []
        for b in demo["batches"]:
            v = {k: copy.deepcopy(b[k]) for k in ("f", "is_tail", "frame_time", "residual", "ops")}
            for op in v["ops"]:
                op["p"]["fl"] = 0
            seq.append(v)
        controls.append(seq)
    require(controls[0] == controls[1] == controls[2], "Causal variants differ beyond omitted/one-frame-shifted flare")
    return {
        "schema": SCHEMA + "-causal",
        "status": "causal_controls_verified",
        "scope": "Decoded input-only causal isolation; pair reports are independently required",
        "differences": {"no_fire": "Only flare pulse299 omitted", "delayed": "Only flare pulse299 shifted to300"},
        "source": [o["source"] for o in oracles],
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    create = sub.add_parser("create-oracle")
    for name in ("native-capture", "native-demo", "compact-demo", "provenance", "native-rng", "output"):
        create.add_argument("--" + name, type=Path, required=True)
    for cmd in ("compare", "compare-control"):
        p = sub.add_parser(cmd)
        for name in (
            "oracle",
            "compact-demo",
            "native-trace",
            "imported-trace",
            "native-rng",
            "imported-rng",
            "native-result",
            "imported-log",
            "output",
        ):
            p.add_argument("--" + name, type=Path, required=True)
        p.add_argument("--imported-probe", type=Path, required=cmd == "compare")
        p.add_argument("--imported-result", type=Path, required=cmd == "compare-control")
        p.add_argument("--imported-exit-code", type=int, required=True)
    causal = sub.add_parser("causal")
    for variant in ("positive", "negative", "delayed"):
        for name in ("oracle", "demo"):
            causal.add_argument("--" + variant + "-" + name, type=Path, required=True)
    causal.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        if args.command == "create-oracle":
            value = make_oracle(
                args.native_capture, args.native_demo, args.compact_demo, args.provenance, args.native_rng
            )
        elif args.command == "causal":
            value = causal_gate(
                *[
                    v
                    for name in ("positive", "negative", "delayed")
                    for v in (
                        strict.load_json(getattr(args, name + "_oracle"), MAX_ORACLE_BYTES),
                        getattr(args, name + "_demo"),
                    )
                ]
            )
        else:
            value = compare_pair(
                strict.load_json(args.oracle, MAX_ORACLE_BYTES),
                args.compact_demo,
                args.native_trace,
                args.imported_trace,
                args.native_rng,
                args.imported_rng,
                args.native_result,
                args.imported_log,
                args.imported_exit_code,
                args.imported_probe,
                args.imported_result,
                args.command == "compare-control",
            )
        encoded = strict.encoded(value)
        require(len(encoded) <= MAX_REPORT_BYTES, "Fan report exceeds output byte budget")
        args.output.write_bytes(encoded)
        print(json.dumps({"status": value.get("status", "created"), "output": str(args.output)}))
        return 0
    except (
        parity.EvidenceError,
        OSError,
        KeyError,
        TypeError,
        IndexError,
        AttributeError,
        RecursionError,
        ValueError,
    ) as error:
        invalid = {"schema": SCHEMA + "-invalid", "status": "invalid", "error": str(error)}
        try:
            args.output.write_bytes(strict.encoded(invalid))
        except OSError as write_error:
            invalid["output_error"] = str(write_error)
        print(json.dumps(invalid), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
