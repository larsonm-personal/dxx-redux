# Settings presets

- [x] Reuse the existing pilot switches and FOV control, preserving unrelated worktree edits
- [x] Add extensible Original Descent and Restore Defaults definitions with confirmation lists
- [x] Keep the existing edit-then-Save workflow; confirmation stages values for customization
- [x] Include counts, map cheats, boss health, base FOV (90 degrees), and Guidebot line
- [x] Preserve the existing cockpit, auto-level, and homing reset values in Restore Defaults
- [x] Verify with scoped formatting, Android compilation and relevant preference/config tests

Future candidates: nearest-player line, rewind support, shield/ammo warnings, texture/HUD filtering

Validation: Android debug Kotlin compilation and 42 settings/config/admin tests passed, including a both-game FOV persistence and manual override regression. Scoped formatting passed. No native engine changes were needed. Device UI was not exercised.

The existing pilot-file prerequisite remains: create/select a pilot before saving. Confirm stages the preset and Save writes it; Original Descent changes the requested settings; the follow-up extends that list as described below.

## Follow-up: server defaults, rewind, filtering

- [x] Add new-server Coop QoL default, rewind support, texture/HUD filtering to presets and editable controls
- [x] Apply rewind preference to native runtime at launch/resume and hide configured rewind buttons/segments and overflow entries without deleting bindings
- [x] Verify both-game graphics persistence, overlay availability, and server-default storage with relevant tests and Android compilation

Nearest-player arrows share the existing server Coop QoL option with Guidebot and warp. Change only the saved new-server default, preserving active/resumable sessions and the separate local visual toggle. Current graphics defaults are nearest texture sampling and HUD filtering on.

Follow-up validation: Android debug Kotlin compilation and 66 targeted tests passed (graphics config, admin menu, remaining touch actions, rewind preference policy). Verified the create-server dialog reads the same shared preference constant used by the preset. Scoped formatting and diff whitespace checks passed. No device UI run.
Android x86_64 CMake debug build passed for both engines, including the existing rewind JNI entry points.
