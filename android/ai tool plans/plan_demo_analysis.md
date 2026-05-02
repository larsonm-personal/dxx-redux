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

## Current Tranche: Android Build Failure Fix (2026-05-01)

- [x] Reproduce and inspect the reported compile error from Android CMake build output
- [x] Fix `d2/main/laser.c` static helper declaration ordering issue (`input_demo_replay_is_player_owned_weapon`)
- [x] Re-run focused Android native build step to confirm `laser.c` compiles cleanly
- [x] If new compile errors appear, address only fallout directly related to this change (none new)

## Current Tranche: New Demo Desync Analysis (2026-05-01)

- [x] Identify newest `.dximdemo` and paired recorded logs
- [x] Replay newest demo with state+RNG comparisons enabled
- [x] Locate first state and RNG divergence points
- [x] Correlate mismatch window with new projectile-path logging
- [x] Capture findings and next instrumentation/fix target

### Tranche Findings Snapshot

- Replay still fails with final energy mismatch (`expected 122`, `actual 107`)
- First visible state drift remains around frame 339 (`position.x` mismatch)
- First comparable RNG mismatch remains at line 6422 around frame 269/270
- At the RNG boundary, expected includes stream-0 calls `6496`/`6497` (`create_awareness_event`, `phys_apply_rot`) in frame 269, but replay jumps from `6495` to `6498` in frame 270
- Projectile/robot accept ordering differs in the same window: recorded has weapon obj `28` robot accept in frame 269; replay shows that accept in frame 270
- Current hypothesis: object interaction ordering around frame 269 shifts AI awareness timing by one frame, which then cascades into later motion/state drift

## Current Tranche: New Demo Desync Analysis (2026-05-01, demo 20260501_210847)

- [x] Locate and replay newest hand-recorded demo artifacts
- [x] Export and compare expected state against replay state trace
- [x] Compare recorded and replay RNG traces for first semantic mismatch
- [x] Correlate first mismatch with new weapon-robot accept sequence logs
- [x] Capture concrete divergence signature for follow-up fix work

### Tranche Findings Snapshot (demo 20260501_210847)

- Replay result mismatches at end (`player0.shields`: expected 154, actual 169)
- First state mismatch is frame 299 (`player0.score`: expected 31700, actual 32100)
- First RNG semantic mismatch is frame 281 at `call_count=6618`: expected `create_awareness_event`, actual `phys_apply_rot`
- New accept-seq probe shows replay-only extra hit in frame 281:
  - recording: frame 281 only `accept_seq=0` (`weapon_obj=47` -> `robot_obj=100`)
  - replay: frame 281 has `accept_seq=0` (`weapon_obj=47`) plus extra `accept_seq=1` (`weapon_obj=48`)
- Robot 100 damage timeline diverges immediately from that extra replay accept:
  - recording frame 281 shields `4259840 -> 3670016`
  - replay frame 281 shields `4259840 -> 3670016 -> 3080192`
- Replay then kills robot 100 earlier (frame 298) while recording keeps it alive through frame 299, matching the +400 score lead at frame 299

## Current Tranche: Dispatch/Reason Instrumentation Follow-up (2026-05-01)

- [x] Add focused weapon-vs-robot dispatch probe in `collide_two_objects`
- [x] Add focused gate/reason probe in `collide_robot_and_weapon` for entry, accept, and skip branches
- [x] Rebuild and rerun demo `d2_descent2_level2_20260501_210847`
- [x] Extract frame 280-300 diagnostics from replay log and compare against recording logs

### Tranche Findings Snapshot (dispatch/reason follow-up)

- Replay frame 281 shows two fully valid accepts on robot 100:
  - `weapon_obj=47` then `weapon_obj=48`
  - both have `accept_gate=1`, `persistent=0`, `hitobj_seen=0`, `parent_sig_eq_robot=0`, `robot_exploding=0`
- No reject/skip branch is involved for the replay-only extra hit; this is not a gate-condition mismatch
- Dispatch probe confirms the extra candidate pair exists in replay collision routing at frame 281 (`weapon_obj=48` vs `robot_obj=100`)
- Recording logs still show only one accept in frame 281 (weapon 47), so the divergence is upstream of `collide_robot_and_weapon` branch logic
- Practical conclusion: the mismatch source is pair generation / object pose evolution before this function, not accept/reject conditions inside it

## Current Tranche: Symmetric In-View Robot Pose Tracking (2026-05-01)

- [x] Add path-agnostic robot discovery when first entering view (no robot-id assumptions)
- [x] Keep per-frame pose logging for all discovered robots after discovery, even when out of view
- [x] Keep behavior symmetric between recording and replay
- [x] Rebuild and validate logs on replay run

### Tranche Findings Snapshot (symmetric pose tracking)

- New log stream `Input demo robot pose track` is emitted from `init_ai_frame` and runs for both recorder and replay sessions
- Discovery is event-based (`step=discover`) and increments a running `tracked_total`
- After discovery, each tracked robot emits one line per frame:
  - `step=pose` while the same robot object/signature still exists
  - `step=missing` if the original tracked robot slot no longer holds that robot (dead/recycled slot)
- Each per-frame line includes `in_view`, `los`, `front_dot`, position, velocity, shields, flags, and robot identity for stable cross-run comparisons

## Current Tranche: Generalized FVI Probe (2026-05-01)

- [x] Remove frame-window hard-coding from FVI sphere-check probe in D2
- [x] Remove robot-id hard-coding from FVI sphere-check probe in D2
- [x] Apply the same generalized probe behavior to D1 for parity
- [x] Keep log volume practical by logging all hits plus only near misses

### Tranche Findings Snapshot (generalized FVI probe)

- FVI probe now applies to any demo and any robot, instead of one frame window and one robot id
- Probe still focuses on player-owned weapon vs robot sphere checks to keep the data relevant
- Miss logging is limited to near-threshold misses (`miss_delta <= 8.0`) while all hits are always logged
- Added `miss_delta` to each probe line to make "just missed" vs "clear miss" comparisons immediate

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

## Latest Findings

Artifact: `d2_descent2_level2_20260501_185821.dximdemo`
Game: `d2`
Checkpoint or new-level start: `save_checkpoint`
Replay command used: `./android/tests/run_input_demo_replay.ps1 -DemoPath .\android\temp_game_logs\d2_descent2_level2_20260501_185821.dximdemo -Game d2 -Mode accelerated -KeepSandbox -Pilot replay -StateLogPath .\temp\input_demo_state_traces\d2_descent2_level2_20260501_185821.actual_state.jsonl -RngLogPath .\temp\input_demo_state_traces\d2_descent2_level2_20260501_185821.actual_rngtrace.jsonl -CompareStateTrace -CompareRngTrace`
Wrapper result: `FAIL`
Final result mismatch: `player0.shields expected 152, actual 142`
First state mismatch: `frame 306 gt=943718 player0.score expected 31900, actual 31500`
First RNG mismatch: `recorded create_awareness_event at frame 267 gt=841482 vs replay create_awareness_event at frame 268 gt=844103`
Relevant replay log lines: no `robot_player_before_bump`, `robot_player_after_bump`, or direct `object_object` probe lines at frames `267/268/306/307`; projectile `weapon_sig=3968` hits robot `100/39` at replay frame `268`; projectile `weapon_sig=4010` kills robot `100/39` at replay frame `307`, and replay awards `score +400` on that same frame
Relevant recorded log lines: projectile `weapon_sig=3968` hits robot `100/39` at recorded frame `267`; projectile `weapon_sig=4010` kills robot `100/39` at recorded frame `305`, so the recorded score is already visible by expected state frame `306`
Best code hypothesis: this artifact does not support the player-robot body-collision hypothesis for the earliest split; the hidden divergence is still on the weapon-vs-robot path, where the same logical projectile events are landing later on replay and shifting `create_awareness_event`, robot damage timing, kill timing, and then score/powerup state
Next change or next probe: add durable live+replay logging on the projectile update and hit-acceptance path for the delayed weapon signatures before `collide_player_and_weapon()`/`collide_weapon_and_robot()` fallout, with signatures and segment/pose data carried through so the first one-frame-late hit can be explained mechanically

Follow-up: `PF_WIGGLE` bob/save-restore check for the same artifact
- `d2/main/controls.c` drives bob from `fix_fastsincos((fix)GameTime64)` with fixed-point math only; there is no separate bob phase accumulator to serialize
- vanilla savegames still write `GameTime64 = 0` in `state_save_all_sub()`, but input-demo checkpoints also record `checkpoint_start_gt = GameTime64` in `newdemo.c`, and `state_restore_all_sub()` adds that value back during replay restore
- existing recorder/replay wiggle probes already match exactly through at least frame `267` / `gt=841482`, including `raw`, `scaled`, `amount`, `ship_wiggle`, `uvec`, and `vel_after`
- conclusion: checkpoint restore is preserving wiggle phase for this replay, and PC-vs-Android floating-point noise in the bob path is not a strong lead for the first split in this artifact

Current logging tranche: projectile timing
- [x] Add local-player weapon lifetime logging on both recorder and replay so projectile pose can be compared before the delayed hit frame
- [x] Add explicit `collide_robot_and_weapon()` entry and skip-reason logs for local-player weapons to separate movement delay from hit rejection
- [x] Validate the edit with file diagnostics, a successful `run-windows-build.ps1 -Target d2` build, and a replay smoke run that emits `Input demo weapon probe ... step=sequence_entry` plus `Input demo weapon robot path` lines in the sandbox log

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
