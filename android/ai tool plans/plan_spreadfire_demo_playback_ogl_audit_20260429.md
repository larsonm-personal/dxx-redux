# Spreadfire Demo Playback OGL Audit 2026 04 29

## Plan

- [x] Reconfirm the concrete spreadfire render ownership point from prior replay probes
- [x] Audit the D2 blob draw path and the OGL texture upload or cache path it uses
- [x] Check recent OGL-side changes or regressions that could make `sprdblob` sample black or skip drawing
- [x] Validate the best local hypothesis with a narrow build or test, then document findings

## Scope

- Focus on D2 desktop OpenGL paths first because the Windows build should be the stable baseline
- Prefer the exact `draw_object_blob()` to OGL upload path over broad renderer exploration
- Only touch code after a falsifiable local hypothesis and a cheap discriminating check are clear

## Findings

- Replay probes still show player-owned spreadfire reaching `draw_object_blob()` with valid on-screen coordinates and valid `sprdblob` CPU bitmap data
- The D2 desktop OGL blob draw path itself is stable: `draw_object_blob()` pages in the bitmap, `Laser_render()` routes blob weapons through `draw_object_blob()`, and `g3_draw_bitmap_full()` still binds and draws the sprite with the long-standing stock upload path
- No recent Windows-side change stood out in the sprite draw or stock upload history, but prior replay-only tint, blend, and depth overrides also failed to change the screenshots, so the renderer disappearance is still unresolved and still points at the OGL GPU texture or draw-state path rather than the CPU-side bitmap data
- A separate replay-startup bug was confirmed in the checkpoint path: `state_restore_all_sub()` can leave replay under `callsign=<empty>`, and the follow-up `new_player_config(); read_player_file();` then loads the nameless `.plr/.plx` fallback instead of the selected local pilot state
- That empty-callsign fallback is the wrong abstraction boundary to fix with demo metadata. Demos should not serialize pure visual toggles like `PlayerCfg.AlphaEffects`; replay should preserve local pilot state for those host-side rendering preferences
- The current host build tree itself already has `alphaeffects=0` in its nameless `.plx`, so the wrapper sandbox was not introducing a uniquely replay-only alpha setting relative to that host environment

## Fix

- Removed the experimental `alpha_effects` demo-metadata extension
- Preserved the pre-replay local callsign across checkpoint restore in both D1 and D2 so replay only reloads a player file when a real local pilot is available, instead of silently falling back to the nameless `.plr/.plx`

## Validation

- `run-windows-build.ps1 -Target both`
- `buildd1\maths\test_input_demo_recorder.exe`
- `buildd1\maths\test_input_demo_replay.exe`
- `buildd2\maths\test_input_demo_recorder.exe`
- `buildd2\maths\test_input_demo_replay.exe`
- All four focused recorder and replay tests passed after the replay-startup fix and demo-format revert