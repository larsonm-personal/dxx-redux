# GuideBot simulation regression files and corpus runner

## Goal

Add one checked-in engine-simulation regression artifact beside each mission
metadata artifact:

```text
game_data/mission_files/Counterstrike.json
game_data/mission_files/Counterstrike.simulation.json
```

The simulation artifact must stay small. It records one result per level,
enough deterministic RNG and timing state to recognize drift, and a mission
summary. Detailed objectives, paths, diagnostics, logs, and world snapshots stay
under `android/temp`.

Add a corpus runner that can regenerate all paired simulation artifacts. Wire it
into `android/regenerate_all_regression_data.ps1` as:

- Part of `All`
- Part of the resumable `T` 45-minute sample, always headless
- A dedicated `Simulation` category, with a headless or headed mode

Headless is the only canonical checked-in producer. Headed mode is a diagnostic
and parity mode that writes temporary results and compares their semantic route
against the canonical headless result.

## Current behavior and input decision

The current route-confirmation executable does not parse
`game_data/mission_files/Counterstrike.json`. It loads the mission and level,
generates current level metadata inside the engine, and repeatedly calls the
live GuideBot planner after each world mutation.

The new runner should make the existing mission JSON a required input, but it
must not replay `route_steps` as scripted movement:

1. The mission JSON is the manifest for mission identity, target index, level
   number, secret status, level filename, static `route_status`, and expected
   `route_steps`.
2. The runner creates a stable hash of the route-relevant projection for every
   level.
3. The engine independently loads the level, rebuilds current route metadata,
   and uses live GuideBot goal selection.
4. The runner compares the engine's completed semantic objectives with the
   checked-in `route_steps` projection.
5. A disagreement becomes `route_mismatch`; the runner must not conceal it by
   feeding the old steps back into the engine.

This makes `Counterstrike.json` an input oracle and pairing manifest while
preserving the requirement that in-game GuideBot routing provides the actual
goals.

## File discovery and pairing

Canonical discovery rules:

1. Recursively enumerate checked-in `*.json` files under
   `game_data/mission_files`.
2. Exclude `*.simulation.json`, support manifests, summary files, and JSON files
   that are not mission metadata.
3. Reuse the mission metadata source resolver instead of inferring asset paths
   solely from basenames.
4. Pair each accepted metadata path with the same relative path and a
   `.simulation.json` suffix.
5. Support both existing shapes:
   - A single mission object, such as `Counterstrike.json`
   - An array of mission objects, currently used by 16 checked-in files
6. Preserve `target_index` to distinguish multiple descriptors stored in one
   source artifact.
7. Use stable work item identity:
   `<relative-json>|<target-index>|<level-num>|<level-file>`.

The common archive, CD source, descriptor, and staging logic should be extracted
from the current metadata helpers into a shared PowerShell helper before adding
simulation-specific orchestration. Do not create a second, subtly different
mission archive discovery implementation.

## Minimal checked-in schema

The paired file mirrors the source JSON shape. A single mission input produces a
single object. An array input produces an array of simulation result objects in
the same `target_index` order.

Proposed `Counterstrike.simulation.json` shape:

```json
{
  "schema": "dxx-guidebot-route-simulation-v1",
  "mission_filename": "d2",
  "target_index": 0,
  "generation": 4,
  "fixed_hz": 60,
  "seed": 1,
  "status": "partial",
  "level_counts": {
    "ok": 3,
    "not_run": 27
  },
  "levels": [
    {
      "level_num": 1,
      "level_file": "d2leva-1.rl2",
      "route_input_sha256": "...",
      "status": "ok",
      "rng_start": { "state": 1, "calls": 0 },
      "objectives": [
        { "n": "red key", "s": 20 },
        { "n": "reactor", "s": 45 },
        { "n": "exit", "s": 50 }
      ],
      "total_frames": 2980,
      "rng_end": { "state": 3494937857, "calls": 768 }
    }
  ]
}
```

Schema rules:

- `generation`, `fixed_hz`, and `seed` are mission-level constants and are not
  duplicated in every level.
- `rng_start` and `rng_end` are level-specific and record only the simulation
  RNG stream. The effects stream remains available in temporary detailed run
  output but is not part of the checked-in regression contract.
- RNG entries contain raw state and call count. The initial state is recorded,
  not merely inferred from `seed`.
- `objectives` contains compact names and cumulative whole simulation seconds
  in semantic objective order.
- `total_frames` is authoritative for duration. `total_seconds` is omitted
  because it is derivable from frames and fixed Hz.
- `route_input_sha256` hashes only the normalized route-relevant level
  projection, not unrelated music or aggregate mission metadata.
- `problem` is present only for a non-`ok` result and contains a short stable
  reason code or message.
- Do not check in verbose labels, segments, walls, paths, per-objective frames,
  radii, logs, detailed traces, or wall-clock timing.
- Full per-run JSON and logs remain in
  `android/temp/guidebot_simulation_regression`.

The route-relevant hash projection should include mission identity, target
index, level number, secret flag, level filename, static route status, and each
step's index, kind, activation kind, segment, side, wall, trigger, key, and
carrier identity when present. Normalize missing optional values before hashing.

## Status contract

Level statuses:

- `ok`: engine confirmed the route and the semantic objective projection agrees
  with the source mission JSON
- `partial`: the engine completed at least one objective but did not confirm the
  complete route
- `failed`: no complete route was confirmed
- `timeout`: deterministic frame budget expired
- `unsupported`: the engine found a progression interaction outside the current
  simulation contract
- `route_mismatch`: live objective identity or order disagrees with the source
  route projection
- `nondeterministic`: repeated canonical headless runs did not normalize to the
  same result
- `not_run`: the level was outside the selected sample or has never been run
- `stale`: an existing level result has a different route-input hash,
  simulation generation, seed, or fixed timestep

Failure records should keep completed objective seconds and final RNG state when
available. This makes a partial route useful without expanding the checked-in
schema.

Mission aggregate status:

- `ok`: every expected level is `ok`
- `partial`: at least one level is `ok` or `partial`, and at least one expected
  level is not `ok`
- `failed`: no level is `ok` or `partial`, and at least one level has a terminal
  failure status
- `not_run`: every expected level is `not_run`
- `stale`: no level has a current terminal result and at least one is stale

`level_counts` contains only statuses with a nonzero count and is serialized in
a fixed status order.

## Canonical corpus runner

Add `android/helpers/regenerate_all_guidebot_simulations.ps1` with this public
interface:

```powershell
param(
    [ValidateSet('Headless', 'Headed')][string]$Mode = 'Headless',
    [string[]]$MissionJson,
    [int[]]$Level,
    [double]$SampleFraction = 1.0,
    [int]$SampleSeed = 0,
    [string]$SampleStatePath,
    [int]$Repeat = 2,
    [switch]$NoBuild,
    [switch]$WriteRegression
)
```

Behavior:

1. Discover and validate mission JSON inputs.
2. Resolve every selected mission entry to its actual descriptor, archive or CD
   source, base-game data, and staged extra directory.
3. Expand entries into stable per-level work items.
4. For `SampleFraction < 1`, select levels through
   `Select-RuntimeHashRingFractionItems`, using a dedicated ring such as
   `regenerate:guidebot-simulation:levels`.
5. Build D2 and the route-confirmation executable once per batch.
6. Start one fresh process per level. Do not reuse engine global state between
   levels.
7. Run twice by default and compare normalized detailed output before accepting
   a canonical result.
8. Compare live completed objectives against the expected route projection.
9. Merge sampled results with existing unselected entries by stable level
   identity.
10. Reclassify unselected entries as `stale` when their route hash or simulation
    contract changed; otherwise retain them exactly.
11. Normalize level ordering by source mission order, then level number with
    normal levels before secrets where required by the source file.
12. Write each paired file atomically only after all selected work items for
    that file have terminal results.
13. Continue after individual failures so the corpus records every attempted
    level, but exit nonzero for runner, schema, nondeterminism, or infrastructure
    failures.

Engine route failure is regression data, not necessarily a runner process
failure. The headless executable must always emit a valid result file for
`partial`, `failed`, `timeout`, and `unsupported`; reserve process exit failure
for crashes, invalid inputs, initialization failures, or unwritable output.

The runner should write a durable batch summary and logs under a timestamped
temporary directory. The summary includes counts, elapsed wall time, selected
work item IDs, changed simulation files, and infrastructure failures, but none
of that bulk belongs in checked-in simulation JSON.

## Headed mode

The dedicated `Simulation` category may run headed or headless.

Headed implementation tasks:

1. Generalize the current Counterstrike level 1 JSONC fixture into a generated
   per-mission, per-level automation script.
2. Reuse mission import and staging helpers so arbitrary mission archives can be
   selected without hand-authored menu scripts.
3. Add a durable route-confirmation result written by the common engine
   serializer, rather than scraping console text.
4. Retrieve the result through `run-as` after each run.
5. Use the same schema normalizer and route-oracle comparison as headless.
6. Reset launcher state between levels and clear logcat before every fixture.
7. Run headed levels serially because they share one emulator.

Canonical write policy:

- `Headless` may update checked-in `.simulation.json` files.
- `Headed` defaults to temporary diagnostic output and compares against the
  checked-in headless result.
- Require explicit `-WriteRegression` to write headed results, print a warning,
  and identify the producer as noncanonical in the temporary report. The
  checked-in schema itself remains producer-neutral and minimal.
- `All` and `T` reject or override headed mode and always use headless.

This avoids routine platform physics timing differences rewriting canonical
frame and objective timing data.

## Manual headed mission and level browser

Add a separate, dependency-free console TUI for quickly finding and watching one
route:

```powershell
.\android\helpers\watch_guidebot_simulation.ps1
```

The browser is a manual diagnostic frontend, not a checked-in regression
producer. It reuses the same mission discovery, staging, headed automation, and
result normalizer as the batch runner.

Mission and level selection:

1. Build a searchable index from all accepted mission metadata JSON files.
2. Index mission display name, mission filename, source JSON relative path,
   target index, level number, level name, and level filename.
3. Start accepting search text immediately. Split it into case-insensitive
   tokens and require every token to match somewhere in the indexed record.
   Queries such as `castaway 2`, `obsidian 10`, and `d2leva-3` should narrow the
   list directly.
4. Show a paged result list with mission, level, static route status, and current
   simulation status when a paired file exists.
5. Support arrow keys or `J` and `K`, Enter to select, Backspace to edit, Escape
   to clear or go back, and `Q` to quit.
6. After selection, show the resolved source, level identity, expected objective
   count, static status, prior simulation status and timing, and headed launch
   command before starting.
7. Provide direct noninteractive filters for repeat use:

```powershell
.\android\helpers\watch_guidebot_simulation.ps1 `
    -MissionJson castaway_redux.json -Level 2
```

Implement the picker with PowerShell console input and ANSI redraw support so it
does not require `fzf`, `Out-GridView`, or another dependency. If interactive
console capabilities are unavailable, fall back to a numbered, line-oriented
search prompt. Keep selection and filtering logic separate from rendering so it
can be tested without a real terminal.

Real-time headed viewing requirements:

1. Launch or reuse the configured Android emulator without hiding its window.
2. Bring the game task to the foreground and keep the emulator visible while
   the fixed-timestep route controller runs.
3. Attach the headed camera to the simulated GuideBot, or use a stable chase
   camera, so the user sees actual route movement rather than the stationary
   player start.
4. Render normal level geometry, doors, triggers, keys, and objective effects.
   Do not use the no-render headless shortcuts in this mode.
5. Poll durable introspection at a modest rate and display a compact terminal
   status line containing current objective index and label, simulation time,
   frame count, actor segment, route status, and latest problem.
6. Keep terminal controls active during the run: `Q` aborts cleanly, `R`
   restarts the same level from canonical state, and Space pauses or resumes the
   manual run without changing its fixed simulation timeline.
7. On completion, leave the final scene visible and show status, objective
   seconds, frames, RNG boundaries, and any comparison against the checked-in
   headless result.
8. Offer Enter to repeat, `H` to run a fresh headless comparison, `B` to return
   to search results, and `Q` to quit.
9. Write all manual results and generated automation scripts under
   `android/temp/guidebot_simulation_manual`; never update a
   `.simulation.json` file from this browser.

Add an `M. Search for and watch one GuideBot route` entry to the top-level
regeneration menu. It launches the browser directly and returns to the menu when
the user exits. It is not included in `All`, `T`, unattended category execution,
runtime estimates, or regeneration summaries.

## Integration into regenerate_all_regression_data.ps1

Add `Simulation` to the parameter validation and stage list:

```text
Key: Simulation
Name: GuideBot engine route simulations
Script: android/helpers/regenerate_all_guidebot_simulations.ps1
Default arguments: -Mode Headless -WriteRegression
```

Menu behavior:

```text
1. Run all categories
2. CD extraction, Android import, and launch regressions
3. Disc and music fingerprints, hashes, tags, and AcoustID data
4. Mission level metadata
5. Missing mission archive metadata only
6. GuideBot engine route simulations
M. Search for and watch one GuideBot route
T. Resumable hash-ring sample targeting 45 minutes
Q. Cancel
```

Command-line behavior:

```powershell
# Canonical full corpus, includes headless simulation
.\android\regenerate_all_regression_data.ps1 -Category All

# Canonical simulation corpus only
.\android\regenerate_all_regression_data.ps1 -Category Simulation `
    -SimulationMode Headless

# Diagnostic headed corpus or filtered run
.\android\regenerate_all_regression_data.ps1 -Category Simulation `
    -SimulationMode Headed

# Every selected category, including a headless simulation level sample
.\android\regenerate_all_regression_data.ps1 -Target45Minutes
```

Add `[ValidateSet('Headless', 'Headed')] $SimulationMode = 'Headless'` to the
outer script. If the interactive user chooses item 6, prompt for mode. For
`All`, `Target45`, or `-Target45Minutes`, force the simulation stage arguments to
`-Mode Headless -WriteRegression` regardless of `SimulationMode`.

The existing 45-minute controller passes `SampleFraction`, `SampleSeed`, and
`SampleStatePath` to every stage. The simulation runner must accept those exact
arguments and sample per-level work items. Its historical runtime estimate is
recorded as a normal stage so later `T` runs allocate the common fraction using
measured runtime. Seed the first default estimate conservatively, then replace
it with an observed full or representative corpus runtime.

## Headed/headless parity regression

Add a maintained integration test that runs Counterstrike level 1 through both
front ends from clean state.

The comparison must require:

- Same schema and simulation generation
- Same canonical seed, fixed Hz, difficulty, and starting RNG state
- Same route-input hash
- Same terminal success classification
- Same objective count, order, route-step index, kind, and activation kind
- Monotonic objective times and total frames in both runs
- Two exact repeats within headless
- Two exact repeats within headed

Do not require Android headed frame counts, ending RNG, or objective seconds to
equal Windows headless values. The existing engine evidence shows deterministic
results within each platform but platform-specific physical timing. That
difference should be visible in the parity report, not classified as route
failure.

This test should run separately from the full corpus and be referenced by the
simulation runner's test documentation. `All` need not launch an emulator once
this maintained parity test exists.

## Implementation phases

### Phase 1: Schema and normalization

1. Add a schema document or constants for version 1.
2. Implement route-relevant projection and SHA-256 hashing.
3. Implement source-shape mirroring for object and array mission files.
4. Implement deterministic ordering, optional failure fields, and aggregate
   statuses.
5. Add a validator that rejects duplicate level identities, missing expected
   levels, unknown statuses, invalid RNG values, nonmonotonic seconds, and
   inconsistent summaries.

Exit condition: fixture outputs normalize byte-identically and object plus array
mission sources both validate.

### Phase 2: Complete engine result contract

1. Extend the shared route-confirmation summary to capture start and end state
   plus call counts for both simulation and effects RNG streams.
2. Capture initial RNG immediately after the canonical reset and before any
   route setup consumes RNG.
3. Capture terminal RNG for all terminal statuses.
4. Ensure every engine route outcome writes JSON before returning.
5. Keep the detailed per-level output as the runner input; let PowerShell reduce
   it to the checked-in schema.

Exit condition: success and controlled-failure fixtures both emit complete,
valid deterministic RNG boundaries.

### Phase 3: Mission work item discovery and staging

1. Extract reusable source discovery and staging from the metadata runner.
2. Pair mission JSON with archives, CD sources, built-in campaigns, and D2XXL
   source directories.
3. Expand multi-descriptor JSON arrays through `target_index`.
4. Build stable per-level identities and route hashes.
5. Report unresolved pairs as infrastructure failures without fabricating
   simulation results.

Exit condition: a dry run lists exactly the expected mission entries and levels
without launching the engine.

### Phase 4: Canonical headless corpus runner

1. Implement process isolation, timeout, result collection, repeat comparison,
   route-oracle comparison, and failure classification.
2. Implement full writes and sampled merges atomically.
3. Preserve current unselected entries and mark mismatched ones stale.
4. Produce durable batch logs and a machine-readable summary.
5. Begin with Counterstrike levels 1 through 3, then the complete Counterstrike
   campaign before expanding to external missions.

Exit condition: two full Counterstrike runs produce byte-identical
`Counterstrike.simulation.json`.

### Phase 5: Regeneration menu and 45-minute sampling

1. Add the `Simulation` category and menu item.
2. Include it in `All` after mission metadata, so its route oracle is current.
3. Include it in `T` with level-granularity hash-ring sampling.
4. Force headless canonical writes in `All` and `T`.
5. Pass stage runtime and failure information into the existing summary report.

Exit condition: stage-selection tests prove `All`, `T`, and `Simulation` invoke
the expected mode and sampling arguments.

### Phase 6: Generic headed mode

1. Generalize mission and level launch automation.
2. Add durable common-controller output retrieval.
3. Run headed work serially and normalize through the same reducer.
4. Default headed mode to temporary, noncanonical comparison output.

Exit condition: the dedicated option can run a selected Counterstrike level in
either mode without hand-editing a JSONC fixture.

### Phase 7: Manual headed browser

1. Implement the searchable mission and level index.
2. Implement the dependency-free console picker and line-oriented fallback.
3. Add direct mission JSON and level filters.
4. Add visible emulator launch, GuideBot camera following, and compact live
   introspection progress.
5. Add abort, restart, pause, repeat, headless comparison, and return-to-search
   controls.
6. Add the manual menu entry without including it in batch categories.

Exit condition: a user can type `castaway 2`, press Enter, and watch the
canonical headed simulation in the visible game without editing a fixture.

### Phase 8: Parity and regression tests

1. Add schema and aggregate-status unit fixtures.
2. Add fake-engine runner tests for every level status.
3. Add sampled-merge and stale-detection tests.
4. Add `All`, `T`, and dedicated category selection tests.
5. Add repeated Counterstrike level 1 headed/headless semantic parity.
6. Add a full Counterstrike headless regeneration test when its runtime is
   acceptable for the relevant integration suite.

Exit condition: the maintained suite proves canonical determinism, menu wiring,
sampling behavior, atomic regeneration, and headed/headless route parity.

### Phase 9: Corpus rollout

1. Generate Counterstrike first and review all non-`ok` levels.
2. Run Castaway Redux levels 1 and 2 and Obsidian level 10 next.
3. Expand to existing `ok` routes to identify engine-confirmation regressions.
4. Expand to partial and failed static routes to classify limitations.
5. Run the remaining mission corpus.
6. Tune per-level frame budgets and stage runtime estimates from measured data.
7. Audit every orphan, stale result, route mismatch, nondeterministic result,
   timeout, and unsupported interaction before declaring the corpus complete.

Exit condition: every eligible mission metadata artifact has a paired simulation
artifact, and every expected level has a current result or a specific recorded
failure classification.

## Required verification

- Windows D2 build and native tests
- Android debug build for headed support
- Scoped code quality for all new and changed scripts and C/C++ files
- `git diff --check`
- Schema fixtures for single-object and multi-mission-array inputs
- Two byte-identical headless Counterstrike levels 1 through 3 runs
- Two exact headed Counterstrike level 1 runs within Android
- One headed/headless semantic parity comparison
- Full `Counterstrike.simulation.json` regeneration repeated twice
- Dry-run and fake-process tests for full, sampled, filtered, failure, timeout,
  nondeterministic, stale, and route-mismatch cases
- Picker filtering and selection tests that do not require a terminal
- One manual browser smoke test that selects Counterstrike level 1, observes
  live objective progress, aborts cleanly, and confirms no regression file was
  changed

## Work status

- [x] Audit the current route-confirmation executable and result schema
- [x] Audit mission JSON shapes and existing regeneration categories
- [x] Decide how existing precalculated routes participate as input
- [x] Define the minimal paired simulation schema and statuses
- [x] Define full, sampled, headed, and headless runner behavior
- [x] Define regeneration menu and 45-minute integration
- [x] Define headed/headless parity requirements
- [x] Define the searchable manual headed browser and live-view requirements
- [x] Implement schema and normalizer
- [x] Extend engine RNG boundary output
- [x] Implement work item discovery and canonical headless runner
- [x] Wire `All`, `T`, and dedicated `Simulation` modes
- [x] Implement generic headed mode, manual browser, and parity regression
- [x] Generate and audit the mission corpus (83 D2 files and 985 levels)

Final generation-3 corpus audit: 83/83 paired files, 985/985 level
records, no stale, `not_run`, or nondeterministic results. Results comprise 152
`ok`, 29 `route_mismatch`, 113 `failed`, 663 `timeout`, and 28
`unsupported` levels. The one descriptor-referenced missing level is recorded
as `unsupported` without launching the engine.
