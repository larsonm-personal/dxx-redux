#!/usr/bin/env python3
"""Asset-free checks for the bounded ordinary-input developer probe."""

from pathlib import Path

root = Path(__file__).resolve().parents[2]
probe = (root / "android/XL_compat/runtime/pumo_player_test.h").read_text()
stock_controls = (root / "d2/main/kconfig.c").read_text()
stock_fire = (root / "d2/main/gamecntl.c").read_text()

assert "Controls = {};" in probe
assert "double thrust = 0, sideways = 0, heading = 0, pitch = 0, bank = 0;" in probe
assert "bool firing = false, firing_secondary = false, secondary_press_pending = false;" in probe
assert "Controls.pitch_time = static_cast<fix>(pitch * (FrameTime / 2));" in probe
assert "Controls.pitch_time = FrameTime/2;" in stock_controls
assert "Controls.pitch_time = -FrameTime/2;" in stock_controls
assert "Controls.heading_time = static_cast<fix>(heading * FrameTime);" in probe
assert "Controls.bank_time = static_cast<fix>(bank * FrameTime);" in probe
assert "Controls.fire_secondary_state = firing_secondary;" in probe
assert "Controls.fire_secondary_count = secondary_press_pending ? 1 : 0;" in probe
assert "if (pressed && !firing_secondary) secondary_press_pending = true;" in probe
assert "do_weapon_select(slot, 1);" in probe
assert "Controls.fire_secondary_state || Controls.fire_secondary_count" in stock_fire

start = probe.index("StartNewGame(initial_level);")
resume = probe.index('if (plan.contains("resume_checkpoint"))', start)
tick = probe.index("const auto tick = [&]")
steps = probe.index('for (const auto &step : plan.at("steps"))')
quick_load = probe.index('require(state_restore_all_sub(file, 0), "native player checkpoint load failed");')
for point, stage in (
    (start, "after_start"),
    (tick, "before_frame"),
    (steps, "before_step"),
    (quick_load, "after_quick_load"),
):
    assert f'require_ordinary_secondary_input("{stage}");' in probe[point : point + 250]
assert (
    'require_ordinary_secondary_input("after_resume");'
    in probe[resume : probe.index('if (plan.contains("track_objects"))', resume)]
)
assert "if (!extended_input_plan) return;" in probe
for field, read in (
    ("game_mode", "Game_mode"),
    ("d1_gameplay", "d1_in_d2_use_d1_gameplay()"),
    ("newdemo_state", "Newdemo_state"),
    ("recorder_active", "input_demo_recorder_is_active()"),
    ("replay_loaded", "input_demo_replay_is_loaded()"),
):
    assert f"{field} = {read};" in probe
    assert f'" {field}="' in probe
assert "game_mode == GM_NORMAL && !d1_gameplay && newdemo_state == ND_STATE_NORMAL" in probe
assert "!recorder_active && !replay_loaded" in probe
assert "ordinary_without_route ? route_confirmation_get_summary()->status : -1" in probe
assert "#if defined(__ANDROID__) || defined(DXX_GUIDEBOT_ROUTE_PLANNER)" in probe
assert "route_status, ROUTE_CONFIRMATION_IDLE, ROUTE_CONFIRMATION_UNSUPPORTED, full_route_runner" in probe
route_source = (root / "android/app/src/main/cpp/shared/route_confirmation.cpp").read_text()
assert "#if defined(__ANDROID__) || defined(DXX_GUIDEBOT_ROUTE_PLANNER)" in route_source
assert "summary.status = ROUTE_CONFIRMATION_UNSUPPORTED;" in route_source
route_header = (root / "android/app/src/main/cpp/shared/route_confirmation.h").read_text()
assert "ROUTE_CONFIRMATION_IDLE = 0" in route_header
assert "ROUTE_CONFIRMATION_UNSUPPORTED = 6" in route_header
assert 'ordinary_without_route ? std::to_string(route_status) : "unobserved"' in probe
assert "require(false, detail.c_str());" in probe
assert (
    'require(pumo_ordinary_input_validation::selection_released(firing, firing_secondary, secondary_press_pending, false), "native weapon selection'
    in probe
)
assert (
    'require(pumo_ordinary_input_validation::selection_released(firing, firing_secondary, secondary_press_pending, true), "native secondary selection'
    in probe
)
assert 'require(!secondary_press_pending, "native checkpoint requires a consumed secondary press")' in probe
assert "secondary_press_pending = false;" in probe[quick_load : quick_load + 250]
for forbidden in (
    "player.secondary_ammo[slot] =",
    "Global_missile_firing_count =",
    "robotfiringsuspended =",
    "obj_create(",
):
    assert forbidden not in probe
print("ordinary input source checks passed")
