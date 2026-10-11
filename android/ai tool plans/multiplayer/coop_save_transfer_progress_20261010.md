# Co-op save transfer progress

## Plan

- Read existing transfer counters without changing packets, retries, deadlines, or completion conditions
- Show client download percentage and a HUD bar; show elapsed waiting time on the host using the existing monotonic transfer start time
- Keep later restore waits distinct from download completion, and hide progress after reset/failure
- Share the implementation between D1 and D2, expose progress to introspection, and verify Android builds and existing restore integration coverage

## Status

- Inspection: the native receiver already counts unique chunks, and both games share the transfer implementation and restore HUD status
- Complete: read-only transfer status uses existing unique received-chunk counts and monotonic transfer timestamps
- Client HUD shows download percentage and bar, with a distinct downloaded/waiting label at 100 percent; host shows elapsed download wait, then elapsed restore wait
- The added barrier timestamp is display-only; no packets, retries, deadlines, input handling, or completion conditions changed
- D1/D2 HUD hooks share bar rendering and reserve space in the existing HUD layout rectangles
- Introspection exposes `coop_restore.download_percent` (-1 outside downloads)

## Validation

- Scoped mixed-language formatting passed, including a follow-up for the incidental build repairs
- Windows CMake builds passed for D1 and D2; `test_multi_save_transfer_policy` passed in both builds
- Android x86_64 D1/D2 native libraries and debug APK built successfully; verified the APK contains both new status implementations before installation
- Automation catalog validation and catalog integration passed
- Extended existing `test_coop_countdown_save_restore.jsonc` and `test_coop_countdown_save_wait.jsonc` to check the host's elapsed wait, initial zero and advancing client percentage, and clearing the percentage after completion
- `test_lan.ps1 -CountdownSave -SkipBuild` passed for D1 and D2 using emulator-5590 (DxxCoopPause) and emulator-5592 (DxxSdk36). Both peers stayed frozen through deliberate source/load delays and lost barrier messages, resumed, and retained saved countdown/inventory
- Logs: `temp/coop_progress_d1_lan.log`, `temp/coop_progress_d2_lan.log`, `temp/coop_progress_apk.log`, `temp/coop_progress_windows_build.log`
- Captured D2 HUD labels and the empty download track; Android System UI's unrelated ANR dialog obscured the center of the client screenshot. Functional progress was checked through engine introspection and automation, including a captured 68-percent D1 download

## Concurrent checkout build repairs

The checkout acquired mod-assets changes during validation. To build the test APK, moved its C++ introspection include outside `extern "C"`, included `gr.h` before `piggy.h`, protected `std::min` from the engine macro, and used D1's `digi_sample_rate` instead of D2's `GameArg.SndDigiSampleRate`. Other concurrent work was preserved
