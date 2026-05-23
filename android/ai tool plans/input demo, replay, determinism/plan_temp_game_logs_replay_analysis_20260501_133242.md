# Plan: Temp Game Logs Replay Analysis 20260501 133242

## Scope

Analyze the newly recorded demo bundle in `android/temp_game_logs/`:

- `d2_descent2_level2_20260501_133242.dximdemo`
- `d2_descent2_level2_20260501_133242.dximdemo.rngtrace.jsonl`
- `debuglog_20260501_133143.txt`

Ignore `android/temp_game_logs/old/`

## Hypothesis

This bundle is expected to diverge. The cheapest discriminating check is the standard host replay with state and RNG compares enabled. If it fails, the first state mismatch and first canonical RNG mismatch should localize whether the break is visible in frame state immediately or whether it starts as hidden control flow that only becomes visible later.

## Plan

1. Run the host replay wrapper on the new `.dximdemo` with replay state and RNG outputs enabled
2. Capture the final replay result, state compare, and RNG compare outputs
3. Identify the first state mismatch and first RNG mismatch
4. Correlate the earliest mismatch with replay sandbox logs, the recorded Android debug log, and the new robot-local / ai-local state logs
5. Summarize the desync location, likely subsystem, and the next concrete code probe or fix

## Commands

```powershell
Set-Location 'C:\local\dxx-redux'

$demo = '.\android\temp_game_logs\d2_descent2_level2_20260501_133242.dximdemo'
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
- [x] Early mismatch correlated with replay logs and recorded-side debug logs
- [x] Result recorded

### Replay Result

- Wrapper command completed and wrote:
  - `android/temp_game_logs/d2_descent2_level2_20260501_133242.dximdemo.actual.json`
  - `temp/input_demo_state_traces/d2_descent2_level2_20260501_133242.actual_state.jsonl`
  - `temp/input_demo_state_traces/d2_descent2_level2_20260501_133242.actual_rngtrace.jsonl`
- Final replay result failed
- Final mismatch summary:
  - `player0.energy`: expected `115`, actual `130`
  - replay ended with `score = 33400`, `shields = 153`, `segment = 110`

### First Visible State Mismatch

- `compare_input_demo_state_trace.ps1` first visible mismatch:
  - frame `785`: `state.player0.score` expected `32100`, actual `32500`
- A narrow window compare for frames `780` to `790` still showed only:
  - frame `785`: score mismatch
  - frame `786`: score mismatch
- This means the first observable divergence is an early `+400` score event, not a broad player-state mismatch

### First Canonical RNG Mismatch

- `compare_input_demo_rng_trace.ps1` first canonical mismatch was at comparable line `21794`
- Expected and actual entries had the same `call_count`, `state_before`, `state_after`, `result`, file, and function:
  - expected: frame `786`, `d2/main/ai.c:create_awareness_event`, `call_count=21868`
  - actual: frame `784`, `d2/main/ai.c:create_awareness_event`, `call_count=21868`
- This is a timing shift, not a different RNG branch or missing RNG call before the mismatch

### Replay-Side Local Evidence

- Replay sandbox log shows the decisive event at frame `784`:
  - `impact probe`: player weapon hit `robot_obj=68`, `robot_id=39`, `seg=117`
  - `robot damage`: `589824`, shields `131072 -> -458752`, `dead=1`
  - `score probe`: `delta=400`, score became `32500`
- Immediately after that hit, replay reports:
  - `rng state mismatch` at frame `785`
  - `frame state mismatch` at frame `785` on `player0.score`

### Recorded-Side Local Evidence

- Recorded Android debug log still shows robot `obj=68 sig=3779 id=39` alive at:
  - frame `783` with `shields=131072`
  - frame `784` with `shields=131072`
  - frame `785` with `shields=131072`
- That rules out a robot-health-state mismatch before the first break
- The recorded score catches up shortly after, since the narrowed state compare only mismatched at frames `785` and `786`
- Taken together, recording and replay both reach the same near-death robot state, but replay lands the killing hit about two frames earlier

### Interpretation

- The earliest desync is an early player-shot collision against robot `68`, not an escort-path rebuild or a hidden AI decision difference
- Because player state still matches through the pre-kill window and robot `68` health also matches, the strongest local explanation is hidden timing or position drift in one of these surfaces:
  - player weapon trajectory timing
  - robot `68` position or movement
  - collision resolution timing between the spreadfire bolts and the robot
- The RNG mismatch is downstream evidence of that earlier hit: `create_awareness_event()` is invoked sooner because the impact happens sooner

### Follow-On Divergence

- Later state mismatches show the early kill does not stay isolated:
  - frame `801`: `robots_alive` expected `66`, actual `65`
  - frame `934`: score expected `32750`, actual `32500`
  - frame `940`: `powerups_remaining` expected `64`, actual `63`
  - frame `951`: `robots_alive` expected `64`, actual `65`
  - frame `955`: energy expected `98`, actual `113`
- The later cascade is consistent with the first early kill perturbing combat timing and pickups, then replay missing or delaying later events that recording still gets

### Next Probe Direction

- If another instrumentation tranche is needed, it should focus on the first-hit collision path for player spreadfire against robot `68` around frames `783` to `786`
- The most likely useful additions are recorded and replay-symmetric logs for:
  - player weapon object position and velocity at robot impacts
  - robot `68` position and velocity at impact time
  - collision-side details for player weapon vs robot impacts before score changes