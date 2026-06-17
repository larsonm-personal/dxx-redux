# D1 input demo under D2 runner path

Goal: add an explicit replay path for running D1 input-demo recordings under the D2 executable so D1-in-D2 gameplay desyncs can be measured.

Plan:

1. [completed] Inspect replay metadata gating and runner selection for the current D1-recording forced-D2 failure.
2. [completed] Add an explicit wrapper option that selects the D2 executable while preserving D1 demo metadata expectations.
3. [completed] Allow the D2 input-demo loader to accept D1 recordings when the explicit D1-in-D2 path is requested.
4. [completed] Format/build D2 and run the new path on `d1_descent_level4_20260616_121645.dximdemo`.
5. [completed] Update notes with the first gameplay or restore failure exposed by the new path.

Notes:

- Current forced D2 run exits before gameplay with `Input demo replay currently supports D2 demos only`.
- The default D2 `-Runner fast` selection uses the dedicated headless-console executable for accelerated checkpoint demos. The D1-in-D2 path should use the full D2 executable first, because this is a gameplay compatibility path rather than a D2 headless regression path.
- Added `run_input_demo_replay.ps1 -D1InD2`, which requires a D1 recording, launches the D2 executable, requires both D2 and D1 core data files, disables headless-console selection, and passes `-inputdemo-d1-in-d2` to the engine.
- Added a flag-gated D2 loader path that accepts D1 recordings only when `-inputdemo-d1-in-d2` is present. Normal D1 and D2 replay loading remains strict.
- Verified D2 build with `.\run-windows-build.ps1 -Target d2`; build completed with existing `d2/main/weapon.c` C4715 warnings.
- New path run:
  `.\android\tests\run_input_demo_replay.ps1 -DemoPath C:\local\dxx-redux\android\regression_demos\d1_descent_level4_20260616_121645.dximdemo -Game d2 -D1InD2 -Mode accelerated -Runner fast -StateLogPath temp\d1_level4_121645_d1_in_d2_actual_state.jsonl -RngLogPath temp\d1_level4_121645_d1_in_d2_actual_rngtrace.jsonl -ReplayDebugLog -SkipExpectedChecks -AllowMissingActualResult -KeepSandbox -TimeoutSeconds 120`
- The D2 executable now accepts the D1 recording and gets to checkpoint restore. The first exposed blocker is `restore: unsupported save version 15 in 'inputdemo_start.dgss'` in `temp\input_demo_runtime_wrapper\d2\d1_descent_level4_20260616_121645\gamelog.txt`.
- Because restore stops before frame 0, the D1-in-D2 actual state trace only contains its metadata record and the RNG trace only contains its metadata record.

Follow-up plan:

1. [in progress] Compare D1 and D2 checkpoint save headers and restore entry points around save version 15.
2. [pending] Add the smallest explicit D1-in-D2 checkpoint path needed to get past restore.
3. [pending] Rebuild D2 and rerun the D1 Level 4 replay path with state/RNG trace output.
4. [pending] Record the next exposed failure or first desync location.
