# Test suite deduplication and runtime pass

Date: 2026-08-06

## Goal

Reduce the normal full-suite runtime by removing obsolete development helpers
and combining tests that repeat expensive emulator setup, while preserving
distinct regression coverage and reliable isolation.

## Guidance carried forward

- Preserve assertions when consolidating scenarios
- Keep D1 and D2 coverage deliberate rather than mechanically duplicated
- Prefer one self-contained scenario over producer/consumer test ordering
- Keep the full headless demo corpus, but use only stratified graphics canaries
  in the normal profile
- Separate smoke coverage from soak or exhaustive matrices
- Do not trade runtime for larger timeouts, weaker assertions, or dependence on
  state left by another top-level test

## Plan

- [x] Read repository instructions and previous rationalization notes
- [x] Inventory current tests and recent per-test runtimes
- [x] Map overlapping coverage and repeated setup/teardown costs
- [x] Rank safe removals and combinations by savings, risk, and validation cost
- [x] Implement the strongest supported tranche
- [x] Run catalog, quality, and order-aware regression validation
- [x] Record measured or historical expected savings and deferred candidates

## Runtime findings

The newest complete report, `report_20260806_131620.md`, took 02:06:42.
The largest actionable costs were not all genuinely long tests:

- `test_boss_health_bar` took 09:40 because one character in the `ice.pig`
  digest was wrong. Each D1/D2 variant consequently performed repeated GOG
  and CD extraction scans before running a roughly 30-second scenario.
- `test_extract` took 03:54 and `test_all_extracts` took 03:59. The latter
  selects a regression spec and invokes the former, so discovery was executing
  the same single-spec extraction workflow twice.
- `test_background_dormancy_unified` took 04:05, while
  `test_launch_to_automap` already paid 02:55 to exercise two background/resume
  cycles in D1 and D2 through the same shared lifecycle implementation.
- `test_counterstrike_level2_switch_completion` took 01:03 to launch directly
  into the level that `test_levelcomplete_touch_skip` already reaches during
  its existing level transition.
- `test_mp` spent a fixed 90 seconds after its complete two-player integration
  smoke on a sustained-connectivity soak. The earlier runtime survey had
  already recommended separating this smoke and soak coverage.

## Implemented tranche

### Extraction owner/support classification

- `test_extract.ps1` is retained as the reusable single-spec driver and manual
  entry point, but declares `test_all_extracts` as its top-level owner.
- PowerShell support ownership is now parsed by one helper and validated by
  both the runner and catalog test.
- The extraction JSON template is owned by `test_all_extracts`, whose
  dependency closure reaches it through `test_extract.ps1`.
- Normal-suite saving from the report: 03:54.

### Lifecycle consolidation

- The unique dormant-state assertions were absorbed into
  `test_launch_to_automap`.
- D1 and D2 retain the existing active-game lifecycle cycles. D2 additionally
  owns the shared main-menu, pause-window, and game-menu workload matrix.
- The active-game and menu phases have explicit state synchronization and do
  not depend on predecessor tests.
- The combined owner passed D1 and D2 in 02:21. The two former report entries
  totaled 07:00, a measured comparison saving of 04:39.

### Counterstrike level-2 consolidation

- The switch-completion route assertions now run immediately after the
  level-complete owner advances naturally into level 2.
- The combined owner passed in 00:31. The two former report entries totaled
  02:31, a measured comparison saving of 02:00 on this emulator state.

### Multiplayer smoke/soak profile

- Direct `test_mp.ps1` runs still default to the 90-second soak.
- Normal `run_all_tests.ps1` runs pass `-SoakSeconds 0`; the new
  `-ExtendedMultiplayer` profile restores the 90-second hold.
- Lobby creation/join, two-way chat, ready, launch, relay traffic, in-game
  state, player count, and callsign assertions remain in the normal profile.
- Deterministic normal-profile saving: 01:30.

### Boss dependency correction

- Corrected the `ice.pig` digest to the canonical value used by the standard
  game-data manifest and other maintained scripts.
- The same D1/D2 boss scenario passed in 00:56 instead of 09:40, saving 08:44
  in the affected report without removing any coverage.

The report-based total reduction represented by this tranche is approximately
20:47. Some timings vary with emulator and dependency cache state, but the
90-second soak removal and eliminated top-level workflows are structural.

## Deliberate retentions

- Keep three mission ZIP batch cases. Multiple archives in one batch are what
  exercise stale imported state, per-item cleanup, and second/third-item
  recovery; reducing it to one would recreate the isolation gap this suite has
  repeatedly exposed.
- Keep both boss engine variants now that their dependency declaration is
  cheap. The HUD implementation has D1 and D2 engine surfaces, so deleting one
  is not justified for a sub-minute combined test.
- Keep the full headless input-demo corpus and normal graphics canaries as
  established by the prior pass.
- Keep KCXF2 and Obsidian as their existing multi-phase owners. Their runtime is
  dominated by unique mission assets and already-amortized level transitions.
- Keep SAF, GOG, xCrash, and Mac extraction paths separate. They exercise
  distinct external interfaces rather than duplicating launcher assertions.

## Deferred candidates

The unimplemented groups from the July 23 round-two survey remain valid, in
roughly this order:

1. Absorb terminal death into `test_launch_to_automap` after all lifecycle
   assertions.
2. Combine Guide-Bot unexplored, secret-reveal automap, and terminal abort on
   pristine Counterstrike level 1.
3. Absorb D2 quick-record coverage into Android save/load dispatch after save
   assertions.
4. Combine controls readability with tiny-help only across an explicit default
   configuration relaunch.
5. Consolidate pilot/controller/intro/title-music preferences as its own
   medium-risk tranche.
6. Absorb resolution into the OGL owner and Lunar metadata into the Trine
   owner, preserving their explicit reset and `clear_mods` boundaries.

These are expected to save another two to four minutes. They were not mixed
into this tranche because their persistent preference, terminal gameplay, or
asset-reset boundaries deserve focused emulator validation.

## Validation

- `test_validate_automation_catalog.ps1`: pass, with 37 standalone JSON tests,
  19 support scripts, and 71 standalone PowerShell tests
- `test_extract_regression_workflow.ps1`: pass
- `test_launch_to_automap.json5`: pass in D1 and D2 after the merge
- `test_levelcomplete_touch_skip.json5`: pass after the merge
- `test_boss_health_bar.json5`: pass in D1 and D2 after digest correction
- `test_mp.ps1 -SkipBuild -SoakSeconds 0`: pass after the preceding emulator
  scenarios; emulator 2 was cold-started during the run
- `run_all_tests.ps1 -Filter __catalog_only__`: pass, 19 support-owned scripts
- scoped `run-code-quality.ps1 -Fix`: pass
- `git diff --check`: pass
