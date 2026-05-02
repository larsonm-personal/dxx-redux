# Plan: Temp Game Logs Replay Analysis 20260501 154249

## Scope

Analyze the newly recorded demo bundle in `android/temp_game_logs/`:

- `d2_descent2_level2_20260501_154249.dximdemo`
- `d2_descent2_level2_20260501_154249.dximdemo.rngtrace.jsonl`
- `debuglog_20260501_154215.txt`

Ignore `android/temp_game_logs/old/`

## Hypothesis

This bundle is expected to diverge, and the user has already confirmed a desync with a player-robot collision somewhere in the run. The cheapest discriminating check is the same focused replay workflow as the prior bundle: rerun host replay with state and RNG outputs, identify the first mismatching step, then correlate the earliest mismatch against the new recorder-side player-hit and motion logs by `gt` rather than raw frame number.

## Plan

1. Run the host replay wrapper on the new `.dximdemo` with replay state and RNG outputs enabled
2. Capture the final replay result, state compare, and RNG compare outputs
3. Identify the first state mismatch and first RNG mismatch
4. Correlate the earliest mismatch with replay sandbox logs, the recorded Android debug log, and the new motion/collision probes
5. Summarize the desync location, likely subsystem, and the next concrete code probe or fix

## Commands

```powershell
Set-Location 'C:\local\dxx-redux'

$demo = '.\android\temp_game_logs\d2_descent2_level2_20260501_154249.dximdemo'
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

- Host replay reproduces the desync and ends with a `position.x` mismatch
- The first visible serialized-state mismatch is frame `312` at `gt=935854`
- Expected frame `312` position is `(-23687021,-2113677,-12087397)` in seg `117`
- Actual replay frame `312` position is `(-23687025,-2113673,-12087416)` in seg `117`
- Frames `298` through `311` in the exported state trace still match exactly
- The replay RNG compare does not show a new value mismatch at the first failure point; the first differing comparable line is a frame and `gt` metadata shift with the same RNG call count and transition
- A collision-scoped replay probe now shows robot `100/39` enters `collide_robot_and_player()` at both `gt=933233` and `gt=935854`, but the handler immediately returns through the `robot->flags & OF_EXPLODING` branch
- There are no `robot_player_before_bump` or `robot_player_after_bump` lines for robot `100/39` in the desync window, and there are no matching bump-probe lines for that contact, so no `bump_two_objects()` force is applied there
- The new exploding-object probe shows robot `100/39` first enters `explode_object()` much earlier, at replay frame `306 gt=920125`, and is marked `OF_EXPLODING` there with a delayed placeholder fireball
- The new physics probe shows the player-vs-robot `HIT_OBJECT` path at `gt=933233` and `gt=935854` leaves the player velocity unchanged before and after `collide_two_objects()`, then immediately takes the `ignore_obj_list` retry path (`retry=1 ignored=1`) because the contact produced no velocity change
- The same replay later shows the exploding robot itself still taking `HIT_OBJECT` retry passes against the player while flagged `OF_EXPLODING` and `CT_NONE`, so the object remains a moving physics participant after the explosion transition

## Interpretation

- The hidden split is one move step earlier than the first visible mismatch: align by `gt`, not raw wrapper frame label
- Recorder-side wrapper logs show the state that serializes as frame `312` at replay wrapper `after_move gt=933233`
- Replay-side sandbox logs show a direct `object_object` collision between player `0/4/0` and robot `100/2/39` in seg `117` at `gt=933233`
- Replay-side sandbox logs show the same player-robot contact again at `gt=935854`
- The recorder-side state at `gt=933233` is already slightly different from replay: recorder `after_move` is `(-23687021,-2113677,-12087397)`, replay `after_move` is `(-23687025,-2113673,-12087416)`
- The larger shield delta at `gt=935854` is not the direct nasty-robot collision path. Replay logs show `You took 0.4 damage from a robot blast`, and the replay-side `player damage` line still has `killer_type=-1`
- Replay also logs a nearby wall-impact and awareness event for weapon `46` on frame `312`, so the more likely sequence is: player-robot contact starts the pose drift at `gt=933233`, then the altered pose changes later blast fallout at `gt=935854`
- The new replay probe falsifies the earlier bump-response hypothesis for this bundle. Robot `100/39` is already exploding when the player reaches it, so `collide_robot_and_player()` exits before any bump or attack path runs
- This bundle does not match the earlier `141150` failure class. The controlling code is not the weapon-threat probe window and not the player-robot bump code; the remaining likely source is one hop earlier in movement or collision acceptance around the exploding robot state
- The tighter replay-only boundary is now: robot `100/39` becomes exploding at `gt=920125`, then later player/object contacts at `gt=933233` and beyond are non-impulse collision-acceptance events that simply trigger the physics ignore-and-retry path
- That means the remaining uncertainty is no longer where replay mutates state, but whether recorder took the same earlier explosion transition and same later retry sequence. The current bundle cannot answer that because it was recorded before these probes existed

## Conclusion

- Earliest hidden split: direct player-vs-robot collision with robot `100/39` at `gt=933233`
- First visible mismatch: serialized frame `312` at `gt=935854`
- Follow-on symptom: replay takes larger blast damage than recorder at `gt=935854` (`24341` vs `18871`)
- Refined root-cause boundary: robot `100/39` contact is real, but the collision handler returns via `OF_EXPLODING` before any bump force runs, so the divergence is not in `bump_two_objects()` or `collide_player_and_nasty_robot()`
- New replay-side boundary: robot `100/39` begins exploding at `gt=920125`, and the later desync-window contacts are unchanged-velocity `HIT_OBJECT` retry events rather than force-producing collisions
- The current replay-only evidence is now close to exhausted; the next decisive comparison requires a fresh recorder-side run with the new probes enabled

## Next Step

Record a new copy of this scenario on the current branch with these probes enabled. The narrowest next comparison is:

1. Reproduce the same bundle or a close equivalent so the recorder-side Android debug log contains the new exploding-object and physics hit-object probes
2. Compare whether recorder also shows robot `100/39` entering `explode_object()` around `gt=920125`
3. Compare whether recorder also shows the unchanged-velocity `HIT_OBJECT` retry sequence at `gt=933233` and `gt=935854`, or whether replay diverges there

## Probe Implementation Tranche

- [x] Add the player-robot bump and contact probe in `d2/main/collide.c`
- [x] Mirror the same probe shape in `d1/main/collide.c`
- [x] Rebuild the touched host replay target
- [x] Rerun the `154249` replay and inspect `gt=933233` and `gt=935854`

## Probe Implementation Tranche 2

- [x] Add symmetrical exploding-object transition probes in `d2/main/fireball.c` and `d1/main/fireball.c`
- [x] Add symmetrical player-vs-exploding-robot physics hit-object probes in `d2/main/physics.c` and `d1/main/physics.c`
- [x] Rebuild the touched host replay targets
- [x] Rerun the `154249` replay and inspect the new exploding-object and physics contact logs around `gt=933233` and `gt=935854`