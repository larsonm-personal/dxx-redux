#!/usr/bin/env python3
"""Regression guards for persisted cockpit-mode validation."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def main() -> None:
    for game in ("d1", "d2"):
        game_source = read(f"{game}/main/game.c")
        playsave_source = read(f"{game}/main/playsave.c")
        predicate = game_source.split("int cockpit_mode_is_persistable", 1)[1].split("}", 1)[0]
        for mode in ("CM_FULL_COCKPIT", "CM_STATUS_BAR", "CM_FULL_SCREEN"):
            assert mode in predicate, f"{game} predicate omits {mode}"
        assert playsave_source.count("cockpit_mode_is_persistable") >= 3, (
            f"{game} pilot readers/writers do not consistently validate cockpit mode"
        )

    jni = read("android/app/src/main/cpp/android_pilot_prefs.cpp")
    writer = jni.split("JNI_FUNC(nativeWriteEnginePrefs)", 1)[1].split("JNI_FUNC(nativeReadVisualPrefs)", 1)[0]
    validation = writer.index("cockpit_mode_is_persistable")
    assert validation < writer.index("GetStringUTFChars")
    assert validation < writer.index("append_pilots(")
    assert validation < writer.index("patch_pilots(targets)")

    kotlin = read("android/app/src/main/java/com/dxxredux/app/NativePilotPreferences.kt")
    assert "mode == 0 || mode == 2 || mode == 3" in kotlin
    for name in ("writeEnginePrefs", "writeEnginePrefsToAll", "writeEngineAndHomingPrefsToAll"):
        body = kotlin.split(f"    fun {name}(", 1)[1].split("\n    fun ", 1)[0]
        guard = body.index("if (!validCockpitMode(cockpitMode)) return -1")
        publication = body.index("nativeWriteEnginePrefs") if "nativeWriteEnginePrefs" in body else body.index("writeEnginePrefs(")
        assert guard < publication, f"{name} publishes before validating cockpit mode"

    print("cockpit mode validation guards passed")


if __name__ == "__main__":
    main()
