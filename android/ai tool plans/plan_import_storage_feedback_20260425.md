# Plan: Imported Storage Feedback Followup 2026-04-25

Investigate launcher regressions and UX gaps found during Shield TV testing
after adding relocatable imported-file storage.

## Feedback Items

1. GOG installer import should avoid copying the `.exe` to app tmp storage
   when extraction can read the SAF file in place.
2. Survey every launcher copy/import/move path and add determinate progress
   where practical, including installer staging and imported-storage moves.
3. Storage Inspector should show both internal app files and external imported
   files, tag each item by location, allow row selection, and open an item menu
   with full path, purpose, close, and confirmed delete.
4. Stale orphan files in app `tmp/` should be cleaned on startup and reported
   via the existing startup popup flow.
5. D2 GOG CD audio import should include the `.gog/.inst` pair by default and
   make extraction progress honest for the large audio image.
6. Diagnose and fix the native crash while scanning game files for MIDI
   playback in the launcher.

## Investigation Steps

- [x] Read the provided native tombstone and identify the crashing stack.
- [x] Trace GOG installer selection/staging/extraction and determine whether
      native extraction can use `/proc/self/fd/<fd>` for SAF inputs.
- [x] Trace GOG CD-audio registration after extraction with relocatable
      imported roots.
- [x] Survey copy operations in launcher Kotlin code and record missing
      progress UI/callbacks.
- [x] Inspect Storage Inspector data model and plan internal/external combined
      listing with item actions.
- [x] Inspect startup popup path and tmp cleanup hooks.
- [x] Implement low-risk fixes found during the trace, then run unit/build/
      lint validation.

## Notes

- Keep d1/d2 source untouched unless native evidence requires it.
- Prefer Kotlin launcher fixes and JNI fd-path bridge additions over broad
  engine changes.
- Any formatter run must use `android\run-code-quality.ps1 -Fix` and be
  allowed to finish.
- Fixed this tranche: GOG `.exe` import reads through a native duplicated fd;
   non-exe installer staging shows copy progress; stored Inno files report
   chunked extraction progress; GOG Redbook playlist paths work with external
   imported roots; MIDI HMP duration scanning frees with `d_free`; tmp cleanup
   reports stale file removal on startup; Storage Inspector lists internal and
   active external imported files with selectable rows and delete confirmation.
- Copy-progress survey follow-up remains for SOW extraction, ZIP/7z extraction,
   custom audio copy/staging, file-set import copy, crash log export, and any
   export/share copies outside the GOG/imported-storage paths fixed here.
- Validation completed: `:app:testDebugUnitTest --tests
   com.dxxredux.app.ImportLocationMigrateTest assembleDebug --no-daemon` passed
   with 8 tests, and `android\run-code-quality.ps1 -Fix` passed.

## Copy Progress Follow-up Phase

- [x] Re-survey current launcher copy/export/import paths after formatter and
   user edits.
- [x] Add progress to SOW extraction if it performs long-running copies.
- [x] Add progress to ZIP/7z extraction or staging where practical.
- [x] Add progress to custom audio copy and staging paths.
- [x] Add progress to file-set import copy paths.
- [x] Add progress to crash log export and other export/share copy paths.
- [x] Run formatter, tests, and build validation after the follow-up work.

Follow-up implementation notes:

- Added `LauncherFileCopy` as the shared byte-copy progress helper and routed
   launcher Kotlin copy paths through it.
- Setup page now shows determinate progress for raw SAF game-file imports,
   ZIP/7z staging and extraction, archive-result installs, demo package
   installs, custom audio import, SOW staging/extraction, and native ISO/CD
   extraction callbacks.
- Music picker custom audio imports and audio archive extraction now report
   progress; launch-time custom-audio staging uses the same copy helper.
- Advanced debug/crash log open, save, and share actions now copy on a worker
   thread and show a progress bar before opening the viewer or share sheet.
- Validation completed: `android\run-code-quality.ps1 -Fix` passed, and
   `:app:testDebugUnitTest --tests com.dxxredux.app.ImportLocationMigrateTest
   --tests com.dxxredux.app.LauncherFileCopyTest assembleDebug --no-daemon`
   passed with 9 tests.

## Installer, Storage Labels, and Pilot Defaults Follow-up

- [x] Reproduce or trace why direct `.exe` InnoSetup reads can report no game
   files for the previously working D2 installer.
- [x] Update Storage Inspector row order to filename, description, location,
   and relative path with filename in green bold text.
- [x] Centralize launcher file purpose labels where practical and add more
   specific game file types such as demo files and DXA mods.
- [x] Confirm how new pilot defaults are created and add any missing graphics
   pilot defaults.
- [x] Run targeted validation after the fixes.

Validation completed: `android\run-code-quality.ps1 -Fix` passed, and
`:app:testDebugUnitTest --tests com.dxxredux.app.ImportLocationMigrateTest
--tests com.dxxredux.app.LauncherFileCopyTest --tests
com.dxxredux.app.LauncherFileLabelsTest assembleDebug --no-daemon` passed.

## Exact GOG Installer Direct-Fd Follow-up

- [x] Verify the two supported Windows GOG installers against the native Inno
   reader by normal path and direct fd.
- [x] Replace `/proc/self/fd` filename use with a native fd-based Inno open
   path for listing and extraction.
- [x] Keep temp-copy fallback only for file providers whose fds cannot be
   seeked/read directly, not for installer-specific behavior.
- [x] Add targeted regression coverage for fd-based Inno opening when the local
   GOG installer fixtures are available.
- [x] Run formatter, native probe/test, and Android build validation.

Validation completed: `android\run-code-quality.ps1 -Fix` passed, native
`gog_fd_tests` passed against the local D1/D2 GOG installer fixtures, and
`:app:testDebugUnitTest --tests com.dxxredux.app.ImportLocationMigrateTest
--tests com.dxxredux.app.LauncherFileCopyTest --tests
com.dxxredux.app.LauncherFileLabelsTest assembleDebug --no-daemon` passed.
