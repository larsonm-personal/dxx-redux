#!/usr/bin/env python3
"""Regression guards for bounded control-center trigger links."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def main() -> None:
    for game in ("d1", "d2"):
        control = read(f"{game}/main/cntrlcen.c")
        validator = control.split("int control_center_triggers_are_valid", 1)[1].split("}", 1)[0]
        assert "cct->num_links < 0" in validator
        assert "cct->num_links > MAX_CONTROLCEN_LINKS" in validator
        assert "cct->seg[i] > highest_segment_index" in validator
        assert "cct->side[i] >= MAX_SIDES_PER_SEGMENT" in validator
        reader = control.split("int control_center_triggers_read_n", 1)[1].split(
            "void control_center_triggers_swap", 1
        )[0]
        assert "control_center_triggers_are_valid" in reader
        assert "PHYSFS_read(fp, &value, sizeof(value), 1) != 1" in reader
        assert "PHYSFS_eof(fp)" not in reader
        assert "return 0" in reader
        assert "control_center_triggers_are_valid(&ControlCenterTriggers" in read(
            f"{game}/main/newdemo.c"
        )
        assert "if (!control_center_triggers_read_n" in read(f"{game}/main/gamesave.c")
        assert "if (!control_center_triggers_read_n_swap" in read(f"{game}/main/state.c")

    adapter = read("android/app/src/main/cpp/shared/secretarea.c")
    assert "control_center_triggers_are_valid(&ControlCenterTriggers" in adapter
    headless = read("android/app/src/main/cpp/headless/headless_metadata_dump_main.cpp")
    assert "control_center_triggers_are_valid(&ControlCenterTriggers" in headless
    print("control center trigger validation guards passed")


if __name__ == "__main__":
    main()
