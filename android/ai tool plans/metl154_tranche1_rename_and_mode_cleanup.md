# Metl154 tranche 1: rename and mode cleanup

## Scope

This tranche implements Part A.1 and A.3 from
`metl154_postfix_cleanup_and_debug_harness.md`:

- rename metl154-specific naming to generic merged-wall naming where the
  symbols are still intended to exist after cleanup
- remove or downgrade dead experiment modes that are no longer part of the
  long-term debug harness
- fill in the tranche 1 research gaps while doing the work

Out of scope for this tranche:

- deleting hardcoded tracked seg/side/face lists in the render and OGL
  diagnostics
- moving Android-only code into `android/app/src/main/cpp/shared/`
- adding new launcher or in-game debug UI
- changing the cached-premerge rendering behavior

## Required validation

1. `android\run-code-quality.ps1 -Fix`
2. `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
3. Emulator smoke run if device availability permits

## Work items

- [x] Audit all `metl154` / `m154` symbol use in d1/, d2/, and android/
- [x] Answer A.1.R1: confirm where merged-wall route classification happens
- [x] Answer A.3.R1: confirm whether automation or tests depend on current
      experiment-mode names or numeric values
- [x] Rename surviving symbols, flags, and UI strings to generic merged-wall
      terminology
- [x] Remove dead experiment modes and update any mode-cycle plumbing
- [x] Update this file with findings and results

## Research notes

### A.1.R1: route classification site audit

Status: answered

- There are two distinct route decisions today, not one accidental duplicate.
- `d1/d2/main/render.c` first chooses the broad legacy texmerge versus
  alt-texmerge path via `GameArg.DbgAltTexMerge`, with an Android-only
  override for the old-merge control branch.
- `d1/d2/arch/ogl/ogl.c` then makes a second Android-only decision inside
  the alt-texmerge branch between cached-premerge (`merge_cached`) and the
  two-pass shader path (`gpu_two_pass`) based on overlay properties.
- Result for tranche 1: rename only the surviving public control surface and
  the live old-merge route branch. Do not try to collapse the two route
  decisions in this tranche.

### A.3.R1: automation and test dependency audit

Status: answered

- No files under `android/game_scripts/` reference `metl154_mode`,
  `metl154_experiment`, or `metl154_snapshot`.
- The only in-repo dependencies were the Android control surface itself:
  `VideoInfoOverlay.kt`, `jni_main.c`, `game_automate.cpp`, and
  `game_introspect.cpp`.
- Result for tranche 1: it is safe to rename the public Android debug flag
  names and introspection keys without breaking committed automation scripts.
- Additional decision: dead experiment modes are no longer surfaced through
  the in-game overlay, JNI setter, or automation setter. Compatibility
  numeric constants remain in the shared header only so tranche 2 can delete
  the old OGL branches without mixing that deletion into this rename pass.

## Status

- [x] Audit complete
- [x] Rename batch complete
- [x] Mode cleanup complete
- [x] Validation complete

## Implementation notes

- Renamed the Android-facing merged-wall control surface to generic names:
  `merged_wall_mode`, `merged_wall_experiment`, `merged_wall_snapshot`,
  plus `g_merged_wall_*` state in the shared header.
- Added temporary compatibility aliases in `debug_tex_overlay.h` so the deep
  metl154-specific diagnostics in d1/d2 keep compiling unchanged during
  tranche 1.
- Reduced the surfaced experiment control to two states only:
  `default` and `force_legacy_texmerge`.
- Updated the live old-merge route control in D1 and D2 `render.c` to use
  the new generic merged-wall experiment naming and a generic `[mwall_exp]`
  log tag.

## Validation result

- `android\run-code-quality.ps1 -Fix`: passed
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest`: passed
- `adb devices`: no available authorized emulator/device in this session
- Emulator smoke run: blocked by device availability, not by code errors
