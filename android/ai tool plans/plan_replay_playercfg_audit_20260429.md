# Replay PlayerCfg Audit 2026 04 29

## Plan

- [x] Enumerate PlayerCfg fields in D1 and D2 and locate their runtime consumers after replay checkpoint restore
- [x] Classify fields into simulation-relevant, presentation-only, and uncertain based on post-restore execution paths
- [x] Patch replay startup logging for the remaining gameplay-facing PlayerCfg fields after the confirmed AutoLeveling fix
- [x] Rebuild the touched desktop targets and rerun a focused replay validation

## Scope

- Audit `PlayerCfg` usage after checkpoint restore in D1 and D2 replay startup
- Focus on fields that can affect physics, controls, AI, timing, or RNG consumption during replay
- Ignore menu-only and launcher-only settings unless they cross into gameplay state during replay

## Findings

- Replay frames bypass the normal live-input config path: `input_demo_apply_replay_frame()` loads recorded `Controls` and `FrameTime`, and `ReadControlsReplayFrame()` only handles replay-safe follow-up work such as rear-view, automap open, and weapon/item actions
- Because of that ordering, live-input-only settings such as `ControlType`, key bindings, joystick deadzones, mouse sensitivity, and `maxFps` are not replay hazards for ordinary in-game frames
- Confirmed gameplay-facing `PlayerCfg` consumers after checkpoint restore are:
	- `AutoLeveling` in `object.c`, already fixed by preserving the restored `PF_LEVELLING` bit across `read_player_file()`
	- `PersistentDebris` in `collide.h`, `object.c`, and `fireball.c`, which changes debris lifetime, bounce behavior, and debris-slot pressure
	- weapon autoselect policy in `weapon.c` and `game.c`: `PrimaryOrder`, `SecondaryOrder`, `NoFireAutoselect`, `SelectAfterFire`, `CycleAutoselectOnly`, and `ClassicAutoselectWeapon`
	- D2-only `HeadlightActiveDefault` in `powerup.c`, which changes whether a headlight pickup immediately sets `PLAYER_FLAGS_HEADLIGHT_ON`
- `state.c` does not restore these `PlayerCfg` fields, so unlike `AutoLeveling` there is no current checkpoint-state source of truth to reconstruct them during replay startup

## Code Changes

- Extended the replay startup config line in `d1/main/inferno.c` and `d2/main/inferno.c` to log the remaining gameplay-facing `PlayerCfg` values path-independently
- Added compact primary/secondary weapon-order hashes so replay artifacts can expose weapon-order drift without dumping full arrays

## Validation

- `run-windows-build.ps1 -Target both` succeeded after the logging changes
- Replayed `android/temp_game_logs/d2_descent2_level1_20260429_074558.dximdemo` again and the host sandbox log now reports:
	- `auto_level=1`
	- `debris=0`
	- `headlight_default=0`
	- `autoselect=(nofire=0,after=0,cycle=0,classic=0)`
	- `order_hash=(0xa413d797,0xa413d797)`
- The same replay still ends with `Input demo replay result matched embedded trailer terminal-exit subset`

## Follow-up

- The remaining latent hazards are now visible in logs, but they are not restorable from the current checkpoint format
- If a future replay diverges on debris, weapon selection, or headlight pickup behavior, the next fix should be recording and restoring this small `PlayerCfg` subset in input-demo metadata rather than inferring it from host defaults