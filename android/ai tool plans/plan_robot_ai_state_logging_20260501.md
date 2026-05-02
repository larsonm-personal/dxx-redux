# Plan: Robot AI State Logging 20260501

## Scope

Add durable robot-local and ai-local logging around the replay-desync-sensitive AI decision points so the next hand-recorded demo can be compared by object identity and state, not by one exact frame number.

## Hypothesis

The next useful signal is not another frame-specific probe. It is event-local state snapshots around robot fire and path/follow decisions, keyed by robot identity, AI mode, path state, and position, so repeated hand-recorded demos can still be compared even when exact frame numbers shift.

## Plan

1. [x] Reuse the existing AI robot-state log shape where practical so the new logs stay consistent
2. [x] Add robot-local and ai-local state snapshots around robot fire and path/follow transitions
3. [x] Keep this tranche D2-scoped because the equivalent D1 input-demo probe surface is not present in the same form
4. [x] Build the touched host target and confirm the new logging compiles cleanly
5. [x] Record findings and the intended usage for the next hand-recorded demo pass

## Findings

- `d2/main/ai2.c` now logs fire-event state snapshots before awareness handling, after awareness handling, and after `set_next_fire_time()`
- The fire probe now includes robot signature so reruns can be correlated by robot identity instead of one exact frame
- `d2/main/aipath.c` now logs a consistent robot-local and ai-local state snapshot alongside:
	- path request begin and done
	- create-path internal probe and detail records
	- follow probe
	- follow advance trigger
	- follow wrap
	- follow advance result
- These state snapshots include the fields that are most useful for repeated hand-recorded reruns:
	- robot signature and id
	- behavior and mode
	- current and goal state
	- path index, path length, hide index, and path direction
	- awareness fields and visibility history
	- retry counters and next-action / next-fire timers
	- current position and velocity

## Intended Use

- On the next hand-recorded demo, compare logs by robot signature, step label, mode, goal segment, path state, and position rather than expecting frame numbers to line up exactly
- The most likely useful anchors are now `kind=robot_fire`, `path request`, `path state`, `follow probe`, and `follow advance result`

## Validation

- `run-windows-build.ps1 -Target d2`
- `android/run-code-quality.ps1 -Fix`
- `run-windows-build.ps1 -Target d2` after formatter pass
