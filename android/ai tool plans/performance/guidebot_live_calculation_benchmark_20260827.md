# GuideBot live calculation benchmark and optimization

## Goal

Extend the existing GuideBot and level-metadata benchmark coverage to include
the live calculations added for current-state certification and switch guidance,
measure their cost, and optimize any material regression without changing route
semantics.

## Phase 1: Audit existing coverage

- [x] Identify the existing benchmark entry points, corpus, retained baselines,
  and regression policy.
- [x] Map the current live certifier phases and rerun scheduling work against
  existing benchmark coverage.
- [x] Define deterministic work-unit and wall-time measurements for uncovered
  calculations.

## Phase 2: Add coverage

- [x] Cover ordinary route certification and strategic prerequisite traversal.
- [x] Cover canonical and detailed switch firing-position searches.
- [x] Cover reachable angle-aware frontier selection after firing search fails.
- [x] Cover sliced execution through completion and invalidation/coalescing
  behavior where it performs calculation work.

## Phase 3: Measure and optimize

- [x] Record current costs for each calculation class and the existing mission
  corpus benchmark.
- [x] Profile or instrument any unexpectedly expensive phase.
- [x] Optimize material hotspots while preserving deterministic selections and
  per-frame work limits.
- [x] Compare final costs with the initial measurements.

## Phase 4: Validate

- [x] Run focused native tests and benchmark policy self-tests.
- [x] Run the pinned mission benchmark when its local game data is available.
- [x] Run Windows D1/D2 builds, scoped code quality, and relevant Android tests.
- [x] Record results and any intentionally retained benchmark thresholds.

## Results

- The existing retained benchmark now runs the live calculation matrix before
  its real-level corpus and stores that output with each snapshot.
- The real corpus grew from nine to eleven levels by adding Obsidian 6 and 12.
  The final run passed all digests at 2.103 aggregate CPU seconds and stayed
  within the retained-history thresholds.
- On the 900-segment live fixture, detailed-switch shootability calls fell from
  902 to 2. Production-sized slices complete the case in 13 slices, down from
  50 at the initial conservative work limit, while retaining the 2 ms deadline.
- The unreachable-switch frontier's maximum callbacks in one slice fell from
  10,834 to 1,474. Its exhaustive 1,428 collision checks remain split across 26
  slices, compared with 15,008 callbacks in the unsliced diagnostic.
- The benchmark also covers ordinary certification, cached switch reuse,
  largest-unexplored-area selection, and the unsliced diagnostic comparison.
- Live work now retains its starting component while GuideBot moves, ranks the
  detailed candidate set by distance and incidence, and tries nearby confirmed
  and approximate poses before either mine-wide fallback.
- The Obsidian 6 switch-and-grate integration passed all 39 steps. Trigger 15
  selected segment 11, completed in 9.3 seconds, recorded zero certifier
  overruns, and peaked at 2.003 ms for a live slice.
- Windows D1/D2 builds, 44 D2 native tests, Android JVM tests, all-ABI debug APK
  assembly, the real Obsidian integration, scoped code quality, and
  `git diff --check` passed.
