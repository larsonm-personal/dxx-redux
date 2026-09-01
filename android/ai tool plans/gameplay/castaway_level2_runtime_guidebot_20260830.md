# Castaway level 2 runtime Guide-Bot investigation

## Problem

The compiled metadata route for Castaway level 2 is complete, but the device
run exported at 2026-08-30 21:18:25 starts with `Find BLUE key`. The blue key is
not present in the compiled route or automap objective list, and later Guide-Bot
guidance is reportedly inconsistent or unhelpful.

## Plan

- [x] Reconstruct every Guide-Bot objective, route decision, and failure from
  the exported log
- [x] Compare runtime objective selection with the generation-21 compiled
  route and automap-visible objectives
- [x] Identify whether the mismatch comes from stale cache data, legacy
  `Next` goal policy, progress matching, or physical frontier selection
- [x] Determine the smallest general correction and the diagnostics or
  regression coverage needed before implementation
- [x] Add regression coverage for temporary switch restoration and sticky
  one-shot completion
- [x] Run scoped formatting, host tests, D1/D2 builds, and Android validation
- [x] Record final validation results here

## Constraints

- Do not include the ordinary Guide-Bot release wall in `Next` planning
- Do not add Castaway-specific production behavior
- Preserve D1 and D2 compatibility and use shared route semantics where
  possible

## Findings

- The device used generation 21 metadata and successfully published the full
  Castaway level 2 canonical route. This was not a stale-cache failure
- Guide-Bot release trigger 30 opens wall 36, temporarily removing the source
  surface for canonical trigger 1. The compiled selector correctly rejected
  that invalid shoot target, but then `Next` fell into the legacy blue, gold,
  red, reactor, exit heuristic. This is the sole source of `Finding BLUE KEY`
- Trigger 0 later closes wall 36. Once the player activated trigger 0 and then
  trigger 1 manually, the compiled route became usable and guided the middle
  switch chain successfully
- After trigger 19 fired, incidental trigger 20 reopened one of its linked
  walls. World-state completion treated the already-disabled one-shot trigger
  19 as required again, blocked the compiled route, and caused a second legacy
  fallback to `Finding REACTOR`
- The general correction is a bounded live prerequisite lookup: when a
  canonical shoot switch has been removed, select an available close or
  illusion-restoring trigger that directly restores its wall, then reevaluate
  the unchanged canonical route. Fired one-shot and disabled close triggers
  must remain complete even if another trigger reverses their wall effects

## Implementation and validation

- The compiled selector and full certifier now synthesize a temporary live
  trigger step when an available close or illusion-on trigger directly
  restores the removed surface of the blocked canonical shoot switch. The
  canonical route is not changed, so the next refresh resumes at trigger 1
- Recovery scans only the current level's bounded trigger, link, and wall
  arrays. It contains no mission, level, segment, wall, or trigger identifiers
- Route completion now treats activated one-shot and disabled triggers as
  permanently spent before inspecting their reversible wall effects. Trigger
  20 can therefore no longer resurrect trigger 19 as a pending objective
- The Android Guide-Bot selector diagnostic now records `prepared=1` when the
  temporary restoring prerequisite was selected
- Focused certifier tests pass for both a fly-through restoring trigger and a
  spent one-shot close trigger whose linked wall is reopened
- D2 Windows build and the focused certifier, metadata scan, and route snapshot
  tests pass
- D1 Windows build passes
- Castaway level 2's checked-in complete route assertion passes
- Android `testDebugUnitTest` and `assembleDebug` pass with JDK 21
- Scoped code-quality checks and `git diff --check` pass

An interactive device replay is still the final behavioral confirmation. In a
new log, the first post-release selector decision should be valid with
`selected=1`, `prepared=1`, and source trigger 0 rather than falling through to
`Finding BLUE KEY`. After incidental trigger 20, the selected route should
continue at trigger 21 rather than `Finding REACTOR`.
