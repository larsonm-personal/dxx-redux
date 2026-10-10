# Shared transient reactor countdown control proposal

Date: 2026-10-09
Status: diagnosis proposal only; no implementation or canonical finding admission
Parent: plan_gq2_0127_automation_20261009.md

## Problem and measured scope

The branch adds the same21line transient countdown state and three-function API to both native cntrlcen.c files. Current D1lines128-148 and D2lines151-171 are identical; LF body SHA256b8ad679d06507b01d15c03df38e384f8fca4316c9b094d816113209d22b8f707. Frozen base7877ad30d05887b8e19869ed4c50075e41e2f88e to headb4997ac4115a3b2ac6d6cf0b8e1459675a7744ef has D1hunk1/new128-148 and D2hunk2/new150-170, each21added0removed. Expected removal42 inherited added lines/two hunks; no saving applied

Both cntrlcen.c files exist in originalfb555eec75e1ed12c8348805ab335afb4c721b06; original symbol inventory contains none of these pause API names. Original D1blobcdb4fe9fa6dbb0ab23e353cb33e7092ef375bee2 and D2blob522a2b786e85f137153cbaba78aa42afb9565093. Paired main/CMakeLists.txt files also exist in original. This is extraction of branch-added behavior, not broad deduplication of original engine logic

## Proposed implementation for a later code tranche

- Add portable android/app/src/main/cpp/shared/reactor_countdown_control.c owning Reactor_countdown_paused, reactor_countdown_is_active, reactor_countdown_set_paused and reactor_countdown_reset_pause
- Compile this source once per game in the paired main/CMakeLists.txt portable source lists before if(ANDROID), alongside matcen_mode.c and multi_gameplay_options.c. Use each game's native headers and globals; do not share state between game libraries or restrict the source to Android
- Remove only the duplicated21line additions from paired cntrlcen.c. Keep Countdown_timer, Countdown_seconds_left, Control_center_destroyed and native countdown simulation ownership in the engine
- Preserve existing cntrlcen.h declarations and all callers. Preserve native pause early-return and reset placements, handmade comments, save/config formats and desktop behavior
- Copy the exact admission expression and fixed-point seconds calculation. Keep positive remaining_time validation, Boolean normalization and return values identical. Numeric-domain improvements belong to their existing separate owners

## Required behavior and later validation

- Both games must link exactly one pause-state definition on Android and desktop. Build paired native targets on both platforms and check new warnings
- Inactive, endlevel and nonpositive-timer requests must reject without mutation. Active pause/resume must preserve the supplied exact fixed-point timer and native displayed-seconds calculation
- Paused countdown frames must leave timer and SIM RNG/rocking/audio/terminal flash/death unchanged. Resume must continue normally. Cosmetic effects outside countdown simulation retain native policy
- Preserve clearing at countdown start, level initialization, native restore initialization and host migration. Cooperative save metadata explicitly stores reactor_paused and reapplies its saved value after native reset; preserve that behavior and validate paused and running cooperative saves separately. Pilot/cheat persistence remains outside this extraction
- Preserve cooperative host authority, connected-sender checks, exact timer synchronization, duplicate-state behavior and competitive rejection. Verify host-local/client request, nonhost/malformed rejection and migration for both games
- Verify automation setter active/inactive cases and map menu flow. Current maintained countdown travel fixture does not directly exercise reactor_countdown_paused; identify or extend meaningful direct coverage in the future implementation tranche
- Run relevant existing tests and paired integration checks only when implementation is authorized. Historical map_reactor_countdown_pause_20260814.md validation claims are context, not acceptance evidence for this extraction
- Recompute base/head/original native diff metrics after implementation, confirming42 inherited added lines removed without accidental native edits. Shared-file/CMake additions must be reported separately from native saving

## Diagnosis acceptance still pending

- [x] Read and compare exact paired current21line blocks
- [x] Read complete paired frozen cntrlcen.c diffs and original symbol attribution
- [x] Read portable paired CMake source registration contexts
- [x] Read whole earlier feature plan in bounded untruncated ranges
- [x] Finish packet/session/reset/caller ownership review for parent0127
- [x] Reconcile existing canonical owners using broader aliases and bounded actual rows; no duplicate finding admission
- [x] Publish parent immutable diagnosis report and audit before assigning final canonical finding/remediation/ranking entries

Provisional severity P3 maintenance/diff minimization. No runtime defect asserted. No final score or ID assigned; zero applied saving. Parent evidence bindings record actual reads and hashes. Whole original sources, current CMake files and native lifecycle remain partial


## Maintained packet fixture coverage reconciliation

Actual test_multi_gameplay_options.c whole230lines and CMake registration100-120 read on2026-10-09. Target links shared multi_gameplay_options.c and matcen_mode.c. Its countdown functions are mocks: active is a fixture Boolean, setter directly assigns timer/pause and returns changed-state, not the native setter return/admission contract. Thus existing captured packet roundtrips cannot validate proposed extraction's native seconds rounding, active/endlevel rejection, paused SIM/RNG behavior or restore/reset lifecycle. Plan meaningful real-owner coverage in later implementation; do not count these packet mocks as that evidence

Existing GQF-0257FIXED/GQR-0243DONE is the earlier paired139line multiplayer packet extraction, not the paired21line countdown domain state/functions proposed here. Preserve that closure and its saving; final canonical duplicate reconciliation remains pending


## Canonical diagnosis admission

Parent0127 allfive diagnosis gates independently audited complete. GQF-0277OPEN/GQR-0263TODO admitted57MEDIUM-HIGH12/21/7/10/7;42expected inherited lines/two hunks,zero appliedsaving. ReportSHA2567c0b6144523db3a67b7f39ea9b039a5120254da9f3aa806163e1fac1525b74e5. Implementation and runtime validation remain pending; no code changes
