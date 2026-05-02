## Host Input Demo Replay

Use `android/tests/run_input_demo_replay.ps1` for manual host replays.

Do not launch `buildd1/main/dxx-redux-d1.exe` or `buildd2/main/dxx-redux-d2.exe` directly for replay checks unless you are debugging launcher args.

Why:

- It writes a windowed sandbox config so the test does not grab fullscreen.
- It always passes `-window`.
- It skips intro startup with `-notitles` for D1 and `-nomovies` for D2.
- It builds a sandbox around the exe so ad hoc test runs do not dirty the main build folder.

Examples:

```powershell
./android/tests/run_input_demo_replay.ps1 -DemoPath .\android\temp_game_logs\d2_descent2_level1_20260427_143150.dximdemo -Game d2 -Mode accelerated
./android/tests/run_input_demo_replay.ps1 -DemoPath .\temp\input_demo_runtime_smoke\d1\demo.dximdemo -Game d1 -Mode accelerated
```

Generate and compare a live per-frame replay trace against the embedded frame
state from the demo:

```powershell
./android/tests/run_input_demo_replay.ps1 -DemoPath .\android\temp_game_logs\d2_descent2_level2_20260430_221250.dximdemo -Game d2 -Mode accelerated -TraceState -KeepSandbox -Pilot replay
```

Generate both replay-side traces in one run so the replay can be compared
against the recorded frame state and the recorded RNG sidecar:

```powershell
./android/tests/run_input_demo_replay.ps1 -DemoPath .\android\temp_game_logs\d2_descent2_level2_20260430_221250.dximdemo -Game d2 -Mode accelerated -KeepSandbox -Pilot replay -TraceState -TraceRng
```

Use `-KeepSandbox` if you need to inspect the generated config or copied exe after a run.

For iterative manual debugging, keep and reuse one sandbox once it has a valid pilot:

```powershell
./android/tests/run_input_demo_replay.ps1 -DemoPath .\android\temp_game_logs\d2_descent2_level1_20260427_143150.dximdemo -Game d2 -Mode accelerated -KeepSandbox -ReuseSandbox -Pilot replay
```

Notes:

- The correct replay flag is `-inputdemo-replay`. A raw direct launch with `-replayinputdemo` just starts a normal game session.
- If a raw run lands at `Enter your pilot name`, stop and go back to the wrapper instead of typing through it in the build output dir.
- `-ReuseSandbox` preserves `Players/` and other sandbox state between runs. This is the safe way to keep a known-good pilot for repeated experiments.
- `-Pilot <name>` tells the game which existing pilot to select inside the reused sandbox.
- `-TraceState` writes `temp/input_demo_state_traces/*.actual_state.jsonl`, exports the expected trace, and runs the state-trace comparer automatically.
- `-StateLogPath <path>` lets you pick the actual replay trace output file explicitly. Add `-CompareStateTrace` if you also want the wrapper to run the compare helper.
- `-TraceRng` writes `temp/input_demo_state_traces/*.actual_rngtrace.jsonl` and runs the RNG trace comparer automatically against `<demo>.rngtrace.jsonl`.
- `-RngLogPath <path>` lets you pick the replay RNG trace output file explicitly. Add `-CompareRngTrace` if you want the wrapper to run only the compare step without the convenience alias.
