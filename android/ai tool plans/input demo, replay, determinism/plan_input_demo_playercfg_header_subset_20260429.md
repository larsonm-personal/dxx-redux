# Input Demo PlayerCfg Header Subset 2026 04 29

## Plan

- [x] Locate the current input-demo header metadata schema and recorder/replay load-save path
- [x] Define the minimal PlayerCfg subset needed for replay determinism and thread it through the shared metadata format
- [x] Apply the subset during replay startup in D1 and D2 without breaking existing desktop behavior
- [x] Rebuild affected targets and run a focused replay validation on the known D2 checkpoint-start artifact

## Scope

- Record only the replay-relevant `PlayerCfg` fields identified in the audit
- Prefer shared input-demo code under `android/app/src/main/cpp/shared/`
- Keep format changes additive and narrow

## Completed

- Added an optional `player_cfg` object to the input-demo header metadata without changing the metadata version
- Threaded the replay-relevant subset through shared recorder settings, demo metadata serialization, replay loading, and replay getters
- Recorded D1 fields: `AutoLeveling`, `PersistentDebris`, `NoFireAutoselect`, `CycleAutoselectOnly`, `SelectAfterFire`, `ClassicAutoselectWeapon`, `PrimaryOrder`, `SecondaryOrder`
- Recorded D2 fields: the same subset plus `HeadlightActiveDefault`
- Applied the recorded subset during replay startup in both `new_level` and `save_checkpoint` flows when present
- Kept backward compatibility for existing demos by falling back to the pre-existing replay-start behavior when `player_cfg` metadata is absent
- Extended the shared recorder and replay tests to verify `player_cfg` round trips in both D1 and D2 builds

## Validation

- `android\run-code-quality.ps1 -Fix -Paths ...` passed on the touched files after correcting the switch spelling to `-Fix`
- `run-windows-build.ps1 -Target both` passed before and after the formatting pass
- `buildd1\maths\test_input_demo_fixture.exe`, `test_input_demo_recorder.exe`, and `test_input_demo_replay.exe` all passed after the final formatting pass
- `buildd2\maths\test_input_demo_fixture.exe`, `test_input_demo_recorder.exe`, and `test_input_demo_replay.exe` all passed after the final formatting pass
- `android\tests\run_input_demo_replay.ps1 -DemoPath .\android\temp_game_logs\d2_descent2_level1_20260429_074558.dximdemo -Game d2 -Mode accelerated -TimeoutSeconds 120` completed with `RESULT: PASS`