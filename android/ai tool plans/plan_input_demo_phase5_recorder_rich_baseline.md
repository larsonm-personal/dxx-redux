# Input Demo Phase 5 Recorder Rich Baseline Tranche

## Goal

Upgrade recorded fixture baselines from the current minimal `result.json` to the
richer shared result format already used by replay actual output.

This tranche should:

- keep the shared recorder helper pure-data
- add a recorder flush path that accepts a caller-supplied final result summary
- use D1 and D2 stop hooks to capture current engine state and pass it into the
  shared recorder on flush
- preserve the current minimal fallback when no result summary is supplied

This tranche does not yet add richer environment counters beyond the current
shared result fields.

## Constraints

- keep gameplay-state capture in D1/D2 engine code, not in the shared recorder
- preserve the existing recorder metadata and stream file layout
- validate the shared recorder boundary first, then validate D1/D2 engine
  integration
- keep D1 and D2 result capture closely aligned because both trees already use
  the live recorder path

## Planned Steps

- [x] Add shared recorder flush-with-result support and host probe coverage
- [x] Add D1 current-result capture and recorder flush wiring
- [x] Add D2 current-result capture reuse and recorder flush wiring
- [x] Run focused desktop validation
- [x] Run Android native validation

## Exit Criteria

- The shared recorder can write either the minimal fallback or a supplied richer
  final result
- Newly recorded D1 and D2 fixtures write the richer baseline `result.json`
- Desktop and Android builds still pass
