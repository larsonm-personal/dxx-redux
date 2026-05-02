# Plan: Temp Game Logs Replay Analysis 20260501 141150

## Scope

Analyze the newly recorded demo bundle in `android/temp_game_logs/`:

- `d2_descent2_level2_20260501_141150.dximdemo`
- `d2_descent2_level2_20260501_141150.dximdemo.rngtrace.jsonl`
- `debuglog_20260501_141109.txt`

Ignore `android/temp_game_logs/old/`

## Hypothesis

This bundle is expected to diverge, and the user already suspects a position desync with at least one robot collision in the run. The cheapest discriminating check is still the standard replay plus state and RNG compares. If the first break again clusters around a player-weapon-vs-robot collision, the new symmetric collision-pose logs should let recording and replay be compared by object identity, pose, and collision point instead of exact frame number.

## Plan

1. Run the host replay wrapper on the new `.dximdemo` with replay state and RNG outputs enabled
2. Capture the final replay result, state compare, and RNG compare outputs
3. Identify the first state mismatch and first RNG mismatch
4. Correlate the earliest mismatch with replay sandbox logs, the recorded Android debug log, and the new collision-pose logs
5. Summarize the desync location, likely subsystem, and the next concrete code probe or fix

## Commands

```powershell
Set-Location 'C:\local\dxx-redux'

$demo = '.\android\temp_game_logs\d2_descent2_level2_20260501_141150.dximdemo'
$game = 'd2'
$demoBase = [System.IO.Path]::GetFileNameWithoutExtension($demo)
$traceDir = '.\temp\input_demo_state_traces'
$expectedState = Join-Path $traceDir ($demoBase + '.expected_state.jsonl')
$actualState = Join-Path $traceDir ($demoBase + '.actual_state.jsonl')
$actualRng = Join-Path $traceDir ($demoBase + '.actual_rngtrace.jsonl')

.\android\tests\run_input_demo_replay.ps1 `
  -DemoPath $demo `
  -Game $game `
  -Mode accelerated `
  -KeepSandbox `
  -Pilot replay `
  -StateLogPath $actualState `
  -RngLogPath $actualRng `
  -CompareStateTrace `
  -CompareRngTrace

.\android\tests\export_input_demo_state_trace.ps1 -DemoPath $demo -OutputPath $expectedState
.\android\tests\compare_input_demo_state_trace.ps1 -ExpectedPath $expectedState -ActualPath $actualState
.\android\tests\compare_input_demo_rng_trace.ps1 -ExpectedPath ($demo + '.rngtrace.jsonl') -ActualPath $actualRng
```

## Findings

- Replay final result failed only on `result.position.x`: expected `-9075092`, actual `-9075077`
- Canonical RNG trace compare passed completely for the bundle, so this is not an RNG or AI control-flow split
- Full state compare first reported `frame=547 state.position.x: expected -14668450, actual -14668449`
- Direct trace inspection shows the first divergent serialized state is broader than the compare summary reports:
  - Frames `543` through `546` match exactly, including RNG metadata and player position/orientation
  - At frame `547`, replay already differs in translation while orientation still matches exactly
  - Recorded frame `547`: `x=-14668450 y=-2814913 z=-5148040 forward=(62499,-14716,13123)`
  - Replay frame `547`: `x=-14668449 y=-2814913 z=-5148038 forward=(62499,-14716,13123)`
  - By frame `548`, replay has also drifted in `y` and `z` while the forward vector still matches
- The recorded Android debug log and replay sandbox agree through the frame `546` state, then diverge during the next player movement integration step
- Replay physics probes around frames `545` through `552` show `fate=none` with no wall hit or object hit at the first break window
- No weapon-vs-robot collision-pose lines cluster around frame `547`; the nearest logged robot collisions in this run are earlier, with the latest nearby cluster at frame `527`

## Interpretation

This run does contain robot collisions, but they are not the first cause of the desync. The first visible break is a free-flight player movement drift in segment `110`, with identical RNG state, identical serialized forward vector, and no local collision at the break window. The failure shape is consistent with a small translational physics or movement arithmetic mismatch that starts inside one move step and then compounds over later frames.

## Next Probe

If more instrumentation is needed, the next useful code probe is to emit the same full player-motion snapshot on the host replay side that the recorded Android log already has, for the exact movement step that starts from frame `546` and produces frame `547`. That should expose the first differing translational intermediate, such as velocity, thrust integration, or position accumulation, instead of continuing to focus on AI or robot-collision timing.

## Status

1. [x] Run the host replay wrapper on the new `.dximdemo` with replay state and RNG outputs enabled
2. [x] Capture the final replay result, state compare, and RNG compare outputs
3. [x] Identify the first state mismatch and first RNG mismatch
4. [x] Correlate the earliest mismatch with replay sandbox logs, the recorded Android debug log, and the new collision-pose logs
5. [x] Summarize the desync location, likely subsystem, and the next concrete code probe or fix