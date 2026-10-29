"""Required original XL native tests and strict original-texture JVM adapter."""

import argparse
import json
import os
import re
import shutil
from pathlib import Path
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parent))
from xl_private_assets import verify

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "XL_compat/tests"))
from reference_sources import SOURCE_ARGUMENTS, reference_parameters, reference_sources, require_source_arguments

CORE_TESTS = (
    {
        "xl_pilot_archive",
        "xl_preview_lifecycle",
        "xl_stock_loader_probe",
        "xl_route_checkpoint_probe",
        "xl_water_arena",
        "xl_observatory_inventory",
        "xl_observatory_descriptor",
        "xl_wall_trigger_chains",
        "xl_wall_trigger_scheduler",
        "xl_object_events",
        "xl_object_chain_delays",
        "xl_speed_boosts",
        "xl_spawn_points",
        "xl_water_actors",
        "xl_lava_volumes",
        "xl_blocked_segments",
        "xl_keyed_closed_walls",
        "xl_doorways",
        "xl_close_doors",
        "xl_changing_lights",
        "xl_flickers",
        "xl_level_messages",
        "xl_level_text_reference",
        "xl_extended_hog",
        "xl_wide_walls",
        "xl_cloaks",
        "xl_trigger_capacity",
        "xl_projectiles",
        "xl_object_capacity",
        "xl_hxm_native",
        "xl_repair_centers",
        "xl_legacy_repair",
        "xl_format34_native",
        "xl_light_index_reference",
        "xl_fog_reference",
        "xl_object_trigger_decode_reference",
        "xl_generator_assignment_reference",
        "xl_generator_assignment_runtime",
        "xl_boss_spawning",
        "xl_boss_transport",
        "xl_trigger_flags",
        "xl_boss_groups",
        "xl_player_resources",
        "xl_player_resource_reference",
        "xl_trigger_flag_reference",
        "xl_equipment_generators",
        "xl_modern_runtime_layouts",
        "xl_modern_equipment_25",
        "xl_modern_equipment_27",
        "xl_modern_generators_25",
        "xl_modern_generators_27",
        "xl_effect_payloads",
        "xl_effect_decode_reference",
        "xl_effect_inspection_gate",
        "xl_konflict_features",
        "xl_dragon_lighting_decode",
        "xl_dragon_wave_decode",
        "xl_dragon_companion_ownership",
        "xl_normal_red_key",
        "xl_normal_reactor",
        "xl_normal_escape",
        "xl_earned_campaign_transition",
    }
    | {"xl_normal_branch_" + str(n) for n in range(1, 13)}
    | {"xl_normal_secret_" + str(n) for n in (4, 205, 206, 17, 81, 225, 14, 227, 199)}
    | {"xl_normal_portal_" + n for n in ("176_182", "193_194", "136_137", "5_185")}
)

# Controlled engine events retain save/restore and stock-mode checks. Repeated
# pilot navigation and experimental campaign searches belong to the longer lane
CAMPAIGN_TESTS = {name for name in CORE_TESTS if name.startswith("xl_normal_")} | {
    "xl_earned_campaign_transition",
    "xl_route_checkpoint_probe",
}
FAST_TESTS = (CORE_TESTS - CAMPAIGN_TESTS) | {
    "xl_modern_layout_contract",
    "xl_inspect_stream_contract",
    "xl_hog_contract",
    "xl_projectile_history_contract",
    "xl_animation_phase_contract",
    "xl_static_pixels_contract",
    "xl_scoped_texture_contract",
    "xl_companion_lighting_contract",
    "xl_wave_contract",
    "xl_mission_labels",
    "xl_lighting_calculation",
    "xl_inventory_contract",
    "xl_level_text_contract",
    "xl_segment_flags_reference",
    "xl_face_point_reference",
    "xl_side_center_reference",
    "xl_fixed_cross_reference",
    "xl_projection_reference",
    "xl_magnitude_reference",
    "xl_hog_reference",
    "xl_modern_layout_reference",
    "xl_hxm_geometry_reference",
    "xl_wall_capacity_reference",
    "xl_trigger_capacity_reference",
    "xl_projectile_reference",
    "xl_nearest_lights_reference",
    "xl_geometry_attenuation_reference",
    "xl_lighting_reference_arithmetic",
    "xl_dynamic_color_reference",
    "xl_neptune_color_precedence",
    "xl_neptune_inventory",
    "xl_neptune_native_layout",
    "xl_wall_teleports",
    "xl_modern_properties",
    "xl_modern_water",
    "xl_neptune_native_features",
    "xl_fog_gpu",
}

# Unfinished campaign acceptance: execution is required, success is not qualification.
EXPERIMENTAL_TESTS = {"xl_level_two_keys"}


def validate_registration(listing, fast=False):
    required = FAST_TESTS if fast else CORE_TESTS | EXPERIMENTAL_TESTS
    selected = [test for test in listing["tests"] if test["name"] in required]
    if len(selected) != len(required) or {test["name"] for test in selected} != required:
        raise ValueError("Required original XL CTests were omitted or duplicated")
    if fast:
        require_source_arguments(selected)


def require_junit(path, names):
    root = ET.parse(path).getroot()
    cases = root.findall(".//testcase")
    found = [case.get("name") for case in cases]
    if len(found) != len(set(found)) or set(found) != set(names):
        raise ValueError("Required XL tests were omitted or duplicated")
    if any(case.find(tag) is not None for case in cases for tag in ("skipped", "failure", "error")):
        raise ValueError("Required XL tests did not all pass")


def native_receipt(path, returncode, fast=False):
    """Require established passes and explicit, non-skipped experimental results."""
    cases = ET.parse(path).getroot().findall(".//testcase")
    names = [case.get("name") for case in cases]
    required = FAST_TESTS if fast else CORE_TESTS
    optional = set() if fast else EXPERIMENTAL_TESTS
    if len(names) != len(set(names)) or set(names) != required | optional:
        raise ValueError("Original XL results were omitted or duplicated")
    experimental = {}
    for case in cases:
        name = case.get("name")
        if any(case.find(tag) is not None for tag in ("skipped", "error")):
            raise ValueError("Original XL execution was skipped or errored")
        failures = case.findall("failure")
        failed = bool(failures)
        if case.get("status") != ("fail" if failed else "run") or len(failures) > 1:
            raise ValueError("Original XL result did not execute normally")
        if name in required and failed:
            raise ValueError("Required original XL regression failed: " + name)
        if failed and (
            failures[0].get("message") != "Failed"
            or (case.findtext("system-out") or "")
            .splitlines()
            .count("XL_EXPERIMENTAL_ACCEPTANCE: xl_level_two_keys route_incomplete")
            != 1
        ):
            raise ValueError("Original XL failure is not classified campaign acceptance")
        if name in optional:
            experimental[name] = "failed" if failed else "passed"
    expected_code = 8 if "failed" in experimental.values() else 0
    if returncode != expected_code:
        raise ValueError("CTest exit status disagrees with original XL receipt")
    return {"required_passed": len(required), "experimental": experimental, "level_two_completion_qualified": False}


def validate_summary(receipt, fast=False):
    """The owner and adapter share exact membership, rather than drifting counts."""
    required = FAST_TESTS if fast else CORE_TESTS
    experimental = receipt.get("experimental")
    if (
        receipt.get("required_passed") != len(required)
        or receipt.get("required_tests") != sorted(required)
        or receipt.get("level_two_completion_qualified") is not False
        or not isinstance(experimental, dict)
        or set(experimental) != (set() if fast else EXPERIMENTAL_TESTS)
        or any(value not in ("passed", "failed") for value in experimental.values())
        or receipt.get("format_witnesses") != (16 if fast else 9)
        or receipt.get("modern_source_levels") != (3 if fast else 0)
    ):
        raise ValueError("Invalid original XL acceptance receipt")
    return {
        "required_passed": len(required),
        "format_witnesses": receipt["format_witnesses"],
        "modern_source_levels": receipt["modern_source_levels"],
    }


def format_results(inspect, stage, work, data):
    for index, row in enumerate(data["format_witnesses"]):
        target = work / ("format-" + str(index) + ".json")
        target.unlink(missing_ok=True)
        run = subprocess.run(
            [str(inspect), str(stage / "witnesses" / row["hog_member"]), str(target)],
            capture_output=True,
            timeout=30,
            check=False,
        )
        result = json.loads(target.read_text())
        if len(result["levels"]) != 1 or result["playable"] is not False:
            raise ValueError("XL format receipt is incomplete")
        inventory = result["levels"][0]["inventory"]
        pair = (row["level_version"], row["game_version"])
        if pair in ((15, 33), (17, 36), (23, 40)):
            if (
                run.returncode
                or inventory["status"] != "inspection_only"
                or (inventory["level_version"], inventory["game_version"]) != pair
            ):
                raise ValueError("Audited XL format failed inspection")
        elif pair == (15, 34):
            if (
                run.returncode != 1
                or inventory["status"] != "rejected"
                or inventory["reason"]
                != "invalid packed light index 2 in 15/34: segment=63 side=0 count=2592 first=37 available=1284"
            ):
                raise ValueError("Original 15/34 packed-light rejection changed")
        elif (
            run.returncode != 1
            or inventory["status"] != "rejected"
            or not inventory["reason"].startswith(
                ("unsupported level version ", "unsupported level/game version pair ")
            )
        ):
            raise ValueError("Unsupported XL format was not explicitly rejected")
    return len(data["format_witnesses"])


def modern_results(inspect, reference, stage, work):
    """Use complete original bytes, preserving known rejections beside successes."""
    original = stage / "modern/boilpnt.hog"
    report = work / "modern-inventory.json"
    report.unlink(missing_ok=True)
    run = subprocess.run([str(inspect), str(original), str(report)], capture_output=True, timeout=90, check=False)
    result = json.loads(report.read_text())
    decoded = {
        "boilpnt.rl2": (23, 1241),
        "canyon.rl2": (27, 2139),
        "canyon2.rl2": (27, 2125),
        "watrwrks.rl2": (27, 3188),
    }
    rejected = {
        "control.rl2": "invalid packed light index ",
        "control2.rl2": "invalid reactor trigger count",
        "subzero.rl2": "offset 923615: unsupported movement layout 255 at object 4",
    }
    rows = result["levels"]
    if (
        run.returncode != 1
        or result["playable"] is not False
        or len(rows) != 7
        or {row["name"] for row in rows} != decoded.keys() | rejected.keys()
    ):
        raise ValueError("Modern original outcome membership differs")
    compared = 0
    for row in rows:
        name, inventory = row["name"], row["inventory"]
        if name in rejected:
            if inventory["status"] != "rejected" or not inventory["reason"].startswith(rejected[name]):
                raise ValueError("Modern original expected rejection changed")
            continue
        version, segments = decoded[name]
        if (
            inventory["status"] != "inspection_only"
            or (inventory["level_version"], inventory["game_version"]) != (version, 40)
            or len(inventory["segments"]) != segments
        ):
            raise ValueError("Modern original format or geometry differs")
        if version == 27:
            check = subprocess.run(
                [str(reference), "--member", str(original), name],
                capture_output=True,
                text=True,
                timeout=30,
                check=True,
            )
            if not re.search(
                rf"\b1 levels, {segments} segments, {segments * 6} sides, [1-9][0-9]* comparisons; [1-9][0-9]* vertex brightness maxima",
                check.stdout,
            ):
                raise ValueError("Modern original source comparison did not execute completely")
            compared += 1
    if compared != 3:
        raise ValueError("Modern original source comparison omitted levels")
    return len(rows), compared


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--jvm-only", action="store_true")
    parser.add_argument(
        "--fast",
        action="store_true",
        help="Required controlled native, source and format checks without campaign searches",
    )
    parser.add_argument(
        "--reference-root", type=Path, help="Existing source tree; consumed files are SHA-pinned by CMake"
    )
    args = parser.parse_args()
    if args.fast and args.jvm_only:
        parser.error("Choose fast native or JVM coverage")
    repo, work = args.repo.resolve(), args.work.resolve()
    stage = repo / "game_data/retail-ci/xl"
    data = verify(stage, modern=args.fast)
    work.mkdir(parents=True, exist_ok=True)
    (work / "original-status.json").unlink(missing_ok=True)

    def run(command, timeout=1200, check=True):
        # Raw engine output stays in the private runner log, never stdout/artifacts
        with (work / "commands.log").open("ab") as log:
            return subprocess.run(
                [str(x) for x in command], cwd=repo, stdout=log, stderr=subprocess.STDOUT, timeout=timeout, check=check
            )

    preview, stock, contracts = (work / x for x in ("preview", "stock", "contracts"))
    run(
        [
            "cmake",
            "-S",
            repo / "d2",
            "-B",
            preview,
            "-DCMAKE_BUILD_TYPE=RelWithDebInfo",
            "-DBUILD_TESTING=ON",
            "-DDXX_XL_DEVELOPER_TESTS=ON",
        ]
    )
    run(["cmake", "--build", preview, "--parallel", "2", "--target", "dxx-redux-d2-headless-metadata"])
    engine = preview / "main/xl-preview-lifecycle"
    if args.jvm_only:
        env = os.environ.copy()
        env.update(
            DXX_XL_LEVELPACK=str(stage / "levelpack.7z"),
            DXX_XL_TEXTURE_FILES=str(stage / "adapter"),
            DXX_XL_TEXTURE_ENGINE=str(engine),
            DXX_XL_RETAIL_DATA=str(repo / "game_data/retail-ci/d2"),
        )
        report = (
            repo
            / "android/app/build/test-results/testDebugUnitTest/TEST-com.dxxredux.app.OriginalStaticTextureTest.xml"
        )
        report.unlink(missing_ok=True)
        with (work / "jvm.log").open("wb") as log:
            subprocess.run(
                [
                    "bash",
                    str(repo / "android/gradlew"),
                    "-p",
                    str(repo / "android"),
                    ":app:cleanTestDebugUnitTest",
                    ":app:testDebugUnitTest",
                    "--tests",
                    "com.dxxredux.app.OriginalStaticTextureTest",
                    "--no-build-cache",
                    "--console=plain",
                ],
                cwd=repo,
                env=env,
                stdout=log,
                stderr=subprocess.STDOUT,
                timeout=1200,
                check=True,
            )
        require_junit(report, {"originalArchiveImportsPublishesAndResolvesStaticTexture"})
        print("PASS: required original texture import/publication/native resolution JVM test")
        return
    reference = reference_sources(work, args.reference_root, run)
    run(["cmake", "-S", repo / "d2", "-B", stock, "-DCMAKE_BUILD_TYPE=RelWithDebInfo", "-DBUILD_TESTING=ON"])
    run(
        [
            "cmake",
            "--build",
            stock,
            "--parallel",
            "2",
            "--target",
            "dxx-redux-d2-headless-metadata",
            *([] if args.fast else ["dxx-redux-d2-headless-route"]),
        ]
    )
    # Imported-D1 Dragon Pit requires the registered D1 bank beside D2 data
    retail_data = work / "retail-data"
    retail_data.mkdir(exist_ok=True)
    for game in ("d1", "d2"):
        for source in (repo / "game_data/retail-ci" / game).iterdir():
            if source.is_file():
                shutil.copyfile(source, retail_data / source.name)
    parameters = {
        "XL_PILOT_HOG": stage / "missions/single/d2-xl.hog",
        "XL_PILOT_DESCRIPTOR": stage / "missions/single/d2-xl.mn2",
        "XL_PILOT_LEVEL": stage / "witnesses/d2leva-1.rl2",
        "XL_ARENA_HOG": stage / "missions/spotlight/rtarena.hog",
        "XL_ARENA_DESCRIPTOR": stage / "missions/spotlight/rtarena.mn2",
        "XL_APOCALYPTIC_HOG": stage / "missions/single/af-xl.hog",
        "XL_OBSERVATORY_HOG": stage / "missions/kruel/obsrv-xl.hog",
        "XL_OBSERVATORY_DESCRIPTOR": stage / "missions/kruel/obsrv-xl.mn2",
        "XL_NEPTUNE_HOG": stage / "missions/spotlight/neptunxl.hog",
        "XL_NEPTUNE_DESCRIPTOR": stage / "missions/spotlight/neptunxl.mn2",
        "XL_KONFLICT_HOG": stage / "missions/spotlight/konflict.hog",
        "XL_KONFLICT_DESCRIPTOR": stage / "missions/spotlight/konflict.mn2",
        "XL_DRAGON_HOG": stage / "missions/spotlight/drgpit.hog",
        "XL_DRAGON_SOUNDS": stage / "mods/drgpit/sounds",
        "XL_RETAIL_DATA": retail_data,
        "XL_PREVIEW_LIFECYCLE": engine,
        "XL_STOCK_METADATA": stock / "main/dxx-redux-d2-headless-metadata",
        "XL_REFERENCE_GEOMETRY_ROOT": reference,
    }
    if not args.fast:
        parameters["XL_STOCK_ROUTE"] = stock / "main/dxx-redux-d2-headless-route"
    else:
        parameters.update(reference_parameters(reference))
    if any(not value.exists() for value in parameters.values()):
        raise ValueError("Required XL native fixture or executable missing")
    run(
        [
            "cmake",
            "-S",
            repo / "android/XL_compat",
            "-B",
            contracts,
            "-DBUILD_TESTING=ON",
            "-DCMAKE_BUILD_TYPE=RelWithDebInfo",
            "-DXL_FOG_GPU_TESTS=" + ("ON" if args.fast else "OFF"),
            *["-D" + key + "=" + str(value) for key, value in parameters.items()],
        ]
    )
    run(["cmake", "--build", contracts, "--parallel", "2"])
    listing = subprocess.check_output(
        ["ctest", "--test-dir", str(contracts), "--show-only=json-v1"], text=True, timeout=30
    )
    required = FAST_TESTS if args.fast else CORE_TESTS
    selected = required | (set() if args.fast else EXPERIMENTAL_TESTS)
    validate_registration(json.loads(listing), fast=args.fast)
    report = work / "original-ctest.xml"
    report.unlink(missing_ok=True)
    started = time.monotonic()
    result = run(
        [
            "ctest",
            "--test-dir",
            contracts,
            "--output-on-failure",
            "--no-tests=error",
            "--timeout",
            "180",
            "--parallel",
            "2" if args.fast else "1",
            "--output-junit",
            report,
            "-R",
            "^(" + "|".join(sorted(selected)) + ")$",
        ],
        check=False,
    )
    receipt = native_receipt(report, result.returncode, fast=args.fast)
    formats = format_results(contracts / "xl_inspect", stage, work, data)
    modern_count, source_count = (
        modern_results(contracts / "xl_inspect", contracts / "xl_layout_reference", stage, work)
        if args.fast
        else (0, 0)
    )
    verify(stage, modern=args.fast)
    receipt.update(
        required_tests=sorted(required),
        format_witnesses=formats + modern_count,
        modern_source_levels=source_count,
        test_seconds=round(time.monotonic() - started, 3),
    )
    validate_summary(receipt, fast=args.fast)
    (work / "original-status.json").write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n")
    print(json.dumps(receipt, sort_keys=True))
    print("PASS: required original XL native tests, source comparisons and explicit original-format outcomes")


if __name__ == "__main__":
    main()
