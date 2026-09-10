# Crossfire L15 native crash and regression reporting

- Reproduce the native access violation and obtain an instrumented diagnostic before changing engine behavior
- Fix the demonstrated cause with minimal original-engine changes
- Keep ordinary routing outcomes in regression JSON and reserve console errors/nonzero runner exits for infrastructure failures
- Verify the crash fixture, reporting distinction, and relevant native/demo regressions

## Evidence

- Original run and fresh isolated run both exit with Windows access violation 0xC0000005 after the reactor, during exit navigation
- Ordinary native exit 2 is already accepted by the runner; the reported crash is a separate infrastructure failure

## Diagnosis and fix

- AddressSanitizer identifies a null font read in gr_get_string_size, called by the died-in-mine modal dialog through DoPlayerDead and the reactor countdown
- Crossfire L15 has base_seconds=0 and Countdown_timer=0 immediately after reactor destruction
- The route sandbox only paused positive timers; the gameplay API intentionally rejects expired timers
- The running route sandbox now also freezes zero/negative timers via the existing pause flag, without changing live gameplay countdown rules or native UI/FVI files
- Normal route outcomes now print neutral Recorded progress; failure details remain in regression JSON
- The runner explicitly exits zero when infrastructure succeeded, avoiding an inherited LASTEXITCODE from an earlier shell command

## Validation

- Crossfire L15 completes twice identically in 6174 frames, with keys, reactor and exit; its regression record was refreshed
- Added test_crossfire_zero_countdown.ps1 covering the actual authored zero timer, full repeated completion, normalized result, and a deliberately stale nonzero shell exit code
- Added test_guidebot_simulation_reporting.ps1 exercising the actual runner completion callback: route failure/timeout with native exit 2 remain ordinary results; crash/missing output remain infrastructure errors
- Runner discovery, schema, master data stage tests and all 49 D2 CTests pass
- Scoped formatting/lint passes
- All 15 existing demo regressions pass (temp/crossfire_fix_demos.log)
- AddressSanitizer rerun completes L15 in 6174 frames without diagnostics (android/temp/crossfire15_asan_fixed)
- Actual Crossfire L1 remains failed with native exit 2, while its batch exits zero and prints neutral progress (android/temp/crossfire1_reporting)
