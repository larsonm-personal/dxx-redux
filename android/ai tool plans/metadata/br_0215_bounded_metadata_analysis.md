# BR-0215 bounded metadata analysis

## Goal

Bound synchronous secret-area scanning, route-planning visibility work, and retained cache memory while preserving deterministic ordinary metadata results.

## Plan

- [x] Map scanner, planner, adapter, JNI, game-load, and headless call paths plus existing test seams
- [x] Replace oversized and repeated scanner work with bounded sparse summaries and linear precomputation
- [x] Add one aggregate route-analysis work budget and cancellation contract across planner and engine visibility callbacks
- [x] Bound planner and engine visibility caches to explicit per-request or process caps
- [x] Add synthetic scaling, overflow, opener-scaling, cancellation, state-size, and ordinary-result regressions
- [x] Run focused tests, paired builds, Android assembly, scoped quality checks, and archive BR-0215

## Validation

- Synthetic scanner cases passed at 30, 450, and 4,500 candidates, including 9,000 segments and deterministic overflow at 31 valid results
- Trigger-opener cases passed at 10, 100, and 254 openers with linear callback bounds
- D1 and D2 native builds and all registered native maths CTests passed
- Exact D1 and D2 base-game secret-area baselines matched the checked-in fixture
- Android debug assembly passed for arm64-v8a, armeabi-v7a, and x86_64
- Scoped code quality and `git diff --check` passed
- The full extraction CTest run passed 21 of 23 tests; unrelated graphics transaction and Chromaprint configuration tests crashed in their existing code paths, while the BR-0215 test passed
