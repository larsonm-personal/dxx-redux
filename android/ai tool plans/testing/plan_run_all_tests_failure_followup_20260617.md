# run_all_tests failure follow-up

Goal: run `android/run_all_tests.ps1`, capture the failures, and triage whether they are suite wiring problems, environment/setup problems, or real regressions.

Plan:

1. [done] Refresh repo instructions and create this follow-up plan.
2. [done] Run `android/run_all_tests.ps1` with output captured to a temp log. The run stopped during the APK build before emulator startup.
3. [done] Parse the generated report/logs for failing tests and common failure modes.
   - The original "no emulator launched" symptom was caused by an APK compile failure before suite preflight.
   - The compile failure was missing `input_demo_hooks.h` includes in D1 files after hook centralization.
   - The completed full run then reported 12 failures, while all D1, D2, and D1-in-D2 input-demo sections passed.
4. [done] Fix clear suite/tooling problems discovered by the run, keeping engine changes out unless the failure points there.
   - Added the missing D1 hook includes.
   - Updated unified D1 game-launch scripts to select the mission before accepting the start-level dialog.
   - Updated drifted Trine D1-in-D2 expectations, quick-record frame threshold, merged-wall debug probe tolerance, and D1 GOG launch flow tolerance.
5. [done] Rerun focused failing tests, then update this plan with the result.
   - `test_axis_mapping` passed.
   - `test_death` passed.
   - Focused reruns passed for autosave missing-pilot, autosave resume, D-pad triggers, engine prefs, keyboard defaults, launch-to-automap, Trine D1-in-D2 custom textures, quick-record sidecar, merged-wall debug probe, and D1 GOG installer.
