# GuideBot route architecture simplification

## Goal

Replace runtime mission rediscovery for `Next` with an ahead-of-time compiled
mission program and a cheap selector over current facts. Preserve classic
GuideBot movement, timer-safe exit preview, and dynamic object tracking.

## Invariants

1. A completed action is never eligible for `Next`.
2. Ordinary key, switch, boss, reactor, and wall changes never invoke the
   dependency planner or a whole-mine visibility search on the game thread.
3. Level analysis owns mission meaning. Runtime code evaluates current facts,
   chooses a compiled action, and selects bounded local guidance.
4. GuideBot movement remains the classic pathfinder and steering system.
5. Reactor and boss are both timer-starting actions. Exit preview may approach
   their frontier but never chooses either as a prerequisite to execute.
6. Switches retain bounded firing choices so a pose compiled for one region
   does not strand GuideBot when the player approaches from another region.

## Implementation

### Phase 1: compiled mission program

- [x] Reuse cached canonical route steps instead of introducing a second action
  graph with duplicate identities, predicates, effects, and prerequisites.
- [x] Treat those steps as the authoritative ordered mission program.
- [x] Extend each shoot-switch step with up to 16 bounded guidance candidates:
  the canonical pose, region approach poses, and exact geometric alternatives.
- [x] Build the static portal graph and strongly connected regions once per
  level analysis, then rank switch candidates over that shared graph.
- [x] Bump the route-analysis cache generation for the expanded payload.

### Phase 2: current-facts selector

- [x] Add a pure compiled selector that excludes completed actions and chooses
  the first unfinished usable action.
- [x] Rebind moving keys, bosses, and reactors to their current live objects.
- [x] Rank compiled switch candidates using one current-connectivity traversal.
  Runtime performs no collision query, visibility sample, dependency search, or
  allocation-heavy whole-mine geometry scan.
- [x] Publish approximate switch guidance explicitly when no precomputed clear
  shot can be confirmed.
- [x] Do not add another fact-fingerprint result cache. The event coalescer
  already suppresses unchanged refreshes, and measured selection is 2 to 21
  microseconds, too small to justify more invalidation state.

### Phase 3: runtime cutover and cleanup

- [x] Cut end-of-level `Next` selection directly over to the compiled selector.
- [x] Remove runtime full-planner fallback, canonical certification, prepared
  fallback adoption, and completed-incumbent restoration from this path.
- [x] Add `compiled_selector` provenance and focused work counters.
- [x] Retain the semantic full planner for initial/cached level analysis and the
  explicitly enabled diagnostic shadow path only. It is not part of ordinary
  `Next` execution.
- [x] Keep unexplored-area selection separate because automap discovery is live
  navigation state, not mission progression.
- [x] Keep non-endpoint bounded planning policies separate. The legacy
  `full_plan_forbidden` label can still describe those unrelated policies, but
  it no longer governs `Next` or represents a mission semantic.

### Phase 4: policy and gameplay verification

- [x] Verify a completed gold key advances directly to its successor rather
  than restoring the stale key objective.
- [x] Verify switch guidance behind grates chooses useful region-local approach
  positions and GuideBot reaches them.
- [x] Verify pre-reactor exit preview retains ordinary switch/key prerequisites
  while excluding boss and reactor timer starters.
- [x] Verify post-reactor and post-boss exit guidance.
- [x] Verify the unrelated largest-unexplored-area policy still works.
- [x] Verify zero runtime semantic full-planner calls in the covered `Next`
  transitions.

### Phase 5: performance and build verification

- [x] Extend the maintained GuideBot calculation benchmark to cover ordinary
  certification, detailed switch search, cached reuse, sliced and unsliced
  unreachable frontiers, unexplored-area selection, compiled action selection,
  and compiled switch guidance.
- [x] Optimize switch preprocessing by sharing the portal graph and component
  decomposition across the level instead of rebuilding them per switch.
- [x] Limit retained candidates to 16 after the larger experimental payload
  exposed excessive stack use in native tests.
- [x] Run scoped formatting and quality checks, focused native tests, Windows D1
  and D2 builds, Android assembly for all configured ABIs, metadata benchmarking,
  and emulator gameplay scenarios.

## Results

- The old live path combined four separate jobs: discovering dependencies,
  simulating progression, choosing an objective, and searching the mine for an
  exact firing pose. That made a tiny state-selection problem inherit the cost
  and failure modes of mission analysis.
- The new boundary is simple: mission analysis compiles the few meaningful
  actions and bounded switch poses once; runtime only filters completed actions,
  rebinds moving targets, and chooses a reachable candidate.
- The stale gold-key failure is fixed because completed actions are discarded
  unconditionally. Failure to guide to the successor cannot resurrect the key.
- Obsidian switch tests now guide to region-appropriate positions, including
  segments 96 and 506 on level 6 and segment 304 on level 7.
- The final 11-level cold-analysis benchmark is 2.082 seconds versus 1.975
  seconds in the latest matching history: +0.107 seconds, or +5.4 percent. This
  is below the repository's 10 percent regression threshold.
- Compiled action selection measures 2 microseconds. Compiled switch guidance
  measures 21 microseconds and performs zero collision or shot-visibility calls.
- The opt-in shadow planner remains useful for diagnostics, but ordinary `Next`
  no longer has a concept of a forbidden full plan. There is simply no runtime
  semantic plan to run.
