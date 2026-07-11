# Plan: D1/D2 PhysFS Upstream Drift Cleanup 2026-07-11

## Goal
- Remove residual non-Android drift from the original D1/D2 PhysFS source and header files while preserving the newly extracted Android initialization hooks

## Steps
- [x] Compare each residual hunk with the merge base and current `upstream/main`
- [x] Classify intentional branch behavior separately from missed upstream fixes
- [x] Apply only current upstream behavior to the non-Android hunks in both games
- [x] Confirm the remaining four-file diff contains only intentional Android hooks or documented branch behavior
- [x] Run scoped quality checks and refresh aggregate D1/D2 metrics
- [x] Build both Windows targets and all configured Android ABI/game combinations
- [x] Run focused D1/D2 filesystem launch coverage and record results
- [x] Record the next extraction candidate from the refreshed `newmenu.c` survey

## Guardrails
- Preserve `physfsx_android_init_search_paths` calls and all Android mount behavior from the completed extraction
- Do not copy an upstream hunk blindly if it conflicts with an intentional branch compatibility change
- Keep D1 and D2 synchronized where upstream behavior is equivalent
- Do not mix the `newmenu.c` extraction itself into this small upstream-sync tranche

## Baseline
- Aggregate D1/D2 diff: 343 files, +50207/-3909 against `upstream/main`
- `d1/include/physfsx.h`: +4/-8
- `d1/misc/physfsx.c`: +12/-7
- `d2/include/physfsx.h`: +4/-8
- `d2/misc/physfsx.c`: +22/-8

## Audit findings
- Upstream commit `917704d3` adds a correct Windows drive-qualified absolute-path fast path; it is compatible and mirrored in both games
- Upstream commit `069eaff5` intends to return an unterminated final text line, but its inner-loop `break` appends repeated EOF bytes before returning
- Preserve that useful upstream intent with an immediate successful return after terminating the partial line
- The D2-only `CON_NORMAL` search-path diagnostic came from resolved texture debugging and can return to upstream `CON_DEBUG`

## Post-cleanup metric
- Aggregate D1/D2 diff: 343 files, +50206/-3880 against `upstream/main`
- This tranche removes 30 lines of churn while retaining a four-line-per-header correction for the upstream unterminated-line defect
- Residual PhysFS diff is limited to Android initialization hooks plus the corrected partial-EOF return

## Validation
- Scoped quality and whitespace checks passed
- `run-windows-build.ps1 -Target both` passed, including normal and headless metadata targets, with no PhysFS warnings
- Forced `:app:externalNativeBuildDebug --rerun-tasks` passed for D1 and D2 on `arm64-v8a`, `armeabi-v7a`, and `x86_64`, with no PhysFS warnings
- `:app:assembleDebug` passed and produced the refreshed APK
- The rebuilt D1 headless analyzer loaded the real unterminated `serenity.msn` descriptor and resolved its final `serenity.rdl` level entry
- `test_engine_prefs_unified.json5` passed 45 of 45 steps in D1 and D2 on the rebuilt APK

## Next candidate
- Keep `newmenu.c` deferred: its current churn is unchanged after three completed slices, and larger extractions still require private-layout or callback-heavy coupling
- Prefer the paired Android HMP memory parser in `d1/misc/hmp.c` and `d2/misc/hmp.c`
- Current HMP growth is approximately +126 and +125 lines, with an estimated 220 to 235 combined-line reduction through a narrow shared parser/adapter
