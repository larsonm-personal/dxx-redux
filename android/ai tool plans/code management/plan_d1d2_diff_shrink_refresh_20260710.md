# Plan: D1/D2 Diff Shrink Refresh 2026-07-10

## Goal
- Restart the upstream-diff cleanup using the current branch state, focusing on files inherited from `upstream/main` and leaving branch-added files alone unless they are duplicated sink files that should clearly move under `android/`

## Prior guidance retained
- Measure against `upstream/main` with `android/helpers/diff_vs_upstream.ps1`
- Optimize upstream-original `d1/` and `d2/` files, not the raw branch line total
- Move Android-owned or branch-owned duplicated helper bodies to `android/app/src/main/cpp/shared/`
- Leave thin, mirrored D1 and D2 hooks at engine-owned call sites
- Pass game state and callbacks across the shared boundary instead of making shared code reach into private translation-unit state
- Do not consolidate inherited D1/D2 duplication or move genuinely cross-platform feature implementations merely to improve the metric
- Keep branch-added files mostly out of scope
- Use small, independently validated slices and record before/after churn for each slice

## Sources reviewed
- `android/ai tool plans/reusable/cleanup.md`
- `android/ai tool plans/code management/d1d2_diff_shrink_study.md`
- `android/ai tool plans/code management/d1d2_shrink_phase2_remaining_and_phase3_candidates.md`
- `android/ai tool plans/code management/d1d2_shrink_phase3_execution_plan.md`
- `android/ai tool plans/code management/plan_d1d2_diff_refresh_biggest_changes_20260519.md`
- `android/ai tool plans/code management/plan_d1d2_diff_shrink_next_20260519.md`
- `android/ai tool plans/code management/cleanup_tranche_20260529_survey.md`
- `android/ai tool plans/networking/cleanup_net_udp_extract.md`
- `android/ai tool plans/networking/cleanup_coop_files_move.md`

## Current baseline
- Branch: `cmake`
- Worktree was clean before this plan was added
- Diff base: `upstream/main`
- Current D1/D2 report: 343 files, 154 D1 files, 189 D2 files, `+51740/-3922`
- Name-status inventory from `upstream/main...HEAD`: 34 added files and 304 modified files under D1/D2, plus non-`A`/`M` entries
- The raw total is not directly comparable to the May baselines because many features landed afterward and upstream also moved
- Current artifacts:
  - `temp/d1d2_diff_numstat.txt`
  - `temp/d1d2_diff_sorted.txt`
  - `temp/d1d2_diff_summary.txt`
  - `temp/d1d2_current_name_status.txt`

## Scope
- First implementation target: `d1/main/newmenu.c` and `d2/main/newmenu.c`
- Shared destination: a focused UI helper under `android/app/src/main/cpp/shared/`
- Required build wiring only where needed
- Out of scope for the first slice: menu behavior changes, broad formatting, input-demo semantics, guidebot route behavior, packet layouts, save formats, and OGL upload-path refactoring

## Why `newmenu.c` is first
- It is the largest clearly paired upstream-original target after excluding branch-added files and feature-specific D2-only work
- Current churn is `d1/main/newmenu.c +1550/-128` and `d2/main/newmenu.c +1530/-124`
- The Android helper families are near-identical between games
- The menu structs are already consumed by shared introspection and automation code, so the shared boundary is established
- The code separates into independently testable families: readable-tiny wrapping, scaled drawing, touch hit testing/logging, reorder handling, and listbox scaling/touch handling

## First slice
- [x] Compare the D1 and D2 readable-tiny wrapping blocks exactly and identify the smallest safe shared interface
- [x] Extract only the readable-tiny text preparation and cleanup helper family, leaving allocation ownership and engine call sites explicit
- [x] Keep D1 and D2 call sites mirrored and avoid changing layout calculations
- [x] Run scoped code quality on the new shared files without broad-formatting `newmenu.c`
- [x] Build both Android native targets and both Windows host targets
- [x] Run focused menu automation for the wrapped readable-tiny path
- [x] Record the before/after `newmenu.c` churn and update this plan

## First slice outcome
- Added `android_newmenu_text_wrap.{c,h}` as the single implementation of trimming, word wrapping, wrapped-item allocation, and wrapped-item cleanup
- Kept text measurement as a callback so font and renderer state remain owned by each `newmenu.c`
- Kept the private `newmenu` ownership swap, readable-tiny policy, layout calculations, and the intentionally different D1/D2 fullscreen palette preparation local
- D1 and D2 now use identical one-call wrapping hooks and one-call cleanup hooks
- Initial Android build found the missing direct `strutil.h` dependency for `d_strdup`; that was fixed before validation continued

## First slice validation
- Scoped `android/run-code-quality.ps1 -Fix` passed for both new shared files
- `:app:externalNativeBuildDebug` passed for all configured Android ABIs and both games
- `run-windows-build.ps1 -Target both` passed, including the normal and headless metadata targets
- The rebuilt APK was installed on `emulator-5554`
- `test_readable_tiny_help_d2.json5` passed and asserted the readable-tiny flag, wrapped-text flag, original item count, expanded item count, and direct scaled rendering
- `git diff --check` passed with only existing CRLF normalization warnings

## First slice metrics
- Overall D1/D2 report moved from `+51740/-3922` to `+51481/-3909`
- `d1/main/newmenu.c` moved from `+1549/-130` to `+1413/-118`
- `d2/main/newmenu.c` moved from `+1529/-126` to `+1404/-125`
- Combined `newmenu.c` churn fell by 274 lines; the shared implementation is 149 lines plus a 21-line header and is outside the upstream-owned D1/D2 metric

## Follow-up candidates
1. Remaining `newmenu.c` touch/listbox helper family
2. Reassess the private direct-render orchestration only if a later shared menu adapter creates a natural boundary
3. `state.c`, split by feature ownership before extraction
4. Continue the established `net_udp.c` shared-helper series
5. `playsave.c` launcher bridge isolation while preserving the C file-format source of truth
6. `multi.c` coop helper extraction where the code is Android-scoped

## Second slice outcome: shared menu-scale primitives
- Moved cropped source-region blitting, rounded coordinate scaling, and `newmenu_item` geometry scaling into the existing `android_menu_scale.{c,h}` implementation
- Reused the shared coordinate helper from both the newmenu direct-render path and the listbox direct-render path
- Kept `android_newmenu_draw_direct_contents` and `android_newmenu_draw_scaled` local because they copy the private `newmenu` struct and call the local static draw routine; extracting those functions now would require a callback/adapter layer larger than the duplicated code it replaces
- D1 and D2 each lost another 40 inserted lines from `newmenu.c`

## Second slice validation
- Scoped `android/run-code-quality.ps1 -Fix` passed for `android_menu_scale.{c,h}`
- `:app:externalNativeBuildDebug` passed for all configured Android ABIs and both games
- `run-windows-build.ps1 -Target both` passed, including normal and headless metadata targets
- The rebuilt APK installed successfully on `emulator-5554`
- `test_menu_scale_d2.json5` passed all 16 steps
- `test_readable_tiny_help_d2.json5` passed all 24 steps
- `git diff --check` passed with only existing CRLF normalization warnings

## Second slice metrics
- Overall D1/D2 report moved from `+51481/-3909` to `+51401/-3909`
- `d1/main/newmenu.c` moved from `+1413/-118` to `+1373/-118`
- `d2/main/newmenu.c` moved from `+1404/-125` to `+1364/-125`
- Combined `newmenu.c` churn fell by another 80 lines, for 354 lines removed across the first two slices

## Targets not chosen first
- `d2/main/input_demo_hooks.c`, `d1/main/input_demo_hooks.c`, `d2/main/d1_in_d2.c`, `d2/main/d1_save_translate.c`, `d2/main/dxa_metadata_patch.cpp`, and `d2/main/d1_custom.c` are branch-added and therefore do not drive this tranche
- `d2/main/escort.c` has the largest modified-file churn, but much of it is substantive route and coop behavior rather than a low-risk duplicated Android helper body
- `d1/d2 arch/ogl/ogl.c` remain large, but prior plans intentionally left the tightly interleaved ETC2/KTX2 upload paths in place after the safer helper extractions landed
- `rand.c`, SDL glue, MVE changes, and required CMake wiring remain non-targets under the prior guidance

## Validation ladder
1. `android/helpers/stop-stale-formatters.ps1`
2. Scoped `android/run-code-quality.ps1 -Fix -Paths ...`
3. Android native build for D1 and D2
4. `run-windows-build.ps1 -Target both`
5. Focused menu automation for both games where the behavior exists
6. `android/helpers/diff_vs_upstream.ps1 -Top 40`
7. `git diff --check`
