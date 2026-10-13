# S21 recommended-mod fuzz campaign

Status: completed 2026-10-12; timestamped progress notes below are historical. See the final completion audit and findings document for outcomes and the remaining open defect

Requested: run the planned eight-hour mod campaign on the attached S21 using the current completed build

- [x] Wait for build 24970 to finish and pin the completed bundle/APK, source revision and hashes
- [x] Prepare the isolated diagnostic app with automation enabled and the completed build's exact native binaries
- [x] Inventory all nine recommendations, texture resolution variants, retail/demo/Mac data and valid mission levels
- [x] Implement deterministic cases, durable journal/resume, bounded execution, artifact capture and failure replay/reduction
- [x] Validate campaign controls and run physical-device smoke coverage including controlled artifact-capture failure
- [x] Complete eight hours of serial baseline, singleton, pair, all-on, level and stateful coverage on the S21
- [x] Reproduce/minimize failures, report coverage and gaps, restore device power settings

Use `com.dxxredux.app.nsdtest` and dedicated file sets. Do not clear or modify the Play installation's saves. Do not edit `android/outstanding_bugs.md`. Native library hashes must match the completed internal build; the diagnostic launcher differs only as needed for application identity, signing and debug automation

Follow the existing overnight design. Mod disabling retains installed archives and rows. Expected unsupported extra-asset dependencies must be distinguished from crashes, bad recovery, hangs and lost saves. Preserve the originally requested campaign even if failures require investigation

## Preparation evidence

- Completed internal bundle and universal APK are version 24970, source `cb32b6a96`
- All 16 ARM64 native libraries are byte-identical between the completed bundle, official APK and the isolated diagnostic wrapper
- Build evidence: `android/temp/mod-fuzz-s21-build/`; source bundle SHA256 `e429dd566daac71a1931c2017696e0412d0185a50b68aa42f7f16ef969da0c52`
- S21 target: `R3CR40Q4XPK`, package `com.dxxredux.app.nsdtest`
- Seven independent fixture sets cover retail D1, D2, D1-in-D2, Vertigo, PC D1/D2 demos and Mac D1
- Normal level inventory: D1 27, D2 24, D1-in-D2 27, Vertigo 20, PC demos 7/3, Mac demo 3
- Seven offline controller checks passed, including interrupted journals, bounded timeout, evidence-before-recovery, bounded reduction and failed-history preservation on resume
- Scoped code quality and both automation catalogs passed
- Physical smoke: 19 cases, 18 passed, one non-crash texture-activation failure; all-on save toggles passed for retail D1, D2, D1-in-D2, Vertigo level 2, PC D2 demo and Mac D1
- Controlled failure reached the intended `current_level_num == 9999` assertion, captured a 736768-byte private evidence archive, and restored `stay_on_while_plugged_in` to 0
- PC D1 demo texture finding reduced to `d1-hires-256-textures-ktx2.dxa` alone: a fresh level renders replacements, but save -> disable -> resume -> enable -> resume leaves stock textures despite the pack remaining enabled and mounted. Gameplay/restore assertions pass; owner verification catches the missing replacements. Evidence: `android/temp/mod-fuzz-s21-repro/`

## Runner and evidence

`android/tests/test_mod_fuzz.ps1 -Serial R3CR40Q4XPK -Profile overnight -Hours 8 -Seed 20261011 -OutputDirectory <new-directory>` prepares local assets and launches the serial controller. `-Resume` retains completed cases and spends the remaining case-time budget. `-ReplayCase <case.json>` reproduces a materialized recipe in the selected inventory. The isolated app must already contain the intended native build

Each case retains its recipe, actual command/steps, automation result, logcat, introspection, installed manifest and diagnostic asset owners. Failures add app-private logs/crash reports/saves and exit information before force-stop. `journal.jsonl` is flushed at planned/started/completed boundaries. `systematic-plan.json`, `level-inventory.json` and `summary.json` record actual coverage and pending cases

Authored UUD fixtures run through the existing acceptance runner for the original mission and extra assets, with both high-resolution priorities and seven palettes. Existing save-toggle, corrupt-restore/menu-Quit and UUD2SP bank-consumption regressions run inside the campaign with explicit larger timeouts

The current native secret fixture exercises the first authored D2/Vertigo secret and revisits it. It does not enumerate every secret or native D1 secret; report that limitation explicitly rather than counting those levels as tested

## Active overnight run

Started 2026-10-12 at 00:26:43 PDT (07:26:43 UTC), Python PID 31016. Expected deadline is approximately 08:26 PDT. Output: `android/temp/mod-fuzz-s21-overnight-24970-20261012/`

`process.json` records the exact command and start time; `build-provenance.json` pins the completed APK/bundle and identical native libraries; `runner-hashes.json` pins the campaign implementation. The schedule contains 197 systematic cases before the randomized remainder. The desktop's AC sleep timeout is already disabled. The phone's original stay-awake setting is 0 and is restored by the runner's finalizer

The campaign is active, not complete. Verify the process, journal and aggregate results before making any completion claim. Preserve failures, perform necessary follow-up and produce the final coverage/gaps report after the deadline

Follow-ups from early case review: observe MF-001 for longer after resume; include its retail D1 128/256/512px reproductions in the findings. The current signature helper can include unrelated system-log `FAIL` strings, so identical controller assertions sometimes produce different signatures. Preserve the current run and group final failures by the actual controller/native cause, not merely raw signature equality; correct and test that helper for future runs

The signature helper has now been corrected on disk for future invocations, with nine offline controller checks passing and scoped code quality passing. The live PID keeps its original code, preserved as `runner-at-start.py` with the startup hash verified. `signature-review.json` demonstrates stable grouping of an actual noisy case/replay pair under the corrected helper. Native relative fault PCs remain significant when grouping crashes. Final analysis must use this corrected grouping without rewriting the live journal

Case 00019 timed out because the host force-stop fallback raced a progressing launcher handoff by six milliseconds. The shared helper now uses the existing bounded correlated wait for physical targets too, with host regression checks passing. The APK is unchanged; subsequent child runners load this helper. Evidence/provenance: `host-handoff-fix.json` and `test-helpers-before-handoff-fix.ps1`. Replay case 00019 after the main campaign and require it to complete before crediting that combination

Completion audit follow-up: the main campaign uses separate per-mode installed lists. Also exercise a shared retail file set with the D1 and D2 recommended packs installed together, game switches in both directions, and all-off/all-on resumes while archives remain installed. Include the Mac-only pack as installed but disabled on PC data. Check game-specific asset ownership and the launcher's separate D1-in-D2 namespace rather than assuming every mounted archive supplies assets to the current mission

The `shared` profile now prepares 16 follow-up cases across D1, D2, D1-in-D2 and Vertigo: all-on toggle/resume, switches to the other engine and back, priority reorder/resume, and all-off gameplay. All 13 archives remain installed, including the disabled Mac-only pack and alternate resolutions. It checks game-specific rendered texture and played sound ownership on both sides of each game switch, then verifies installed archives, enabled flags and mounted paths. Post-resume observation is ten seconds. Run this profile only after the active serial campaign, using a new output directory and the same native APK. Ten offline control checks and scoped code quality passed; physical shared-profile validation remains pending

At approximately 01:47 PDT, all four authored UUD variants and the three save-toggle/menu-recovery/UUD2SP regressions had passed. The live campaign entered pair coverage with 61 completed attempts: 52 passed, eight texture-owner assertion failures and one host-handoff timeout. These counts include reproduction attempts and are not final coverage totals

At 02:13 PDT, all 24 planned pairs and four reversed-priority cases had completed. Pair outcomes were 22 passes and two native D1 texture-reactivation failures (MF-001); D2, D1-in-D2 and Vertigo level 2 each passed all six pairs. All four reversed-priority cases passed. The live process then started `00085-level-d1-L1`. Across 89 completed attempts including replays, 78 passed, ten failed asset-owner assertions and one hit the already diagnosed host-handoff timeout. No native crash or ANR was classified. The level sweep and randomized remainder are still pending

At 02:34 PDT, all 27 normal native D1 level starts had passed with the recommended mods enabled and Game Logs off (`00085` through `00111`). D2 level 1 also passed and level 2 was active. The journal contained 117 completed attempts: 106 passes, the same ten MF-001 owner assertions and the one MF-002 host timeout. Latest reviewed battery evidence was USB-powered at 79% and 26.1 C. The live Python PID remained 31016; the eight-hour deadline is unchanged

At 02:52 PDT, all 24 normal native D2 level starts had passed (`00112` through `00135`). Each case's final `asset_trace.current_level_palette` confirms the actual loaded palette: four levels each for Group A, Water, Fire, Ice, Alien 1 and Alien 2. D1-in-D2 level 1 also passed, with level 2 active. Totals were 141 attempts, 130 passes, ten MF-001 assertions and one MF-002 host timeout, with no classified native crash or ANR. The remaining level sweeps, secret cases, randomized remainder and post-run follow-ups are still outstanding

At approximately 03:14 PDT, all 27 D1-in-D2 normal level starts had passed (`00136` through `00162`), and Vertigo level 1 was active. No additional failures appeared during that sweep. The latest reviewed captured battery status remained USB-powered at 79% with a 25.3 C battery temperature

At 03:41 PDT, the journal had no pending systematic IDs: all 197 scheduled systematic cases had run, with failures retained. All 111 normal-level starts passed (D1 27, D2 24, D1-in-D2 27, Vertigo 20, PC demos 7/3, Mac D1 3). Vertigo's 20 snapshots covered all six D2 palettes (Water 8, Group A 2, Fire 5, Alien 1 3, Ice 1, Alien 2 1). Both first-secret entry/exit/revisit cases passed; the native fixture verifies the negative level ID and retained robot-state marker on revisit. Other secret levels remain untested

The live process entered the seeded randomized remainder; `00198-random-vertigo-L7` passed and `00199-random-d2-L5` was active. Totals were 203 completed attempts including replays: 192 passes, ten MF-001 assertions and one MF-002 host timeout. The eight-hour campaign is still running, and the post-run replay, longer MF-001 observation, shared-file-set profile, final coverage report and power-restoration verification remain required

At approximately 03:42 PDT, random case `00199` found MF-003: Counterstrike level 5 with only UUD2SP enabled rejects the subsequent disabled-mod restore as requiring UUD2 textures. Automatic replay reproduced it. The captured save has an out-of-range texture ID 910 and a stock level-source hash; root cause is not yet established. After the main campaign, run its prepared `no-mod-replay.json` from a new output inventory to distinguish stock-level save validation from mod dependence. The future controller now preserves save/restore transitions when the last mod is removed, with eleven offline controls and scoped formatting passing; the running controller and pinned APK are unchanged. Subsequent random cases `00200` through `00202` passed

At approximately 03:55 PDT, `00210-random-d2-L11` and its replay found MF-004: a UUD2TP/high-resolution-sound toggle restore rejected mission robot/weapon definitions. Supporting read-only evidence found a Vertigo secret companion in the Counterstrike save set after replay, but the primary had already been cleared by the next case. A controlled same-script Vertigo-secret -> fresh Counterstrike -> save/restore follow-up is required to separate native cross-mission state from partial automation cleanup. The future failure capture now includes secret companions and lettered save slots, with twelve offline checks and scoped quality passing. The running campaign remains unchanged and subsequent Vertigo random cases passed

At 07:56 PDT, detached follow-up PID 12492 was launched from `android/temp/mod-fuzz-s21-followups-24970-20261012/followup.py`. It holds the original campaign process handle, verifies its creation time, and waits for normal completion before running eleven serial tasks from `plan.json`. The queue has a two-hour launch budget, per-child timeouts, durable status/results, and power restoration. Do not start another device test while either process is active. This replaces conversation-driven polling and survives another daybreak interruption

New engine code addresses MF-004 by clearing active secret files and the checkpoint companion cache before a fresh Android D2 or D1-in-D2 game, preserving per-slot companions. Both engines compiled; the extended native fixture is queued after original-APK reproductions. The original installed APK remains pinned to build 24970. Thirteen offline controller checks pass, including single-script prefix composition without between-mission cleanup. See the findings document for the verified Counterstrike/Vertigo save pair and remaining limitations

Completion update, 08:28 PDT: the main process reached its deadline and exited normally with 451 attempts (380 passed, 70 assertions, one diagnosed host timeout; no classified native crash/ANR). All systematic IDs and 13 archives are covered. The native fixture passed all 7/20 D1/D2 cases. A helper-import error prevented the first ten app follow-up jobs from executing; those attempts are preserved, the wrapper is corrected and offline-validated, and PID 23932 now runs those ten jobs serially. The findings document and `completion-audit.json` distinguish completed work from pending app results. Do not start a competing device test while that process is active


## Final completion audit

Completed 2026-10-12. The main eight-hour budget produced 451 attempts (380 passes, 70 known assertions and one diagnosed host timeout), all 197 systematic cases and 219 random cases, with 35 reproductions. All 111 normal levels and all 13 archives are covered; no native crash or ANR was classified. Nine targeted original-APK cases completed, all 16 shared-list cases passed, and both engine fixes passed four full-app replays. Native fixtures passed 27 mod-save and 34 renderer cases; the old renderer libraries failed the new transient-metadata regression as expected

All detached jobs have exited. Live device checks verified the original diagnostic APK SHA-256 and version 24970, the original stay-awake value 0, and unchanged tested source hashes. Final results and explicit coverage gaps are in `mod_fuzz_24970_findings_20261012.md`; durable aggregate evidence is in the follow-up directory `completion-audit.json` and `final-device-verification.json`. MF-003 remains a documented stock-save range-check defect; finishing the testing goal does not claim that defect is fixed
