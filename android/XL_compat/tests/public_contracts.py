"""Build pinned XL source adapters and require every asset-free contract."""

import argparse
import json
from pathlib import Path
import subprocess
import time
import xml.etree.ElementTree as ET

from reference_sources import reference_parameters, reference_sources, require_source_arguments

PUBLIC_TESTS = {
    "xl_animation_phase_contract",
    "xl_companion_lighting_contract",
    "xl_dynamic_color_reference",
    "xl_effect_decode_reference",
    "xl_effect_payloads",
    "xl_face_point_reference",
    "xl_fixed_cross_reference",
    "xl_fog_reference",
    "xl_generator_assignment_reference",
    "xl_geometry_attenuation_reference",
    "xl_hog_contract",
    "xl_hog_reference",
    "xl_hxm_geometry_reference",
    "xl_inspect_stream_contract",
    "xl_inventory_contract",
    "xl_level_text_contract",
    "xl_level_text_reference",
    "xl_light_index_reference",
    "xl_lighting_calculation",
    "xl_lighting_reference_arithmetic",
    "xl_magnitude_reference",
    "xl_mission_labels",
    "xl_modern_layout_contract",
    "xl_modern_layout_reference",
    "xl_nearest_lights_reference",
    "xl_object_trigger_decode_reference",
    "xl_player_resource_reference",
    "xl_projectile_history_contract",
    "xl_projectile_reference",
    "xl_projection_reference",
    "xl_scoped_texture_contract",
    "xl_segment_flags_reference",
    "xl_side_center_reference",
    "xl_static_pixels_contract",
    "xl_trigger_capacity_reference",
    "xl_trigger_flag_reference",
    "xl_wall_capacity_reference",
    "xl_wave_contract",
}


def validate_registration(listing, gpu=False):
    required = PUBLIC_TESTS | ({"xl_fog_gpu"} if gpu else set())
    tests = listing["tests"]
    if len(tests) != len(required) or {test["name"] for test in tests} != required:
        raise ValueError("Public XL CTests differ from the required asset-free selection")
    require_source_arguments(tests)


def require_result(path, returncode, gpu=False):
    required = PUBLIC_TESTS | ({"xl_fog_gpu"} if gpu else set())
    cases = ET.parse(path).getroot().findall(".//testcase")
    if (
        returncode != 0
        or len(cases) != len(required)
        or {case.get("name") for case in cases} != required
        or any(case.get("status") != "run" for case in cases)
        or any(case.find(tag) is not None for case in cases for tag in ("skipped", "failure", "error"))
    ):
        raise ValueError("Required public XL contracts were omitted, skipped or failed")
    return len(required)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--reference-root", type=Path, help="Existing source tree; consumed files remain SHA-pinned")
    parser.add_argument("--gpu", action="store_true", help="Require the Linux surfaceless GPU controls")
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[3]
    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=True)
    (work / "public-status.json").unlink(missing_ok=True)
    started = time.monotonic()

    def run(command, check=True):
        return subprocess.run([str(value) for value in command], cwd=repo, check=check)

    reference = reference_sources(work, args.reference_root, run)
    build = work / "build"
    run(
        [
            "cmake",
            "-S",
            repo / "android/XL_compat",
            "-B",
            build,
            "-DBUILD_TESTING=ON",
            "-DCMAKE_BUILD_TYPE=Debug",
            "-DXL_FOG_GPU_TESTS=" + ("ON" if args.gpu else "OFF"),
            *["-D" + key + "=" + str(value) for key, value in reference_parameters(reference).items()],
        ]
    )
    run(["cmake", "--build", build, "--config", "Debug", "--parallel", "2"])
    listing = json.loads(
        subprocess.check_output(["ctest", "--test-dir", str(build), "-C", "Debug", "--show-only=json-v1"], text=True)
    )
    validate_registration(listing, gpu=args.gpu)
    report = work / "public-ctest.xml"
    report.unlink(missing_ok=True)
    tests_started = time.monotonic()
    result = run(
        [
            "ctest",
            "--test-dir",
            build,
            "-C",
            "Debug",
            "--output-on-failure",
            "--no-tests=error",
            "--timeout",
            "60",
            "--parallel",
            "2",
            "--output-junit",
            report,
        ],
        check=False,
    )
    passed = require_result(report, result.returncode, gpu=args.gpu)
    receipt = {
        "required_passed": passed,
        "required_tests": sorted(PUBLIC_TESTS | ({"xl_fog_gpu"} if args.gpu else set())),
        "test_seconds": round(time.monotonic() - tests_started, 3),
        "total_seconds": round(time.monotonic() - started, 3),
    }
    (work / "public-status.json").write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n")
    print(f"PASS: {passed} required public XL contracts and source comparisons")


if __name__ == "__main__":
    main()
