# Pumo phone loading and restore feedback

Preserve the preceding uncommitted metadata-browser fixes and all user saves.
Investigate supplied debuglog_20261102_110656.txt and
crash_error_restore_1793646880264_15103_1.txt before modifying behavior.

1. Attribute mission selection and base-reset stalls, optimize proven redundant
   work without weakening asset identity, and show native loading progress
2. Diagnose XL fog depth-transfer rejection and make its failure dialog readable
   and touch-dismissable without bypassing required hazard rendering
3. Reconcile original mission descriptor/readme level counts and verified playtest
   scope with phone GPU limitations
4. Diagnose the D2X save rejection at Guidebot runtime; request the original save
   if needed, preserve it, and validate the production writer/reader contract
5. Run scoped native/Android tests, build and owned-device integration serially;
   record tested limits and cleanup without publishing or committing

Initial evidence: Pumo HOG fingerprint costs 12.121 seconds for 196505200 bytes;
asset baseline resets cost roughly 5-6 seconds. Hub rejects scene-depth transfer.
Save version 47 rejects Guidebot runtime at offset 342713 (runtime start 313933),
then returns to menu after another asset reset. No native crash stack was captured.

## Changes and evidence

- D1/D2 mission loading now owns a scoped progress overlay, including descriptor
  preflight, asset resets and archive fingerprinting. Unknown-length stages animate
  on the Android UI thread; archive reads report byte-based progress. The archive
  phase cannot auto-hide the overlay before the enclosing load finishes. Returning
  to base assets and new-game XL resource/graphics checks also report progress
- Android debug SHA clients compile optimized while retaining strict floating-point
  flags and full byte verification. All clients need the setting because linked
  inline SHA implementations can otherwise come from an unoptimized translation
  unit. On the same owned emulator, 4931752-byte asset hashes changed from about
  497 ms to 31 ms; this is not a measured phone timing
- Fog errors use bounded lines and include actual depth values. The production
  numeric rejection is preserved, including rejection of non-finite values. Added
  a failure-only direct-sampling control: the emulator returns 0 after scene-depth
  copying, but 0.625 after directly clearing the depth texture. Both 16-bit and
  24-bit transfer probes fail. Adding a color attachment did not fix this and was
  reverted. The phone's Adreno 830 remains unqualified; no fog bypass was added
- Found a concrete older-save regression: XL raised MAX_TRIGGERS from 100 to 254
  without changing the GuideBot save version. Decode the two exact framed lengths,
  zero the absent extended trigger flags, and retain rejection of malformed frames
  and truncated data. Added field-level diagnostics for other failures. User says
  the failing player.sg8 is not exported, so this is not an exact-save recovery claim
- The supplied archive's PM Readme.txt explicitly says v1.1 is a 12-level release.
  Its original descriptor has 14 entries including pm-hub.rl2 and mvend.rl2. The
  menu matches the package, not a 30-plus-level campaign. Prior hub/PMC checks were
  on a host-GPU emulator and do not establish physical-phone fog support

## Validation

- Extended real-level GuideBot save continuity passes both current and 100-trigger
  framed layouts, non-applying validation, applying restore, and truncation
  rejection; both routing modes and commands, twice on D2 levels 1 and 11
- 5 native CTest targets passed: upstream compatibility and input-demo fixture,
  recorder, replay and result. Asset-verification Python suite: 19 tests, one skipped
- 3 loading-overlay JVM tests passed; Android x86_64 debug APK assembled
- Extended fog rejection automation passed all 17 steps, checking progress updates,
  hidden overlay at the warning, positive OK-button bounds, dismissal, and stock
  mission start. This uses keyboard dismissal; physical touch acceptance is untested
- D1 and D2 native game targets compile. Broad desktop all-target builds encounter
  an unrelated test_game_automate_input missing SDL_types.h include configuration;
  the focused D2 game and GuideBot test targets build successfully
- Scoped formatter/lint and whitespace checks pass. No regeneration or commits

Evidence: temp/pumo-phone-loading logs, temp/xl-fog-driver-rejection receipts,
and android/temp/guidebot_original_navigation/run_d269460d60f74db688755310e172b761.
The owned DxxTraversal20261102 emulator uses serial emulator-5556. Test runners
back up profiles and restore them. One ADB failure interrupted restoration after
a passing test: the intact tar was copied to profile-recovery.tar, restored, and
all hashes matched recovered.sha256. Later protected runs restored normally.
The final APK passed the 17-step run in fog-final-test.log. Final before/after
profile hashes match, the device backup was removed by the runner, and the owned
emulator was gracefully stopped. The host recovery tar remains available.

## Remaining validation

Physical-phone fog rendering is not fixed by these changes. Recheck the short
warning and numeric diagnostics on that GPU before choosing a replacement depth
capture path; preserve hazard rendering. Export the failing save to establish
whether the 100-trigger regression explains this particular file. Check actual
phone mission timings, touch dismissal, and ordinary save resume. Preserve the
original save and published playtest bundles. Existing metadata-browser edits
remain separate uncommitted work in this same worktree.
