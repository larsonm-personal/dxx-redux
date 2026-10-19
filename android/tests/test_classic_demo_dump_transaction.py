import argparse
import hashlib
import json
import os
import shutil
import struct
import subprocess
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
TEMP_ROOT = REPO_ROOT / "temp"
MAX_PLAYERS = 8
MAX_SEGMENTS = 9000
MAX_OBJECTS = 1000
MAX_WALLS = 255
MAX_PRIMARY_WEAPONS = 10
MAX_SECONDARY_WEAPONS = 10
MAX_SUPER_LASER_LEVEL = 5
MAX_D1_HOSTAGES = 10
GM_NETWORK = 4

RT_POLYOBJ = 1
RT_FIREBALL = 2
RT_LASER = 3
RT_HOSTAGE = 4
RT_MORPH = 6
OBJ_ROBOT = 2
OBJ_HOSTAGE = 3
OBJ_PLAYER = 4
OBJ_WEAPON = 5
OBJ_POWERUP = 7
OBJ_FIREBALL = 1
OBJ_CLUTTER = 11
OBJ_LIGHT = 13
CT_EXPLOSION = 2
CT_LIGHT = 14
MT_NONE = 0
OF_ATTACHED = 16


def file_hash(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run_dump(executable: Path, data_dir: Path, source: Path, output: Path) -> int:
    result = subprocess.run(
        [
            str(executable),
            "-hogdir",
            str(data_dir),
            "-nomovies",
            "-nomusic",
            "-nosound",
            "-classicdemo-dump-json",
            str(source),
            str(output),
        ],
        cwd=REPO_ROOT,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
        timeout=10,
    )
    return result.returncode


def initial_wall_count_offset(demo: bytes) -> int:
    if len(demo) < 68 or demo[:3] not in (b"\x01\x0f\x03", b"\x01\x10\x03"):
        raise AssertionError("fixture is not a supported D2 classic demo")
    game_mode = struct.unpack_from("<i", demo, 7)[0]
    if game_mode != 0:
        raise AssertionError("wall-boundary fixture must be a single-player demo")
    mission_length = demo[56]
    event_offset = 57 + mission_length + 8
    if len(demo) < event_offset + 7 or demo[event_offset] != 28:
        raise AssertionError("fixture does not begin with a new-level event")
    return event_offset + 3


def initial_mission_length_offset(demo: bytes) -> int:
    if len(demo) < 64 or demo[:3] not in (b"\x01\x0f\x03", b"\x01\x10\x03"):
        raise AssertionError("fixture is not a supported D2 classic demo")
    mission_offset = 56
    mission_length = demo[mission_offset]
    mission_end = mission_offset + 1 + mission_length
    if mission_length == 0 or mission_end > len(demo) or demo[mission_end - 1] != 0:
        raise AssertionError("fixture has an invalid initial mission string")
    return mission_offset


def trailer_state_offsets(demo: bytes) -> tuple[int, int, int, int]:
    if len(demo) < 22:
        raise AssertionError("fixture is too short for a classic demo trailer")
    byte_count = struct.unpack_from("<H", demo, len(demo) - 4)[0]
    trailer_start = len(demo) - 4 - byte_count
    primary_offset = trailer_start + 16
    secondary_offset = trailer_start + 17
    laser_level_offset = trailer_start + 58
    if trailer_start < 0 or laser_level_offset >= len(demo) - 4:
        raise AssertionError("fixture has an invalid classic demo trailer")
    return primary_offset, secondary_offset, laser_level_offset, trailer_start


def player_record(player: int) -> bytes:
    if player < 0 or player >= MAX_PLAYERS:
        raise AssertionError(f"invalid synthetic player: {player}")
    return bytes((3, ord("p"), ord("0") + player, 0, 1, 0, 0, 0, 0))


def d2_multiplayer_fixture(demo: bytes, player_count: int) -> tuple[bytes, int, int]:
    if player_count < 1 or player_count > MAX_PLAYERS:
        raise AssertionError(f"invalid synthetic player count: {player_count}")
    if len(demo) < 80 or demo[:3] not in (b"\x01\x0f\x03", b"\x01\x10\x03"):
        raise AssertionError("fixture is not a supported D2 classic demo")
    if struct.unpack_from("<i", demo, 7)[0] != 0:
        raise AssertionError("synthetic multiplayer source must begin as single-player")

    encoded_players = bytes((player_count,)) + b"".join(player_record(player) for player in range(player_count))
    converted = bytearray(demo[:11] + encoded_players + demo[15:])
    struct.pack_into("<i", converted, 7, GM_NETWORK)

    mission_offset = 11 + len(encoded_players) + 41
    mission_length = converted[mission_offset]
    event_offset = mission_offset + 1 + mission_length + 8
    if mission_length < 1 or converted[mission_offset + mission_length] != 0 or converted[event_offset] != 28:
        raise AssertionError("synthetic multiplayer header layout mismatch")

    trailer_byte_count = struct.unpack_from("<H", converted, len(converted) - 4)[0]
    trailer_start = len(converted) - 4 - trailer_byte_count
    trailer_count_offset = trailer_start + 59
    converted[trailer_count_offset : trailer_count_offset + 4] = encoded_players
    trailer_byte_count += len(encoded_players) - 4
    struct.pack_into("<H", converted, len(converted) - 4, trailer_byte_count)

    final_trailer_start = len(converted) - 4 - trailer_byte_count
    final_trailer_count_offset = final_trailer_start + 59
    if converted[final_trailer_count_offset] != player_count:
        raise AssertionError("synthetic multiplayer trailer layout mismatch")
    return bytes(converted), event_offset, final_trailer_count_offset


def insert_event(demo: bytes, event_offset: int, event: bytes) -> bytes:
    return demo[:event_offset] + event + demo[event_offset:]


def demo_shortpos(segment: int = 0, oriented: bool = False, signed_boundaries: bool = False) -> bytes:
    orientation = b""
    if oriented:
        orientation = (
            bytes((128, 0, 0, 0, 128, 0, 0, 0, 128)) if signed_boundaries else bytes((127, 0, 0, 0, 127, 0, 0, 0, 127))
        )
    signed_value = -32768 if signed_boundaries else 0
    return orientation + struct.pack(
        "<hhhhhhh",
        signed_value,
        signed_value,
        signed_value,
        segment,
        signed_value,
        signed_value,
        signed_value,
    )


def demo_object_header(
    render_type: int,
    object_type: int,
    object_id: int,
    segment: int = 0,
    signed_boundaries: bool = False,
) -> bytes:
    oriented = render_type in (RT_POLYOBJ, RT_HOSTAGE, RT_MORPH)
    return struct.pack("<BBBBh", render_type, object_type, object_id, 0, 0) + demo_shortpos(
        segment, oriented, signed_boundaries
    )


def light_laser_object(segment: int = 0) -> bytes:
    return (
        demo_object_header(RT_LASER, OBJ_LIGHT, 0, segment)
        + struct.pack("<BBi", CT_LIGHT, MT_NONE, 1 << 16)
        + struct.pack("<iiib", 0, 0, 0, -1)
        + struct.pack("<i", 1 << 16)
    )


def invalid_model_object(model_num: int) -> bytes:
    return (
        demo_object_header(RT_POLYOBJ, OBJ_LIGHT, 0)
        + struct.pack("<BBi", CT_LIGHT, MT_NONE, 1 << 16)
        + struct.pack("<iiib", 0, 0, 0, -1)
        + struct.pack("<i", 1 << 16)
        + struct.pack("<ii", model_num, 0)
    )


def player_poly_object(tmap_override: int, signed_boundaries: bool = False) -> bytes:
    return (
        demo_object_header(RT_POLYOBJ, OBJ_PLAYER, 0, signed_boundaries=signed_boundaries)
        + struct.pack("<iiib", 0, 0, 0, -1)
        + struct.pack("<iiiiii", 0, 0, 0, 0, 0, 0)
        + struct.pack("<i", tmap_override)
    )


def vclip_object(vclip_num: int, frame_num: int) -> bytes:
    return (
        demo_object_header(RT_FIREBALL, OBJ_LIGHT, 0)
        + struct.pack("<BBi", CT_LIGHT, MT_NONE, 1 << 16)
        + struct.pack("<iiib", 0, 0, 0, -1)
        + struct.pack("<i", 1 << 16)
        + struct.pack("<iiB", vclip_num, 1 << 16, frame_num)
    )


def hostage_object(hostage_id: int) -> bytes:
    return (
        demo_object_header(RT_HOSTAGE, OBJ_HOSTAGE, hostage_id)
        + struct.pack("<iiib", 0, 0, 0, -1)
        + struct.pack("<iiB", 0, 1 << 16, 0)
    )


def attached_explosion_object(delete_objnum: int) -> bytes:
    header = bytearray(demo_object_header(RT_LASER, OBJ_FIREBALL, 0))
    header[3] = OF_ATTACHED
    return (
        bytes(header)
        + struct.pack("<BBi", CT_EXPLOSION, MT_NONE, 1 << 16)
        + struct.pack("<iiib", 0, 0, 0, -1)
        + struct.pack("<iih", 0, 0, delete_objnum)
    )


def require_malformed_preserves_output(
    executable: Path,
    data_dir: Path,
    source: Path,
    output: Path,
    prior: bytes,
) -> None:
    output.write_bytes(prior)
    if run_dump(executable, data_dir, source, output) == 0:
        raise AssertionError(f"malformed demo unexpectedly converted: {source.name}")
    if output.read_bytes() != prior:
        raise AssertionError(f"malformed demo changed prior output: {source.name}")
    if list(output.parent.glob(f"{output.name}.tmp.*")):
        raise AssertionError(f"malformed demo leaked temporary output: {source.name}")


def require_conversion(
    executable: Path,
    data_dir: Path,
    source: Path,
    output: Path,
    expected: bytes | None = None,
) -> bytes:
    if run_dump(executable, data_dir, source, output) != 0:
        raise AssertionError(f"valid or recoverable demo conversion failed: {source.name}")
    converted = output.read_bytes()
    if expected is not None and converted != expected:
        raise AssertionError(f"ignored event changed decoded output: {source.name}")
    if list(output.parent.glob(f"{output.name}.tmp.*")):
        raise AssertionError(f"demo conversion leaked temporary output: {source.name}")
    return converted


def require_rejected_without_change(
    executable: Path, data_dir: Path, source: Path, output: Path, expected_hash: str
) -> None:
    if run_dump(executable, data_dir, source, output) == 0:
        raise AssertionError(f"alias unexpectedly accepted: {source} -> {output}")
    if file_hash(source.resolve()) != expected_hash:
        raise AssertionError(f"source changed after rejection: {source}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--data-dir", required=True, type=Path)
    parser.add_argument("--demo", required=True, type=Path)
    args = parser.parse_args()

    executable = args.executable.resolve()
    data_dir = args.data_dir.resolve()
    demo = args.demo.resolve()
    for path in (executable, data_dir, demo):
        if not path.exists():
            raise FileNotFoundError(path)

    TEMP_ROOT.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="classic_demo_dump_", dir=TEMP_ROOT) as directory:
        case_dir = Path(directory)
        source = case_dir / "source.dem"
        shutil.copyfile(demo, source)
        source_hash = file_hash(source)

        require_rejected_without_change(executable, data_dir, source, source, source_hash)
        relative_source = Path(os.path.relpath(source, REPO_ROOT))
        require_rejected_without_change(executable, data_dir, relative_source, source, source_hash)
        hard_link = case_dir / "hard-link.dem"
        os.link(source, hard_link)
        require_rejected_without_change(executable, data_dir, source, hard_link, source_hash)

        malformed = case_dir / "malformed.dem"
        malformed.write_bytes(b"not a classic demo")
        prior_output = case_dir / "prior.jsonl"
        prior_output.write_bytes(b"preserve me")
        prior_hash = file_hash(prior_output)
        if run_dump(executable, data_dir, malformed, prior_output) == 0:
            raise AssertionError("malformed input unexpectedly converted")
        if file_hash(prior_output) != prior_hash:
            raise AssertionError("malformed input changed the prior output")

        demo_bytes = source.read_bytes()
        base_output = require_conversion(executable, data_dir, source, case_dir / "base.jsonl")
        mission_offset = initial_mission_length_offset(demo_bytes)
        for mission_length in (0, 10, 255):
            mission_demo = case_dir / f"mission-length-{mission_length}.dem"
            mutated = bytearray(demo_bytes)
            mutated[mission_offset] = mission_length
            mission_demo.write_bytes(mutated)
            require_malformed_preserves_output(
                executable, data_dir, mission_demo, prior_output, b"preserve mission length"
            )
        unterminated_mission = case_dir / "mission-unterminated.dem"
        mutated = bytearray(demo_bytes)
        mission_length = mutated[mission_offset]
        mutated[mission_offset + mission_length] = ord("x")
        unterminated_mission.write_bytes(mutated)
        require_malformed_preserves_output(
            executable, data_dir, unterminated_mission, prior_output, b"preserve mission terminator"
        )
        missing_mission_length = case_dir / "mission-length-truncated.dem"
        missing_mission_length.write_bytes(demo_bytes[:mission_offset])
        require_malformed_preserves_output(
            executable, data_dir, missing_mission_length, prior_output, b"preserve missing mission length"
        )
        for payload_bytes in range(mission_length):
            truncated_mission = case_dir / f"mission-payload-truncated-{payload_bytes}.dem"
            truncated_mission.write_bytes(demo_bytes[: mission_offset + 1 + payload_bytes])
            require_malformed_preserves_output(
                executable, data_dir, truncated_mission, prior_output, b"preserve mission truncation"
            )

        for player_count in (0, MAX_PLAYERS + 1, 255):
            header_count_demo = case_dir / f"player-header-count-{player_count}.dem"
            mutated = bytearray(demo_bytes)
            struct.pack_into("<i", mutated, 7, GM_NETWORK)
            mutated[11] = player_count
            header_count_demo.write_bytes(mutated)
            require_malformed_preserves_output(
                executable, data_dir, header_count_demo, prior_output, b"preserve player header count"
            )
        missing_header_count = case_dir / "player-header-count-missing.dem"
        mutated = bytearray(demo_bytes[:11])
        struct.pack_into("<i", mutated, 7, GM_NETWORK)
        missing_header_count.write_bytes(mutated)
        require_malformed_preserves_output(
            executable, data_dir, missing_header_count, prior_output, b"preserve missing player header count"
        )

        multiplayer_bytes, multiplayer_event_offset, multiplayer_trailer_count_offset = d2_multiplayer_fixture(
            demo_bytes, 1
        )
        multiplayer_demo = case_dir / "player-count-1.dem"
        multiplayer_demo.write_bytes(multiplayer_bytes)
        multiplayer_output = require_conversion(
            executable, data_dir, multiplayer_demo, case_dir / "player-count-1.jsonl"
        )
        for player_count in (0, MAX_PLAYERS + 1, 255):
            trailer_count_demo = case_dir / f"player-trailer-count-{player_count}.dem"
            mutated = bytearray(multiplayer_bytes)
            mutated[multiplayer_trailer_count_offset] = player_count
            trailer_count_demo.write_bytes(mutated)
            require_malformed_preserves_output(
                executable, data_dir, trailer_count_demo, prior_output, b"preserve player trailer count"
            )

        maximum_players_bytes, maximum_players_event_offset, _ = d2_multiplayer_fixture(demo_bytes, MAX_PLAYERS)
        maximum_players_demo = case_dir / "player-count-8.dem"
        maximum_players_demo.write_bytes(maximum_players_bytes)
        maximum_players_output = require_conversion(
            executable, data_dir, maximum_players_demo, case_dir / "player-count-8.jsonl"
        )

        player_events = {
            "cloak": (bytes((29,)), b""),
            "decloak": (bytes((30,)), b""),
            "death": (bytes((32,)), b""),
            "kill": (bytes((33,)), bytes((1,))),
            "connect-new": (bytes((34,)), bytes((1, 2, ord("x"), 0))),
            "connect-existing": (
                bytes((34,)),
                bytes((0, 2, ord("o"), 0)) + bytes(8) + bytes((2, ord("n"), 0)),
            ),
            "reconnect": (bytes((35,)), b""),
            "disconnect": (bytes((36,)), b""),
            "score": (bytes((37,)), struct.pack("<i", 1)),
        }
        for event_name, (event_prefix, event_suffix) in player_events.items():
            for player_index in (MAX_PLAYERS, 255):
                event_demo = case_dir / f"player-event-{event_name}-index-{player_index}.dem"
                event_demo.write_bytes(
                    insert_event(
                        multiplayer_bytes,
                        multiplayer_event_offset,
                        event_prefix + bytes((player_index,)) + event_suffix,
                    )
                )
                require_conversion(
                    executable,
                    data_dir,
                    event_demo,
                    case_dir / f"player-event-{event_name}-index-{player_index}.jsonl",
                    multiplayer_output,
                )

            valid_event_demo = case_dir / f"player-event-{event_name}-index-7.dem"
            valid_event_demo.write_bytes(
                insert_event(
                    multiplayer_bytes,
                    multiplayer_event_offset,
                    event_prefix + bytes((MAX_PLAYERS - 1,)) + event_suffix,
                )
            )
            require_conversion(
                executable,
                data_dir,
                valid_event_demo,
                case_dir / f"player-event-{event_name}-index-7.jsonl",
            )

        counter_boundary_demo = case_dir / "counter-arithmetic-boundaries.dem"
        counter_boundary_bytes = bytearray(multiplayer_bytes)
        struct.pack_into("<hh", counter_boundary_bytes, 17, 32767, 32767)
        counter_boundary_demo.write_bytes(
            insert_event(
                bytes(counter_boundary_bytes),
                multiplayer_event_offset,
                bytes((32, 0, 33, 0, 1)) + struct.pack("<BbiBbi", 37, 0, 2_147_483_647, 37, 0, 1),
            )
        )
        require_conversion(
            executable,
            data_dir,
            counter_boundary_demo,
            case_dir / "counter-arithmetic-boundaries.jsonl",
        )

        player_score_boundary_demo = case_dir / "player-score-arithmetic-boundaries.dem"
        player_score_boundary_demo.write_bytes(
            insert_event(
                demo_bytes,
                initial_wall_count_offset(demo_bytes) - 3,
                struct.pack("<BiBiBi", 38, 2_147_483_647, 38, 2_147_483_647, 38, 1),
            )
        )
        require_conversion(
            executable,
            data_dir,
            player_score_boundary_demo,
            case_dir / "player-score-arithmetic-boundaries.jsonl",
        )

        full_connect_demo = case_dir / "player-count-8-connect-new.dem"
        full_connect_demo.write_bytes(
            insert_event(
                maximum_players_bytes,
                maximum_players_event_offset,
                bytes((34, MAX_PLAYERS - 1, 1, 2, ord("x"), 0)),
            )
        )
        require_conversion(
            executable,
            data_dir,
            full_connect_demo,
            case_dir / "player-count-8-connect-new.jsonl",
            maximum_players_output,
        )

        wall_offset = initial_wall_count_offset(demo_bytes)
        event_offset = wall_offset - 3
        header_weapon_offsets = (event_offset - 2, event_offset - 1)
        trailer_offsets = trailer_state_offsets(demo_bytes)
        for location, offsets in (("header", header_weapon_offsets), ("trailer", trailer_offsets[:2])):
            for weapon_class, offset, invalid_index in (
                ("primary", offsets[0], MAX_PRIMARY_WEAPONS),
                ("secondary", offsets[1], MAX_SECONDARY_WEAPONS),
            ):
                weapon_demo = case_dir / f"weapon-{location}-{weapon_class}-invalid.dem"
                mutated = bytearray(demo_bytes)
                mutated[offset] = invalid_index
                weapon_demo.write_bytes(mutated)
                require_malformed_preserves_output(
                    executable,
                    data_dir,
                    weapon_demo,
                    prior_output,
                    f"preserve {location} {weapon_class} weapon".encode(),
                )

            valid_weapon_demo = case_dir / f"weapon-{location}-valid-boundaries.dem"
            mutated = bytearray(demo_bytes)
            mutated[offsets[0]] = MAX_PRIMARY_WEAPONS - 1
            mutated[offsets[1]] = MAX_SECONDARY_WEAPONS - 1
            valid_weapon_demo.write_bytes(mutated)
            require_conversion(
                executable,
                data_dir,
                valid_weapon_demo,
                case_dir / f"weapon-{location}-valid-boundaries.jsonl",
            )

        weapon_events = {
            "primary-new": (bytes((20, 0, 10, 0)), bytes((20, 0, 9, 9))),
            "primary-old": (bytes((20, 0, 9, 10)), bytes((20, 0, 9, 9))),
            "secondary-new": (bytes((20, 1, 10, 0)), bytes((20, 1, 9, 9))),
            "secondary-old": (bytes((20, 1, 9, 10)), bytes((20, 255, 9, 9))),
        }
        ammo_event = struct.pack("<Bhh", 39, 0, 1) + struct.pack("<Bhh", 40, 0, 1)
        for event_name, (invalid_event, valid_event) in weapon_events.items():
            invalid_demo = case_dir / f"weapon-event-{event_name}-invalid.dem"
            invalid_demo.write_bytes(insert_event(demo_bytes, event_offset, invalid_event + ammo_event))
            require_conversion(
                executable,
                data_dir,
                invalid_demo,
                case_dir / f"weapon-event-{event_name}-invalid.jsonl",
                base_output,
            )

            valid_demo = case_dir / f"weapon-event-{event_name}-valid.dem"
            valid_demo.write_bytes(insert_event(demo_bytes, event_offset, valid_event))
            require_conversion(
                executable,
                data_dir,
                valid_demo,
                case_dir / f"weapon-event-{event_name}-valid.jsonl",
            )

        laser_offsets = {
            "header": mission_offset - 1,
            "trailer": trailer_offsets[2],
        }
        for location, offset in laser_offsets.items():
            for invalid_level in (MAX_SUPER_LASER_LEVEL + 1, 255):
                laser_demo = case_dir / f"laser-{location}-invalid-{invalid_level}.dem"
                mutated = bytearray(demo_bytes)
                mutated[offset] = invalid_level
                laser_demo.write_bytes(mutated)
                require_malformed_preserves_output(
                    executable,
                    data_dir,
                    laser_demo,
                    prior_output,
                    f"preserve {location} laser level {invalid_level}".encode(),
                )

            valid_laser_demo = case_dir / f"laser-{location}-valid-maximum.dem"
            mutated = bytearray(demo_bytes)
            mutated[offset] = MAX_SUPER_LASER_LEVEL
            valid_laser_demo.write_bytes(mutated)
            require_conversion(
                executable,
                data_dir,
                valid_laser_demo,
                case_dir / f"laser-{location}-valid-maximum.jsonl",
            )

        invalid_laser_events = {
            "negative-old": bytes((42, 255, 0)),
            "negative-new": bytes((42, 0, 255)),
            "oversized-old": bytes((42, MAX_SUPER_LASER_LEVEL + 1, 0)),
            "oversized-new": bytes((42, 0, MAX_SUPER_LASER_LEVEL + 1)),
        }
        for event_name, event in invalid_laser_events.items():
            laser_demo = case_dir / f"laser-event-{event_name}.dem"
            laser_demo.write_bytes(insert_event(demo_bytes, event_offset, event))
            require_conversion(
                executable,
                data_dir,
                laser_demo,
                case_dir / f"laser-event-{event_name}.jsonl",
                base_output,
            )

        valid_laser_event_demo = case_dir / "laser-event-valid-maximum.dem"
        valid_laser_event_demo.write_bytes(
            insert_event(
                demo_bytes,
                event_offset,
                bytes((42, MAX_SUPER_LASER_LEVEL, MAX_SUPER_LASER_LEVEL)),
            )
        )
        require_conversion(
            executable,
            data_dir,
            valid_laser_event_demo,
            case_dir / "laser-event-valid-maximum.jsonl",
        )

        for invalid_frame_length in (0, 10, -1):
            frame_link_demo = case_dir / f"trailer-frame-link-{invalid_frame_length}.dem"
            mutated = bytearray(demo_bytes)
            struct.pack_into("<h", mutated, trailer_offsets[3], invalid_frame_length)
            frame_link_demo.write_bytes(mutated)
            require_malformed_preserves_output(
                executable,
                data_dir,
                frame_link_demo,
                prior_output,
                f"preserve unsafe trailer frame link {invalid_frame_length}".encode(),
            )

        frame_metadata_cases = {
            "zero-time": (1, 0),
            "negative-time": (1, -1),
            "maximum-time": (1, 2_147_483_647),
            "minimum-frame-count": (-2_147_483_648, 1),
        }
        for name, (frame_count, frame_time) in frame_metadata_cases.items():
            frame_metadata_demo = case_dir / f"frame-metadata-{name}.dem"
            frame_metadata_demo.write_bytes(
                insert_event(
                    demo_bytes,
                    event_offset,
                    struct.pack("<Bhii", 2, 11, frame_count, frame_time),
                )
            )
            require_conversion(
                executable,
                data_dir,
                frame_metadata_demo,
                case_dir / f"frame-metadata-{name}.jsonl",
            )

        level_offsets = {
            "initial-new": event_offset + 1,
            "initial-old": event_offset + 2,
            "trailer": len(demo_bytes) - 2,
        }
        for location, offset in level_offsets.items():
            invalid_level_demo = case_dir / f"level-{location}-invalid.dem"
            mutated = bytearray(demo_bytes)
            mutated[offset] = 127
            invalid_level_demo.write_bytes(mutated)
            require_malformed_preserves_output(
                executable,
                data_dir,
                invalid_level_demo,
                prior_output,
                f"preserve invalid {location} level".encode(),
            )

            zero_level_demo = case_dir / f"level-{location}-legacy-zero.dem"
            mutated = bytearray(demo_bytes)
            mutated[offset] = 0
            zero_level_demo.write_bytes(mutated)
            require_conversion(
                executable,
                data_dir,
                zero_level_demo,
                case_dir / f"level-{location}-legacy-zero.jsonl",
            )

        sound_once_demo = case_dir / "event-sound-once.dem"
        sound_once_demo.write_bytes(insert_event(demo_bytes, event_offset, struct.pack("<Bi", 6, 0)))
        require_conversion(
            executable,
            data_dir,
            sound_once_demo,
            case_dir / "event-sound-once.jsonl",
            base_output,
        )

        standalone_secret_demo = case_dir / "event-secret-status-standalone.dem"
        standalone_secret_demo.write_bytes(insert_event(demo_bytes, event_offset, struct.pack("<Bi", 48, 1)))
        require_conversion(
            executable,
            data_dir,
            standalone_secret_demo,
            case_dir / "event-secret-status-standalone.jsonl",
            base_output,
        )

        unknown_opcode_demo = case_dir / "event-unknown-opcode-255.dem"
        unknown_opcode_demo.write_bytes(insert_event(demo_bytes, event_offset, bytes((255,))))
        require_malformed_preserves_output(
            executable,
            data_dir,
            unknown_opcode_demo,
            prior_output,
            b"preserve unknown event opcode",
        )

        valid_hostage_event_demo = case_dir / "hostage-event-valid-zero.dem"
        valid_hostage_event_demo.write_bytes(insert_event(demo_bytes, event_offset, struct.pack("<Bi", 10, 0)))
        valid_hostage_output = require_conversion(
            executable,
            data_dir,
            valid_hostage_event_demo,
            case_dir / "hostage-event-valid-zero.jsonl",
        )
        for hostage_id in (-1, MAX_D1_HOSTAGES, 2_147_483_647):
            hostage_demo = case_dir / f"hostage-event-opaque-{hostage_id}.dem"
            hostage_demo.write_bytes(insert_event(demo_bytes, event_offset, struct.pack("<Bi", 10, hostage_id)))
            require_conversion(
                executable,
                data_dir,
                hostage_demo,
                case_dir / f"hostage-event-opaque-{hostage_id}.jsonl",
                valid_hostage_output,
            )

        opaque_hostage_object_demo = case_dir / "hostage-object-opaque-255.dem"
        opaque_hostage_object_demo.write_bytes(
            insert_event(demo_bytes, event_offset, bytes((4,)) + hostage_object(255))
        )
        require_conversion(
            executable,
            data_dir,
            opaque_hostage_object_demo,
            case_dir / "hostage-object-opaque-255.jsonl",
        )

        world_events = {
            "wall-hit-segment": (
                struct.pack("<Biiii", 8, MAX_SEGMENTS, 0, 0, -1),
                struct.pack("<Biiii", 8, MAX_SEGMENTS - 1, 5, 0, -1),
            ),
            "wall-hit-side": (
                struct.pack("<Biiii", 8, 0, 6, 0, -1),
                struct.pack("<Biiii", 8, 0, 5, 0, -1),
            ),
            "wall-hit-player": (
                struct.pack("<Biiii", 8, 0, 0, 0, MAX_PLAYERS),
                struct.pack("<Biiii", 8, 0, 0, 0, MAX_PLAYERS - 1),
            ),
            "trigger-segment": (
                struct.pack("<Biiii", 9, MAX_SEGMENTS, 0, 0, 0),
                struct.pack("<Biiii", 9, MAX_SEGMENTS - 1, 5, MAX_OBJECTS - 1, 0),
            ),
            "trigger-side": (
                struct.pack("<Biiii", 9, 0, 6, 0, 0),
                struct.pack("<Biiii", 9, 0, 5, 0, 0),
            ),
            "trigger-object": (
                struct.pack("<Biiii", 9, 0, 0, MAX_OBJECTS, 0),
                struct.pack("<Biiii", 9, 0, 0, MAX_OBJECTS - 1, 0),
            ),
            "wall-toggle": (
                struct.pack("<Bii", 13, MAX_SEGMENTS, 0),
                struct.pack("<Bii", 13, MAX_SEGMENTS - 1, 5),
            ),
            "effect-blowup": (
                struct.pack("<Bhbiii", 21, 0, 6, 0, 0, 0),
                struct.pack("<Bhbiii", 21, MAX_SEGMENTS - 1, 5, 0, 0, 0),
            ),
            "primary-wall-texture": (
                struct.pack("<Bhbhbh", 26, 0, 0, 0, 0, -1),
                struct.pack("<Bhbhbh", 26, MAX_SEGMENTS - 1, 5, MAX_SEGMENTS - 1, 5, 0),
            ),
            "overlay-wall-texture": (
                struct.pack("<Bhbhbh", 27, 0, 0, 0, 0, 0x3FFF),
                struct.pack("<Bhbhbh", 27, MAX_SEGMENTS - 1, 5, MAX_SEGMENTS - 1, 5, 0),
            ),
            "door-opening": (
                struct.pack("<Bhb", 41, MAX_SEGMENTS, 0),
                struct.pack("<Bhb", 41, MAX_SEGMENTS - 1, 5),
            ),
            "cloaking-wall-front": (
                struct.pack("<BBBBBBhhhh", 44, MAX_WALLS, 0, 0, 0, 0, 0, 0, 0, 0),
                struct.pack("<BBBBBBhhhh", 44, MAX_WALLS - 1, MAX_WALLS - 1, 0, 0, 0, 0, 0, 0, 0),
            ),
            "cloaking-wall-back": (
                struct.pack("<BBBBBBhhhh", 44, 0, MAX_WALLS, 0, 0, 0, 0, 0, 0, 0),
                struct.pack("<BBBBBBhhhh", 44, 0, MAX_WALLS - 1, 0, 0, 0, 0, 0, 0, 0),
            ),
        }
        for event_name, (invalid_event, valid_event) in world_events.items():
            for variant, event in (("invalid", invalid_event), ("valid-boundary", valid_event)):
                event_demo = case_dir / f"world-event-{event_name}-{variant}.dem"
                event_demo.write_bytes(insert_event(demo_bytes, event_offset, event))
                require_conversion(
                    executable,
                    data_dir,
                    event_demo,
                    case_dir / f"world-event-{event_name}-{variant}.jsonl",
                    base_output,
                )

        object_events = {
            "render-type": bytes((4, 8, OBJ_LIGHT)),
            "object-type": bytes((4, RT_LASER, 254)),
            "hidden-object-type": bytes((4, 0, 254)),
            "empty-orientation": bytes((4,))
            + struct.pack("<BBBBh", RT_POLYOBJ, OBJ_LIGHT, 0, 0, 0)
            + bytes(9)
            + demo_shortpos(),
            "segment": bytes((4,)) + light_laser_object(MAX_SEGMENTS),
            "robot-id": bytes((4,)) + demo_object_header(RT_POLYOBJ, OBJ_ROBOT, 255),
            "powerup-id": bytes((4,)) + demo_object_header(RT_LASER, OBJ_POWERUP, 255),
            "player-id": bytes((4,)) + demo_object_header(RT_POLYOBJ, OBJ_PLAYER, MAX_PLAYERS),
            "weapon-id": bytes((4,))
            + demo_object_header(RT_LASER, OBJ_WEAPON, 255)
            + struct.pack("<BBi", 9, MT_NONE, 1 << 16),
            "clutter-model": bytes((4,)) + demo_object_header(RT_POLYOBJ, OBJ_CLUTTER, 255),
            "control-type": bytes((4,))
            + demo_object_header(RT_LASER, OBJ_LIGHT, 0)
            + struct.pack("<BBi", 255, MT_NONE, 1 << 16),
            "movement-type": bytes((4,))
            + demo_object_header(RT_LASER, OBJ_LIGHT, 0)
            + struct.pack("<BBi", CT_LIGHT, 255, 1 << 16),
            "model": bytes((4,)) + invalid_model_object(-1),
            "texture-override": bytes((4,)) + player_poly_object(2_147_483_647),
            "vclip": bytes((4,)) + vclip_object(2_147_483_647, 0),
            "vclip-frame": bytes((4,)) + vclip_object(0, 255),
            "explosion-delete-object": bytes((4,)) + attached_explosion_object(MAX_OBJECTS),
        }
        for event_name, event in object_events.items():
            event_demo = case_dir / f"object-event-{event_name}-invalid.dem"
            event_demo.write_bytes(insert_event(demo_bytes, event_offset, event))
            require_malformed_preserves_output(
                executable,
                data_dir,
                event_demo,
                prior_output,
                f"preserve object {event_name}".encode(),
            )

        valid_object_events = {
            "hidden-unused-object": bytes((4, 0, 255)),
            "maximum-segment": bytes((4,)) + light_laser_object(MAX_SEGMENTS - 1),
            "player-model": bytes((4,)) + player_poly_object(-1),
            "vclip": bytes((4,)) + vclip_object(0, 0),
            "orphan-attachment": bytes((4,)) + attached_explosion_object(-1),
            "extra-viewer-attachment-chain": bytes((3, 1))
            + attached_explosion_object(-1)
            + bytes((4,))
            + attached_explosion_object(-1),
            "signed-scaling-boundaries": struct.pack("<BhB", 22, -32768, 4)
            + player_poly_object(-1, signed_boundaries=True),
        }
        for event_name, event in valid_object_events.items():
            event_demo = case_dir / f"object-event-{event_name}-valid.dem"
            event_demo.write_bytes(insert_event(demo_bytes, event_offset, event))
            require_conversion(
                executable,
                data_dir,
                event_demo,
                case_dir / f"object-event-{event_name}-valid.jsonl",
            )

        for wall_count in (-1, 256, 2_147_483_647):
            wall_demo = case_dir / f"wall-count-{wall_count}.dem"
            mutated = bytearray(demo_bytes)
            struct.pack_into("<i", mutated, wall_offset, wall_count)
            wall_demo.write_bytes(mutated)
            require_malformed_preserves_output(executable, data_dir, wall_demo, prior_output, b"preserve wall count")
        for count_bytes in range(4):
            truncated = case_dir / f"wall-count-truncated-{count_bytes}.dem"
            truncated.write_bytes(demo_bytes[: wall_offset + count_bytes])
            require_malformed_preserves_output(
                executable, data_dir, truncated, prior_output, b"preserve count truncation"
            )
        one_wall = bytearray(demo_bytes[: wall_offset + 4])
        struct.pack_into("<i", one_wall, wall_offset, 1)
        for record_bytes in range(7):
            truncated = case_dir / f"wall-record-truncated-{record_bytes}.dem"
            truncated.write_bytes(one_wall + bytes(record_bytes))
            require_malformed_preserves_output(
                executable, data_dir, truncated, prior_output, b"preserve record truncation"
            )

        output = case_dir / "result.jsonl"
        if run_dump(executable, data_dir, source, output) != 0:
            raise AssertionError("valid distinct conversion failed")
        records = [json.loads(line) for line in output.read_text(encoding="utf-8").splitlines()]
        if len(records) < 2 or records[0].get("type") != "header":
            raise AssertionError("conversion output is missing its header")
        if records[-1].get("type") != "result":
            raise AssertionError("conversion output is missing its result")
        if list(case_dir.glob("*.tmp.*")):
            raise AssertionError("classic demo conversion leaked a temporary output")

    print("classic demo dump transaction tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
