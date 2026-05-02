# Plan: Awareness Probe Logging 20260501

## Scope

Instrument the D2 awareness path around the first replay desync for `d2_descent2_level2_20260501_103312.dximdemo`

## Hypothesis

The first divergence is a missing awareness-trigger RNG draw at frame `435`. The most likely causes are:

1. an unprobed `create_awareness_event()` callsite firing on recording but not replay
2. a call reaching `create_awareness_event()` on both sides but diverging before the `d_rand()` at `ai.c:1514`

## Plan

1. Add focused replay logging around `create_awareness_event()` in `d2/main/ai.c`
2. Add a missing local probe for the robot-fire awareness callsite in `d2/main/ai2.c`
3. Rebuild the D2 host target
4. Re-run the failing demo with replay traces enabled
5. Correlate the new awareness logs at frame `435` against the recorded trace and update the hypothesis

## Findings

- Added replay-only awareness result logging in `d2/main/ai.c` that records:
	- whether the call was skipped as observer
	- whether multiplayer gating allowed awareness processing
	- whether `add_awareness_event()` returned true
	- awareness queue count before and after the call
	- agitation before and after the call
	- the post-`d_rand` gate value and pass/fail decision when the RNG path is reached
- Added a replay-only robot-fire probe in `d2/main/ai2.c` before `create_awareness_event(obj, PA_NEARBY_ROBOT_FIRED)`
- Rebuilt D2 with `run-windows-build.ps1 -Target d2`
- Replayed `d2_descent2_level2_20260501_103312.dximdemo` with the same wrapper command

## Updated Interpretation

- Replay does **not** log any awareness event at frame `435`
- Replay also does **not** log the new `kind=robot_fire` probe at frame `435`
- The first replay awareness log in that window is still the weapon-wall awareness at frame `438`
- However, the replay frame-state trace still matches the recorded frame-state trace exactly through frames `435` to `438`, including:
	- `game_time64`
	- player position, segment, and orientation
	- `rng.s`
	- `rng.c`
- The replay RNG sidecar still skips the recorded frame-`435` draw and jumps from call count `11696` to `11698`
- That means the frame-`435` issue is no longer best described as a confirmed gameplay RNG divergence

## Current Hypothesis

The evidence now supports one of these two explanations more strongly than the original one-call desync hypothesis:

1. the replay RNG sidecar is missing one real RNG call around frame `435`, even though live RNG state and call count still advance correctly by frame `436`
2. there is an AI-internal control-flow difference around frame `435`, but not a visible player-motion or broad frame-state mismatch yet

## What This Rules Out

- A broad player position mismatch at frame `435` is unlikely
- A broad player orientation mismatch at frame `435` is unlikely
- A broad floating-point drift in visible player movement is unlikely in this first window because the player-facing frame state remains bit-identical there

## What Remains Plausible

- A robot-local or AI-local state mismatch that is not part of the current frame-state trace
- A timing mismatch in AI-local timers such as `next_action`, `next_fire`, `aware_time`, or awareness queue contents
- A robot location or visibility mismatch that only exists in AI object state, not in player-visible state
- An RNG trace instrumentation gap or context-attribution gap around frame `435`

## Next Probe

- If continuing this tranche, instrument the RNG trace path itself for missing-context calls around frame `435`
- Add a targeted replay dump of the relevant AI locals or traced robots in the `435` to `438` window so robot-local state can be compared directly
