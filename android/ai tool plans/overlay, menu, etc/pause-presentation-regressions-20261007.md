# Pause presentation regressions

## Plan

1. Trace the new pause snapshot through touch visibility and survey transient screens, automap, native menus, graphics confirmation, and co-op transitions
2. Separate pause-warning presentation from simulation pause, give transient screens priority over overlay visibility, and restore the native screen-mode check
3. Extend policy and existing D1/D2 pause integration coverage, exposing the actual warning visibility through introspection
4. Run scoped formatting, JVM tests, native builds, and emulator regression checks while preserving unrelated worktree changes

## Findings

- The unconditional simulation-paused visibility fallback bypasses death/endlevel suppression and enables controls during briefings and score screens
- All screen-advance kinds (death, fly-out, movie, briefing, level-complete, post-level) need the same exclusion, including while not yet ready for Skip/Continue
- Single-player automap intentionally stops simulation but retains its own touch navigation
- Native menus and graphics confirmation already explain their modal state; background and co-op presentation waits should not add a gameplay pause warning
- The pause snapshot lost the SCREEN_GAME condition formerly used by nativeIsInGame

## Implementation and verification

- Transient-screen and intro state now override every touch-overlay visibility source, including pause, controller menus, and settings-tray grace
- Pause-warning presentation is separate from simulation pause: automap, transient screens, native menus, graphics confirmation, background, and co-op waits suppress it; explicit user and settings pauses retain it
- Restored SCREEN_GAME admission in the native snapshot used by the in-game query
- Added actual warning visibility to UI introspection and extended the existing pause integration runner
- Scoped formatting/lint passed (`temp/pause_presentation_format.log`)
- Isolated x86_64 APK and both native engines built; JVM suite passed with 1,148 tests, zero failures/errors, one skipped (`temp/pause_presentation_build_retry.log`)
- Both automation catalog checks passed (`temp/pause_presentation_catalog.log`, `temp/pause_presentation_suite_catalog.log`)
- D1 and D2 emulator integration passed: explicit pauses, save-menu handoff, graphics confirmation, single-player automap, post-level summary, fly-out, score review, briefing, and restored gameplay controls (`temp/pause_presentation_integration.log`, `android/temp/pause-state-20261007-185952/results.json`)
- Movie and death hooks were surveyed in both engines (including D1-in-D2 briefings); all use the same transient-screen exclusion covered by policy tests, without a separate movie/death emulator run
- Build warnings were confined to unrelated extraction/HUD code; no new warnings in changed files
