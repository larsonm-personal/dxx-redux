"""Fresh native screenshots without dependencies on historical input demos."""

import hashlib
import json
import math
from pathlib import Path
import shutil

from PIL import Image

from generate_store_assets import PACKAGE, write_json
from store_asset_media import review, validate_graphics


def validate_stills(output):
    output = Path(output)
    manifest = json.loads((output / "live-stills.json").read_text())
    selections = manifest["selections"]
    names = [row["file"] for row in selections]
    if len(names) != 8 or len(set(names)) != 8:
        raise ValueError("Live store capture requires eight distinct images")
    if {path.name for path in (output / "selected-stills").glob("*.png")} != set(names):
        raise ValueError("Selected images differ from the live capture manifest")
    launcher = json.loads((output / "launcher/top-state.json").read_text())
    if not launcher.get("resume_candidate", {}).get("has_thumbnail"):
        raise ValueError("Launcher capture requires a populated save")
    for index, row in enumerate(selections):
        if Path(row["file"]).name != row["file"]:
            raise ValueError("Invalid selected image basename")
        path = output / "selected-stills" / row["file"]
        if hashlib.sha256(path.read_bytes()).hexdigest() != row["sha256"]:
            raise ValueError("Selected PNG changed: " + row["file"])
        expected_size = (1080, 2400) if index < 2 else (2400, 1080)
        with Image.open(path) as picture:
            if picture.format != "PNG" or picture.size != expected_size or list(picture.size) != row["size"]:
                raise ValueError("Unexpected screenshot format or size: " + row["file"])
            picture.verify()
        if row["method"] == "live-engine-authored-scene":
            state = json.loads((output / "states" / (Path(row["file"]).stem + ".json")).read_text())
            if not state["in_game"] or state["player_dead"] or state["current_level_num"] != row["level"]:
                raise ValueError("Gameplay screenshot must show a living player on its selected level")
            validate_graphics(state)
            if row["progress_profile"] != PROGRESS_PROFILES[row["file"]]:
                raise ValueError("Screenshot progress profile differs from the recipe")
            verify_progress(state, row["progress_profile"])
            if row["progress"] != progress_counts(state):
                raise ValueError("Progress evidence differs from native state")
            if row["progress_profile"]["explosion"]:
                event = row.get("combat_event", {})
                if event.get("kills_after", 0) <= event.get("kills_before", 0) or not event.get("explosions"):
                    raise ValueError("Missing native weapon kill/explosion evidence")
        if row["method"] == "live-native-flyout":
            state = json.loads((output / "states/08-d1-flyout.json").read_text())
            if state.get("endlevel_sequence", 0) < 2:
                raise ValueError("Fly-out screenshot was not captured during the native exit")
    return {
        "images": len(selections),
        "progress": [
            {"file": row["file"], **row["progress"]}
            for row in selections
            if row["method"] == "live-engine-authored-scene"
        ],
        "replay_validation": manifest["replay_validation"],
    }


def pose(capture, segment, point, target=None):
    heading = pitch = 0
    if target:
        dx, dy, dz = (target[key] - point[key] for key in ("x", "y", "z"))
        heading = round(math.atan2(dx, dz) * 65536 / math.tau)
        pitch = round(-math.atan2(dy, math.hypot(dx, dz)) * 65536 / math.tau)
    capture.step("pose_view", segment=segment, **point, pitch=pitch, heading=heading, bank=0, post_delay_ms=100)


PROGRESS_PROFILES = {
    "03-d1-level5.png": {"hostages": "none", "secrets": "none", "kills": 0, "explosion": False},
    "04-d1-level5.png": {"hostages": "some", "secrets": "some", "kills": 1, "explosion": True},
    "05-d1-level18.png": {"hostages": "all", "secrets": "some", "kills": 2, "explosion": True},
    "06-d2-level9.png": {"hostages": "some", "secrets": "some", "kills": 2, "explosion": True},
    "07-d1-level7-boss.png": {"hostages": "none", "secrets": "some", "kills": 2, "explosion": False},
}


def progress_counts(state):
    player, secrets = state["player"], state["secret_areas"]
    return {
        "robots_killed": player["robots_killed_level"],
        "robots_total": player["robots_level"],
        "hostages_rescued": player["hostages_on_board"],
        "hostages_total": player["hostages_level"],
        "secrets_found": secrets["found_count"],
        "secrets_total": secrets["total"],
    }


def verify_progress(state, profile):
    counts = progress_counts(state)
    for kind, current, total in (
        (profile["hostages"], counts["hostages_rescued"], counts["hostages_total"]),
        (profile["secrets"], counts["secrets_found"], counts["secrets_total"]),
    ):
        valid = (
            current == 0 if kind == "none" else current == total and total > 0 if kind == "all" else 0 < current < total
        )
        if not valid:
            raise ValueError(f"Expected {kind} progress, got {current}/{total}: {counts}")
    if counts["robots_killed"] < profile["kills"]:
        raise ValueError("Missing real weapon kills: " + str(counts))
    if profile["kills"] == 0 and counts["robots_killed"] != 0:
        raise ValueError("Zero-progress view contains robot kills")


def secret_segments(state):
    return {segment for item in state["secret_areas"]["secrets"] for segment in item["segments"]}


def live_robots(state, excluded=()):
    return [robot for robot in state["level_scene"]["robots"] if robot["alive"] and robot["segment"] not in excluded]


def frame_robot(capture, robot, excluded=()):
    pose(capture, robot["segment"], robot["position"])
    initial = capture.state()
    segment = initial["level_scene"]["segment"]
    robot.update(next(item for item in initial["level_scene"]["robots"] if item["object"] == robot["object"]))
    distance = max(18, robot["radius"] * 4)
    farthest = max(math.dist(list(vertex.values()), list(robot["position"].values())) for vertex in segment["vertices"])
    portals = [
        side for side in segment["sides"] if side["child"] >= 0 and side["wall"] < 0 and side["child"] not in excluded
    ]
    if farthest < distance and portals:
        portal = max(
            portals, key=lambda side: math.dist(list(side["center"].values()), list(robot["position"].values()))
        )
        pose(capture, portal["child"], portal["center"])
        segment = capture.state()["level_scene"]["segment"]
    center = {
        key: sum(vertex[key] for vertex in segment["vertices"]) / len(segment["vertices"]) for key in ("x", "y", "z")
    }
    candidates = [center] + [
        {key: center[key] * 0.25 + vertex[key] * 0.75 for key in center} for vertex in segment["vertices"]
    ]
    point = min(
        candidates, key=lambda point: abs(math.dist(list(point.values()), list(robot["position"].values())) - distance)
    )
    pose(capture, segment["index"], point, robot["position"])
    # Collision handling can move a ship whose radius overlaps a wall. Aim from
    # the actual settled pose, not the requested point before physics resolved it
    settled = capture.state()
    point = {key: settled["position"][key] for key in ("x", "y", "z")}
    robot.update(next(item for item in settled["level_scene"]["robots"] if item["object"] == robot["object"]))
    pose(capture, settled["position"]["segment"], point, robot["position"])
    return {"segment": settled["position"]["segment"], "position": point}


def shoot_robot(capture, robot, filename=None):
    before = capture.state()["player"]["robots_killed_level"]
    # Mixer button IDs mirror TouchBindings.MIXER_BTN_BASE + BTN_FIRE_PRIMARY
    capture.step("send_button", button=100, held=1)
    try:
        killed = capture.wait(lambda state: state["player"]["robots_killed_level"] > before, timeout=20)
        event = {
            "target_object": robot["object"],
            "kills_before": before,
            "kills_after": killed["player"]["robots_killed_level"],
            "method": "native-primary-weapon",
        }
        if filename:
            exploding = capture.wait(lambda state: bool(state["level_scene"]["explosions"]), timeout=8)
            event["explosions"] = exploding["level_scene"]["explosions"]
            write_json(capture.output / "events" / (Path(filename).stem + ".json"), exploding)
            capture.screenshot("selected-stills/" + filename)
        return event
    finally:
        capture.step("send_button", button=100, held=0, pressed=0)


def apply_progress(capture, profile, name):
    state = capture.state()
    desired = (
        0 if profile["hostages"] == "none" else state["player"]["hostages_level"] if profile["hostages"] == "all" else 1
    )
    while state["player"]["hostages_on_board"] < desired:
        hostage = state["level_scene"]["hostages"][0]
        count = state["player"]["hostages_on_board"]
        pose(capture, hostage["segment"], hostage["position"])
        # TouchBindings.MIXER_BTN_BASE + BTN_ACCELERATE: real contact pickup
        capture.automate(
            [
                {"action": "send_button", "button": 102, "held": 1},
                {"action": "wait_ms", "ms": 150},
                {"action": "send_button", "button": 102, "held": 0, "pressed": 0},
                {"action": "wait_for", "expect": {"player.hostages_on_board": {"gte": count + 1}}, "timeout_ms": 8000},
            ],
            name=name + "-rescue",
        )
        state = capture.state()
    if profile["secrets"] == "some" and state["secret_areas"]["found_count"] == 0:
        hostages = {item["segment"] for item in state["level_scene"]["hostages"]}
        choices = [
            item
            for item in state["secret_areas"]["secrets"]
            if not item["found"] and item.get("capture_segment") not in hostages
        ]
        if not choices:
            raise RuntimeError("No safe authored secret entrance for " + name)
        secret = choices[0]
        pose(capture, secret["capture_segment"], secret["capture_position"])
        capture.wait(lambda state: state["secret_areas"]["found_count"] > 0, timeout=8)
    while capture.state()["player"]["robots_killed_level"] < profile["kills"]:
        state = capture.state()
        excluded = secret_segments(state)
        robots = live_robots(state, excluded)
        targets = [robot for robot in robots if robot["radius"] >= 4] or robots
        target = min(targets, key=lambda robot: robot["shields"])
        frame_robot(capture, target, excluded)
        shoot_robot(capture, target)


def capture_combat(capture, game, level, names):
    capture.launcher()
    capture.command("launch", game=game)
    capture.pilot_menu(game)
    capture.campaign(game)
    # Use the engine's existing level-warp cheat; no objects or media are fabricated
    code = "gabbagabbaheyfarmerjoe" if game == "d1" else "freespace"
    capture.automate([{"action": "key", "key": key, "post_delay_ms": 40} for key in code], name=f"warp-{game}-{level}")
    capture.automate(
        [{"action": "key", "key": key} for key in str(level)]
        + [{"action": "key", "key": "enter", "post_delay_ms": 300}]
        + ([{"action": "key", "key": "escape"}] if game == "d1" else [{"action": "skip_briefing", "timeout_ms": 20000}])
        + [{"action": "wait_for", "expect": {"in_game": "true", "current_level_num": str(level)}, "timeout_ms": 20000}],
        name=f"enter-{game}-{level}",
    )
    capture.automate(
        [
            {"action": "set_debug", "field": field, "value": "true"}
            for field in ("show_boss_health_bar", "show_robot_hostage_counts")
        ],
        name=f"hud-{game}-{level}",
    )
    capture.step("set_debug", field="cockpit_mode", value="3")
    capture.automate(
        [
            {"action": "key", "key": key, "post_delay_ms": 40}
            for key in ("racerx" if game == "d1" else "almightyhonestbob")
        ]
        + [{"action": "wait_ms", "ms": 2000}],
        name=f"camera-protection-{game}-{level}",
    )
    capture.verify_graphics(f"stills-{game}-{level}")
    # Existing debug shortcut holds authored robots in place while framing the camera
    capture.step("key", key="p", modifier="delete")
    evidence = []
    for name in names:
        profile = PROGRESS_PROFILES[name]
        apply_progress(capture, profile, name)
        state = capture.state()
        excluded = secret_segments(state) if profile["secrets"] == "none" else set()
        robots = live_robots(state, excluded)
        if not robots:
            raise RuntimeError("No live authored robots for " + name)
        robot = (
            max(robots, key=lambda item: item["shields"])
            if game == "d1" and level == 7
            else min(robots, key=lambda item: item["shields"])
        )
        if profile["explosion"]:
            targets = [item for item in robots if item["radius"] >= 4] or robots
            robot = min(targets, key=lambda item: item["shields"])
        framing = frame_robot(capture, robot, excluded)
        if profile["explosion"]:
            event = shoot_robot(capture, robot, name)
        else:
            capture.screenshot("selected-stills/" + name)
            event = None
        state = capture.state()
        verify_progress(state, profile)
        write_json(capture.output / "states" / (Path(name).stem + ".json"), state)
        row = {
            "file": name,
            "game": game,
            "level": level,
            "robot": robot,
            "pose": framing,
            "method": "live-engine-authored-scene",
            "progress_profile": profile,
            "progress": progress_counts(state),
            "combat_event": event,
        }
        write_json(capture.output / "selections" / (Path(name).stem + ".json"), row)
        evidence.append(row)
    capture.step("key", key="p", modifier="delete")
    return evidence


def capture_stills(capture, apk):
    capture.shell("am", "force-stop", PACKAGE)
    capture.install(apk)
    capture.shell("pm", "clear", PACKAGE)
    capture.prepare(apk)
    capture.seed()
    destination = capture.output / "selected-stills"
    destination.mkdir(exist_ok=True)
    evidence = []
    for name in ("01-launcher-top.png", "02-launcher-bottom.png"):
        source = capture.output / "launcher" / name.replace("launcher-", "")
        shutil.copyfile(source, destination / name)
        evidence.append({"file": name, "method": "live-launcher"})
    evidence += capture_combat(capture, "d1", 5, ["03-d1-level5.png", "04-d1-level5.png"])
    evidence += capture_combat(capture, "d1", 18, ["05-d1-level18.png"])
    evidence += capture_combat(capture, "d2", 9, ["06-d2-level9.png"])
    evidence += capture_combat(capture, "d1", 7, ["07-d1-level7-boss.png"])
    capture.launcher()
    capture.command("launch", game="d1")
    capture.pilot_menu("d1")
    capture.campaign("d1")
    capture.step("trigger_endlevel", value="from_exit_tunnel", capture_fps=60)
    state = capture.wait(lambda state: state.get("endlevel_sequence", 0) == 2, timeout=150)
    capture.screenshot("selected-stills/08-d1-flyout.png")
    write_json(capture.output / "states/08-d1-flyout.json", state)
    evidence.append({"file": "08-d1-flyout.png", "method": "live-native-flyout"})
    for row in evidence:
        path = destination / row["file"]
        with Image.open(path) as picture:
            picture.verify()
            row["size"] = list(picture.size)
        row["sha256"] = hashlib.sha256(path.read_bytes()).hexdigest()
    write_json(
        capture.output / "live-stills.json",
        {
            "apk_sha256": hashlib.sha256(apk.read_bytes()).hexdigest(),
            "selections": evidence,
            "replay_validation": "Live staged gameplay captures; no input replay equivalence claimed",
        },
    )
    write_json(capture.output / "stills-validation.json", validate_stills(capture.output))
    review(capture.output)
    capture.launcher()
