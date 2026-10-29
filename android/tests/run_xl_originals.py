"""Required original XL native tests and strict original-texture JVM adapter."""

import argparse
import json
import os
import shutil
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET

sys.path.insert(0, str(Path(__file__).resolve().parent))
from xl_private_assets import verify

REFERENCE_COMMIT = "2576a98b5de310e3ac7c9777df2677d1627b8bae"
REFERENCE_URL = "https://github.com/arbruijn/d2x-xl.git"

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


# Unfinished campaign acceptance: execution is required, success is not qualification.
EXPERIMENTAL_TESTS = {"xl_level_two_keys"}


def require_junit(path, names):
    root = ET.parse(path).getroot()
    cases = root.findall(".//testcase")
    found = [case.get("name") for case in cases]
    if len(found) != len(set(found)) or set(found) != set(names):
        raise ValueError("Required XL tests were omitted or duplicated")
    if any(case.find(tag) is not None for case in cases for tag in ("skipped", "failure", "error")):
        raise ValueError("Required XL tests did not all pass")


def native_receipt(path, returncode):
    """Require established passes and explicit, non-skipped experimental results."""
    cases = ET.parse(path).getroot().findall(".//testcase")
    names = [case.get("name") for case in cases]
    if len(names) != len(set(names)) or set(names) != CORE_TESTS | EXPERIMENTAL_TESTS:
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
        if failed and (
            failures[0].get("message") != "Failed"
            or (case.findtext("system-out") or "")
            .splitlines()
            .count("XL_EXPERIMENTAL_ACCEPTANCE: xl_level_two_keys route_incomplete")
            != 1
        ):
            raise ValueError("Original XL failure is not classified campaign acceptance")
        if name in CORE_TESTS and failed:
            raise ValueError("Required original XL regression failed")
        if name in EXPERIMENTAL_TESTS:
            experimental[name] = "failed" if failed else "passed"
    expected_code = 8 if "failed" in experimental.values() else 0
    if returncode != expected_code:
        raise ValueError("CTest exit status disagrees with original XL receipt")
    return {"required_passed": len(CORE_TESTS), "experimental": experimental, "level_two_completion_qualified": False}


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
        elif (
            run.returncode != 1
            or inventory["status"] != "rejected"
            or not inventory["reason"].startswith(
                ("unsupported level version ", "unsupported level/game version pair ")
            )
        ):
            raise ValueError("Unsupported XL format was not explicitly rejected")


def reference_sources(work, provided, run):
    """Supply the source tree required to register and execute pinned comparisons."""
    reference = provided.resolve() if provided else work / "reference-source"
    if not provided:
        if not reference.exists():
            run(["git", "init", reference])
            # Existing source oracles pin CRLF working-tree bytes on every host
            run(["git", "-C", reference, "config", "core.autocrlf", "true"])
            run(["git", "-C", reference, "fetch", "--depth=1", REFERENCE_URL, REFERENCE_COMMIT])
            run(["git", "-C", reference, "checkout", "--detach", REFERENCE_COMMIT])
        actual = subprocess.check_output(
            ["git", "-C", str(reference), "rev-parse", "HEAD"], text=True, timeout=30
        ).strip()
        if actual != REFERENCE_COMMIT:
            raise ValueError("Private XL reference checkout differs from the required commit")
    if not (reference / "objects/objectio.cpp").is_file():
        raise ValueError("Private XL source comparisons require the pinned reference tree")
    return reference


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--jvm-only", action="store_true")
    parser.add_argument(
        "--reference-root", type=Path, help="Existing source tree; consumed files are SHA-pinned by CMake"
    )
    args = parser.parse_args()
    repo, work = args.repo.resolve(), args.work.resolve()
    stage = repo / "game_data/retail-ci/xl"
    data = verify(stage)
    work.mkdir(parents=True, exist_ok=True)

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
            "dxx-redux-d2-headless-route",
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
        "XL_STOCK_ROUTE": stock / "main/dxx-redux-d2-headless-route",
        "XL_REFERENCE_GEOMETRY_ROOT": reference,
    }
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
            *["-D" + key + "=" + str(value) for key, value in parameters.items()],
        ]
    )
    run(["cmake", "--build", contracts, "--parallel", "2"])
    listing = subprocess.check_output(
        ["ctest", "--test-dir", str(contracts), "--show-only=json-v1"], text=True, timeout=30
    )
    if not (CORE_TESTS | EXPERIMENTAL_TESTS) <= {test["name"] for test in json.loads(listing)["tests"]}:
        raise ValueError("Required original XL CTests were not registered")
    report = work / "original-ctest.xml"
    report.unlink(missing_ok=True)
    (work / "original-status.json").unlink(missing_ok=True)
    result = run(
        [
            "ctest",
            "--test-dir",
            contracts,
            "--output-on-failure",
            "--no-tests=error",
            "--timeout",
            "180",
            "--output-junit",
            report,
            "-R",
            "^(" + "|".join(sorted(CORE_TESTS | EXPERIMENTAL_TESTS)) + ")$",
        ],
        check=False,
    )
    receipt = native_receipt(report, result.returncode)
    (work / "original-status.json").write_text(json.dumps(receipt, sort_keys=True) + "\n")
    print(json.dumps(receipt, sort_keys=True))
    format_results(contracts / "xl_inspect", stage, work, data)
    verify(stage)
    print("PASS: required original XL native tests and nine version-pair outcomes")


if __name__ == "__main__":
    main()
