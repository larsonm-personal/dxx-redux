# Bounded public first-play campaign

1. Add an opt-in fixed selection using the existing nightly planner and hosted lanes
2. First run existing JVM, shareware autosave/lifecycle and LAN transport recovery owners
3. Add disposable-emulator-only public import/engine-transition and two-peer gameplay variants
4. Require fresh correlated evidence, independent review and negative controls before publication

Keep retail defaults and trusted asset restrictions unchanged. Provider imports exercise the production post-picker path; the public import owner also opens and cancels DocumentsUI. Actual system-picker file selection, download cancellation, unsuppressed graphics onboarding and phone behavior require separate evidence. The existing runner clears saves between cold-import phases; the mission phase below keeps its normal save/restore and resume in one invocation. No merges or cmake pushes.

## Generated public mission extension

Extend only `master:test_demo_import_public_first_play` (`test_demo_import.ps1 -PublicFirstPlay`) with one fixed owned phase, `test_public_first_play_mission.jsonc`, after native D2 and picker cancellation. Keep the existing 900-second owner timeout/estimate and 2520-second android-single campaign selection. The prior owner passed in 181.3 seconds in run 38863180109 at `3a13a888feadf05e7d87619287c7e2022649c07f`; this is headroom evidence, not a measurement of the new phase. Require the extended hosted run to complete within the unchanged limit before claiming it fits.

1. Validate the already pinned public D2 demo HOG and extract only `d2leva-1.sl2` (75440 bytes, SHA-256 `133f34dff976ad7e5a84e2c4a16301f89bd833c954e9903c899613d6f7299302`)
2. Generate `fpplay.zip` at runtime containing `fpplay.mn2` and `fpplay.hog`; the descriptor maps level one to the unique HOG member `fpplay1.rl2`, whose bytes are unchanged
3. Stage only provider source bytes and invoke the existing production `import_picked_uris` callback with a URI array; mission ZIP import is immediate and has no base-installer confirmation dialog
4. Require a fresh production import log, exact installed ZIP hash, and exactly one enabled managed D2 mission owner named Public First Play; preserve the fixture hashes and publication snapshot
5. Tap Launch Descent 2, observe the Public First Play mission picker entry, select it normally, choose level one and Trainee, and assert live custom ownership
6. Fire one concussion missile (7 to 6), use the ordinary Escape menu to save, fire again (6 to 5), and use the ordinary load menu to restore six
7. Use `enter_launcher` to create the synchronous automation auto-exit save, verify its mission filename `fpplay` and level one, then tap the actual Load Last Save button and require six missiles, the same custom owner, callsign, difficulty, and living unpaused gameplay
8. Delete the owned mission, clear the now-empty mod bookkeeping, and recheck the exact pinned base inventory; remove runtime ZIP/provider sources and never commit extracted assets

Source checks: `SetupActivity.kt` routes mission ZIPs directly through `ModManager.importMissionZip`; `SetupAutomationApi.kt` exposes exact mod ownership; `mission.c` enumerates custom missions beside the demo, mounts the custom HOG, and reads its descriptor level mapping. `piggy.c` permits demo HAM/PIG fallback, and `gamesave.c` parses the original level version from its bytes. `gamecntl.c`, `state.c`, and `newmenu.c` supply the ordinary save/load menu route. `ResumeSaveBridge.kt` supplies saved mission identity; `game_automate.cpp` explicitly performs the synchronous auto-exit save.

This is synthetic packaging, import, selection, and save/resume coverage using a playable public level, not Obsidian, Vertigo, or retail mission qualification. The internal level title remains Ahayweh Gate. There is no direct selected-level-filename introspection: the unique descriptor/HOG-member mapping, pinned member and imported ZIP hashes, active `mod/fpplay.zip` owner, saved `fpplay` mission name, and restored nondefault ammunition bind the level identity. Missing custom bytes cannot fall back to the base demo member because `fpplay1.rl2` exists only in the generated HOG. No production telemetry is added.

## Graphics lifecycle failure evidence

Preserve the existing same-process and rollback assertions. Before cleanup, the exact public D1/D2 graphics lifecycle owner captures bounded main/system/crash logcat tails, original PID query diagnostics, app exit information, lifecycle focus/power state, graphics record, native fatal marker and at most two app-owned tombstones. Only fixed public-owner filenames are collected; no retail files, device grants or system tombstone access. The earlier D2 PID mismatch remains unclassified because its failure-window logs were overwritten. Review the shared recovery-before-park race fix independently from this evidence change and require controlled old-ordering failure plus both game owners.
