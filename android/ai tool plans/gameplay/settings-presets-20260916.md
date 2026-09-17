# Settings presets

- [x] Reuse the existing pilot switches and FOV control, preserving unrelated worktree edits
- [x] Add extensible Original Descent and Restore Defaults definitions with confirmation lists
- [x] Apply presets immediately on confirmation; keep individual settings editable
- [x] Include counts, map cheats, boss health, base FOV (90 degrees), and Guidebot line
- [x] Preserve the existing cockpit, auto-level, and homing reset values in Restore Defaults
- [x] Verify with scoped formatting, Android compilation and relevant preference/config tests

Future candidates: nearest-player line, rewind support, shield/ammo warnings, texture/HUD filtering

Validation: Android debug Kotlin compilation and 42 settings/config/admin tests passed, including a both-game FOV persistence and manual override regression. Scoped formatting passed. No native engine changes were needed. Device UI was not exercised.

The existing pilot-file prerequisite remains: create/select a pilot before saving. Confirm now applies the preset immediately (updated by the final follow-up below); Original Descent changes the requested settings; the follow-up extends that list as described below.

## Follow-up: server defaults, rewind, filtering

- [x] Add new-server Coop QoL default, rewind support, texture/HUD filtering to presets and editable controls
- [x] Apply rewind preference to native runtime at launch/resume and hide configured rewind buttons/segments and overflow entries without deleting bindings
- [x] Verify both-game graphics persistence, overlay availability, and server-default storage with relevant tests and Android compilation

Nearest-player arrows share the existing server Coop QoL option with Guidebot and warp. Change only the saved new-server default, preserving active/resumable sessions and the separate local visual toggle. Current graphics defaults are nearest texture sampling and HUD filtering on.

Follow-up validation: Android debug Kotlin compilation and 66 targeted tests passed (graphics config, admin menu, remaining touch actions, rewind preference policy). Verified the create-server dialog reads the same shared preference constant used by the preset. Scoped formatting and diff whitespace checks passed. No device UI run.
Android x86_64 CMake debug build passed for both engines, including the existing rewind JNI entry points.

## Immediate confirmation and compact preview

- [x] Share the existing save operation between Save and preset Confirm; show failures in the dialog
- [x] Use the requested descriptions and compact rows with labeled green/red pills
- [x] Enable original homing in both presets and Android loader/new-pilot defaults, preserving saved values and desktop defaults
- [x] Run scoped formatting, Kotlin checks and Android native build

Immediate-confirm validation: scoped formatting and whitespace checks passed; Android x86_64 native engines and Kotlin compiled; all 66 targeted settings/config/admin/touch tests passed. The native rebuild emitted existing warnings in unrelated source files. No device UI run.
