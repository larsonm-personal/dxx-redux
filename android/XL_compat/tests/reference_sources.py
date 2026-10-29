"""Pinned public XL source checkout and required comparison executables."""

from pathlib import Path
import subprocess

REFERENCE_COMMIT = "2576a98b5de310e3ac7c9777df2677d1627b8bae"
REFERENCE_URL = "https://github.com/arbruijn/d2x-xl.git"

# These CMake scripts can run without their optional source oracle. The required
# owners require the built oracle in the registered command, not just a test name
SOURCE_ARGUMENTS = {
    name: ("REFERENCE",)
    for name in (
        "xl_object_trigger_decode_reference",
        "xl_generator_assignment_reference",
        "xl_effect_decode_reference",
        "xl_modern_runtime_layouts",
        "xl_wall_teleports",
        "xl_wall_trigger_chains",
        "xl_wall_trigger_scheduler",
        "xl_object_events",
        "xl_object_chain_delays",
        "xl_speed_boosts",
        "xl_spawn_points",
        "xl_blocked_segments",
        "xl_close_doors",
        "xl_changing_lights",
        "xl_flickers",
        "xl_cloaks",
        "xl_projectiles",
        "xl_object_capacity",
        "xl_keyed_closed_walls",
        "xl_doorways",
        "xl_repair_centers",
        "xl_water_actors",
        "xl_generator_assignment_runtime",
        "xl_boss_spawning",
        "xl_boss_transport",
        "xl_equipment_generators",
        "xl_modern_equipment_25",
        "xl_modern_generators_25",
        "xl_modern_equipment_27",
        "xl_modern_generators_27",
        "xl_legacy_repair",
        "xl_format34_native",
        "xl_hxm_native",
    )
}
SOURCE_ARGUMENTS.update(
    xl_lava_volumes=("REFERENCE", "MOTION_REFERENCE"),
    xl_observatory_descriptor=("REFERENCE_GATES",),
    xl_pog_native=("REFERENCE", "GPU"),
)


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
            raise ValueError("XL reference checkout differs from the required commit")
    if not (reference / "objects/objectio.cpp").is_file():
        raise ValueError("XL source comparisons require the pinned reference tree")
    return reference


def reference_parameters(reference):
    return {
        "XL_REFERENCE_GEOMETRY_ROOT": reference,
        "XL_REFERENCE_COLOR_ROOT": reference,
        "XL_REFERENCE_RENDERLIB": reference / "render/renderlib.cpp",
        "XL_REFERENCE_LIGHT": reference / "lighting/light.cpp",
        "XL_REFERENCE_LIGHT_PRECALC": reference / "lighting/lightprecalc.cpp",
        "XL_REFERENCE_OGL_COLOR": reference / "ogl/ogl_color.cpp",
        "XL_REFERENCE_SEGMENT_FLAGS": reference / "main/findsegment.cpp",
        "XL_REFERENCE_WEAPON_CONTROL": reference / "weapons/controlweapon.cpp",
    }


def require_source_arguments(tests):
    for test in tests:
        for argument in SOURCE_ARGUMENTS.get(test["name"], ()):
            prefix = "-D" + argument + "="
            paths = [value[len(prefix) :] for value in test.get("command", []) if value.startswith(prefix)]
            if len(paths) != 1 or not Path(paths[0]).is_file():
                raise ValueError("Required XL source oracle missing: " + test["name"])
