## Host Input Demo Replay

Use `android/tests/run_input_demo_replay.ps1` for manual host replays.

For the existing interactive picker, run `android/run_all_tests.ps1 -ReplayDemo`
from the repository root. It includes user recordings and all 28 tracked public
fixtures, automatically chooses their shareware data, and defaults to a visible
realtime window. See [Watch one recording](../regression_demos/README.md#watch-one-recording)
for data setup, pause/step controls, native/imported choices and known-fault labels.

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
- `-TraceState` writes `temp/input_demo_state_traces/*.actual_state.jsonl.gz`, exports the expected trace, and runs the state-trace comparer automatically. Default and determinism-matrix state traces use gzip directly; explicit `-StateLogPath` values retain their requested format.
- `-StateLogPath <path>` lets you pick the actual replay trace output file explicitly. Add `-CompareStateTrace` if you also want the wrapper to run the compare helper.
- `-TraceRng` writes `temp/input_demo_state_traces/*.actual_rngtrace.jsonl` and runs the RNG trace comparer automatically against `<demo>.rngtrace.jsonl`.
- `-RngLogPath <path>` lets you pick the replay RNG trace output file explicitly. Add `-CompareRngTrace` if you want the wrapper to run only the compare step without the convenience alias.

## Public shareware runtime smoke

The existing generated three-frame smoke uses pinned DOS shareware data for both
engines by default. It still verifies replay identity, difficulty and frame count,
frame-state output, and complete RNG trace publication. It does not replace the
recorded retail replay corpus or its gameplay-state oracles. The generated RNG
inputs are synthetic and may emit mismatch diagnostics; this smoke asserts trace
publication rather than deterministic equality.

Prepare public data explicitly once (PowerShell 7, CMake, and a C compiler):

```powershell
pwsh -NoProfile -File android/helpers/setup_shareware_test_data.ps1 -Download
pwsh -NoProfile -File android/tests/test_input_demo_runtime_smoke.ps1 -Game both
```

Setup uses the existing bounded ZIP reader and native SOW volume assembler; it
never launches a DOS installer. It checks the package and extracted file hashes
against the existing import oracles. Subsequent runs are offline and fail if the
shareware fixture is missing, modified, or contaminated by preferred D2 retail
assets. See `game_data/demo installers/README.md` for fixture provenance and setup.

The smoke runs the real windowed SDL/OpenGL games. On headless Linux use Xvfb and
software GL; a dummy video driver is insufficient. For explicitly selected existing
builds, bypass automatic build selection with `-D1Exe` and `-D2Exe`:

```bash
SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy LIBGL_ALWAYS_SOFTWARE=1 \
  xvfb-run -a pwsh -NoProfile -File android/tests/test_input_demo_runtime_smoke.ps1 \
  -D1Exe "$PWD/buildd1/main/d1x-redux" -D2Exe "$PWD/buildd2/main/d2x-redux"
```

Use `-DataEdition retail` to run the unchanged retail data requirements and
assertions. `-DataDir` selects one explicit edition-appropriate data directory;
for separate game directories, invoke `-Game d1` and `-Game d2` separately.
Public smoke outputs are isolated under `temp/input_demo_runtime_smoke/shareware/`,
and retail outputs under the adjacent `retail/` directory. This host coverage does
not test Android launcher import, device storage, or Android rendering.

## True console versus headed replay

`-Runner headless-console -Mode accelerated` runs native D1 and D2 new-level or
checkpoint demos using dedicated console mains. It does not initialize SDL
video or create an OpenGL context. Unsupported combinations fail explicitly.
`-ExecutablePath` works with this runner as well as with desktop runners.
`-Runner visual` retains normal drawing/presentation; on Linux cloud hosts use
Xvfb for this headed path. `windowed-no-present` still uses a desktop window and
is not interchangeable with the console backend.

The compact public corpus is integrated through
`test_input_demo_regressions.ps1 -Corpus public` (headless default) and
`test_input_demo_regressions_graphics.ps1 -Corpus public`. See the
[corpus guide](../regression_demos/README.md#compact-public-corpus-primary-headless-and-secondary-headed)
for verification scope, runtime negative controls and explicit binary options.
