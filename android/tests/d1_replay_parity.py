"""Paired D1 observations, with explicit gaps before full simulation qualification.

Uses the existing replay wrapper and engine checkpoint readers. This module never
interprets a DGSS payload or substitutes gameplay values from an actual result.
"""

import argparse
import copy
import gzip
import hashlib
import itertools
import json
import math
import platform
import shutil
import struct
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path


class EvidenceError(ValueError):
    pass


# Relative layout matters: PowerShell loads its helpers beside the staged runner
HARNESS_FILES = (
    "android/tests/d1_replay_parity.py",
    "android/tests/run_input_demo_replay.ps1",
    "android/tests/input_demo_assets.py",
    "android/helpers/input_demo_assets.ps1",
    "android/tests/input_demo_host_build_guard.ps1",
    "android/helpers/test_host_platform.ps1",
    "android/helpers/shareware_test_data.ps1",
    "android/helpers/verified_dependencies.ps1",
    "android/helpers/powershell_compat.ps1",
    "android/helpers/input_demo_replay_menu.ps1",
    "android/helpers/input_demo_fixture_catalog.ps1",
    "android/helpers/headless_process_pool.ps1",
    "android/helpers/process_lifetime.ps1",
    "android/helpers/output_disk_space.ps1",
    "android/helpers/retain-recent-artifacts.ps1",
    "android/helpers/clean-old-artifacts.ps1",
)


def write_json(path, value):
    Path(path).write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def digest(path):
    sha = hashlib.sha256()
    with Path(path).open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            sha.update(block)
    return sha.hexdigest()


def records(path):
    path = Path(path)
    opener = gzip.open if path.suffix == ".gz" else open
    with opener(path, "rt", encoding="utf-8-sig") as source:
        for number, line in enumerate(source, 1):
            if not line.strip() or line.lstrip().startswith("//"):
                continue
            try:
                value = json.loads(line)
            except ValueError as error:
                raise EvidenceError(f"{path}:{number}: invalid JSON") from error
            if not isinstance(value, dict):
                raise EvidenceError(f"{path}:{number}: expected an object")
            yield value


def read_demo(path):
    header = checkpoint = result = None
    count = 0
    for record in records(path):
        kind = record.get("type")
        if header is None and kind != "header":
            raise EvidenceError("Recording must begin with its header")
        if kind == "header":
            if header is not None:
                raise EvidenceError("Duplicate recording header")
            header = record
        elif kind == "checkpoint":
            if checkpoint is not None or count:
                raise EvidenceError("Unexpected checkpoint placement")
            # Pin the engine's declared decoded checksum and the untouched record
            checkpoint = {key: value for key, value in record.items() if key != "data"}
            checkpoint["record_sha256"] = hashlib.sha256(
                json.dumps(record, sort_keys=True, separators=(",", ":")).encode()
            ).hexdigest()
        elif kind == "frame":
            if result is not None or record.get("f") != count:
                raise EvidenceError("Nonsequential recording frames")
            count += 1
        elif kind == "result":
            if result is not None:
                raise EvidenceError("Duplicate recording result")
            result = record["result"]
    if not header or header.get("game") != "d1" or not count:
        raise EvidenceError("A nonempty D1 recording is required")
    if count != header.get("frame_count") or result is None:
        raise EvidenceError("Recording frame count or terminal result is incomplete")
    if header.get("start_mode") == "save_checkpoint" and checkpoint is None:
        raise EvidenceError("Recording checkpoint is missing")
    return {
        "path": str(Path(path).resolve()),
        "sha256": digest(path),
        "header": header,
        "checkpoint": checkpoint,
        "result": result,
    }


def imported_mission(header):
    mission = header["mission"]
    return "descent" if mission in ("", "d1", "descent") else mission


def validate_identity(value, header, engine):
    mission = imported_mission(header) if engine == "d2" else header["mission"]
    for key, expected in (
        ("game", engine),
        ("mission", mission),
        ("difficulty", header["difficulty"]),
        ("frame_count", header["frame_count"]),
    ):
        # Live observations start before the player's eventual stop is known.
        # Each reader still requires the exact contiguous horizon at EOF.
        if (
            key == "frame_count"
            and value.get("type") == "meta"
            and value.get("source") == "record"
            and type(value.get(key)) is int
            and value[key] == 0
        ):
            continue
        if value.get(key) != expected:
            raise EvidenceError(f"Unexpected {key}: {value.get(key)!r}; expected {expected!r}")


def canonical_result(value, header, engine):
    validate_identity(value, header, engine)
    value = copy.deepcopy(value)
    # Only executable/content naming differs; no actual supplies expected values
    value["game"] = "d1"
    value["mission"] = header["mission"]
    return value


def difference(expected, actual, path="$"):
    if type(expected) is not type(actual):
        return {"path": path, "expected": expected, "actual": actual}
    if isinstance(expected, dict):
        for key in sorted(expected.keys() | actual.keys()):
            if key not in expected or key not in actual:
                return {
                    "path": f"{path}.{key}",
                    "expected": expected.get(key, "<missing>"),
                    "actual": actual.get(key, "<missing>"),
                }
            found = difference(expected[key], actual[key], f"{path}.{key}")
            if found:
                return found
    elif isinstance(expected, list):
        if len(expected) != len(actual):
            return {"path": path + ".length", "expected": len(expected), "actual": len(actual)}
        for index, (left, right) in enumerate(zip(expected, actual)):
            found = difference(left, right, f"{path}[{index}]")
            if found:
                return found
    elif expected != actual:
        return {"path": path, "expected": expected, "actual": actual}
    return None


# Versioned contract for input_demo_state_trace_diag and its JSON emitter.
# Keep names and array lengths synchronized; unknown fields remain compared.
FRAME_DIAGNOSTIC_VERSION = 2
FRAME_DIAGNOSTIC_FIELDS = {
    name: length
    for length, names in (
        (
            0,
            (
                "awareness_events camera_awake_robots danger_laser_robots d_tick_count runtime_state_hash "
                "object_allocator_num_objects object_signature_seed object_free_list_count object_free_list_hash "
                "object_free_head0 object_free_head1 object_free_head2 object_free_head3 object_homer_frame_count "
                "object_current_homer_frame_time object_do_homer_frame weapon_next_laser_delta "
                "weapon_next_missile_delta weapon_last_laser_delta weapon_next_flare_delta "
                "weapon_auto_fusion_delta weapon_last_omega_delta weapon_global_laser_firing_count "
                "weapon_global_missile_firing_count weapon_fusion_charge weapon_spreadfire_toggle "
                "weapon_missile_gun weapon_proximity_dropped weapon_helix_orientation weapon_smartmines_dropped "
                "primary_weapon_picked_up secondary_weapon_picked_up player_vel_x player_vel_y player_vel_z "
                "player_last_x player_last_y player_last_z player_bump_frame player_bump_count "
                "player_bump_step_hash player_bump_other_obj player_bump_other_sig player_bump_other_type "
                "player_bump_other_id player_bump_damage_flag player_bump_force_mag player_bump_damage_raw "
                "player_bump_damage_scaled player_bump_other_attack_type player_bump_rel_vel_x "
                "player_bump_rel_vel_y player_bump_rel_vel_z player_bump_force_x player_bump_force_y "
                "player_bump_force_z player_bump_player_mass player_bump_other_mass player_weapon_count "
                "player_weapon_hash highest_object_index live_object_count live_object_hash "
                "object_slot_bucket_size object_focus_slot_base robot_object_count robot_state_hash "
                "robot_changed_obj robot_changed_sig robot_changed_id robot_changed_bucket "
                "robot_changed_prev_hash robot_changed_hash robot_changed_type robot_changed_seg "
                "robot_changed_control robot_changed_movement robot_changed_render robot_changed_flags "
                "robot_changed_x robot_changed_y robot_changed_z robot_changed_last_x robot_changed_last_y "
                "robot_changed_last_z robot_changed_vel_x robot_changed_vel_y robot_changed_vel_z "
                "robot_changed_rotvel_x robot_changed_rotvel_y robot_changed_rotvel_z robot_changed_model "
                "robot_changed_subobj_flags robot_sample_obj robot_sample_sig robot_sample_id robot_sample_seg "
                "robot_sample_model robot_sample_subobj_flags robot_sample_behavior robot_sample_mode "
                "robot_sample_cur_state robot_sample_goal_state robot_sample_anim_at_goal "
                "robot_sample_anim_angles_hash robot_sample_goal_angles_hash robot_sample_delta_angles_hash "
                "robot_sample_goal_state_hash robot_sample_achieved_state_hash robot_sample_goal_seg "
                "robot_sample_hide_index robot_sample_path_dir robot_sample_prev_vis robot_sample_aware "
                "robot_sample_aware_time robot_sample_since robot_sample_next_action robot_sample_retry "
                "robot_sample_retry_chain robot_sample_path_index robot_sample_path_length "
                "robot_sample_phys_flags robot_sample_vel_x robot_sample_vel_y robot_sample_vel_z "
                "robot_sample_pos_x robot_sample_pos_y robot_sample_pos_z robot_sample_goal_x robot_sample_goal_y "
                "robot_sample_goal_z robot_sample_next_goal_x robot_sample_next_goal_y robot_sample_next_goal_z "
                "robot_sample_mass robot_sample_drag robot_sample_brakes robot_sample_fvec_x robot_sample_fvec_y "
                "robot_sample_fvec_z robot_sample_rvec_x robot_sample_rvec_y robot_sample_rvec_z "
                "robot_sample_uvec_x robot_sample_uvec_y robot_sample_uvec_z robot_sample_orient_hash "
                "robot_sample_rotthrust_x robot_sample_rotthrust_y robot_sample_rotthrust_z robot_sample_rotvel_x "
                "robot_sample_rotvel_y robot_sample_rotvel_z robot_ai_static_state_hash "
                "robot_ai_static_without_changed_hash robot_ai_static_changed_obj robot_ai_static_changed_sig "
                "robot_ai_static_changed_id robot_ai_static_changed_prev_hash robot_ai_static_changed_hash "
                "robot_ai_static_changed_behavior robot_ai_static_changed_flags_hash "
                "robot_ai_static_changed_current_gun robot_ai_static_changed_current_state "
                "robot_ai_static_changed_goal_state robot_ai_static_changed_path_dir "
                "robot_ai_static_changed_submode robot_ai_static_changed_goalside "
                "robot_ai_static_changed_skip_ai_count robot_ai_static_changed_hide_segment "
                "robot_ai_static_changed_hide_index robot_ai_static_changed_path_length "
                "robot_ai_static_changed_cur_path_index robot_ai_static_changed_follow_start "
                "robot_ai_static_changed_follow_end robot_ai_static_changed_danger_laser_num "
                "robot_ai_static_changed_danger_laser_sig robot_ai_local_state_hash robot_anim_pose_state_hash "
                "robot_anim_pose_changed_obj robot_anim_pose_changed_sig robot_anim_pose_changed_id "
                "robot_anim_pose_changed_prev_hash robot_anim_pose_changed_hash robot_anim_pose_changed_model "
                "robot_anim_pose_changed_subobj_flags robot_anim_pose_changed_anim_angles_hash "
                "robot_anim_pose_changed_goal_angles_hash robot_anim_pose_changed_delta_angles_hash "
                "robot_anim_pose_changed_goal_state_hash robot_anim_pose_changed_achieved_state_hash "
                "robot_anim_pose_changed_current_gun robot_anim_pose_changed_current_state "
                "robot_anim_pose_changed_goal_state weapon_object_count weapon_state_hash weapon_sample_obj "
                "weapon_sample_sig weapon_sample_id weapon_sample_seg weapon_sample_control "
                "weapon_sample_movement weapon_sample_render weapon_sample_flags weapon_sample_phys_flags "
                "weapon_sample_x weapon_sample_y weapon_sample_z weapon_sample_last_x weapon_sample_last_y "
                "weapon_sample_last_z weapon_sample_vel_x weapon_sample_vel_y weapon_sample_vel_z "
                "weapon_sample_size weapon_sample_shields weapon_sample_lifeleft weapon_sample_parent_type "
                "weapon_sample_parent_num weapon_sample_parent_sig fireball_object_count fireball_state_hash "
                "fireball_changed_obj fireball_changed_sig fireball_changed_id fireball_changed_bucket "
                "fireball_changed_prev_hash fireball_changed_hash fireball_changed_seg fireball_changed_control "
                "fireball_changed_movement fireball_changed_render fireball_changed_flags fireball_changed_x "
                "fireball_changed_y fireball_changed_z fireball_changed_last_x fireball_changed_last_y "
                "fireball_changed_last_z fireball_changed_size fireball_changed_shields fireball_changed_lifeleft "
                "fireball_sample_obj fireball_sample_sig fireball_sample_id fireball_sample_hash "
                "fireball_sample_seg fireball_sample_control fireball_sample_movement fireball_sample_render "
                "fireball_sample_flags fireball_sample_x fireball_sample_y fireball_sample_z "
                "fireball_sample_last_x fireball_sample_last_y fireball_sample_last_z fireball_sample_size "
                "fireball_sample_shields fireball_sample_lifeleft fireball_sample_attached_obj "
                "fireball_sample_spawn_time fireball_sample_delete_time fireball_sample_delete_objnum "
                "fireball_sample_attach_parent fireball_sample_prev_attach fireball_sample_next_attach "
                "debris_object_count debris_state_hash segment_object_list_count segment_object_list_hash "
                "segment_object_link_error_count player_weapon_obj0 player_weapon_sig0 player_weapon_id0 "
                "player_weapon_obj1 player_weapon_sig1 player_weapon_id1 player_weapon_obj2 player_weapon_sig2 "
                "player_weapon_id2 player_weapon_obj3 player_weapon_sig3 player_weapon_id3 ai_probe_skip_count "
                "ai_probe_skip_obj ai_probe_skip_sig ai_probe_skip_id ai_probe_timeslice_count "
                "ai_probe_timeslice_obj ai_probe_timeslice_sig ai_probe_timeslice_id ai_probe_process_count "
                "ai_probe_process_obj ai_probe_process_sig ai_probe_process_id ai_probe_phys_skip_count "
                "ai_probe_phys_skip_obj ai_probe_phys_skip_sig ai_probe_phys_skip_id ai_probe_phys_skip_before "
                "ai_probe_phys_skip_after "
            ),
        ),
        (
            32,
            (
                "object_slot_counts object_slot_hashes object_focus_slot_hashes robot_object_bucket_hashes "
                "robot_ai_static_bucket_hashes robot_ai_local_bucket_hashes robot_anim_pose_bucket_hashes "
            ),
        ),
        (
            256,
            (
                "robot_ai_static_trace_slots robot_ai_static_trace_sigs robot_ai_static_trace_ids "
                "robot_ai_static_trace_hashes robot_ai_static_trace_flags_hashes "
                "robot_ai_static_trace_current_guns robot_ai_static_trace_current_states "
                "robot_ai_static_trace_goal_states robot_ai_static_trace_path_dirs robot_ai_static_trace_submodes "
                "robot_ai_static_trace_goalsides robot_ai_static_trace_skip_ai_counts "
                "robot_ai_static_trace_hide_segments robot_ai_static_trace_hide_indexes "
                "robot_ai_static_trace_path_lengths robot_ai_static_trace_cur_path_indexes "
                "robot_ai_static_trace_follow_starts robot_ai_static_trace_follow_ends "
                "robot_ai_static_trace_danger_nums robot_ai_static_trace_danger_sigs "
            ),
        ),
        (
            8,
            (
                "weapon_trace_slots weapon_trace_sigs weapon_trace_ids weapon_trace_hashes weapon_trace_segs "
                "weapon_trace_lifeleft weapon_trace_track_goals weapon_trace_fvec_x weapon_trace_fvec_y "
                "weapon_trace_fvec_z weapon_trace_vel_x weapon_trace_vel_y weapon_trace_vel_z "
                "fireball_trace_slots fireball_trace_sigs fireball_trace_ids fireball_trace_hashes "
                "fireball_trace_segs fireball_trace_lifeleft fireball_trace_delete_objnums "
                "fireball_trace_attached_objs "
            ),
        ),
        (7, ("segment_trace_segs segment_trace_counts segment_trace_hashes segment_trace_heads ")),
        (
            84,
            (
                "segment_trace_objs segment_trace_sigs segment_trace_types segment_trace_ids segment_trace_prevs "
                "segment_trace_nexts "
            ),
        ),
    )
    for name in names.split()
}


def validate_frame_diagnostics(diag, frame):
    if not isinstance(diag, dict):
        raise EvidenceError(f"Diagnostic observations must be objects at frame {frame}")
    for name, length in FRAME_DIAGNOSTIC_FIELDS.items():
        if name not in diag:
            raise EvidenceError(f"Missing diagnostic field at frame {frame}: {name}")
        value = diag[name]
        if length:
            valid = isinstance(value, list) and len(value) == length and all(type(item) is int for item in value)
        else:
            valid = type(value) is int
        if not valid:
            raise EvidenceError(f"Invalid diagnostic field at frame {frame}: {name}")


def frames(path, header, engine, recorded=False):
    count = 0
    last_ft = None
    meta_seen = False
    for record in records(path):
        if record.get("type") == ("header" if recorded else "meta"):
            if meta_seen:
                raise EvidenceError("Duplicate frame metadata")
            # start_replay emits the recording identity, including in D2.
            # Executable identity is validated separately in the terminal result.
            validate_identity(record, header, header["game"])
            if record.get("level") != header["level"] or record.get("start_mode") != header["start_mode"]:
                raise EvidenceError("Replay did not start from the recorded level/checkpoint mode")
            if not recorded and (
                type(record.get("diag_version")) is not int or record["diag_version"] != FRAME_DIAGNOSTIC_VERSION
            ):
                raise EvidenceError("Missing or unsupported frame diagnostic schema")
            meta_seen = True
            continue
        if record.get("type") not in ("frame", "frame_state"):
            continue
        if not meta_seen or record.get("f") != count:
            raise EvidenceError("Frame metadata missing or frame order changed")
        count += 1
        last_ft = record.get("ft", last_ft)
        if last_ft is None or "state" not in record or "rng" not in record or "diag" not in record:
            raise EvidenceError(f"Required observations missing at frame {record.get('f')}")
        if not recorded:
            validate_frame_diagnostics(record["diag"], record["f"])
        record["ft"] = last_ft
        yield record
    if count != header["frame_count"]:
        raise EvidenceError(f"Missing frame evidence: expected {header['frame_count']}, got {count}")


def compare_frames(left, right, header, right_engine, recorded=False):
    first = {}
    diagnostics = {}
    count = 0
    for a, b in itertools.zip_longest(frames(left, header, "d1", recorded), frames(right, header, right_engine)):
        if a is None or b is None:
            raise EvidenceError("Unequal trace lengths")
        for key in ("f", "ft", "state", "rng", "diag"):
            if key not in first:
                found = difference(a[key], b[key], f"$.{key}")
                if found:
                    first[key] = {"frame": count, **found}
        if not isinstance(a["diag"], dict) or not isinstance(b["diag"], dict):
            raise EvidenceError(f"Diagnostic observations must be objects at frame {count}")
        # An early representation difference must not hide a later gameplay
        # difference elsewhere in the diagnostics. Preserve the strict verdict
        # and inventory each field independently, without an exclusion list
        for key in sorted(a["diag"].keys() | b["diag"].keys()):
            entry = diagnostics.setdefault(key, {"frames_compared": 0, "frames_different": 0, "first_difference": None})
            entry["frames_compared"] += 1
            found = difference(
                {key: a["diag"][key]} if key in a["diag"] else {},
                {key: b["diag"][key]} if key in b["diag"] else {},
                "$.diag",
            )
            if found:
                entry["frames_different"] += 1
                if entry["first_difference"] is None:
                    entry["first_difference"] = {"frame": count, **found}
        count += 1
    return {
        "frames_compared": count,
        "first_differences": first,
        "diagnostics": diagnostics,
        "status": "fail" if first else "pass",
    }


def rng_events(path, stream=0):
    count = 0
    meta = None
    for record in records(path):
        if record.get("type") == "meta":
            if meta is not None or record.get("truncated") is not False:
                raise EvidenceError("RNG trace is duplicate or truncated")
            meta = record
            continue
        if meta is None or record.get("type") not in ("rand", "srand"):
            raise EvidenceError("Missing RNG metadata or unsupported event")
        if record.get("seq") != count:
            raise EvidenceError("RNG event sequence is incomplete")
        count += 1
        # Preserve complete SIM, FX and XL presentation traces, including interleaving
        if type(record.get("stream", 0)) is not int or record.get("stream", 0) not in (0, 1, 2):
            raise EvidenceError("Unsupported RNG stream")
        required = (
            "frame",
            "gt",
            "call_count",
            "state_before",
            "state_after",
            "seed" if record["type"] == "srand" else "result",
        )
        if not all(key in record for key in required):
            raise EvidenceError("Incomplete RNG event")
        if record.get("stream", 0) != stream:
            continue
        # Global seq interleaves FX; source function/path/line are engine labels
        yield {key: value for key, value in record.items() if key not in ("seq", "file", "func", "line", "stream")}
    if meta is None or count != meta.get("events"):
        raise EvidenceError("RNG trace event count is incomplete")


def compare_rng(left, right, stream=0):
    first = None
    first_values = None
    count = 0
    for a, b in itertools.zip_longest(rng_events(left, stream), rng_events(right, stream)):
        if first is None:
            found = difference(a, b)
            if found:
                first = {"event": count, **found}
        if first_values is None:
            # Keep object-context differences in the strict verdict, but also
            # expose algorithm divergence that an earlier annotation can mask
            values = lambda event: (
                None
                if event is None
                else {key: value for key, value in event.items() if key not in ("ctx_obj", "ctx_sig", "ctx_id")}
            )
            found = difference(values(a), values(b))
            if found:
                first_values = {"event": count, **found}
        count += 1
    count_key = ("simulation_events_compared", "effects_events_compared", "presentation_events_compared")[stream]
    return {
        "status": "fail" if first else "pass",
        count_key: count,
        "first_difference": first,
        "first_value_difference": first_values,
    }


def object_states(path, header, engine="d1"):
    """Reconstruct the engine's lossless slot deltas, including deletion/reuse."""
    objects = {}
    count = 0
    capacity = None
    meta_seen = False
    for row in records(path):
        if row.get("type") == "meta":
            if meta_seen:
                raise EvidenceError("Duplicate object trace metadata")
            validate_identity(row, header, header["game"])
            if row.get("level") != header["level"] or row.get("start_mode") != header["start_mode"]:
                raise EvidenceError("Object trace does not describe the recorded start")
            meta_seen = True
        if row.get("type") != "object_state":
            continue
        if (
            not meta_seen
            or type(row.get("version")) is not int
            or row["version"] != 2
            or type(row.get("f")) is not int
            or row["f"] != count
            or row.get("reset") is not (count == 0)
        ):
            raise EvidenceError("Invalid object trace schema, reset or frame sequence")
        validate_object_storage(row, engine, xl_storage_profile(header, engine))
        if capacity is not None and capacity != row["capacity"]:
            raise EvidenceError("Object trace capacity changed")
        capacity = row["capacity"]
        if not isinstance(row.get("slots"), dict):
            raise EvidenceError("Missing object slot deltas")
        for key, value in row["slots"].items():
            if not key.isdecimal() or str(int(key)) != key or not 0 <= int(key) < capacity:
                raise EvidenceError("Invalid object slot identity")
            if value is None:
                if key not in objects:
                    raise EvidenceError("Deleting an absent object slot")
                del objects[key]
            else:
                objects[key] = value
        validate_object_cardinality(row, objects)
        count += 1
        yield {**{k: row[k] for k in ("capacity", "allocator", "segment_heads", "clock", "rng")}, "objects": objects}
    if count != header["frame_count"]:
        raise EvidenceError(f"Missing object evidence: expected {header['frame_count']}, got {count}")


def field_differences(expected, actual, path="$"):
    """Visit every differing named field; unrelated layout fields cannot mask it."""
    if isinstance(expected, (dict, list)) and type(expected) is type(actual) and expected == actual:
        # Python considers True == 1; JSON encodings retain that type distinction
        if json.dumps(expected, sort_keys=True) == json.dumps(actual, sort_keys=True):
            return
    if type(expected) is not type(actual):
        yield {"path": path, "expected": expected, "actual": actual}
    elif isinstance(expected, dict):
        for key in sorted(expected.keys() | actual.keys()):
            name = f"{path}.{key}"
            if key not in expected or key not in actual:
                yield {"path": name, "expected": expected.get(key, "<missing>"), "actual": actual.get(key, "<missing>")}
            else:
                yield from field_differences(expected[key], actual[key], name)
    elif isinstance(expected, list):
        if len(expected) != len(actual):
            yield {"path": path + ".length", "expected": len(expected), "actual": len(actual)}
        for i, (a, b) in enumerate(zip(expected, actual)):
            yield from field_differences(a, b, f"{path}[{i}]")
    elif expected != actual:
        yield {"path": path, "expected": expected, "actual": actual}


def simulation_rng_storage(rng):
    # Both streams remain required and typed in raw evidence. FX history is
    # cosmetic; keep availability and unknown fields, but not its seed/count
    return [rng[0], {key: value for key, value in rng[1].items() if key not in ("state", "calls")}]


def compare_objects(left, right, header, right_engine="d1", left_engine="d1"):
    validate_comparison_engines(header, left_engine, right_engine)
    first = {}
    count = 0
    for a, b in itertools.zip_longest(
        object_states(left, header, left_engine), object_states(right, header, right_engine)
    ):
        if a is None or b is None:
            raise EvidenceError("Unequal object trace lengths")
        # Name fields independently of slot, while preserving the first concrete
        # slot/frame in each example. All raw values remain in the archived trace
        for group in ("capacity", "allocator", "segment_heads", "clock", "rng"):
            expected, actual = a[group], b[group]
            if group == "rng" and left_engine == "d1":
                expected, actual = simulation_rng_storage(expected), simulation_rng_storage(actual)
            for found in field_differences(expected, actual, f"$.{group}"):
                first.setdefault(found["path"], {"frame": count, **found})
        for slot in sorted(a["objects"].keys() | b["objects"].keys(), key=int):
            expected = a["objects"].get(slot)
            actual = b["objects"].get(slot)
            if left_engine == "d2":
                expected, actual = semantic_d2_object(expected), semantic_d2_object(actual)
            elif right_engine == "d2":
                if expected is not None:
                    expected = canonical_native_d1_object(expected)
                if actual is not None:
                    actual = canonical_d1_object(actual)
            for found in field_differences(expected, actual, "$.object"):
                first.setdefault(found["path"], {"frame": count, "slot": int(slot), **found})
        count += 1
    return {"status": "fail" if first else "pass", "frames_compared": count, "first_field_differences": first}


def safe_check(operation):
    try:
        return operation()
    except (EvidenceError, OSError, ValueError, KeyError) as error:
        return {"status": "incomplete", "error": str(error)}


WORLD_GROUPS = (
    "globals",
    "tick",
    "players",
    "weapons",
    "walls",
    "doors",
    "triggers",
    "segments",
    "automap",
    "reactor",
    "fuelcenters",
    "robotcenters",
    "effects",
    "stuck",
    "morphs",
    "secrets",
    "ai",
    "exploding_walls",
    "death",
)


def integer_fields(names, **nested):
    return {**dict.fromkeys(names.split(), int), **nested}


# Required common fields of input_demo_world_trace.cpp schema 10. Extra engine
# fields remain in the raw comparison; this is validation, not normalization
VECTOR_SCHEMA = (int, int, int)
AI_LOCAL_SCHEMA = integer_fields(
    "player_awareness_type retry_count consecutive_retries mode previous_visibility rapidfire_count "
    "goal_segment next_fire player_awareness_time time_player_seen time_player_sound_attacked "
    "next_misc_sound_time time_since_processed",
    goal_angles=[VECTOR_SCHEMA],
    delta_angles=[VECTOR_SCHEMA],
    goal_state=[int],
    achieved_state=[int],
)

AI_SAVED_LOCAL_FIELDS = "last_see_time last_attack_time wait_time".split()
AI_SAVED_STATIC_FIELDS = "follow_path_start_seg follow_path_end_seg".split()


def validate_comparison_content(header, engine):
    if header.get("game") != "d1" or engine not in ("d1", "d2"):
        raise EvidenceError("D1 storage mappings require D1 content and a known executable")


def validate_comparison_engines(header, left_engine, right_engine):
    if left_engine == "d1":
        validate_comparison_content(header, right_engine)
    elif left_engine != "d2" or right_engine != "d2" or header.get("game") != "d2":
        raise EvidenceError("Native D2 comparison requires D2 content on both sides")


def xl_storage_profile(header, engine):
    # Capacity bounds mirror XL_compat/capacity.h. A D1 import cannot opt into XL
    environment = header.get("environment", {})
    return (
        engine == "d2"
        and header.get("game") == "d2"
        and header.get("version") == 6
        and isinstance(environment, dict)
        and environment.get("required") == ["xl-clock-input-v1"]
        and type(environment.get("slow_motion_speedup")) is int
        and environment["slow_motion_speedup"] == 6
    )


def semantic_d2_object(value):
    if value is None or "polyobj" not in value:
        return value
    # object.c's two draws and endlevel_multi_impl.h select overrides only >0
    # Normalize only the two observed absence sentinels; preserve other values
    poly = value["polyobj"]
    if poly["alt_textures"] in (-1, 0):
        return {**value, "polyobj": {**poly, "alt_textures": 0}}
    return value


def semantic_d2_world(row):
    if row["type"] == "object_boundary":
        return {**row, "slots": {slot: semantic_d2_object(obj) for slot, obj in row["slots"].items()}}
    # effects.c reads sidenum only under EF_ONE_SHOT; collide.c/camera_glass.cpp
    # assign it before setting that flag. Save restore clears the inactive side
    effects = [
        {**effect, "sidenum": -1}
        if effect["segnum"] == -1 and not effect["flags"] & 2 and effect["sidenum"] in (-1, 0)
        else effect
        for effect in row["state"]["effects"]
    ]
    ai = row["state"]["ai"]
    # ai.c processes and saves only [0, Num_awareness_events); enqueue writes
    # every known field before increasing the count. Retain raw trailing storage
    # in the trace and unknown fields in the comparison, but ignore stale values
    # of those three known fields outside the active queue
    awareness = [
        entry if index < ai["Num_awareness_events"] else {**entry, "segment": 0, "type": 0, "position": [0, 0, 0]}
        for index, entry in enumerate(ai["awareness"])
    ]
    state = {**row["state"], "effects": effects, "ai": {**ai, "awareness": awareness}}
    if "xl_native" in state:
        native = state["xl_native"]
        route = native["route"]
        if route["enabled"]:
            # guidebot_route.c shares this exact bounded wait with its poll consumer.
            # The codec rebases the raw host deadline; preserve it in the receipt
            # while comparing the remaining wait, including immediate eligibility.
            state["xl_native"] = {
                **native,
                "route": {**route, "Escort_route_cache_poll_time": route["cache_poll_wait"]},
            }
    return {**row, "state": state}


def canonical_d1_ai_storage(value, fields):
    """Relocate only the original fields preserved by d1_save_translate.c.

    Keep unknown extension members and all D2-only fields as differences. Never
    mutate reconstructed deltas, which also supply subsequent frames
    """
    result = dict(value)
    saved = dict(result["d1_saved"])
    for field in fields:
        if field in result:
            raise EvidenceError(f"Ambiguous native AI field: {field}")
        result[field] = saved.pop(field)
    if saved:
        result["d1_saved"] = saved
    else:
        del result["d1_saved"]
    return result


def canonical_native_d1_object(value):
    # Native get_reactor_definition ignores the ID; imported saves/mines select
    # definition zero. Keep the raw source ID archived and all model/gun fields
    return {**value, "id": 0} if value["type"] == 9 else value


def canonical_d1_object(value):
    result = dict(value)
    if value["control_type"] in (1, 11):
        result["ai"] = canonical_d1_ai_storage(value["ai"], AI_SAVED_STATIC_FIELDS)
        result["ai"] = without_neutral_fields(
            result["ai"], {"dying_sound_playing": 0, "dying_start_time": 0}, "object.ai"
        )
    if value["type"] == 2:
        result["ai_local"] = canonical_d1_ai_local(value["ai_local"])
    if value["control_type"] == 13:
        # D2 reads creation_time only behind PF_SPAT_BY_PLAYER in do_powerup
        # Native recordings cannot invoke D2's manual spit-powerup operation
        result["powerup"] = without_neutral_fields(value["powerup"], {"flags": 0}, "object.powerup")
        del result["powerup"]["creation_time"]
    if value["control_type"] == 16:
        for key in ("reactor_gun_pos", "reactor_gun_dir"):
            result[key] = trim_neutral_capacity(value[key], 4, [0, 0, 0], "object." + key)
    return result


def without_neutral_fields(value, fields, path):
    result = dict(value)
    for key, neutral in fields.items():
        if type(result[key]) is not type(neutral) or result[key] != neutral:
            raise EvidenceError(f"Unexpected D2-only state at {path}.{key}: {result[key]!r}; expected {neutral!r}")
        del result[key]
    return result


def trim_neutral_capacity(value, count, neutral, path):
    if any(item != neutral for item in value[count:]):
        raise EvidenceError(f"Populated D2-only capacity at {path}")
    return value[:count]


def canonical_d1_ai_local(value):
    # Original checkpoint translation clears these fields; native AI dispatch
    # precedes D2's next_action_time/next_fire2 updates in ai.c
    return without_neutral_fields(
        canonical_d1_ai_storage(value, AI_SAVED_LOCAL_FIELDS), {"next_action_time": 0, "next_fire2": 0}, "ai.local"
    )


def canonical_d1_world(row):
    result = dict(row)
    if row["type"] == "object_boundary":
        result["slots"] = {slot: canonical_d1_object(value) for slot, value in row["slots"].items()}
    else:
        state = dict(row["state"])
        ai = dict(state["ai"])
        ai["local_default"] = canonical_d1_ai_local(ai["local_default"])
        ai["locals"] = {slot: canonical_d1_ai_local(value) for slot, value in ai["locals"].items()}
        # Native boss dispatch never consumes D2's hit timestamp. Restore,
        # new-ship and network cloak reset it to -10 seconds; native damage
        # and contact update the owned integers instead
        boss = without_neutral_fields(ai["boss"], {"Boss_hit_time": -10 * 65536}, "ai.boss")
        boss["Boss_hit_this_frame"] = boss.pop("d1_hit_pending")
        boss["Boss_been_hit"] = boss.pop("d1_been_hit")
        ai["boss"] = boss
        # Only D2 companion path polishing writes this clock. A baseline
        # native-content comparison cannot silently admit that activity
        ai["path_runtime"] = without_neutral_fields(
            ai["path_runtime"], {"last_buddy_polish_path_tick": 0}, "ai.path_runtime"
        )
        # D2 segment mirrors are written by ai_do_cloak_stuff/init_ai_for_ship
        # and used by D2/companion pathing. Native frame/path dispatch consumes
        # the preserved positions, never these mirrors. Missile-camera waking
        # likewise rejects native enemies before changing their awareness
        # The typed raw slot/segment domains are checked before this mapping
        del ai["Believed_player_seg"]
        del ai["Ai_last_missile_camera"]
        ai["cloak"] = [{key: value for key, value in cloak.items() if key != "last_segment"} for cloak in ai["cloak"]]
        state["ai"] = ai
        state["triggers"] = [canonical_d1_trigger(value) for value in state["triggers"]]
        state["players"] = [
            {
                **player,
                **{
                    key: trim_neutral_capacity(player[key], 5, 0, "player." + key)
                    for key in ("primary_ammo", "secondary_ammo")
                },
            }
            for player in state["players"]
        ]
        state["robotcenters"] = [
            {**center, "robot_flags": trim_neutral_capacity(center["robot_flags"], 1, 0, "robotcenter.robot_flags")}
            for center in state["robotcenters"]
        ]
        state["walls"] = [
            without_neutral_fields(wall, {"cloak_value": 0, "controlling_trigger": -1}, "wall")
            for wall in state["walls"]
        ]
        result["state"] = state
    return result


def canonical_d1_trigger(value):
    # Diagnostic contract of d1_in_d2_decode_trigger/source_flags. Native actions
    # occupy pad, ON is bit 6, and one-shot remains bit 1
    if value["flags"] & 128 == 0 or value["flags"] & ~194:
        raise EvidenceError("Imported trigger lacks native identity or has unmapped D2 flags")
    actions = value["pad"] & 255
    flags = (actions & 15) | ((actions & 240) << 2)
    flags |= 16 if value["flags"] & 64 else 0
    flags |= 32 if value["flags"] & 2 else 0
    representative = next(
        (kind for flag, kind in ((256, 4), (8, 3), (1, 0), (64, 2), (128, 5), (512, 6)) if flags & flag), 0
    )
    if value["type"] != representative:
        raise EvidenceError("Imported trigger type contradicts its original actions")
    result = dict(value)
    saved = dict(value["d1_saved"])
    result.update(type=saved.pop("type"), link_num=saved.pop("link_num"), flags=flags)
    del result["pad"]
    if saved:
        result["d1_saved"] = saved
    else:
        del result["d1_saved"]
    return result


# Diagnostic schema mirrors input_demo_object_trace.cpp and the two object.h
# layouts. Preserve extra fields in comparisons; never use this as a projection
PHYSICS_SCHEMA = integer_fields(
    "mass drag brakes turnroll flags",
    velocity=VECTOR_SCHEMA,
    thrust=VECTOR_SCHEMA,
    rotvel=VECTOR_SCHEMA,
    rotthrust=VECTOR_SCHEMA,
)
OBJECT_SCHEMA = integer_fields(
    "signature type id next prev control_type movement_type render_type flags segnum "
    "attached_obj size shields contains_type contains_id contains_count matcen_creator lifeleft",
    pos=VECTOR_SCHEMA,
    last_pos=VECTOR_SCHEMA,
    orient=(VECTOR_SCHEMA,) * 3,
)
OBJECT_STORAGE_SCHEMA = integer_fields(
    "capacity",
    allocator=integer_fields(
        "num_objects highest_object_index signature_seed homer_frame_count current_homer_frame_time do_homer_frame",
        free_obj_list=[int],
    ),
    segment_heads=[int],
    clock=integer_fields("game_time frame_time d_tick_count"),
    rng=(integer_fields("available state calls"),) * 2,
    slots={},
)


def validate_ai_local(local, engine, path):
    validate_world_value(local, AI_LOCAL_SCHEMA, path)
    native_fields = "last_see_time last_attack_time wait_time"
    engine_fields = "next_action_time next_fire2"
    validate_world_value(local, integer_fields(engine_fields if engine == "d2" else native_fields), path)
    if engine == "d2":
        validate_world_value(local, {"d1_saved": integer_fields(native_fields)}, path)
    elif "d1_saved" in local:
        raise EvidenceError(f"D2 saved storage in native AI local: {path}")
    if set((native_fields if engine == "d2" else engine_fields).split()) & local.keys():
        raise EvidenceError(f"Mixed native and D2 AI local storage: {path}")
    for key in ("goal_angles", "delta_angles", "goal_state", "achieved_state"):
        if len(local[key]) != 10:
            raise EvidenceError(f"Incomplete AI submodel state: {path}.{key}")


def validate_object(value, engine, path, xl=False, capacity=1000):
    validate_world_value(value, OBJECT_SCHEMA, path)
    if not (
        0 <= value["type"] <= (15 if engine == "d2" else 14)
        or (xl and engine == "d2" and value["type"] in (16, 18, 19, 20))
    ):
        raise EvidenceError(f"Unsupported live object type: {path}")
    movement, control, render = (value[key] for key in ("movement_type", "control_type", "render_type"))
    camera = xl and engine == "d2" and value["type"] == 16 and control == 18
    if (
        movement not in (0, 1, 3)
        or (control not in (0, 1, 2, 4, 5, 6, 9, 10, 11, 12, 13, 14, 15, 16) and not camera)
        or not 0 <= render <= 7
    ):
        raise EvidenceError(f"Unsupported object union discriminator: {path}")
    required = {}
    if movement == 1:
        required["physics"] = PHYSICS_SCHEMA
    elif movement == 3:
        required["spin_rate"] = VECTOR_SCHEMA
    if control in (1, 11) or camera:
        variant = (
            "dying_sound_playing dying_start_time" if engine == "d2" else "follow_path_start_seg follow_path_end_seg"
        )
        required["ai"] = integer_fields(
            "behavior hide_segment hide_index path_length cur_path_index "
            "danger_laser_signature danger_laser_num " + variant,
            flags=(int,) * 11,
        )
        if engine == "d2":
            required["ai"]["d1_saved"] = integer_fields("follow_path_start_seg follow_path_end_seg")
    elif control == 9:
        required["weapon"] = integer_fields(
            "parent_type parent_num parent_signature creation_time last_hitobj track_goal "
            "multiplier creation_framecount",
        )
        required["weapon"].update({"xl_hit_order": [int]} if xl else {"hitobj_list": (int,) * 1000})
    elif control == 2:
        required["explosion"] = integer_fields(
            "spawn_time delete_time delete_objnum attach_parent prev_attach next_attach"
        )
    elif control == 14:
        required["light_intensity"] = int
    elif control == 13:
        required["powerup"] = integer_fields("count creation_time flags" if engine == "d2" else "count")
    elif control == 16:
        required["reactor_gun_pos"] = required["reactor_gun_dir"] = (VECTOR_SCHEMA,) * (8 if engine == "d2" else 4)
    if render in (1, 6) or (render == 0 and value["type"] == 12):
        required["polyobj"] = integer_fields(
            "model_num subobj_flags tmap_override alt_textures", anim_angles=(VECTOR_SCHEMA,) * 10
        )
    elif render in (2, 4, 5, 7):
        required["vclip"] = integer_fields("vclip_num frametime framenum")
    if value["type"] == 2:
        required["ai_local"] = AI_LOCAL_SCHEMA
    validate_world_value(value, required, path)
    if control == 9:
        weapon = value["weapon"]
        if ("hitobj_list" if xl else "xl_hit_order") in weapon:
            raise EvidenceError(f"Mixed classic and XL projectile storage: {path}")
        if xl:
            hits = weapon["xl_hit_order"]
            # runtime/projectile.h and projectile.cpp: ordered, distinct slots,
            # including deleted/reused victims, with the newest hit mirrored
            if (
                len(hits) > 20
                or len(set(hits)) != len(hits)
                or any(not 0 <= victim < capacity for victim in hits)
                or (hits and weapon["last_hitobj"] != hits[-1])
                or (not hits and weapon["last_hitobj"] != -1)
            ):
                raise EvidenceError(f"Invalid XL projectile hit order: {path}")
    if control in (1, 11) or camera:
        incompatible = (
            AI_SAVED_STATIC_FIELDS if engine == "d2" else ("d1_saved", "dying_sound_playing", "dying_start_time")
        )
        if set(incompatible) & value["ai"].keys():
            raise EvidenceError(f"Mixed native and D2 static AI storage: {path}.ai")
    if value["type"] == 2:
        validate_ai_local(value["ai_local"], engine, path + ".ai_local")


def validate_object_storage(row, engine, xl=False):
    if engine not in ("d1", "d2"):
        raise EvidenceError("Unknown object storage engine")
    validate_world_value(row, OBJECT_STORAGE_SCHEMA, "$.object_storage")
    valid_capacity = 1000 <= row["capacity"] <= 32768 if xl and engine == "d2" else row["capacity"] == 1000
    if not valid_capacity or len(row["allocator"]["free_obj_list"]) != row["capacity"]:
        raise EvidenceError("Incomplete object allocator storage")
    for slot, value in row["slots"].items():
        if not slot.isdecimal() or str(int(slot)) != slot or not 0 <= int(slot) < row["capacity"]:
            raise EvidenceError("Invalid object slot identity")
        if value is None:
            if row["type"] == "object_boundary" or row["reset"]:
                raise EvidenceError("Object snapshot cannot delete a slot")
        else:
            validate_object(value, engine, f"$.slots.{slot}", xl, row["capacity"])
    if row["type"] == "object_boundary":
        validate_object_cardinality(row, row["slots"])


def validate_object_cardinality(row, objects):
    allocator = row["allocator"]
    if len(objects) != allocator["num_objects"]:
        raise EvidenceError("Live object storage does not match its allocator count")
    if not -1 <= allocator["highest_object_index"] < row["capacity"] or any(
        int(slot) > allocator["highest_object_index"] for slot in objects
    ):
        raise EvidenceError("Live object storage exceeds its highest allocated slot")


ENDLEVEL_FLY_SCHEMA = integer_fields(
    "object speed first_time transition_reached offset_frac offset_dist",
    angles=VECTOR_SCHEMA,
    step=VECTOR_SCHEMA,
    angstep=VECTOR_SCHEMA,
    headvec=VECTOR_SCHEMA,
)
ENDLEVEL_SCHEMA = integer_fields(
    "sequence data_loaded transition_segment exit_segment outside explosion_playing "
    "mine_destroyed movie_played current_speed desired_speed camera",
    exit_point=VECTOR_SCHEMA,
    ground_exit_point=VECTOR_SCHEMA,
    side_exit_point=VECTOR_SCHEMA,
    station_position=VECTOR_SCHEMA,
    exit_orientation=(VECTOR_SCHEMA,) * 3,
    surface_orientation=(VECTOR_SCHEMA,) * 3,
    exit_angles=VECTOR_SCHEMA,
    player_angles=VECTOR_SCHEMA,
    player_destination_angles=VECTOR_SCHEMA,
    camera_angles=VECTOR_SCHEMA,
    camera_destination_angles=VECTOR_SCHEMA,
    frame=integer_fields("timer bank_rate explosion_wait1 explosion_wait2 ext_expl_halflife sound_count"),
    fly=(ENDLEVEL_FLY_SCHEMA,) * 2,
    explosion=[OBJECT_SCHEMA],
)

WORLD_SCHEMA = {
    "death": integer_fields("active exploded eggs_dropped aborted elapsed saved_flags saved_control"),
    "exploding_walls": integer_fields("capacity", slots={"*": (int, int, int)}),
    "endlevel": ENDLEVEL_SCHEMA,
    "ai": integer_fields(
        "Ai_initialized Overall_agitation local_capacity path_free_index Num_awareness_events",
        Believed_player_pos=VECTOR_SCHEMA,
        local_default=AI_LOCAL_SCHEMA,
        locals={"*": AI_LOCAL_SCHEMA},
        path_runtime=integer_fields(
            "last_tick_garbage_collected player_path_length player_hide_index "
            "player_cur_path_index player_following_path_flag player_goal_segment"
        ),
        paths=[integer_fields("segment", point=VECTOR_SCHEMA)],
        cloak=[integer_fields("last_time", last_position=VECTOR_SCHEMA)],
        awareness=[integer_fields("segment type", position=VECTOR_SCHEMA)],
        boss=integer_fields(
            "Boss_cloak_start_time Boss_cloak_end_time Last_teleport_time Boss_teleport_interval "
            "Boss_cloak_interval Boss_cloak_duration Last_gate_time Gate_interval "
            "Boss_dying_start_time Boss_dying Boss_dying_sound_playing Num_boss_teleport_segs",
            teleport_segments=[int],
        ),
    ),
    "globals": integer_fields(
        "Player_num N_players Current_level_num Next_level_num Game_mode Game_suspended "
        "Difficulty_level Difficulty_level_changed Difficulty_level_min_seen Difficulty_level_max_seen "
        "GameTime64 FrameTime Endlevel_sequence Collision_delay_last_play_time "
        "Fusion_next_sound_time Fuelcen_last_sound_time"
    ),
    "tick": integer_fields("count step timer"),
    "players": [
        integer_fields(
            "connected objnum flags energy shields shields_delta shields_time shields_time_hours "
            "shields_certain lives level laser_level starting_level killer_objnum primary_weapon_flags "
            "secondary_weapon_flags primary_weapon secondary_weapon last_score score time_level time_total "
            "cloak_time invulnerable_time KillGoalCount net_killed_total net_kills_total num_kills_level "
            "num_kills_total num_robots_level num_robots_total hostages_rescued_total hostages_total "
            "hostages_on_board hostages_level homing_object_dist hours_level hours_total",
            primary_ammo=[int],
            secondary_ammo=[int],
        )
    ],
    "weapons": integer_fields(
        "Next_laser_fire_time Last_laser_fired_time Next_missile_fire_time Next_flare_fire_time "
        "Auto_fire_fusion_cannon_time Global_laser_firing_count Global_missile_firing_count "
        "fusion_charge spreadfire_toggle missile_gun proximity_dropped helix_orientation "
        "smartmines_dropped last_omega_fire_time PrimaryWeaponPickedUp SecondaryWeaponPickedUp "
        "delayed_primary_autoselect_weapon_index delayed_secondary_autoselect_weapon_index"
    ),
    "walls": [integer_fields("segnum sidenum hps linked_wall type flags state trigger clip_num keys")],
    "doors": [integer_fields("n_parts time", front=(int, int), back=(int, int))],
    "triggers": [integer_fields("type flags num_links value time", segments=[int], sides=[int])],
    "segments": [
        integer_fields(
            "value special matcen_num static_light",
            sides=[integer_fields("wall texture overlay", uvls=[VECTOR_SCHEMA])],
        )
    ],
    "automap": [int],
    "reactor": integer_fields(
        "Control_center_destroyed Countdown_timer Countdown_seconds_left Total_countdown_time "
        "Control_center_been_hit Control_center_player_been_seen Control_center_next_fire_time "
        "Control_center_present Dead_controlcen_object_num controlcen_death_silence "
        "Reactor_countdown_paused num_links",
        segments=[int],
        sides=[int],
    ),
    "fuelcenters": [
        integer_fields("Type segnum Flag Enabled Lives Capacity MaxCapacity Timer Disable_time", center=VECTOR_SCHEMA)
    ],
    "robotcenters": [integer_fields("hit_points interval segnum fuelcen_num", robot_flags=[int])],
    "effects": [integer_fields("time_left frame_count flags segnum sidenum dest_bm_num wall_bitmap object_bitmap")],
    "stuck": integer_fields("count", slots={"*": (int, int, int)}),
    "morphs": {
        "*": integer_fields(
            "object Morph_sig n_submodels_active morph_save_control_type morph_save_movement_type",
            submodel_active=[int],
            n_morphing_points=[int],
            submodel_startpoints=[int],
            physics=integer_fields(
                "mass drag brakes turnroll flags",
                velocity=VECTOR_SCHEMA,
                thrust=VECTOR_SCHEMA,
                rotvel=VECTOR_SCHEMA,
                rotthrust=VECTOR_SCHEMA,
            ),
            points=[(VECTOR_SCHEMA, VECTOR_SCHEMA, int)],
        )
    },
    "secrets": [(int, int)],
}


XL_PLAYER_CFG_SCHEMA = integer_fields(
    "auto_leveling persistent_debris headlight_active_default no_fire_autoselect cycle_autoselect_only "
    "select_after_fire classic_autoselect_weapon autoselect_only_once original_homing guidebot_routing_mode",
    primary_order=[int],
    secondary_order=[int],
)


XL_TIME_SCHEMA = {
    "version": int,
    "bound": bool,
    "anchor": (int, int, int, int),
    "slow_motion": {
        "clocks": ((float, int, int, bool),) * 2,
        "transition_sound": bool,
        "slow_requested": bool,
        "bullet_requested": bool,
        "pickups": [(int, int, int)],
    },
    "weapon_clock": (int, int),
}
XL_CAMERA_SCHEMA = integer_fields(
    "version ticks",
    states=[
        integer_fields(
            "monitor owner signature type id angle delta t0 brakes", live=bool, original=(VECTOR_SCHEMA,) * 3
        )
    ],
)
# D2 controls.h/player.h/weapon.h: 16-bit wiggle phase, 8 guided player slots,
# and 4 XL-owned seismic detonation slots; object clocks use the observed pool size
ESCORT_SIGNED_FIELDS = (
    "Max_escort_length Escort_kill_object Escort_goal_object Escort_special_goal Escort_goal_index "
    "Buddy_messages_suppressed Escort_goal_secret_seg Escort_goal_secret_side Buddy_objnum Buddy_allowed_to_talk "
    "Looking_for_marker Last_buddy_key Stolen_item_index Escort_goal_message_signature Escort_goal_message_owner"
).split()
ESCORT_UNSIGNED_FIELDS = "Escort_goal_message_generation Escort_goal_message_sequence".split()
ESCORT_CLOCK_FIELDS = (
    "Escort_goal_message_next_send Escort_last_path_created Buddy_sorry_time Last_buddy_message_time "
    "Buddy_last_seen_player Buddy_last_player_path_created Last_come_back_message_time Buddy_last_missile_time "
    "Re_init_thief_time Last_thief_hit_time"
).split()
# guidebot_escort_state.h / guidebot_goal_message.h: ten stolen-item bytes and
# the complete 150-byte message buffer, including native trailing storage
XL_ESCORT_SCHEMA = integer_fields(
    " ".join(("version", "persistent_message", *ESCORT_SIGNED_FIELDS, *ESCORT_UNSIGNED_FIELDS, *ESCORT_CLOCK_FIELDS)),
    stolen=(int,) * 10,
    Escort_goal_message_text=(int,) * 150,
)
ROUTE_SIGNED_FIELDS = (
    "Escort_route_goal_active Escort_route_goal_target_seg Escort_route_goal_objective_kind "
    "Escort_route_goal_activation_kind Escort_route_goal_requires_guided_missile "
    "Escort_route_goal_objective_seg Escort_route_goal_objective_side Escort_route_goal_objective_wall "
    "Escort_route_goal_objective_trigger Escort_route_goal_objective_object "
    "Escort_route_goal_objective_key_index Escort_route_goal_guidance_mode Escort_route_goal_guidance_seg "
    "Escort_route_goal_frontier_player_openable_door Escort_unexplored_route_target_active "
    "Escort_unexplored_route_target_component_size Escort_unexplored_route_target_target_seg "
    "Escort_unexplored_route_target_waypoint_seg Escort_unexplored_route_target_direct_reachable "
    "Escort_hostage_object Escort_hostage_signature Escort_route_target_mode Escort_route_metadata_dirty "
    "Escort_route_cache_improvement_pending Escort_route_dirty_notification_time_valid "
    "Escort_route_last_audit_rescan_time_valid Escort_route_goal_request_pending Escort_nav_trace_last_pos_x "
    "Escort_nav_trace_last_pos_y Escort_nav_trace_last_pos_z Escort_nav_trace_last_signature "
    "Escort_nav_trace_last_goal Escort_nav_trace_last_path_index Escort_nav_trace_stall_samples "
    "Escort_nav_liveness_anchor_x Escort_nav_liveness_anchor_y Escort_nav_liveness_anchor_z "
    "Escort_nav_liveness_signature Escort_route_avoid_from_seg Escort_route_avoid_seg "
    "Escort_route_avoid_from_seg2 Escort_route_avoid_seg2 Escort_route_avoid_trigger Escort_route_avoid_wall "
    "Escort_route_progress_signature Escort_route_progress_seg Escort_route_progress_path_index "
    "Escort_route_progress_target_seg Escort_route_progress_stall_samples Escort_route_path_recalc_pending "
    "Escort_route_path_recalc_goal_kind Escort_route_path_recalc_goal_seg "
    "Escort_route_path_recalc_goal_trigger Escort_route_path_recalc_goal_wall Escort_route_logged_readiness"
).split()
ROUTE_UNSIGNED_FIELDS = (
    "Escort_route_seen_revision Escort_route_metadata_rescan_count Escort_route_guidance_full_search_count "
    "Escort_route_ignored_nonowner_key_change_count Escort_route_boss_move_invalidation_count "
    "Escort_route_wall_generation Escort_route_trigger_generation Escort_route_object_generation "
    "Escort_route_reactor_generation Escort_route_automap_generation Escort_route_pending_event_mask "
    "Escort_route_pending_audit_mask Escort_route_deferred_live_event_mask "
    "Escort_route_event_notification_count Escort_route_notification_coalesced_count "
    "Escort_route_redundant_dirty_domain_count Escort_route_event_coalesced_rescan_count "
    "Escort_route_publish_latency_sample_count Escort_route_publish_latency_last_ticks "
    "Escort_route_publish_latency_max_ticks Escort_route_ignored_nonowner_event_count "
    "Escort_route_audit_check_count Escort_route_audit_discovery_count Escort_route_audit_only_discovery_count "
    "Escort_route_audit_work_total Escort_route_audit_work_max Escort_route_audit_deferred_count "
    "Escort_route_certificate_check_count Escort_route_certificate_failure_count "
    "Escort_route_certificate_work_total Escort_route_certificate_work_max Escort_route_path_retained_count "
    "Escort_route_path_replaced_count Escort_route_invalid_path_stopped_count Escort_route_audit_domain_cursor "
    "Escort_route_stall_recovery_count Escort_route_path_recalc_limiter_count "
    "Escort_route_path_recalc_suppressed_count"
).split()
ROUTE_CLOCK_FIELDS = (
    "Escort_hostage_next_plan Escort_route_first_dirty_notification_time Escort_route_audit_next_time "
    "Escort_route_last_audit_rescan_time Escort_route_completion_check_time Escort_nav_trace_next_time "
    "Escort_nav_liveness_last_progress_time Escort_route_progress_next_time "
    "Escort_route_path_recalc_requested_time Escort_route_path_recalc_due_time Escort_route_cache_poll_time"
).split()
# Shared guidebot_route_state capacities: label64, recalc4, native trigger pool
XL_ROUTE_SCHEMA = integer_fields(
    " ".join(
        (
            "trigger_capacity",
            "restore_pending",
            "cache_poll_wait",
            *ROUTE_SIGNED_FIELDS,
            *ROUTE_UNSIGNED_FIELDS,
            *ROUTE_CLOCK_FIELDS,
        )
    ),
    version=int,
    enabled=bool,
    Escort_route_goal_label=(int,) * 64,
    Escort_route_path_recalc_limiter_timestamps=(int,) * 4,
    Escort_route_activated_triggers=[int],
)
# Typed metadata contract mirrors guidebot_metadata_fields.h and the native capacities
METADATA_I32 = range(-2147483648, 2147483648)
METADATA_U32 = range(0, 4294967296)
METADATA_U64 = range(0, 18446744073709551616)
METADATA_BYTE = range(0, 256)
METADATA_LONG_DOUBLE_SCHEMA = {"negative": bool, "bits": [range(0, 2)], "exponent": range(-16494, 16385)}

METADATA_LEVEL_METADATA_ROUTE_STEP_SCHEMA = {
    "kind": METADATA_I32,
    "seg": METADATA_I32,
    "side": METADATA_I32,
    "wall_num": METADATA_I32,
    "trigger_num": METADATA_I32,
    "trigger_type": METADATA_I32,
    "key_index": METADATA_I32,
    "key_carrier_objnum": METADATA_I32,
    "can_be_bypassed": METADATA_I32,
    "activation_kind": METADATA_I32,
    "requires_guided_missile": METADATA_I32,
    "guided_missile_point_count": METADATA_I32,
    "is_switch_restorer": METADATA_I32,
    "restored_wall_num": METADATA_I32,
    "switch_shot_quality": METADATA_I32,
    "switch_shot_incidence_cosine": METADATA_I32,
    "switch_guidance_candidate_count": range(0, 17),
    "switch_guidance_candidate_seg": (METADATA_I32,) * 16,
    "switch_guidance_candidate_pos": ((METADATA_I32,) * 3,) * 16,
    "switch_guidance_candidate_quality": (METADATA_I32,) * 16,
    "switch_guidance_candidate_incidence": (METADATA_I32,) * 16,
    "path_segment_count": METADATA_I32,
    "path_terminal_segment": METADATA_I32,
    "activation_pos_valid": METADATA_I32,
    "activation_pos": (METADATA_I32,) * 3,
    "aim_pos_valid": METADATA_I32,
    "aim_pos": (METADATA_I32,) * 3,
    "label_pos_valid": METADATA_I32,
    "label_pos": (METADATA_I32,) * 3,
    "distance_from_previous": float,
    "label": (METADATA_BYTE,) * 64,
    "trigger_type_name": (METADATA_BYTE,) * 96,
    "opened_link_count": range(0, 11),
    "opened_link_seg": (METADATA_I32,) * 10,
    "opened_link_side": (METADATA_I32,) * 10,
    "opened_link_wall": (METADATA_I32,) * 10,
}

METADATA_LEVEL_METADATA_STATE_SCHEMA = {
    "energy_center_segment_count": METADATA_I32,
    "energy_center_raw_count": METADATA_I32,
    "energy_center_count": METADATA_I32,
    "energy_center_group_distance": METADATA_I32,
    "energy_center_nearest_raw_distance": METADATA_I32,
    "matcen_segment_count": METADATA_I32,
    "matcen_raw_count": METADATA_I32,
    "matcen_count": METADATA_I32,
    "mine_volume": float,
    "mine_volume_normalized": float,
    "travel_distance": float,
    "travel_time_seconds": METADATA_I32,
    "guidebot_count": METADATA_I32,
    "guidebot_placed": METADATA_I32,
    "guidebot_accessible": METADATA_I32,
    "guidebot_placement_note": (METADATA_BYTE,) * 128,
    "guidebot_note": (METADATA_BYTE,) * 128,
    "route_status": METADATA_I32,
    "route_problem": (METADATA_BYTE,) * 128,
    "route_note": (METADATA_BYTE,) * 128,
    "route_required_key_mask": METADATA_I32,
    "route_completing_key_mask_set": METADATA_I32,
    "unnecessary_key_mask": METADATA_I32,
    "route_step_count": range(0, 97),
    "route_steps": (METADATA_LEVEL_METADATA_ROUTE_STEP_SCHEMA,) * 96,
}

METADATA_ROUTE_PLANNER_PLAN_SUMMARY_SCHEMA = {
    "endpoint_kind": METADATA_I32,
    "route_step_count": range(0, 97),
    "first_pending_step": range(-1, 96),
    "first_pending_path_segment_count": METADATA_I32,
    "first_pending_path_terminal_segment": METADATA_I32,
    "partial_frontier_segment": METADATA_I32,
}

METADATA_ROUTE_SNAPSHOT_SUMMARY_SCHEMA = {
    "topology_hash": METADATA_U64,
    "state_hash": METADATA_U64,
    "start_hash": METADATA_U64,
    "progression_hash": METADATA_U64,
    "navigation_hash": METADATA_U64,
    "trigger_hash": METADATA_U64,
    "object_hash": METADATA_U64,
    "automap_hash": METADATA_U64,
    "actor_hash": METADATA_U64,
    "segment_count": METADATA_I32,
    "wall_count": METADATA_I32,
    "trigger_count": METADATA_I32,
    "object_count": METADATA_I32,
    "start_segment": METADATA_I32,
    "key_mask": METADATA_I32,
    "control_center_destroyed": METADATA_I32,
    "topology_generation": METADATA_U32,
    "start_generation": METADATA_U32,
    "progression_generation": METADATA_U32,
    "navigation_generation": METADATA_U32,
    "trigger_generation": METADATA_U32,
    "object_generation": METADATA_U32,
    "automap_generation": METADATA_U32,
    "actor_generation": METADATA_U32,
}

METADATA_GUIDEBOT_ROUTE_VALIDITY_CERTIFICATE_SCHEMA = {
    "status": METADATA_I32,
    "source_trigger": METADATA_I32,
    "source_wall": METADATA_I32,
    "source_object": METADATA_I32,
    "frontier_segment": METADATA_I32,
}

METADATA_GUIDEBOT_ROUTE_DECISION_SCHEMA = {
    "version": METADATA_U32,
    "status": METADATA_I32,
    "target_policy": METADATA_I32,
    "requested_target_segment": METADATA_I32,
    "objective_kind": METADATA_I32,
    "activation_kind": METADATA_I32,
    "requires_guided_missile": METADATA_I32,
    "objective_trigger": METADATA_I32,
    "objective_wall": METADATA_I32,
    "objective_key": METADATA_I32,
    "objective_object": METADATA_I32,
    "objective_segment": METADATA_I32,
    "objective_side": METADATA_I32,
    "guidance_position_valid": METADATA_I32,
    "guidance_position": (METADATA_I32,) * 3,
    "path_terminal_segment": METADATA_I32,
    "partial_frontier_segment": METADATA_I32,
    "dependency_mask": METADATA_U32,
    "topology_hash": METADATA_U64,
    "state_hash": METADATA_U64,
    "start_hash": METADATA_U64,
    "progression_hash": METADATA_U64,
    "navigation_hash": METADATA_U64,
    "trigger_hash": METADATA_U64,
    "object_hash": METADATA_U64,
    "automap_hash": METADATA_U64,
    "actor_hash": METADATA_U64,
    "input_hash": METADATA_U64,
    "semantic_hash": METADATA_U64,
    "guidance_hash": METADATA_U64,
    "decision_hash": METADATA_U64,
    "certificate": METADATA_GUIDEBOT_ROUTE_VALIDITY_CERTIFICATE_SCHEMA,
}

METADATA_GUIDEBOT_ROUTE_CERTIFIER_WORKSPACE_SCHEMA = {
    "reachable": (METADATA_BYTE,) * 9000,
    "queue": (METADATA_I32,) * 9000,
    "strategic_distance": (METADATA_I32,) * 9000,
    "physical_distance": (METADATA_I32,) * 9000,
    "firing_cache_valid": METADATA_I32,
    "firing_cache_num_segments": range(0, 9001),
    "firing_cache_num_walls": METADATA_I32,
    "firing_cache_trigger": METADATA_I32,
    "firing_cache_wall": METADATA_I32,
    "firing_cache_aim": (METADATA_I32,) * 3,
    "firing_cache_segment": METADATA_I32,
    "firing_cache_path_segment_count": METADATA_I32,
    "firing_cache_position": (METADATA_I32,) * 3,
    "firing_cache_shot_quality": METADATA_I32,
    "firing_cache_incidence_cosine": METADATA_I32,
    "job_active": METADATA_I32,
    "job_start_segment": METADATA_I32,
    "job_num_segments": range(0, 9001),
    "reach_head": range(0, 9001),
    "reach_tail": range(0, 9001),
    "reach_side": range(0, 7),
    "reach_complete": METADATA_I32,
    "job_visited_segments": METADATA_U32,
    "job_evaluated_edges": METADATA_U32,
    "job_evaluated_firing_positions": METADATA_U32,
    "firing_search_active": METADATA_I32,
    "firing_search_pass": METADATA_I32,
    "firing_search_segment": METADATA_I32,
    "firing_search_detailed_count": range(0, 9),
    "firing_search_selection_segment": METADATA_I32,
    "firing_search_selection_complete": METADATA_I32,
    "firing_search_detailed_index": range(0, 9),
    "firing_search_detailed_sample": METADATA_I32,
    "firing_search_detailed_segments": (METADATA_I32,) * 8,
    "firing_search_detailed_scores": (METADATA_LONG_DOUBLE_SCHEMA,) * 8,
    "firing_search_original_segment": METADATA_I32,
    "firing_search_original_position": (METADATA_I32,) * 3,
    "firing_search_best_segment": METADATA_I32,
    "firing_search_best_path_distance": METADATA_I32,
    "firing_search_best_position": (METADATA_I32,) * 3,
    "firing_search_best_quality": METADATA_I32,
    "firing_search_best_incidence": METADATA_I32,
    "firing_search_best_score": METADATA_LONG_DOUBLE_SCHEMA,
    "firing_frontier_active": METADATA_I32,
    "firing_frontier_phase": METADATA_I32,
    "firing_frontier_goal_segment": METADATA_I32,
    "firing_frontier_init_segment": METADATA_I32,
    "firing_frontier_head": range(0, 9001),
    "firing_frontier_tail": range(0, 9001),
    "firing_frontier_side": METADATA_I32,
    "firing_frontier_scan_segment": METADATA_I32,
    "firing_frontier_best_segment": METADATA_I32,
    "firing_frontier_best_remaining": METADATA_I32,
    "firing_frontier_best_incidence": METADATA_I32,
    "unexplored_active": METADATA_I32,
    "unexplored_init_segment": METADATA_I32,
    "unexplored_scan_segment": METADATA_I32,
    "unexplored_component_head": range(0, 9001),
    "unexplored_component_tail": range(0, 9001),
    "unexplored_component_side": METADATA_I32,
    "unexplored_component_size": METADATA_I32,
    "unexplored_component_target": METADATA_I32,
    "unexplored_component_distance": METADATA_I32,
    "unexplored_best_size": METADATA_I32,
    "unexplored_best_target": METADATA_I32,
    "unexplored_best_distance": METADATA_I32,
}

METADATA_GUIDEBOT_ROUTE_CERTIFIER_SUMMARY_SCHEMA = {
    "selected_step": METADATA_I32,
    "selected_segment": METADATA_I32,
    "blocking_step": METADATA_I32,
    "blocking_segment": METADATA_I32,
    "blocking_reason": METADATA_I32,
    "used_prepared_fallback": METADATA_I32,
    "required_steps_low": METADATA_U64,
    "visited_segments": METADATA_U32,
    "evaluated_edges": METADATA_U32,
    "evaluated_actions": METADATA_U32,
    "rejected_actions": METADATA_U32,
    "evaluated_firing_positions": METADATA_U32,
    "reranked_firing_position": METADATA_I32,
    "firing_cache_hit": METADATA_I32,
    "approximate_firing_position": METADATA_I32,
    "steep_firing_position": METADATA_I32,
}

METADATA_LEVEL_METADATA_LIVE_WORK_SUMMARY_SCHEMA = {
    "full_plan_calls": METADATA_U32,
    "blocked_full_plan_calls": METADATA_U32,
    "deferred_refreshes": METADATA_U32,
    "retained_incumbents": METADATA_U32,
    "deferred_without_incumbent": METADATA_U32,
    "route_ticks": METADATA_U32,
    "deferred_ticks": METADATA_U32,
    "completed_ticks": METADATA_U32,
    "tick_overruns": METADATA_U32,
    "compiled_selector_calls": METADATA_U32,
    "compiled_selector_successes": METADATA_U32,
    "compiled_selector_failures": METADATA_U32,
    "compiled_selector_max_evaluated_actions": METADATA_U32,
    "pending_event_mask": METADATA_U32,
    "pending": METADATA_I32,
    "reachability_cursor": METADATA_I32,
    "firing_candidate_cursor": METADATA_I32,
    "firing_candidate_pass": METADATA_I32,
    "unexplored_candidate_cursor": METADATA_I32,
    "last_tick_us": METADATA_U64,
    "max_tick_us": METADATA_U64,
    "last_refresh_us": METADATA_U64,
    "max_refresh_us": METADATA_U64,
    "refresh_overruns": METADATA_U32,
}

XL_METADATA_SCHEMA = {
    "version": range(1, 2),
    "canonical_state": METADATA_LEVEL_METADATA_STATE_SCHEMA,
    "canonical_plan_summary": METADATA_ROUTE_PLANNER_PLAN_SUMMARY_SCHEMA,
    "canonical_plan_summary_valid": METADATA_I32,
    "live_route_state": METADATA_LEVEL_METADATA_STATE_SCHEMA,
    "live_route_state_valid": METADATA_I32,
    "live_plan_summary": METADATA_ROUTE_PLANNER_PLAN_SUMMARY_SCHEMA,
    "live_plan_summary_valid": METADATA_I32,
    "live_candidate_state": METADATA_LEVEL_METADATA_STATE_SCHEMA,
    "live_candidate_summary": METADATA_ROUTE_PLANNER_PLAN_SUMMARY_SCHEMA,
    "live_candidate_certificate": METADATA_GUIDEBOT_ROUTE_VALIDITY_CERTIFICATE_SCHEMA,
    "canonical_snapshot": METADATA_ROUTE_SNAPSHOT_SUMMARY_SCHEMA,
    "canonical_snapshot_valid": METADATA_I32,
    "canonical_analysis_profile_hash": METADATA_U64,
    "live_snapshot": METADATA_ROUTE_SNAPSHOT_SUMMARY_SCHEMA,
    "live_snapshot_valid": METADATA_I32,
    "progression_object_audit_hash": METADATA_U64,
    "progression_object_audit_hash_valid": METADATA_I32,
    "navigation_access_audit_hash": METADATA_U64,
    "navigation_access_audit_hash_valid": METADATA_I32,
    "route_start_objnum": METADATA_I32,
    "route_start_seg": METADATA_I32,
    "live_route_target_seg": METADATA_I32,
    "route_readiness": METADATA_I32,
    "route_revision": METADATA_U32,
    "route_certifier_workspace": METADATA_GUIDEBOT_ROUTE_CERTIFIER_WORKSPACE_SCHEMA,
    "route_frontier_workspace": METADATA_GUIDEBOT_ROUTE_CERTIFIER_WORKSPACE_SCHEMA,
    "route_certifier_summary": METADATA_GUIDEBOT_ROUTE_CERTIFIER_SUMMARY_SCHEMA,
    "live_certificate": METADATA_GUIDEBOT_ROUTE_VALIDITY_CERTIFICATE_SCHEMA,
    "published_route_decision": METADATA_GUIDEBOT_ROUTE_DECISION_SCHEMA,
    "published_route_decision_valid": METADATA_I32,
    "live_route_provenance": METADATA_I32,
    "live_work_summary": METADATA_LEVEL_METADATA_LIVE_WORK_SUMMARY_SCHEMA,
    "live_route_work_pending": METADATA_I32,
}

XL_OBJECT_EVENT_SCHEMA = {
    **dict.fromkeys("authored_owner owner signature destination audio_key value time".split(), METADATA_I32),
    "operated": bool,
    "type": METADATA_BYTE,
    "flags": METADATA_BYTE,
    "num_links": range(0, 11),
    "pad": range(-128, 128),
    "segments": (range(-32768, 32768),) * 10,
    "sides": (range(-32768, 32768),) * 10,
}
XL_BOSS_EVENT_SCHEMA = {
    **dict.fromkeys("authored_owner owner signature id interval dying sound home".split(), METADATA_I32),
    **dict.fromkeys(
        "last cloak_start cloak_end teleport hit death_start".split(), range(-9223372036854775808, 9223372036854775808)
    ),
    "generated": bool,
    "teleport_segments": [METADATA_I32],
}
XL_EVENTS_SCHEMA = {
    "version": range(1, 2),
    "objects": [XL_OBJECT_EVENT_SCHEMA],
    "bosses": {"live": [XL_BOSS_EVENT_SCHEMA], "restore_pending": bool, "checkpoint": [XL_BOSS_EVENT_SCHEMA]},
    "equipment": {
        "makers": [
            {
                **dict.fromkeys("segment owner".split(), METADATA_I32),
                "timer": range(-9223372036854775808, 9223372036854775808),
                "enabled": range(0, 2),
                "pending": range(0, 2),
                "flags": (METADATA_U32,) * 2,
            }
        ],
        "station_links": [(METADATA_I32,) * 2],
    },
    "producers": {
        "implicit_flags": METADATA_U32,
        "ready_mask": METADATA_U32,
        "authored": [(METADATA_U32, METADATA_U32, METADATA_I32, METADATA_I32)],
    },
}
XL_BOOST_SCHEMA = {
    **dict.fromkeys("slot owner signature".split(), METADATA_I32),
    "boosted": range(-1, 2),
    "speed": range(0, 11),
    "steps": range(0, 10),
    "phase": range(0, 65536),
    **dict.fromkeys("velocity minimum maximum source destination direction".split(), (METADATA_I32,) * 3),
}
XL_SPAWN_SCHEMA = {
    **dict.fromkeys("segment side".split(), METADATA_I32),
    "position": (METADATA_I32,) * 3,
    "orientation": (METADATA_I32,) * 9,
}
# Keep fixed storage aligned with runtime/projectile.h (20 hits) and spawn_points.h (16 slots)
XL_PROJECTILE_SCHEMA = {
    **dict.fromkeys("slot owner signature".split(), METADATA_I32),
    "count": range(1, 21),
    "victims": (METADATA_I32,) * 20,
}
XL_MOTION_SCHEMA = {
    "version": range(1, 2),
    "boosts": {"last_speed": range(0, 11), "states": [XL_BOOST_SCHEMA]},
    "spawns": {"configured": bool, "points": (XL_SPAWN_SCHEMA,) * 16},
    "progression": {
        "mission": str,
        "states": [range(-1, 2)],
        "next": range(-32768, 32768),
        "cache_restore": bool,
        "bound": bool,
    },
    "projectiles": [XL_PROJECTILE_SCHEMA],
}
XL_NATIVE_SCHEMA = integer_fields(
    "version omega_charge wiggle_phase",
    afterburner_charge=[int],
    afterburner_times=[int],
    guided=((int, int),) * 8,
    metadata=XL_METADATA_SCHEMA,
    events=XL_EVENTS_SCHEMA,
    motion=XL_MOTION_SCHEMA,
    escort=XL_ESCORT_SCHEMA,
    route={"version": int, "enabled": bool},
    seismic=integer_fields(
        "strength pending magnitude phase fraction", interval=[int], detonations=((bool, int, float),) * 4
    ),
)


def validate_world_value(value, schema, path):
    if schema is str:
        if type(value) is not str:
            raise EvidenceError(f"World field must be a string: {path}")
    elif schema is int:
        if type(value) is not int:
            raise EvidenceError(f"World field must be an integer: {path}")
    elif schema is bool:
        if type(value) is not bool:
            raise EvidenceError(f"World field must be a boolean: {path}")
    elif schema is float:
        if type(value) is not float or not math.isfinite(value):
            raise EvidenceError(f"World field must be a finite float: {path}")
    elif isinstance(schema, range):
        if type(value) is not int or value not in schema:
            raise EvidenceError(f"World integer is outside its native range: {path}")
    elif isinstance(schema, dict):
        if not isinstance(value, dict):
            raise EvidenceError(f"World field must be an object: {path}")
        if "*" in schema:
            for key, item in value.items():
                validate_world_value(item, schema["*"], f"{path}.{key}")
        else:
            for key, child in schema.items():
                if key not in value:
                    raise EvidenceError(f"Missing world field: {path}.{key}")
                # Trace pools contain millions of integer leaves; retain exact
                # typing without a recursive call or diagnostic path per value
                if child is int:
                    if type(value[key]) is not int:
                        raise EvidenceError(f"World field must be an integer: {path}.{key}")
                elif isinstance(child, range):
                    if type(value[key]) is not int or value[key] not in child:
                        raise EvidenceError(f"World integer is outside its native range: {path}.{key}")
                else:
                    validate_world_value(value[key], child, f"{path}.{key}")
    else:
        fixed = isinstance(schema, tuple)
        if not isinstance(value, list) or (fixed and len(value) != len(schema)):
            raise EvidenceError(f"World field has an invalid array shape: {path}")
        for index, item in enumerate(value):
            child = schema[index] if fixed else schema[0]
            if child is int:
                if type(item) is not int:
                    raise EvidenceError(f"World field must be an integer: {path}[{index}]")
            elif isinstance(child, range):
                if type(item) is not int or item not in child:
                    raise EvidenceError(f"World integer is outside its native range: {path}[{index}]")
            else:
                validate_world_value(item, child, f"{path}[{index}]")


def validate_world_state(state, engine=None, xl=False, objects=None):
    validate_world_value(state, WORLD_SCHEMA, "$.state")
    if xl:
        validate_world_value(
            state,
            {
                "xl_time": XL_TIME_SCHEMA,
                "xl_cameras": XL_CAMERA_SCHEMA,
                "xl_triggers": [(int,) * 10],
                "xl_player_cfg": XL_PLAYER_CFG_SCHEMA,
                "xl_births": {"version": int, "next": int, "objects": [(int,) * 4]},
                "xl_native": XL_NATIVE_SCHEMA,
            },
            "$.state",
        )
        cfg = state["xl_player_cfg"]
        births = state["xl_births"]
        capacity = state["ai"]["local_capacity"]
        native = state["xl_native"]
        metadata = native["metadata"]
        events = native["events"]
        motion = native["motion"]
        for rows in (motion["boosts"]["states"], motion["projectiles"]):
            slots = [row["slot"] for row in rows]
            if len(rows) > capacity or slots != sorted(set(slots)) or any(not 0 <= slot < capacity for slot in slots):
                raise EvidenceError("Invalid XL motion owner slots or capacity")
        progress = motion["progression"]
        # d2/main/mission.h: 127 mission levels, plus the unused level-zero sentinel
        if len(progress["states"]) > 128 or (progress["states"] and progress["states"][0] != 0):
            raise EvidenceError("Invalid XL progression level table")
        if progress["bound"] and (len(progress["states"]) < 2 or not progress["mission"]):
            raise EvidenceError("Missing bound XL progression source or levels")
        if not events["bosses"]["restore_pending"] and events["bosses"]["checkpoint"]:
            raise EvidenceError("Inactive XL boss restore checkpoint has live storage")
        if len(events["producers"]["authored"]) > 20 or events["producers"]["ready_mask"] >> len(
            events["producers"]["authored"]
        ):
            raise EvidenceError("Invalid XL producer readiness or source count")
        for owner in ("canonical_state", "live_route_state", "live_candidate_state"):
            value = metadata[owner]
            for key in ("guidebot_placement_note", "guidebot_note", "route_problem", "route_note"):
                if 0 not in value[key]:
                    raise EvidenceError("Unterminated Guide-Bot metadata text")
            for step in value["route_steps"]:
                if 0 not in step["label"] or 0 not in step["trigger_type_name"]:
                    raise EvidenceError("Unterminated Guide-Bot metadata step text")
        for owner in ("route_certifier_workspace", "route_frontier_workspace"):
            value = metadata[owner]
            for score in (value["firing_search_best_score"], *value["firing_search_detailed_scores"]):
                bits = score["bits"]
                if (
                    len(bits) > 113
                    or (bits and (bits[0] != 1 or bits[-1] != 1))
                    or (not bits and score["exponent"] != 0)
                ):
                    raise EvidenceError("Noncanonical Guide-Bot metadata binary score")
        route = native["route"]
        if route["version"] != 1:
            raise EvidenceError("Unsupported XL route observation")
        if route["enabled"]:
            validate_world_value(route, XL_ROUTE_SCHEMA, "$.state.xl_native.route")
            if (
                route["trigger_capacity"] != 254
                or route["restore_pending"] not in (0, 1)
                or not 0 <= route["cache_poll_wait"] < 2 * 65536
                or any(not -2147483648 <= route[key] <= 2147483647 for key in ROUTE_SIGNED_FIELDS)
                or any(not 0 <= route[key] <= 4294967295 for key in ROUTE_UNSIGNED_FIELDS)
                or any(not -9223372036854775808 <= route[key] <= 9223372036854775807 for key in ROUTE_CLOCK_FIELDS)
                or not 0 <= route["Escort_route_target_mode"] <= 2
                or not 0 <= route["Escort_route_path_recalc_limiter_count"] <= 4
                or len(route["Escort_route_activated_triggers"]) != route["trigger_capacity"]
                or any(
                    not 0 <= value <= 255
                    for value in (*route["Escort_route_goal_label"], *route["Escort_route_activated_triggers"])
                )
                or 0 not in route["Escort_route_goal_label"]
                or any(
                    not -9223372036854775808 <= value <= 9223372036854775807
                    for value in route["Escort_route_path_recalc_limiter_timestamps"]
                )
            ):
                raise EvidenceError("Invalid XL route field, timer or array storage")
        escort = native["escort"]
        if (
            escort["version"] != 1
            or escort["persistent_message"] not in (0, 1)
            or any(not -2147483648 <= escort[key] <= 2147483647 for key in ESCORT_SIGNED_FIELDS)
            or any(not 0 <= escort[key] <= 4294967295 for key in ESCORT_UNSIGNED_FIELDS)
            or any(not -9223372036854775808 <= escort[key] <= 9223372036854775807 for key in ESCORT_CLOCK_FIELDS)
            or not -1 <= escort["Buddy_objnum"] < capacity
            or not 0 <= escort["Stolen_item_index"] < 10
            or any(not 0 <= value <= 255 for value in (*escort["stolen"], *escort["Escort_goal_message_text"]))
            or 0 not in escort["Escort_goal_message_text"]
        ):
            raise EvidenceError("Invalid XL escort/thief observation")
        if (
            native["version"] != 1
            or not 0 <= native["omega_charge"] <= 65536
            or not 0 <= native["wiggle_phase"] <= 65535
            or len(native["afterburner_charge"]) != len(state["players"])
            or any(not 0 <= charge <= 65536 for charge in native["afterburner_charge"])
            or len(native["afterburner_times"]) != capacity
            or any(not -9223372036854775808 <= time <= 9223372036854775807 for time in native["afterburner_times"])
            or any(
                not -1 <= slot < capacity or not -1 <= signature <= 2147483647 or (slot == -1 and signature != -1)
                for slot, signature in native["guided"]
            )
        ):
            raise EvidenceError("Invalid XL native weapon, control or object-clock observation")
        seismic = native["seismic"]
        epoch = state["globals"]["GameTime64"]
        maximum_epoch = 72057594037927935  # save_epoch_format.h: INT64_MAX / 128
        interval = seismic["interval"]
        if (
            not -2147483648 <= seismic["strength"] <= 2147483647
            or not -2147483648 <= seismic["pending"] <= 2147418112
            or not -2147483648 <= seismic["magnitude"] <= 2147483647
            or not 0 <= seismic["phase"] <= 24
            or not 0 <= seismic["fraction"] <= 65535
            or len(interval) not in (0, 2)
            or (
                interval
                and (
                    not -maximum_epoch <= interval[0] <= 0
                    or not interval[0] < interval[1] <= 2147483647
                    or interval[1] - interval[0] > 2147483647
                    or any(not -maximum_epoch <= epoch + time <= maximum_epoch for time in interval)
                )
            )
            or any(
                not 0.0 <= scale <= 1.0
                or not -2147483647 <= age <= 0
                or (not active and age != 0)
                or not -maximum_epoch <= epoch + age <= maximum_epoch
                for active, age, scale in seismic["detonations"]
            )
        ):
            raise EvidenceError("Invalid XL native seismic observation")
        if (
            set(births) != {"version", "next", "objects"}
            or births["version"] != 1
            or not 0 <= births["next"] < 18446744073709551615
            or len(births["objects"]) > capacity
        ):
            raise EvidenceError("Invalid XL birth observation header")
        previous = -1
        orders = set()
        for slot, signature, born, order in births["objects"]:
            if (
                not previous < slot < capacity
                or not 0 <= signature <= 2147483647
                or not 0 <= born <= state["globals"]["GameTime64"]
                or not 0 <= order <= births["next"]
                or (not order and born)
                or (order and order in orders)
            ):
                raise EvidenceError("Invalid XL birth identity, time or order")
            previous = slot
            if order:
                orders.add(order)
        if objects is not None and [(slot, signature) for slot, signature, _, _ in births["objects"]] != [
            (int(slot), value["signature"]) for slot, value in sorted(objects.items(), key=lambda item: int(item[0]))
        ]:
            raise EvidenceError("XL births do not match live object identities")
        for key in XL_PLAYER_CFG_SCHEMA:
            if key.endswith("_order"):
                if sorted(cfg[key]) != list(range(10)) + [255]:
                    raise EvidenceError("Invalid XL player weapon order")
            elif cfg[key] not in (0, 1):
                raise EvidenceError("Invalid XL player configuration boolean")
        time = state["xl_time"]
        clock, pickup_clock, rendered, cached_clock = time["anchor"]
        if (
            time["version"] != 1
            or not time["bound"]
            or not 26 <= clock <= 9223372034707292160
            or not 0 <= pickup_clock <= 2147483647
            or not 0 <= rendered <= 4294967295
            or not 0 <= cached_clock <= 2147483647
            or not -9223372036854775808 <= time["weapon_clock"][0] <= 9223372036854775807
            or not 0 <= time["weapon_clock"][1] <= 4294967295
        ):
            raise EvidenceError("Invalid or unbound XL clock observation")
        for speed, direction, age, active in time["slow_motion"]["clocks"]:
            if not 1 <= speed <= 3 or direction not in (-1, 0, 1) or not 0 <= age <= 26:
                raise EvidenceError("Invalid XL transition clock")
        previous = -1
        capacity = state["ai"]["local_capacity"]
        for slot, identity, age in time["slow_motion"]["pickups"]:
            if not previous < slot < capacity or identity not in (48, 49) or not 0 <= age <= 2147483647:
                raise EvidenceError("Invalid XL pickup clock observation")
            previous = slot
        cameras = state["xl_cameras"]
        # camera_bindings.h keeps the pinned source's one hundred monitors
        if cameras["version"] != 1 or not 0 <= cameras["ticks"] <= 2147483647 or len(cameras["states"]) > 100:
            raise EvidenceError("Invalid XL camera observation")
        for monitor, camera in enumerate(cameras["states"]):
            if (
                camera["monitor"] != monitor
                or not -1 <= camera["owner"] < capacity
                or camera["signature"] < -1
                or not -32768 <= camera["angle"] <= 32767
                or not -32768 <= camera["delta"] <= 32767
                or not -1 <= camera["t0"] <= 2147483647
                or not -2147483648 <= camera["brakes"] <= 2147483647
            ):
                raise EvidenceError("Invalid XL camera controller state")
        if len(state["xl_triggers"]) != len(state["triggers"]):
            raise EvidenceError("Missing XL trigger observations")
        for elapsed, delay, actor, shot, operated, destination, attached, signature, active, age in state[
            "xl_triggers"
        ]:
            if (
                not 0 <= elapsed <= 72057594037927935
                or not -1 <= delay <= 900000
                or not -1 <= actor < capacity
                or shot not in (0, 1)
                or operated not in (0, 1)
                or destination < 0
                or attached not in (0, 1)
                or signature < 0
                or active not in (0, 1)
                or not 0 <= age <= 750
                or (not active and age)
                or (not operated and (elapsed or delay != -1 or actor != -1 or shot or attached or signature))
            ):
                raise EvidenceError("Invalid XL trigger observation")
    weapons = state["weapons"]
    if (
        weapons["PrimaryWeaponPickedUp"] not in (0, 1)
        or weapons["SecondaryWeaponPickedUp"] not in (0, 1)
        or weapons["delayed_primary_autoselect_weapon_index"] not in (-1, 0, 1, 2, 3, 4, 16)
        or weapons["delayed_secondary_autoselect_weapon_index"] not in (-1, 0, 1, 2, 3, 4)
    ):
        raise EvidenceError("Invalid native weapon autoselection state")
    death = state["death"]
    if (
        any(death[key] not in (0, 1) for key in ("active", "exploded", "eggs_dropped", "aborted"))
        or not 0 <= death["elapsed"] <= 2147483647
        or not 0 <= death["saved_flags"] <= 255
        or not 0 <= death["saved_control"] <= 255
        or (not death["active"] and any(death[key] for key in ("aborted", "elapsed", "saved_flags", "saved_control")))
    ):
        raise EvidenceError("Invalid player death gameplay state")
    exploding = state["exploding_walls"]
    if exploding["capacity"] != 10:
        raise EvidenceError("Invalid exploding-wall capacity")
    for slot, (segment, side, time) in exploding["slots"].items():
        if (
            not slot.isdecimal()
            or str(int(slot)) != slot
            or not 0 <= int(slot) < 10
            or not 0 <= segment < len(state["segments"])
            or not 0 <= side < 6
            or not 0 <= time <= 65536
        ):
            raise EvidenceError("Invalid active exploding-wall slot or lifetime")
    endlevel = state["endlevel"]
    if len(endlevel["explosion"]) != int(bool(endlevel["explosion_playing"])):
        raise EvidenceError("Missing or inactive external endlevel explosion")
    for explosion in endlevel["explosion"]:
        validate_object(explosion, engine, "$.state.endlevel.explosion[0]", xl, state["ai"]["local_capacity"])
    ai = state["ai"]
    # Fixed source pools are emitted in full, even beyond their live ranges
    if len(ai["paths"]) != 2500 or len(ai["cloak"]) != 8 or len(ai["awareness"]) != 64:
        raise EvidenceError("Incomplete AI path, cloak or awareness storage")
    if not 0 <= ai["path_free_index"] <= len(ai["paths"]) or not 0 <= ai["Num_awareness_events"] <= len(
        ai["awareness"]
    ):
        raise EvidenceError("Invalid AI path allocator or awareness count")
    if not (1000 <= ai["local_capacity"] <= 32768 if xl and engine == "d2" else ai["local_capacity"] == 1000):
        raise EvidenceError("Invalid AI local capacity")
    # Both engines expose these source pools with fixed capacities
    boss = ai["boss"]
    for count_key, pool_key in (
        ("Num_boss_teleport_segs", "teleport_segments"),
        ("Num_boss_gate_segs", "gate_segments"),
    ):
        # Native shareware has no gating pool
        if pool_key == "gate_segments" and not ({count_key, pool_key} & boss.keys()):
            continue
        validate_world_value(boss, integer_fields(count_key, **{pool_key: [int]}), "$.state.ai.boss")
        if len(boss[pool_key]) != 100 or not 0 <= boss[count_key] <= len(boss[pool_key]):
            raise EvidenceError("Incomplete boss segment storage or invalid active count")
    d2_storage = bool({"next_action_time", "next_fire2"} & ai["local_default"].keys())
    if engine is not None and (engine not in ("d1", "d2") or d2_storage != (engine == "d2")):
        raise EvidenceError("World AI storage does not match the declared executable")
    for player in state["players"]:
        if any(len(player[key]) != (10 if d2_storage else 5) for key in ("primary_ammo", "secondary_ammo")):
            raise EvidenceError("Incomplete player ammunition capacity")
    for center in state["robotcenters"]:
        if len(center["robot_flags"]) != (2 if d2_storage else 1):
            raise EvidenceError("Incomplete robot-center flag capacity")
    for wall in state["walls"]:
        if d2_storage:
            validate_world_value(wall, integer_fields("cloak_value controlling_trigger"), "$.state.wall")
        elif {"cloak_value", "controlling_trigger"} & wall.keys():
            raise EvidenceError("D2-only wall fields in native storage")
    if len(state["triggers"]) > (254 if xl and engine == "d2" else 100):
        raise EvidenceError("Trigger count exceeds its source capacity")
    for trigger in state["triggers"]:
        if len(trigger["segments"]) != 10 or len(trigger["sides"]) != 10 or not 0 <= trigger["num_links"] <= 10:
            raise EvidenceError("Incomplete trigger link storage")
        if d2_storage:
            validate_world_value(
                trigger, integer_fields("pad", d1_saved=integer_fields("type link_num")), "$.state.trigger"
            )
            if "link_num" in trigger or not -128 <= trigger["pad"] <= 127 or not 0 <= trigger["flags"] <= 255:
                raise EvidenceError("Invalid or mixed D2 trigger encoding")
            original = trigger["d1_saved"]
        else:
            validate_world_value(trigger, integer_fields("link_num"), "$.state.trigger")
            if {"pad", "d1_saved"} & trigger.keys() or not 0 <= trigger["flags"] <= 1023:
                raise EvidenceError("Invalid or mixed native trigger encoding")
            original = trigger
        if not all(-128 <= original[key] <= 127 for key in ("type", "link_num")):
            raise EvidenceError("Original trigger bytes are outside their signed storage")
    native_fields = "last_see_time last_attack_time wait_time"
    engine_fields = "next_action_time next_fire2"
    local_fields = engine_fields if d2_storage else native_fields
    incompatible_fields = native_fields if d2_storage else engine_fields
    validate_world_value(
        boss,
        integer_fields(
            "Boss_hit_time d1_hit_pending d1_been_hit" if d2_storage else "Boss_hit_this_frame Boss_been_hit"
        ),
        "$.state.ai.boss",
    )
    incompatible_boss = (
        ("Boss_hit_this_frame", "Boss_been_hit") if d2_storage else ("Boss_hit_time", "d1_hit_pending", "d1_been_hit")
    )
    if set(incompatible_boss) & boss.keys():
        raise EvidenceError("Mixed native and D2 boss storage")
    if d2_storage:
        validate_world_value(ai, integer_fields("Believed_player_seg Ai_last_missile_camera"), "$.state.ai")
        if not -1 <= ai["Believed_player_seg"] < len(state["segments"]):
            raise EvidenceError("D2 believed-player segment is outside the mine")
        if not -1 <= ai["Ai_last_missile_camera"] < ai["local_capacity"]:
            raise EvidenceError("D2 missile-camera slot is outside object storage")
        validate_world_value(
            ai["path_runtime"], integer_fields("last_buddy_polish_path_tick"), "$.state.ai.path_runtime"
        )
        for index, cloak in enumerate(ai["cloak"]):
            validate_world_value(cloak, integer_fields("last_segment"), f"$.state.ai.cloak[{index}]")
            if not -1 <= cloak["last_segment"] < len(state["segments"]):
                raise EvidenceError("D2 cloak segment is outside the mine")
    elif (
        {"Believed_player_seg", "Ai_last_missile_camera"} & ai.keys()
        or "last_buddy_polish_path_tick" in ai["path_runtime"]
        or any("last_segment" in cloak for cloak in ai["cloak"])
    ):
        raise EvidenceError("D2-only AI caches in native storage")
    # Schema 3's canonical default is the complete value of AI slot zero
    # Nonzero clocks and every other field apply to all omitted slots too
    if "0" in ai["locals"]:
        raise EvidenceError("AI slot zero must be represented by its declared default")
    for slot in ai["locals"]:
        if not slot.isdecimal() or str(int(slot)) != slot or not 0 <= int(slot) < ai["local_capacity"]:
            raise EvidenceError("Invalid AI local slot identity")
    for local in (ai["local_default"], *ai["locals"].values()):
        validate_world_value(local, integer_fields(local_fields), "$.state.ai.local")
        if d2_storage:
            validate_world_value(local, {"d1_saved": integer_fields(native_fields)}, "$.state.ai.local")
        elif "d1_saved" in local:
            raise EvidenceError("D2 saved storage in native world AI local")
        if set(incompatible_fields.split()) & local.keys():
            raise EvidenceError("Mixed native and D2 AI local storage")
        for key in ("goal_angles", "delta_angles", "goal_state", "achieved_state"):
            if len(local[key]) != 10:
                raise EvidenceError("Incomplete AI submodel state")


def world_states(path, header, engine="d1"):
    """Require both raw boundaries and every lossless world delta in order."""
    state = {}
    count = 0
    boundaries = set()
    meta_seen = False
    terminal = False
    xl = xl_storage_profile(header, engine)
    object_capacity = None
    boundary_objects = None
    for row in records(path):
        kind = row.get("type")
        if kind == "meta":
            if meta_seen:
                raise EvidenceError("Duplicate world metadata")
            validate_identity(row, header, header["game"])
            if row.get("level") != header["level"] or row.get("start_mode") != header["start_mode"]:
                raise EvidenceError("World trace does not describe the recorded start")
            meta_seen = True
        if kind not in ("world_state", "world_boundary", "object_boundary"):
            continue
        if (
            not meta_seen
            or type(row.get("version")) is not int
            or row["version"] != (2 if kind == "object_boundary" else 18 if xl else 10)
        ):
            raise EvidenceError("Missing world metadata or unsupported schema")
        if kind.endswith("boundary"):
            phase = row.get("phase")
            key = (kind, phase)
            if phase not in ("restored", "terminal") or key in boundaries or row.get("reset") is not True:
                raise EvidenceError("Missing, aborted or duplicate boundary evidence")
            expected_frame = 0 if phase == "restored" else header["frame_count"]
            # A level-exit callback can finish after the final observed frame but
            # before advancing the replay cursor. Preserve that raw cursor as a
            # compared field; never admit an unobserved or skipped final frame
            valid_cursors = (0,) if phase == "restored" else (expected_frame - 1, expected_frame)
            if type(row.get("f")) is not int or row["f"] not in valid_cursors or count != expected_frame:
                raise EvidenceError("Boundary is not at the complete restore/terminal frame")
            if phase == "terminal":
                terminal = True
            boundaries.add(key)
            if kind == "world_boundary":
                if not isinstance(row.get("state"), dict) or not all(group in row["state"] for group in WORLD_GROUPS):
                    raise EvidenceError("Incomplete world boundary")
                validate_world_state(row["state"], engine, xl, boundary_objects)
                if row["state"]["ai"]["local_capacity"] != object_capacity:
                    raise EvidenceError("AI and object capacities disagree")
            else:
                validate_object_storage(row, engine, xl)
                if object_capacity is not None and object_capacity != row["capacity"]:
                    raise EvidenceError("Object boundary capacity changed")
                object_capacity = row["capacity"]
                boundary_objects = row["slots"]
            yield row
        else:
            if terminal or type(row.get("f")) is not int or count != row["f"] or row.get("reset") is not (count == 0):
                raise EvidenceError("World frame sequence/reset changed")
            if not all((kind, "restored") in boundaries for kind in ("object_boundary", "world_boundary")):
                raise EvidenceError("World simulation precedes its restored-state boundaries")
            if not isinstance(row.get("state"), dict):
                raise EvidenceError("Missing world delta")
            state.update(row["state"])
            if not all(group in state for group in WORLD_GROUPS):
                raise EvidenceError("Incomplete per-frame world state")
            validate_world_state(state, engine, xl)
            if state["ai"]["local_capacity"] != object_capacity:
                raise EvidenceError("AI and object capacities disagree")
            count += 1
            yield {**row, "state": state}
    required = {(kind, phase) for kind in ("object_boundary", "world_boundary") for phase in ("restored", "terminal")}
    if boundaries != required or count != header["frame_count"]:
        raise EvidenceError("Missing complete world frames or restore/terminal boundaries")


def compare_world(left, right, header, right_engine="d1", left_engine="d1"):
    validate_comparison_engines(header, left_engine, right_engine)
    first = {}
    count = 0
    for a, b in itertools.zip_longest(
        world_states(left, header, left_engine), world_states(right, header, right_engine)
    ):
        if a is None or b is None:
            raise EvidenceError("Unequal world/boundary trace lengths")
        if left_engine == "d2":
            a, b = semantic_d2_world(a), semantic_d2_world(b)
        elif right_engine == "d2":
            b = canonical_d1_world(b)
            if a["type"] == "object_boundary":
                a = {**a, "slots": {slot: canonical_native_d1_object(value) for slot, value in a["slots"].items()}}
        # Exit-animation cosmetics are diagnostics, not saved gameplay state
        # Keep sequence ownership and unknown fields; retain the full raw trace
        compared = []
        for row in (a, b):
            if left_engine == "d2":
                # XL presentation independence is still unqualified: compare
                # all RNG streams, endlevel state and unknown extension fields
                compared.append(row)
                continue
            if row["type"] == "object_boundary":
                row = {**row, "rng": simulation_rng_storage(row["rng"])}
            else:
                state = dict(row["state"])
                state["endlevel"] = {
                    key: value
                    for key, value in state["endlevel"].items()
                    if key == "sequence" or key not in ENDLEVEL_SCHEMA
                }
                row = {**row, "state": state}
            compared.append(row)
        for found in field_differences(*compared):
            # A restored-state difference must not conceal a terminal-only one
            key = f"{a['type']}:{a.get('phase', 'frame')}:{found['path']}"
            first.setdefault(key, {"frame": a["f"], **found})
        count += 1
    return {"status": "fail" if first else "pass", "records_compared": count, "first_field_differences": first}


def compare_pair(demo, left, right, engine, recorded=False):
    header = demo["header"]

    def terminal():
        expected = demo["result"] if recorded else json.loads(Path(left["result"]).read_text())
        actual = json.loads(Path(right["result"]).read_text())
        first = difference(canonical_result(expected, header, "d1"), canonical_result(actual, header, engine))
        return {"status": "fail" if first else "pass", "first_difference": first}

    checks = {
        "terminal_result": safe_check(terminal),
        "checkpoint_collision_clock": safe_check(
            lambda: compare_checkpoint_collision_clock(demo, right["state"], engine)
        ),
        "frames": safe_check(lambda: compare_frames(left["state"], right["state"], header, engine, recorded)),
        "simulation_rng": safe_check(lambda: compare_rng(left["rng"], right["rng"])),
    }
    if not recorded:
        checks["object_states"] = safe_check(lambda: compare_objects(left["state"], right["state"], header, engine))
        checks["world_states"] = safe_check(lambda: compare_world(left["state"], right["state"], header, engine))
    statuses = [item["status"] for item in checks.values()]
    return {
        "status": "fail" if "fail" in statuses else "incomplete" if "incomplete" in statuses else "pass",
        "checks": checks,
        "diagnostics": {
            "effects_rng": safe_check(lambda: compare_rng(left["rng"], right["rng"], stream=1)),
            "presentation_rng": safe_check(lambda: compare_rng(left["rng"], right["rng"], stream=2)),
        },
    }


def compare_checkpoint_collision_clock(demo, trace, engine):
    """Check the recording's restore contract even if both engines omit it."""
    if demo["header"]["start_mode"] != "save_checkpoint":
        return {"status": "pass", "applicable": False}
    checkpoint = demo.get("checkpoint")
    if not isinstance(checkpoint, dict):
        raise EvidenceError("Missing checkpoint metadata for collision clock validation")
    # New saves also carry this clock; without independent metadata the opaque
    # engine-owned save payload cannot supply an expected value to this checker
    if "collision_delay_last_play_time" not in checkpoint:
        return {
            "status": "incomplete",
            "applicable": True,
            "reason": "No independent recorded collision clock metadata",
        }
    expected = checkpoint["collision_delay_last_play_time"]
    if type(expected) is not int:
        raise EvidenceError("Invalid recorded checkpoint collision clock")
    for row in world_states(trace, demo["header"], engine):
        if row["type"] == "world_boundary" and row["phase"] == "restored":
            actual = row["state"]["globals"]["Collision_delay_last_play_time"]
            return {
                "status": "pass" if actual == expected else "fail",
                "applicable": True,
                "expected": expected,
                "actual": actual,
            }
    raise EvidenceError("Missing restored world boundary for collision clock validation")


def compress_trace(path):
    if path.suffix == ".gz":
        return path
    target = path.with_suffix(path.suffix + ".gz")
    # A failed compression must leave the source intact, never a valid-looking
    # truncated .gz beside it
    partial = target.with_name(target.name + ".partial")
    with path.open("rb") as source, gzip.open(partial, "wb", compresslevel=1) as output:
        shutil.copyfileobj(source, output)
    with gzip.open(partial, "rb") as check:
        actual = hashlib.sha256()
        for block in iter(lambda: check.read(1024 * 1024), b""):
            actual.update(block)
    if actual.hexdigest() != digest(path):
        raise EvidenceError(f"Trace compression verification failed: {path}")
    partial.replace(target)
    path.unlink()
    return target


def require_disk_space(path, minimum_gb):
    directory = Path(path).resolve()
    while not directory.exists():
        directory = directory.parent
    free = shutil.disk_usage(directory).free
    if free < minimum_gb * 1024**3:
        raise EvidenceError(
            f"Insufficient output disk space at {directory}: "
            f"{free / 1024**3:.2f} GiB free, {minimum_gb:g} GiB reserve required"
        )


def executable_info(path):
    path = Path(path)
    with path.open("rb") as source:
        magic = source.read(64)
        if magic[:2] == b"MZ":
            source.seek(struct.unpack_from("<I", magic, 60)[0])
            pe = source.read(6)
            if pe[:4] != b"PE\0\0":
                raise EvidenceError("Invalid executable header")
            architecture = {0x14C: "x86", 0x8664: "x86_64", 0xAA64: "arm64"}.get(
                struct.unpack_from("<H", pe, 4)[0], "unknown"
            )
        elif magic[:4] == b"\x7fELF":
            architecture = "ELF64" if magic[4] == 2 else "ELF32"
        else:
            architecture = "unclassified"
    return {"path": str(path.resolve()), "sha256": digest(path), "architecture": architecture}


def snapshot_executable(path, directory):
    """Keep every capture on the same executable and colocated runtime libraries."""
    source = Path(path).resolve()
    directory.mkdir(parents=True)
    files = [source] + sorted(
        p for p in source.parent.iterdir() if p.is_file() and p.suffix.lower() == ".dll" and p != source
    )
    original = {p: digest(p) for p in files}
    dependencies = []
    for item in files:
        target = directory / item.name
        shutil.copy2(item, target)
        if digest(target) != original[item]:
            raise EvidenceError(f"Executable package changed while staging: {item}")
        dependencies.append({"source": str(item), "path": str(target.resolve()), "sha256": original[item]})
    if any(digest(item) != expected for item, expected in original.items()):
        raise EvidenceError("Executable package changed while staging; finish the build before capture")
    info = executable_info(directory / source.name)
    info.update(source=str(source), package=dependencies)
    return info


def snapshot_untracked_sources(repo, directory, minimum_free_gb):
    """Keep new source files that a tracked working-tree patch cannot contain."""
    repo = Path(repo).resolve()
    directory = Path(directory)

    def inventory():
        return sorted(
            name
            for name in subprocess.check_output(["git", "ls-files", "--others", "--exclude-standard", "-z"], cwd=repo)
            .decode("utf-8")
            .split("\0")
            if name
        )

    names = inventory()
    manifest = []
    for name in names:
        source = repo / name
        if source.is_symlink() or not source.is_file() or not source.resolve().is_relative_to(repo):
            raise EvidenceError(f"Untracked source is not a regular repository file: {name}")
        require_disk_space(directory, minimum_free_gb)
        target = directory / name
        target.parent.mkdir(parents=True, exist_ok=True)
        before = digest(source)
        shutil.copy2(source, target)
        if digest(target) != before or digest(source) != before:
            raise EvidenceError(f"Untracked source changed while staging: {name}")
        manifest.append(
            {"source": name, "path": str(target.resolve()), "sha256": before, "bytes": target.stat().st_size}
        )
    if names != inventory() or any(digest(repo / item["source"]) != item["sha256"] for item in manifest):
        raise EvidenceError("Untracked sources changed while staging; finish edits before capture")
    return manifest


def snapshot_harness(repo, directory):
    repo, directory = Path(repo).resolve(), Path(directory).resolve()
    hashes = {name: digest(repo / name) for name in HARNESS_FILES}
    package = []
    for name, expected in hashes.items():
        target = directory / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(repo / name, target)
        if digest(target) != expected:
            raise EvidenceError(f"Replay harness changed while staging: {name}")
        package.append({"source": name, "path": str(target), "sha256": expected})
    if any(digest(repo / name) != expected for name, expected in hashes.items()):
        raise EvidenceError("Replay harness changed while staging; finish edits before capture")
    return {"root": str(directory), "package": package}


def check_harness(harness):
    if len(harness["package"]) != len(HARNESS_FILES) or {item["source"] for item in harness["package"]} != set(
        HARNESS_FILES
    ):
        raise EvidenceError("Staged replay harness dependency inventory is incomplete")
    for item in harness["package"]:
        if Path(item["path"]) != Path(harness["root"]) / item["source"]:
            raise EvidenceError(f"Replay harness dependency is outside its staged location: {item['source']}")
        if not Path(item["path"]).is_file() or digest(item["path"]) != item["sha256"]:
            raise EvidenceError(f"Staged replay harness changed: {item['source']}")


def capture(args, demo, directory, name, imported, assets, executable, harness):
    require_disk_space(args.output, args.minimum_free_gb)
    require_disk_space(args.repo, args.minimum_free_gb)
    check_harness(harness)
    run = directory / name
    run.mkdir()
    paths = {
        key: run / filename
        for key, filename in (("result", "result.json"), ("state", "state.jsonl.gz"), ("rng", "rng.jsonl"))
    }
    command = [
        args.pwsh,
        "-NoProfile",
        "-File",
        str(Path(harness["root"]) / "android/tests/run_input_demo_replay.ps1"),
        "-RepositoryRoot",
        str(args.repo),
        "-DemoPath",
        demo["path"],
        "-DataDir",
        str(assets),
        "-Runner",
        "fast",
        "-Mode",
        "accelerated",
        "-RenderProfile",
        "default",
        "-ReplayRobotLabels",
        "hide",
        "-SkipExpectedChecks",
        "-SandboxSuffix",
        args.output.name + "-" + name,
        "-TimeoutSeconds",
        str(args.timeout),
        "-ResultCopyPath",
        str(paths["result"]),
        "-StateLogPath",
        str(paths["state"]),
        "-RngLogPath",
        str(paths["rng"]),
        "-MinimumFreeSpaceGB",
        str(args.minimum_free_gb),
    ]
    if getattr(args, "allow_unverified_assets", False):
        command += ["-AllowUnverifiedAssets"]
    command += ["-ExecutablePath", executable["path"]]
    command += ["-D1InD2"] if imported else ["-Game", "d1"]
    info = {"command": command, "executable": executable, "artifacts": {key: str(path) for key, path in paths.items()}}
    write_json(run / "launch.json", info)
    print(f"{directory.name}: {name}", flush=True)
    with (run / "runner.log").open("w", encoding="utf-8") as log:
        completed = subprocess.run(command, cwd=args.repo, stdout=log, stderr=subprocess.STDOUT, check=False)
    info["exit_code"] = completed.returncode
    if any(digest(item["path"]) != item["sha256"] for item in executable["package"]):
        info["error"] = "Staged executable package changed during capture"
    try:
        check_harness(harness)
    except EvidenceError as error:
        info["harness_error"] = str(error)
    for key in ("state", "rng"):
        if paths[key].is_file():
            paths[key] = compress_trace(paths[key])
    info["artifacts"] = {key: str(path) for key, path in paths.items()}
    write_json(run / "launch.json", info)
    if completed.returncode or "error" in info or "harness_error" in info:
        raise EvidenceError(f"{name} capture failed; see {run / 'runner.log'}")
    for path in paths.values():
        if not path.is_file():
            raise EvidenceError(f"Missing required capture: {path}")
    return paths


def run(args):
    require_disk_space(args.output, args.minimum_free_gb)
    args.output.mkdir(parents=True, exist_ok=False)
    # Held for staging, captures and comparison. Retention skips open leases,
    # including another run still comparing its completed engine captures
    with (args.output / "producer.lock").open("w", encoding="ascii") as lease:
        if sys.platform != "win32":
            import fcntl

            fcntl.flock(lease.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        lease.write("paired D1 replay capture and comparison\n")
        lease.flush()
        harness = snapshot_harness(args.repo, args.output / "harness")
        harness_path = args.output / "harness.json"
        write_json(harness_path, harness)
        # Run the comparator itself from the same frozen package as its children
        command = [
            sys.executable,
            str(Path(harness["root"]) / "android/tests/d1_replay_parity.py"),
            "--harness-manifest",
            str(harness_path),
            "--repo",
            str(args.repo),
            "--data",
            str(args.data),
            "--native",
            str(args.native),
            "--imported",
            str(args.imported),
            "--output",
            str(args.output),
            "--pwsh",
            args.pwsh,
            "--timeout",
            str(args.timeout),
            "--minimum-free-gb",
            str(args.minimum_free_gb),
        ]
        if getattr(args, "allow_unverified_assets", False):
            command.append("--allow-unverified-assets")
        for demo in args.demo:
            command.extend(("--demo", str(demo)))
        write_json(args.output / "controller-launch.json", {"command": command})
        return subprocess.run(command, cwd=args.repo, check=False).returncode


def run_with_lease(args, harness):
    assets = args.output / "d1-assets"
    assets.mkdir()
    asset_manifest = []
    for name in ("descent.hog", "descent.pig"):
        require_disk_space(args.output, args.minimum_free_gb)
        matches = [path for path in args.data.iterdir() if path.name.lower() == name and path.is_file()]
        if len(matches) != 1:
            raise EvidenceError(f"Exactly one {name} required in {args.data}")
        target = assets / name
        shutil.copyfile(matches[0], target)
        asset_manifest.append({"source": str(matches[0]), "staged": str(target), "sha256": digest(target)})
    native = snapshot_executable(args.native, args.output / "binaries" / "native")
    imported = snapshot_executable(args.imported, args.output / "binaries" / "imported")
    manifest = {
        "schema": 2,
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "host": platform.platform(),
        "assets": asset_manifest,
        "executables": {"native": native, "imported": imported},
        "harness": harness,
        "settings": {
            "runner": "fast",
            "render_profile": "default",
            "companion": False,
            "cameras": "checkpoint settings",
            "player_cfg": "unchanged recording header",
        },
        "demos": [],
    }
    manifest["git_revision"] = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=args.repo, text=True).strip()
    working_diff = subprocess.check_output(["git", "diff", "HEAD"], cwd=args.repo)
    (args.output / "source.patch").write_bytes(working_diff)
    manifest["working_diff_sha256"] = hashlib.sha256(working_diff).hexdigest()
    manifest["untracked_sources"] = snapshot_untracked_sources(
        args.repo, args.output / "untracked-sources", args.minimum_free_gb
    )
    report = {
        "schema": 1,
        "cases": [],
        "qualification": "incomplete",
        "coverage_gaps": [
            "Declared frame diagnostics are required, but the complete simulation-state and lifetime audit remains open",
            "Restore/terminal object, world and saved AI storage are observed, but transition/cache and static-runtime coverage is incomplete",
            "World/player records require explicit cross-engine identity/layout mappings and a complete saved-global audit",
            "Complete presentation state and target-platform qualification are not yet covered",
        ],
    }
    for path in args.demo:
        case = {"demo": str(path), "status": "incomplete"}
        report["cases"].append(case)
        try:
            demo = read_demo(path)
            rng_reference = Path(str(path) + ".rngtrace.jsonl")
            demo["recorded_rng"] = {
                "path": str(rng_reference),
                "sha256": digest(rng_reference) if rng_reference.is_file() else None,
            }
            manifest["demos"].append(demo)
            write_json(args.output / "manifest.json", manifest)
            if demo["header"]["mission"] not in ("", "d1", "descent"):
                raise EvidenceError("Custom mission staging is not yet qualified by this base-campaign runner")
            directory = args.output / path.stem
            directory.mkdir()
            runs = {}
            # Always attempt all three captures, even if one is incomplete
            for name in ("native-a", "native-repeat", "imported"):
                try:
                    runs[name] = capture(
                        args,
                        demo,
                        directory,
                        name,
                        name == "imported",
                        assets,
                        imported if name == "imported" else native,
                        harness,
                    )
                except (OSError, EvidenceError) as error:
                    case.setdefault("capture_errors", {})[name] = str(error)
            recorded = {"state": path, "rng": Path(str(path) + ".rngtrace.jsonl")}
            for relationship, left_name, right_name, engine in (
                ("recording_to_native", None, "native-a", "d1"),
                ("native_repeatability", "native-a", "native-repeat", "d1"),
                ("native_to_imported", "native-a", "imported", "d2"),
            ):
                if right_name not in runs or (left_name is not None and left_name not in runs):
                    case[relationship] = {"status": "incomplete", "error": "Required capture failed"}
                else:
                    print(f"{directory.name}: comparing {relationship}", flush=True)
                    case[relationship] = compare_pair(
                        demo, runs[left_name] if left_name else recorded, runs[right_name], engine, left_name is None
                    )
            case["status"] = (
                "fail"
                if any(
                    case[key]["status"] == "fail"
                    for key in ("recording_to_native", "native_repeatability", "native_to_imported")
                )
                else "incomplete"
            )
        except (OSError, EvidenceError, ValueError, KeyError) as error:
            case["error"] = str(error)
        write_json(args.output / "report.json", report)
    for asset in asset_manifest:
        if digest(asset["staged"]) != asset["sha256"]:
            report["asset_error"] = "Staged assets changed during capture"
    try:
        check_harness(harness)
    except EvidenceError as error:
        report["harness_error"] = str(error)
    write_json(args.output / "report.json", report)
    print(f"Report: {args.output / 'report.json'}", flush=True)
    print("Full fidelity qualification: INCOMPLETE (see explicit coverage gaps)", flush=True)
    return 1 if any(case["status"] == "fail" for case in report["cases"]) else 2


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--data", type=Path, required=True)
    parser.add_argument("--native", type=Path, required=True)
    parser.add_argument("--imported", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--allow-unverified-assets", action="store_true")
    parser.add_argument("--pwsh", required=True)
    parser.add_argument("--timeout", type=int, default=300)
    parser.add_argument("--minimum-free-gb", type=float, default=4)
    parser.add_argument("--demo", type=Path, action="append", required=True)
    parser.add_argument("--harness-manifest", type=Path, help=argparse.SUPPRESS)
    args = parser.parse_args()
    if not 0.01 <= args.minimum_free_gb <= 1048576:
        parser.error("--minimum-free-gb must be between 0.01 and 1048576")
    for name in ("repo", "data", "native", "imported", "output"):
        setattr(args, name, getattr(args, name).resolve())
    args.demo = [path.resolve() for path in args.demo]
    if args.harness_manifest:
        harness = json.loads(args.harness_manifest.read_text(encoding="utf-8"))
        check_harness(harness)
        if Path(__file__).resolve() != Path(harness["root"]) / "android/tests/d1_replay_parity.py":
            raise EvidenceError("Frozen controller must execute from the staged harness")
        return run_with_lease(args, harness)
    return run(args)


if __name__ == "__main__":
    sys.exit(main())
