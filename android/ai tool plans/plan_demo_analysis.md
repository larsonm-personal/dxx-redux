# Demo Analysis Template

Use this as the default workflow for hand-recorded replay analysis in:

```text
C:\local\dxx-redux\android\temp_game_logs\
```

## Artifact Types

- `.dem`: legacy demo format. Treat this as supporting reference only. It records at 20 Hz and requires interpolation, so it is not precise enough for late desync localization.
- `.dximdemo`: primary replay artifact. It stores a checkpoint save plus per-frame inputs and expected running state so replay can be compared directly against recorded behavior.
- `.dximdemo.rngtrace.jsonl`: full recorded RNG stream for the `.dximdemo`. Replay can write the same schema so the first extra or missing RNG call can be located mechanically.
- `debuglog*.txt`: recorded-side Android debug logs that can help correlate the first mismatch to concrete game behavior.

## Core Goal

The main question is not just whether replay fails. The main question is where replay first becomes wrong.

The general sequence is:

1. Replay the `.dximdemo` on host with replay-side state and RNG outputs enabled.
2. Compare the replay final result against the embedded expected result trailer.
3. If replay diverges, find:
   - the first observable frame-state mismatch
   - the first RNG trace mismatch
4. Correlate those earliest mismatch points with durable logs and nearby code.
5. Fix the concrete restore or runtime bug, or add mirror logging that can be compared on recording and replay.

## Preflight

Use PowerShell from repo root:

```powershell
Set-Location 'C:\local\dxx-redux'
```

List candidate artifacts in the root temp-game-log directory only:

```powershell
Get-ChildItem -LiteralPath '.\android\temp_game_logs' -File |
  Sort-Object LastWriteTime -Descending |
  Select-Object LastWriteTime, Length, Name
```

Pick the newest replayable `.dximdemo` in that directory:

```powershell
$demo = Get-ChildItem -LiteralPath '.\android\temp_game_logs' -Filter '*.dximdemo' -File |
  Sort-Object LastWriteTime -Descending |
  Select-Object -First 1 -ExpandProperty FullName
$demo
```

If the replay binaries are missing, build first:

```powershell
Test-Path '.\buildd2\main\dxx-redux-d2.exe'
Test-Path '.\buildd2\main\dxx-redux-d2-headless.exe'
```

If either returns `False` for D2 analysis:

```powershell
.\run-windows-build.ps1 -Target d2
```

If either returns `False` for D1 analysis:

```powershell
.\run-windows-build.ps1 -Target d1
```

## Standard Variables

Use explicit paths so every run writes predictable outputs:

```powershell
$demo = '.\android\temp_game_logs\d2_descent2_level2_20260501_085718.dximdemo'
$game = 'd2'
$demoBase = [System.IO.Path]::GetFileNameWithoutExtension($demo)
$traceDir = '.\temp\input_demo_state_traces'
$expectedState = Join-Path $traceDir ($demoBase + '.expected_state.jsonl')
$actualState = Join-Path $traceDir ($demoBase + '.actual_state.jsonl')
$actualRng = Join-Path $traceDir ($demoBase + '.actual_rngtrace.jsonl')
$actualResult = $demo + '.actual.json'
$sandboxLog = Join-Path '.\temp\input_demo_runtime_wrapper' (Join-Path $game (Join-Path $demoBase 'gamelog.txt'))
$recordedRng = $demo + '.rngtrace.jsonl'
```

For D1, only change:

```powershell
$game = 'd1'
```

## Baseline Replay Command

This is the default full replay command for analysis. Use it first.

```powershell
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
```

What this gives you:

- replay result JSON at `$actualResult`
- replay state trace at `$actualState`
- replay RNG trace at `$actualRng`
- a kept sandbox and `gamelog.txt` at `$sandboxLog`
- immediate wrapper summary for final result, state compare, and RNG compare

For repeated runs after the sandbox already has the correct pilot:

```powershell
.\android\tests\run_input_demo_replay.ps1 `
  -DemoPath $demo `
  -Game $game `
  -Mode accelerated `
  -KeepSandbox `
  -ReuseSandbox `
  -Pilot replay `
  -StateLogPath $actualState `
  -RngLogPath $actualRng `
  -CompareStateTrace `
  -CompareRngTrace
```

For the short convenience form when default output locations are acceptable:

```powershell
.\android\tests\run_input_demo_replay.ps1 -DemoPath $demo -Game $game -Mode accelerated -KeepSandbox -Pilot replay -TraceState -TraceRng
```

## Focused Compare Commands

If the wrapper fails, rerun the focused helpers directly so the first mismatch is printed clearly.

Export expected state from the recording:

```powershell
.\android\tests\export_input_demo_state_trace.ps1 -DemoPath $demo -OutputPath $expectedState
```

Compare expected recorded state to replay state:

```powershell
.\android\tests\compare_input_demo_state_trace.ps1 -ExpectedPath $expectedState -ActualPath $actualState
```

If you also want `ft` and recorded frame RNG metadata compared in the state trace:

```powershell
.\android\tests\compare_input_demo_state_trace.ps1 -ExpectedPath $expectedState -ActualPath $actualState -CompareFrameMetadata
```

Compare recorded RNG trace to replay RNG trace:

```powershell
.\android\tests\compare_input_demo_rng_trace.ps1 -ExpectedPath $recordedRng -ActualPath $actualRng
```

## Narrowing to a Small Frame Window

After the first mismatch is known, shrink the expected state export to a small window around it:

```powershell
$frame = 296
$start = [Math]::Max(0, $frame - 10)
$end = $frame + 10

.\android\tests\export_input_demo_state_trace.ps1 `
  -DemoPath $demo `
  -OutputPath $expectedState `
  -StartFrame $start `
  -EndFrame $end

.\android\tests\compare_input_demo_state_trace.ps1 -ExpectedPath $expectedState -ActualPath $actualState -CompareFrameMetadata
```

Use this when the replay already fails late and you want a small comparison surface.

## Log Correlation Commands

The wrapper sandbox log is the fastest replay-side behavioral source once a frame is known.

Inspect replay-side lines near one frame number:

```powershell
$frame = 296
Select-String -Path $sandboxLog -Pattern "frame=$frame\b|frame=$($frame - 1)\b|frame=$($frame + 1)\b" |
  ForEach-Object { $_.Line }
```

Get the newest recorded debug log in the same directory as the demo:

```powershell
$demoDir = Split-Path $demo -Parent
$recordedDebugLog = Get-ChildItem -LiteralPath $demoDir -Filter 'debuglog*.txt' -File |
  Sort-Object LastWriteTime -Descending |
  Select-Object -First 1 -ExpandProperty FullName
$recordedDebugLog
```

Inspect the same frame window in the recorded debug log:

```powershell
$frame = 296
Select-String -Path $recordedDebugLog -Pattern "frame=$frame\b|frame=$($frame - 1)\b|frame=$($frame + 1)\b" |
  ForEach-Object { $_.Line }
```

When a mismatch is not frame-local and you need more context, widen the grep window first before adding new code logging.

## Decision Tree

### Case 1: final result matches and both compares pass

- Treat the replay as good enough for that artifact.
- Record the artifact name, build used, and pass result.

### Case 2: final result fails, but state mismatch is much later than RNG mismatch

- Prioritize the RNG mismatch first.
- Use the first divergent RNG call to identify the subsystem and callsite.
- Then inspect logs around that frame for the concrete robot, path build, awareness event, or weapon event.

### Case 3: first state mismatch comes before the first RNG mismatch

- Prioritize state reconstruction or deterministic math.
- Check checkpoint restore state, level start state, object fields, AI mode, segment, timers, and floating point sensitive calculations.

### Case 4: final mismatch is tiny, but late-frame state or RNG still differs

- Treat it as real drift, not a cosmetic pass.
- Record the first mismatching frame and first mismatching RNG sequence anyway.
- Use that smaller late mismatch as the new anchor for the next tranche.

## Code Investigation Rules

- Fix the earliest mismatch, not the loudest final mismatch.
- Prefer state-trace and RNG-trace mirror values over one-off printlines.
- If more logging is needed, add durable fields that can be produced on both recording and replay.
- Prefer logging object signatures over object indices when tracking spawned or recycled objects.
- Do not rely on `.dem` interpolation for the final diagnosis.
- when targeting new log lines at re-runs, *I do not re-run the exact same demo*, it's done by hand, and the frame numbers and events will be slightly different, but generally the demos are showing the same categories of desyncs over and over. make the logging independent of specific frames or events

## Findings Template

Fill this in for each analyzed artifact.

```text
Artifact:
Game:
Checkpoint or new-level start:
Replay command used:
Wrapper result:
Final result mismatch:
First state mismatch:
First RNG mismatch:
Relevant replay log lines:
Relevant recorded log lines:
Best code hypothesis:
Next change or next probe:
```

## Minimum Repeatable Workflow

If you only need the shortest complete analysis sequence, use this exact block:

```powershell
Set-Location 'C:\local\dxx-redux'

$demo = '.\android\temp_game_logs\d2_descent2_level2_20260501_085718.dximdemo'
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
