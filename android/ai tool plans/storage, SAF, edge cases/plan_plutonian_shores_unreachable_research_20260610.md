# Plutonian Shores unreachable metadata research

## Goal
- Determine why Plutonian Shores levels 1 and 5 are reported as `target unreachable or blocked by unsupported door`.

## Steps
- [x] Locate the generated metadata and scanner path that produces the warning.
- [x] Inspect the affected level entries and compare with nearby successful levels.
- [x] Trace the travel estimator behavior closely enough to identify whether this is bad level data, unsupported scanner logic, or a scanner bug.
- [x] Summarize findings and recommend whether a code fix, metadata refresh, or follow-up instrumentation is needed.

## Findings
- Levels 1 and 5 both fail when routing to the reactor, not when collecting hostages or reaching the exit.
- The route graph reaches all hostages once keys are considered, and the exit side is reachable.
- The reactor segment remains unreachable even with all keys because the scanner treats `WALL_CLOSED` (`type=5`) walls as unsupported solid blockers.
- In-game, `WALL_CLOSED` can represent a transparent/render-past barrier. The player may be able to shoot the reactor through it without physically entering the reactor segment.
- Recommended follow-up: handle reactor targets as shootable targets through transparent walls, rather than requiring physical ship access to the reactor segment. This should be a general scanner improvement, not a Plutonian Shores special case.
