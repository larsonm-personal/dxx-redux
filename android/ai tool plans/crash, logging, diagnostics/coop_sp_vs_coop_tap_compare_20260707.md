# Coop single-player vs coop tap compare

## Goal
- Compare the attached single-player and coop tap diagnostics for the level 7 texture issue
- Decide whether the mismatch is level/wall state, tap target selection, or render/texture state
- Implement a targeted fix if the logs identify one

## Plan
- [done] Extract the single-player and coop tap `side_state`, `side_tex_state`, texture, and render lines
- [done] Compare the raw side/wall/door/effect state and signatures
- [done] Trace the code path responsible for the first meaningful divergence
- [done] Patch the smallest behavior change or diagnostic improvement justified by the logs
- [done] Validate with scoped quality checks and relevant builds

## Notes
- The selected fallback face is identical in SP and coop: `seg=169 side=0 tmap1=73 tmap2=0x132`, with matching raw state, UV signature, source bitmap hashes, and merged GL texture hash
- Both taps report `status=no_crosshair_face`; the selected face bbox is above the canvas center in both runs
- The current comparison does not support "coop changed this side to the wrong texture"; it supports "the probe selected a fallback face that is not under the tap"
- Added all-rendered-face tap diagnostics so the next SP/coop tap pair logs the actual center-hit or nearest non-merged `g3_draw_tmap` face candidates, plus side/texture/GPU readback state for the best candidate

## Validation
- `.\android\run-code-quality.ps1 -Fix -Paths android\app\src\main\cpp\shared\merged_wall_debug.c "android\ai tool plans\crash, logging, diagnostics\coop_sp_vs_coop_tap_compare_20260707.md"`
- `$env:JAVA_HOME='C:\local\jdk-21'; $env:Path="$env:JAVA_HOME\bin;$env:Path"; .\gradlew.bat :app:externalNativeBuildDebug` from `android\`
