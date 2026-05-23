# Plan: Temp Game Logs Replay Analysis 20260501 103312

## Scope

Analyze the newly recorded demo bundle in `android/temp_game_logs/`:

- `d2_descent2_level2_20260501_103312.dximdemo`
- `d2_descent2_level2_20260501_103312.dximdemo.rngtrace.jsonl`
- `debuglog_20260501_103226.txt`

Ignore `android/temp_game_logs/old/`

## Hypothesis

The replay should now get past the earlier guidebot checkpoint failure. If it still desyncs, the first actionable mismatch will likely be a later runtime divergence that can be isolated by the replay state trace and replay RNG trace.

## Plan

1. Run the host replay wrapper on the new `.dximdemo` with replay state and RNG outputs enabled
2. Capture the final replay result, state compare, and RNG compare outputs
3. If replay diverges, identify the first state mismatch and first RNG mismatch
4. Correlate the earliest mismatch with replay sandbox logs and the recorded Android debug log
5. Summarize the desync location, likely subsystem, and the next concrete code probe or fix

## Commands

```powershell
Set-Location 'C:\local\dxx-redux'

$demo = '.\android\temp_game_logs\d2_descent2_level2_20260501_103312.dximdemo'
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
- [x] First state mismatch identified
- [x] First RNG mismatch identified
- [x] Logs correlated
- [x] Root-cause hypothesis recorded

### Replay Result

- Wrapper command completed and wrote:
  - `android/temp_game_logs/d2_descent2_level2_20260501_103312.dximdemo.actual.json`
  - `temp/input_demo_state_traces/d2_descent2_level2_20260501_103312.actual_state.jsonl`
  - `temp/input_demo_state_traces/d2_descent2_level2_20260501_103312.actual_rngtrace.jsonl`
- Final replay result failed
- First final-result mismatch: `player0.energy`, expected `98`, actual `83`

### First State Mismatch

- `compare_input_demo_state_trace.ps1` first mismatch:
  - frame `582`
  - `state.player0.score`: expected `33100`, actual `32700`
- The frame-state records at `582` otherwise match for the fields checked here:
  - same `game_time64`
  - same player position and segment
  - same `energy` and `shields`
  - same `rng.s` and `rng.c`
  - same `level_summary.robots_alive`, `robots_killed`, `hostages_remaining`, and `powerups_remaining`
- This means the first visible state desync is a score-only divergence of `400`, not a broad state collapse, and it occurs after RNG has re-synchronized by frame `582`

### First RNG Mismatch

- `compare_input_demo_rng_trace.ps1` first mismatch:
  - trace line `11525` in compare output
  - first differing event `seq=11523`
  - recorded: frame `435`, `d2/main/ai.c:create_awareness_event`, `call_count=11697`
  - replay: frame `438`, `d2/main/aipath.c:create_random_xlate`, `call_count=11698`
- Direct trace read shows the concrete difference:
  - recorded has one extra `d_rand` at frame `435` inside `create_awareness_event`
  - replay does not consume that call
  - replay then reaches the shared buddy path build at frame `438` one RNG draw later in the sequence

### Log Correlation

- Replay sandbox log and recorded Android debug log both show the escort path rebuild at frame `438`:
  - buddy object `55`
  - `time_to_visit_player` leads to `create_path_to_player`
  - path detail matches `segs=96,455,86,86`
- This means the first mismatch is not that replay alone took the buddy path branch
- Instead, recording consumed one additional awareness RNG call before the shared frame-`438` escort path work
- No frame-`435` `fire probe`, `impact probe`, or `awareness probe` lines were present in the narrowed log search

### Current Hypothesis

- After adding the awareness-result and robot-fire probes, replay still shows no awareness event and no robot-fire probe at frame `435`
- The frame-state trace remains bit-identical through frames `435` to `438`, including player pose and frame-level RNG state, so the frame-`435` issue is not well supported as a broad movement or floating-point drift
- The strongest current explanation is now either:
  - an AI-internal state mismatch that does not appear in the current frame-state trace, or
  - an RNG sidecar trace gap around frame `435`, because the live frame-state RNG count advances to `11697` by frame `436` even though the replay RNG sidecar omits the corresponding event record
- The later frame-`582` score divergence remains the first visible gameplay mismatch and is still a likely downstream consequence of earlier hidden AI-state drift

### Next Probe

- Instrument the replay RNG trace path itself or otherwise expose untraced RNG calls around frame `435`
- Add a targeted replay dump of AI-local or robot-local state for the traced robots in the `435` to `438` window so hidden robot-state drift can be compared directly
