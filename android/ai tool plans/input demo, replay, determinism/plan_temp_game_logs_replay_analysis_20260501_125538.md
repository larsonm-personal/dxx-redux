# Plan: Temp Game Logs Replay Analysis 20260501 125538

## Scope

Analyze the newly recorded demo bundle in `android/temp_game_logs/`:

- `d2_descent2_level2_20260501_125538.dximdemo`
- `d2_descent2_level2_20260501_125538.dximdemo.rngtrace.jsonl`
- `debuglog_20260501_125453.txt`

Ignore `android/temp_game_logs/old/`

## Hypothesis

The newest logging additions around robot fire and AI/path local state should make the first meaningful divergence easier to classify by robot identity and state transition, even if exact frame numbers differ from earlier demos.

## Plan

1. Run the host replay wrapper on the new `.dximdemo` with replay state and RNG outputs enabled
2. Capture the final replay result, state compare, and RNG compare outputs
3. If replay diverges, identify the first state mismatch and first RNG mismatch
4. Correlate the earliest mismatch with replay sandbox logs, the recorded Android debug log, and the new robot-local / ai-local state logs
5. Summarize the desync location, likely subsystem, and the next concrete code probe or fix

## Commands

```powershell
Set-Location 'C:\local\dxx-redux'

$demo = '.\android\temp_game_logs\d2_descent2_level2_20260501_125538.dximdemo'
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

- [x] Replay executed
- [x] Final result compared
- [x] State trace compared
- [x] RNG trace compared
- [x] New robot-local / ai-local logs observed where relevant
- [x] Result recorded

### Replay Result

- Wrapper command completed and wrote:
  - `android/temp_game_logs/d2_descent2_level2_20260501_125538.dximdemo.actual.json`
  - `temp/input_demo_state_traces/d2_descent2_level2_20260501_125538.actual_state.jsonl`
  - `temp/input_demo_state_traces/d2_descent2_level2_20260501_125538.actual_rngtrace.jsonl`
- Final replay result matched the embedded trailer
- Final result values matched, including:
  - `player0.energy = 103`
  - `player0.shields = 171`
  - `player0.score = 33800`
  - `position.segment = 112`

### State Trace

- `compare_input_demo_state_trace.ps1` compared `719` frame state records
- State trace result: `PASS`

### RNG Trace

- `compare_input_demo_rng_trace.ps1` result: `PASS`
- Canonical comparable lines matched exactly: `19110`
- The enriched replay-side RNG trace still contained supplemental filtered records, which is expected after the broader instrumentation:
  - expected `skipped=993 raw=20104`
  - actual `skipped=2080 raw=21191`
- The compare helper reported `Note: ignored sequence-only differences starting at comparable line 1`, which is expected because supplemental records shift raw `seq` numbering without changing the canonical sim-stream event order

### New Logging Surface

- The new robot-local and ai-local logs were present in the replay output, including:
  - `Input demo replay path state` for buddy path requests and follow probes
  - `Input demo replay fire state` around repeated `robot_fire` events for robot `sig=3780`
- Because the replay matched completely, these logs were confirmation only and did not require deeper mismatch correlation in this run

### Conclusion

- This artifact is a clean replay pass under the current instrumentation and code state
- No new desync localization work was needed for this bundle
