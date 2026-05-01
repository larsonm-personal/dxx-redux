# Input Demo Desync Analysis

This workflow keeps replay diagnosis tied to the check values already recorded
inside `.dximdemo` frame records. Use it when a hand-recorded demo replays on
host but diverges before the final result trailer.

## Core Idea

The `.dximdemo` file already contains expected per-frame state under each frame
record:

```jsonl
{"type":"frame","f":810,"ft":2622,"input":{},"rng":{"s":2636896831,"c":22066},"state":{"game_time64":2233467}}
```

Replay should be able to write the same shape as actual observed state:

```jsonl
{"type":"frame_state","source":"replay","f":810,"ft":2622,"rng":{"s":2636896831,"c":22066},"state":{"game_time64":2233467}}
```

With `-inputdemo-state-log <path>`, the first mismatch can be found with a
direct JSON comparison instead of hand-built probe printlines.

## Current Helpers

Extract the expected state stream from a recording:

```powershell
./android/tests/export_input_demo_state_trace.ps1 -DemoPath ./android/temp_game_logs/d2_descent2_level2_20260430_221250.dximdemo
```

Limit extraction to a known window:

```powershell
./android/tests/export_input_demo_state_trace.ps1 -DemoPath ./android/temp_game_logs/d2_descent2_level2_20260430_221250.dximdemo -StartFrame 800 -EndFrame 830
```

Compare a generated expected trace to another trace or to the source demo:

```powershell
./android/tests/compare_input_demo_state_trace.ps1 -ExpectedPath ./temp/input_demo_state_traces/d2_descent2_level2_20260430_221250.expected_state.jsonl -ActualPath ./android/temp_game_logs/d2_descent2_level2_20260430_221250.dximdemo
```

Use `-CompareFrameMetadata` when you also want to compare effective `ft` and
RNG data. Leave it off when focusing only on gameplay state.

## Replay Flow

After engine support is added, a live host replay should look like this:

```powershell
$demo = './android/temp_game_logs/d2_descent2_level2_20260430_221250.dximdemo'
$expected = './temp/input_demo_state_traces/d2_level2.expected_state.jsonl'
$actual = './temp/input_demo_state_traces/d2_level2.actual_state.jsonl'

./android/tests/export_input_demo_state_trace.ps1 -DemoPath $demo -OutputPath $expected
./android/tests/run_input_demo_replay.ps1 -DemoPath $demo -Game d2 -Mode accelerated -KeepSandbox -Pilot replay -StateLogPath $actual -CompareStateTrace
./android/tests/compare_input_demo_state_trace.ps1 -ExpectedPath $expected -ActualPath $actual
```

For the common case, let the wrapper pick the output path and run the compare
automatically:

```powershell
./android/tests/run_input_demo_replay.ps1 -DemoPath $demo -Game d2 -Mode accelerated -KeepSandbox -Pilot replay -TraceState
```

## Strategy

- First compare frame state, not subsystem logs. The state trace answers where
  the replay first becomes observably wrong.
- Then inspect durable frame events near that frame: score, impact, robot
  damage, player damage, weapon creation, powerup drop, powerup pickup, and
  powerup removal.
- Use `.rngtrace.jsonl` as supporting evidence by frame and RNG call count. It
  identifies random calls, but semantic events are needed to know which robot or
  powerup those calls belonged to.
- Avoid interpolation. Compare only recorded frames, replay frames, durable
  events, and RNG trace records that actually exist.
- Prefer object signatures over object numbers when following spawned powerups,
  because object slots can be reused.
- Keep temporary replay printlines narrow. If a probe explains a class of
  desyncs, convert it into a durable event or state-summary field.

## Powerup Desync Checklist

For suspected robot-dropped powerup desyncs, collect these in durable events:

- drop decision: source object index, signature, type, id, position, segment,
  `contains_type`, `contains_id`, `contains_count`, `contains_prob`, RNG state,
  RNG call count, and decision result
- replacement or suppression: original contents, final contents, reason, player
  energy or shields, nearby duplicate result, RNG state, and RNG call count
- spawn: created powerup index, signature, id, position, velocity, segment,
  lifeleft, flags, movement type, control type, and render type
- pickup or removal: powerup signature, id, position, segment, player energy and
  shields before and after, and removal reason

The compact per-frame summary should also track live powerup counts by id,
dead-but-not-deleted powerups, and nearest energy or shield powerups to the
player. That makes future hand-recorded demos useful even when the exact robot
drop differs.
