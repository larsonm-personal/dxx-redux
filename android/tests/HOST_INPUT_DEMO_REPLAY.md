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

Use `-KeepSandbox` if you need to inspect the generated config or copied exe after a run.