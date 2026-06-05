# Extract In-game Reorder Helper Plan - 2026-06-04

## Goal
Move a meaningful part of the Android-only in-game autoselect reorder implementation out of duplicated `d1/` and `d2/` code into `android/app/src/main/cpp/shared`.

## Scope

- Keep desktop behavior untouched.
- Keep the existing local `newmenu.c` adapters for:
  - row hit-testing, because it depends on private menu layout helpers
  - item swapping, because it directly mutates `newmenu_item`
  - scroll visibility, because it uses private scroll fields
- Share the Android reorder state machine:
  - state struct
  - initialization
  - button/touch press tracking
  - grab/drop transitions
  - hold threshold polling

## Work Items

- [x] Add this plan.
- [x] Add shared `android_menu_reorder.h`.
- [x] Replace duplicated reorder state fields in D1/D2 with the shared state struct.
- [x] Replace duplicated grab/drop/hold/poll logic in D1/D2 with shared helper calls.
- [x] Run scoped quality/build/test checks.
- [x] Update status.

## Verification

- [x] `android/run-code-quality.ps1 -Fix -Paths @('android/app/src/main/cpp/shared/android_menu_reorder.h','android/ai tool plans/overlay, menu, etc/plan_extract_reorder_helper_20260604.md')`
- [x] `android/gradlew.bat :app:assembleDebug`
- [x] `android/helpers/run_test.ps1 -ScriptName test_autoselect_crash_unified.json5 -Install -Game d2 -TimeoutSeconds 300`
