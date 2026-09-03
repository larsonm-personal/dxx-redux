# Counterstrike level 9 hidden-door simulation investigation

## Plan

- [x] Reproduce the level 9 terminal result with focused engine logs
- [x] Trace movement and objective state across the successive hidden doors
- [x] Identify whether termination comes from a timeout, repeat guard, or route mismatch
- [x] Locate the hidden-door-only pause and compare it with keyed-door handling
- [x] Implement the smallest general corrections supported by the evidence
- [x] Verify deterministic completion or a more precise remaining failure
- [x] Run a nearby control level, focused tests, and quality checks

## Findings

- The apparent force-quit was the semantic-repeat guard after three hidden-door objectives, not an engine quit or elapsed run timeout
- Hidden doors were recorded when shot instead of when traversed, then reappeared as required objectives after auto-closing
- A hidden door is now complete only after crossing it; after both adjoining segments are explored, the live planner treats it as a discovered reopenable door
- Exact hidden-door objectives and discovered hidden doors can be opened with early path flares, with collision opening retained as fallback
- The first two hidden-door completions are now 46 frames apart instead of 78, removing the extra rounded-second delay
- Two deterministic runs progressed through all three hidden doors and collected the blue key before exposing a separate fly-through-trigger approach bug at segment 465
- Counterstrike level 1 remained deterministic and `ok`; the D2 build, all 45 D2 tests, focused certifier test, and scoped quality checks passed
