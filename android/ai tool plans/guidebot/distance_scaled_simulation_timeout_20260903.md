# Distance-scaled GuideBot simulation timeout

## Goal

Replace the blanket ten-minute in-engine GuideBot simulation limit with a deterministic per-level budget derived from the precalculated route distance, while retaining fast stall detection and a separate infrastructure timeout.

## Phases

- [x] Audit the in-engine, headless process, headed automation, and browser timeout layers
- [x] Define a bounded deterministic distance-to-seconds policy with safe behavior for missing or partial route metadata
- [x] Pass the per-level budget into headed and headless engine runs and report it in diagnostics
- [x] Add focused policy and runner integration tests
- [x] Validate representative short and long routes, build D2, run CTest, and run scoped code-quality checks

## Initial findings

- The in-engine absolute limit is currently 600 simulation seconds for every level
- Independent no-progress detection still stops a stationary or non-improving actor after 60 simulation seconds
- The headless process watchdog defaults to 180 wall-clock seconds
- Headed automation waits up to 720 seconds for route completion and gives its outer test process 900 seconds
- The selected policy is `ceil(30 + travel_distance / 30)` simulation seconds, clamped to 30 through 300 seconds
- All 443 currently confirmed simulation records with matching mission metadata fit this policy; the narrowest observed margin is about 25 seconds on Obsidian level 1
- Missing, invalid, or negative route distance receives the 30-second minimum
- The engine receives the calculated budget explicitly, so headless, desktop, and Android-headed confirmation use the same deterministic frame limit
- The separate process watchdog retains startup/build slack and is no longer fixed at 900 seconds for manual headed runs
- A forced one-second engine budget terminates deterministically at frame 61 with the expected timeout reason
- Counterstrike levels 1 and 12 remain confirmed and deterministic twice under 107-second and 300-second budgets, completing in 29.3 and 188.1 simulation seconds
- The Windows D2 build, Android debug APK build, all 45 D2 CTests, browser/schema tests, timeout integration test, and scoped quality checks pass
