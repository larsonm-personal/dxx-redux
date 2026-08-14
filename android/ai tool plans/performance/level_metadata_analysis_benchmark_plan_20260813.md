# Level metadata analysis benchmark and optimization plan

## Goal

Design a representative, reproducible level-metadata benchmark that reports
per-phase timing, finishes in at most three minutes on the reference machine,
runs with the full test suite, and updates a checked-in history only when a
result differs by at least 10 percent from its prior accepted value.

This is a planning and measurement tranche. It does not implement the benchmark
or optimize the analyzer.

## Research checklist

- [x] Trace the host analyzer, phase boundaries, cache behavior, and output files
- [x] Trace full-suite entry points and identify the least disruptive integration
- [x] Inventory available D1, D2, mod, and stress-test levels
- [x] Run baseline timing experiments on candidate levels
- [x] Verify output determinism or define a stable equivalence digest
- [x] Survey likely hot paths and rank optimization experiments
- [x] Specify history update, noise suppression, and failure policies
- [x] Produce the implementation and validation plan

## Findings

### Existing analyzer and timing boundaries

Both the Android metadata worker and the host metadata executable ultimately run
`secret_area_rescan_current_level()`, so a host benchmark can exercise the same
secret scan, topology snapshot, route planner, visibility tests, and output state
without emulator noise.

The current host executable only analyzes an entire mission. It needs a signed
level-number selector so benchmark samples retain their real normal or secret
level number. Rewriting a temporary one-level mission is not equivalent because
`Current_level_num` participates in the analysis profile and cache key.

The existing progress callback identifies these ordered operations:

1. `secret_areas`
2. `level_topology`
3. `level_summary`
4. inclusive `route_planning`
5. repeated `route_visibility` tasks inside route planning
6. repeated `route_target_visibility` tasks inside route planning

The host callback does not currently timestamp them. The Android game has broad
profiling logs for prepare and analyze paths, but those logs are not emitted as a
structured result by the isolated metadata worker. The benchmark should time the
native progress events directly and time level load and serialization around the
existing calls.

Durable route and partial-visibility caches must not determine benchmark speed.
The benchmark should explicitly bypass durable cache reads and writes while
retaining the in-analysis visibility memoization that is part of the algorithm.
Otherwise a cache left by an earlier test can turn a computation benchmark into
a cache-hit benchmark.

### Measurement experiments on 2026-08-13

Reference machine:

- Intel Core Ultra 9 285H, 16 cores and 16 logical processors
- Windows host RelWithDebInfo executables built by the repository build helper
- Full registered D1 and D2 data hashes listed in the proposed manifest below

Measured whole-mission wall times are useful for sizing the future per-level
corpus. They are not proposed checked-in baseline values because the current
executable cannot isolate levels or report phase timing.

| Mission | Levels | Runs in milliseconds | Median | Full-output SHA-256 |
| --- | ---: | --- | ---: | --- |
| First Strike | 30 | 721, 779, 794, 687, 730 | 730 | `f4b677d19accc5548e54698ffee6ae43e76cc739870c79f097d07fb64424c0a4` |
| Counterstrike | 30 | 2035, 2004, 2017, 2004, 2059 | 2017 | `feeac9f22fd6152e593ee0d48d1e5367cd2754c808dbc294ade7b292449f43d6` |
| KC-XF2 | 5 | 654 | 654 | `1c494b3ba7d72c9e3910b2ce222965c2a560cb09aaa8b8fcf799f8826cb326f0` |
| Obsidian | 18 | 34885, 36167, 34199 | 34885 | `f8ea65a93e23499c7be554d1989ac27154a94b5957027dc6fc6432e61c61f7a1` |
| Uneasy 4 | 1 | about 195000 | about 195000 | `5d99a0d383609a9c23c13362e48163f12073773d436ead896bf346500d5af05d` |

Counterstrike varied by about 2.7 percent and Obsidian by about 5.6 percent, with
identical output hashes on every repeat. First Strike varied by 14.7 percent in
relative terms because the complete run took less than one second. Therefore a
relative threshold alone is insufficient. History updates need both a 10 percent
relative threshold and a small absolute-time threshold.

Uneasy 4 by itself exceeded the desired three-minute limit. It should be an
explicit stress profile, not part of the default full-suite benchmark. It is a
valuable optimization target after ordinary-level improvements, and the stress
runner should use a separate timeout and history series.

The checked-in normalized output was deterministic in all repeated experiments.
This supports a full canonical level-result SHA-256 as the equivalence guard.

### Selected default corpus

The default corpus should contain nine levels and target a mix of cheap,
ordinary, switch-heavy, secret, large, and custom mission behavior:

| ID | Source | Level | Reason |
| --- | --- | ---: | --- |
| `d1-first-strike-01` | First Strike | 1 | ordinary D1 level and short route baseline |
| `d1-first-strike-10` | First Strike | 10 | 11 secrets and an eight-step route |
| `d1-first-strike-21` | First Strike | 21 | longest built-in D1 semantic route, nine steps |
| `d2-counterstrike-01` | Counterstrike | 1 | ordinary D2 baseline |
| `d2-counterstrike-02` | Counterstrike | 2 | four shoot-switch steps and one fly-through step |
| `d2-counterstrike-12` | Counterstrike | 12 | 120 robots, 18 secrets, and mixed trigger routing |
| `d2-counterstrike-secret-03` | Counterstrike | -3 | negative level handling and 11 secret candidates |
| `d2-kcxf2-03` | KC-XF2 | 3 | ten route steps, three switches, and three fly-throughs |
| `d2-obsidian-11` | Obsidian | 11 | 19 route steps and 11 shoot-switch steps |

Use these exact input identities:

- `DESCENT.HOG`: `83d76ff0c46bb2e7348a49bdd287ad764abeda0d851bfb16b42c1ede93b21052`
- `DESCENT.PIG`: `093f9cc029200e9d71d5e14f2f06e5e876a658dd64dc664d6911c5d24d7b64fe`
- `DESCENT2.HOG`: `f1abf516512739c97b43e2e93611a2398fc9f8bc7a014095ebc2b6b2fd21b703`
- `DESCENT2.HAM`: `5233242206c677d65db7f075dd61f2b0a1b7bbe8cd65f56d769efaee1cc38b4d`
- `GROUPA.PIG`: `facdde6cf8a2cab99ea39ba06931872a1fe5636fe211e61fb58c57d706bf627b`
- `kcxf2.zip`: `2dbb1b1f73b3dcbc3711e597907528938b527e25da73b349a108a47f9726c770`
- `Obsidian.zip`: `441c970b8ebab45f9544c00cd44843f1fce7cda649ee402ef6e4e407f94ef5ce`

KC-XF2 and Obsidian archives are small enough to stage before the timed region.
Archive extraction, executable startup, and mission mounting should be reported
as harness overhead but excluded from analysis phase comparisons.

### Likely sources of avoidable cost

The static survey points to route visibility as the first optimization target:

- `select_trigger_firing_path()` runs a route search, then up to two sweeps of
  every reachable segment for each candidate switch source
- each detailed segment sweep can test face, vertex, and edge-derived firing
  positions, with each test reaching `find_vector_intersection()`
- occupiability of the same firing positions is recomputed for different target
  walls even though occupiability is target-independent
- `search_routes()` allocates and initializes node, closed, visit-order, and heap
  storage on repeated searches over the same snapshot
- equivalent search states can recur while resolving semantic dependencies, but
  no search-result memoization is present
- the visibility hash table stops growing at 65,536 entries; saturation can turn
  later reusable queries into bypasses and repeated collision work
- topology opener construction scans all walls for every unique trigger link,
  producing an avoidable trigger-link by wall-count term
- snapshot and route path vectors perform repeated allocation and copying where
  level bounds are already known
- production checkpoint JSON is truncated and rewritten as often as every 100
  milliseconds, and partial visibility chunks are flushed during computation;
  these are separate I/O costs that should not be confused with planner CPU time

Several tempting changes are unsafe as initial optimizations. Parallel visibility
search, candidate reordering, approximate geometry, changing distance arithmetic,
or selecting a merely equivalent route can change tie resolution and Guide-Bot
behavior. They should not be attempted until the exact output digest and stronger
route-state tests prove equivalence.

## Proposed implementation

### Phase 1: Add a single-level structured timing mode

- [ ] Add signed `-level <n>` selection to the D1 and D2 headless metadata
  executable while preserving the mission's real level number
- [ ] Add `-analysis-timing-json-out <path>` and keep the existing metadata JSON
  output available in the same run
- [ ] Add an explicit benchmark cache mode that bypasses durable route-cache and
  partial-checkpoint reads and writes without disabling in-run memoization
- [ ] Record wall and thread CPU time in microseconds internally, serializing
  seconds with fixed precision
- [ ] Record these inclusive top-level values per level:
  `load_level`, `secret_areas`, `level_topology`, `level_summary`,
  `route_planning`, `serialize_output`, and `level_total`
- [ ] For repeated `route_visibility` and `route_target_visibility` work, record
  task count, sum, maximum, and individual task durations; document that these
  are children of inclusive `route_planning` and must not be summed into total
- [ ] Include stable diagnostic work counters: segments, walls, triggers,
  objects, route steps, route searches, heap pops, edge evaluations, visibility
  samples, FVI calls, visibility-cache hits, misses, entries, resizes, bypasses,
  and checkpoint I/O counts
- [ ] Emit process setup and mission mount timing separately as non-comparable
  harness overhead
- [ ] Unit test timing task reconstruction, repeated same-name tasks, unfinished
  tasks, signed level selection, cache bypass, and normalized JSON ordering

Prefer a small timing recorder beside the host analyzer. Use the existing progress
callback for phase boundaries and add counters at the narrow shared hot points.
Do not add wall-clock calls inside every geometry candidate; counters there are
cheaper and preserve the behavior being measured.

### Phase 2: Add the benchmark manifest and correctness guard

- [ ] Add `android/benchmarks/level_metadata_analysis_manifest.json5` containing
  the nine corpus entries, exact source hashes, signed level numbers, expected
  level filenames, and per-level hard timeouts
- [ ] Stage mod archives under `android/temp` before starting timed work
- [ ] Normalize each selected level's metadata result in the producer and hash
  the complete canonical level JSON
- [ ] Check every repetition against the manifest's expected output digest and
  against other repetitions
- [ ] Fail immediately on missing inputs, wrong input hashes, analysis failure,
  output mismatch, timeout, or a non-finite timing value
- [ ] Provide an explicit `-AcceptOutputChange` maintenance switch that prints a
  semantic diff before updating expected hashes; ordinary full-suite runs must
  never accept changed output

An optimization tranche is successful only when every canonical result digest
is unchanged. A deliberate metadata behavior change requires separate review and
an explicit digest update before performance history can advance.

### Phase 3: Make measurements stable and bound runtime

- [ ] Run one untimed warm-up of the corpus to populate executable, asset, and OS
  file caches while durable analysis caches remain disabled
- [ ] Run at least three measured corpus repetitions and use the median wall and
  CPU result for each level and phase
- [ ] Continue up to five repetitions only when the first three have more than
  7 percent spread and the total time budget permits
- [ ] Use thread CPU seconds as the primary comparison metric and retain wall
  seconds as the user-experience and contention metric
- [ ] Enforce a 165-second measurement budget, a 180-second process-tree timeout,
  and per-level timeouts so cleanup can complete within three minutes
- [ ] Abort without changing history if the machine sleeps, the run is cancelled,
  the analyzer times out, or output correctness fails
- [ ] Record benchmark version, algorithm version, build type, compiler identity,
  OS, architecture, CPU model, logical processor count, source commit, dirty-tree
  state, and a fingerprint of the analyzer source files

The first accepted baseline should confirm that the nine-level corpus finishes
comfortably below 165 seconds on the reference host. If it does not, retain all
ordinary levels but reduce repetitions before removing corpus coverage.

### Phase 4: Add low-noise checked-in history

- [ ] Add normalized
  `android/benchmarks/level_metadata_analysis_history.json`
- [ ] Key comparisons by benchmark version and environment identity. A run on a
  different CPU, OS, architecture, compiler, or build type reports results but
  does not rewrite an existing reference series
- [ ] Require an explicit `-AcceptBaseline` to create the first series for a new
  environment; ordinary full-suite runs must not create one record per developer
  machine
- [ ] Compare against the latest record in the matching series so several small
  changes accumulate until they cross the accepted baseline threshold
- [ ] Append one complete snapshot only when at least one comparable metric meets
  both conditions:
  - relative difference is at least 10 percent
  - absolute difference is at least 0.10 seconds for one level or phase, or 0.25
    seconds for the suite aggregate
- [ ] Treat improvements and regressions symmetrically and record the triggering
  metrics and signed percentage changes
- [ ] Leave the history file byte-for-byte unchanged below threshold
- [ ] Write by temporary file plus atomic replacement, UTF-8 without BOM, LF
  endings, stable key order, and fixed numeric precision
- [ ] Never append a partial, failed, timed-out, non-comparable, or
  output-mismatching run

The benchmark should print a concise warning and create an ordinary git diff on
a significant comparable change. Timing changes alone should not fail the test;
correctness failures and exceeding the three-minute hard limit should fail it.
This flags both improvements and regressions without turning machine load into a
red build.

### Phase 5: Integrate with the full suite

- [ ] Add `android/tests/test_level_metadata_benchmark.ps1` as a normal
  no-infrastructure host test
- [ ] Add it to `$noInfraTests`, give it a 180-second timeout, and include it in
  host-tool prerequisite detection in `android/run_all_tests.ps1`
- [ ] Reuse the existing standard game-data resolver and host build guard rather
  than duplicating asset or compiler discovery
- [ ] Always write the current detailed run to
  `android/temp/level_metadata_benchmark/current.json` for diagnosis, even when
  checked-in history remains unchanged
- [ ] Add focused tests proving below-threshold runs do not touch history and
  above-threshold comparable runs append exactly one normalized record
- [ ] Run the wrapper directly, through a filtered full-suite invocation, and in
  the normal full suite before accepting the initial baseline

### Phase 6: Optimization experiment sequence

Run one optimization experiment at a time. For every experiment, capture the
same benchmark, work counters, and canonical output digests before deciding to
keep it.

1. [ ] Add target-independent firing-position occupiability memoization. This is
   low risk because it caches an exact predicate and does not change candidate
   order
2. [ ] Precompute immutable detailed firing candidates per segment, including
   face, vertex, and edge-derived positions, while retaining their exact current
   order and coordinates
3. [ ] Reuse route-search workspaces to remove repeated vector allocation and
   full-capacity initialization; first measure allocations and search counts
4. [ ] Memoize `search_routes()` only for byte-equivalent query and progress
   states, with explicit invalidation after every simulated key, trigger, wall,
   or position change
5. [ ] Build a trigger-to-source-wall index in one wall pass and remove repeated
   all-wall scans from topology construction
6. [ ] Measure visibility-cache load factor, probe length, and bypasses on
   Obsidian. If the 65,536-entry cap is reached, test capacity sizing from segment
   and switch counts or a larger bounded table
7. [ ] Reserve snapshot, search, path, and candidate vector capacities from known
   level counts, keeping all iteration and tie-breaking order unchanged
8. [ ] Measure checkpoint JSON and partial-cache I/O separately on Android. Test
   less frequent checkpoint publication or larger partial chunks only if core
   CPU improvements leave I/O material
9. [ ] Use branch-and-bound or geometry broad-phase rejection only after adding
   proof-oriented tests that show it cannot discard the current winning firing
   position
10. [ ] Revisit Uneasy 4 with the optional stress profile. Do not add it to the
    default full suite until it completes within the shared time budget

Reject or revert any experiment that changes a canonical digest, Guide-Bot route
step, activation position, firing position, tie result, readiness state, failure
classification, or deterministic replay result.

### Expected implementation files

- `android/app/src/main/cpp/headless/headless_metadata_dump_main.cpp`
- `android/app/src/main/cpp/shared/secretarea.c`
- `android/app/src/main/cpp/shared/secretarea.h`
- `android/app/src/main/cpp/shared/route_planner.cpp`
- `android/app/src/main/cpp/shared/route_planner.h`
- `android/benchmarks/level_metadata_analysis_manifest.json5`
- `android/benchmarks/level_metadata_analysis_history.json`
- `android/tests/test_level_metadata_benchmark.ps1`
- focused C/C++ and PowerShell policy tests
- `android/run_all_tests.ps1`

### Acceptance criteria

- The default benchmark analyzes all nine selected levels and terminates within
  three minutes, including cleanup
- Every result is proven identical to its checked expected metadata digest
- Results report seconds for each top-level phase and aggregate repeated
  visibility tasks without double-counting inclusive route-planning time
- CPU and wall timing plus stable work counters make regressions attributable to
  a phase and type of work
- Every full test-suite run invokes the benchmark
- A comparable change below 10 percent and the absolute-time floor leaves the
  checked-in history byte-for-byte unchanged
- A comparable change at or above the threshold appends one normalized snapshot
  and clearly reports the changed metrics
- Failed, interrupted, timed-out, mismatching, and foreign-machine runs never
  mutate checked-in history
- Uneasy 4 remains available as an explicit stress benchmark without violating
  the default full-suite time budget

## Implementation results

- [x] Added signed single-level selection, structured phase timing, cache bypass,
  and stable route-search/collision work counters to both host analyzers
- [x] Added the pinned nine-level manifest, deterministic metadata digests,
  warm-up plus three-sample median runner, bounded per-level and suite timeouts,
  and current-run detail output
- [x] Added checked-in, environment-keyed history with explicit first-baseline
  acceptance, 10 percent plus absolute thresholds, and atomic normalized writes
- [x] Added the benchmark to automatic full-suite discovery as a no-infrastructure
  test with a 180-second timeout
- [x] Captured a pre-optimization baseline of 8.929 aggregate CPU seconds and an
  optimized result of 0.869 seconds with identical output digests
- [x] Increased the bounded visibility cache from 65,536 to 262,144 entries.
  Obsidian level 11 had saturated the old table with 60,290 bypasses and long
  linear-probe chains; the retained change reduced bypasses to zero
- [x] Added exact firing-position occupiability memoization. It was initially
  rejected when sharing the saturated old table made aggregate CPU regress from
  8.929 to 20.018 seconds. After fixing capacity, it improved Obsidian by about
  17 percent and was retained
- [x] Tried reserving the route-search heap and visit-order vectors. Counterstrike
  level 2 measured 0.266 seconds with it and 0.264 seconds without it, so the
  change was rolled back
- [x] Direct benchmark and D1/D2 native host suites pass; Android external native
  builds pass for arm64-v8a, armeabi-v7a, and x86_64
- [x] The invalid Uneasy 4 catalog entry was repaired and filtered full-suite
  dispatch now passes for both the benchmark and the emulator liveness test
- [ ] The JVM unit suite has one unrelated existing failure initializing
  SevenZip native bindings in `ModManagerMissionZipTest`; 806 other tests pass

## Planning status

- [x] Research and measurement complete
- [x] Corpus and history policy selected
- [x] Implementation and optimization experiments complete

## Follow-up tranche: dispatcher repair and bounded-cache experiments

- [x] Make the Uneasy 4 gameplay liveness automation a valid standalone catalog
  entry and verify that filtered full-suite dispatch reaches the requested test
- [x] Prevent the visibility cache from ever reaching a pathological full-table
  probe state while preserving its memory bound and exact cached predicates
- [x] Measure the ordinary nine-level corpus and Uneasy 4 stress case before
  retaining the cache-load guard
- [x] Try the next low-risk repeated geometry or route-planning optimization only
  if counters show material remaining cost; roll it back below the established
  significance threshold
- [x] Run scoped quality, native host, Android native, benchmark, and dispatcher
  validation and record any unrelated blockers

### Follow-up results

- The Uneasy 4 liveness JSON is now standalone. Catalog validation reports 42
  standalone JSON tests and 19 valid support scripts. Filtered dispatch passed
  both the metadata benchmark and the complete 60-second Uneasy 4 emulator test
- Visibility-cache admission now stops at 70 percent when the 262,144-entry
  memory bound is reached. This preserves short unsuccessful probes instead of
  allowing a full table to turn every miss into a complete table scan
- Occupiability uses a separate bounded 65,536-slot direct-mapped memo. This
  keeps cheap pose predicates from displacing expensive collision-ray results
  and removes occupiability records from persistent visibility checkpoints
- Uneasy 4 completes in a 5.39-second median with the same
  `5d99a0d383609a9c23c13362e48163f12073773d436ead896bf346500d5af05d`
  output hash. The split cache reduced collision queries from 245,171 to 204,718,
  route-planning CPU from about 2.56 to 2.09 seconds, and total CPU by about 8
  percent. The ordinary nine-level corpus remained within noise at 0.890 seconds
- Caching wall centers measured 5.90 seconds versus 5.88 seconds without it and
  was rolled back. Moving Guide-Bot accessibility behind canonical route planning
  measured 5.93 seconds versus 5.88 seconds and was also rolled back
- Scoped code quality, D1/D2 Windows builds, all native CTests, Android native
  builds for arm64-v8a, armeabi-v7a, and x86_64, pinned benchmark digests, and
  both filtered dispatcher runs pass
