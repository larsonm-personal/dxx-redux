# Android Route Analysis Cache

## Objective

Determine whether physical switch-route analysis can cause gameplay jitter, and identify the smallest legitimate persistence scheme if in-memory caching is insufficient.

## Audit plan

- [x] Trace visibility-cache ownership, keys, capacity, lookup behavior, and invalidation.
- [x] Trace canonical route creation and reuse during level load and gameplay replans.
- [x] Check whether generated mission metadata is loaded as a route cache or only displayed.
- [x] Measure or inspect when expensive FVI sampling runs on Android.
- [x] Recommend or implement only the minimum necessary caching, versioned by the existing build number if persistence is warranted.

## Findings

- The visibility cache is an in-process open-addressed hash table. It starts at 4,096 entries, doubles above 70 percent load, and has no fixed upper bound.
- The final Obsidian level 2 integration run reported 74,456 entries, 34,033 hits, 29,410 misses, and three resets.
- Cache invalidation hashes the current level geometry and `WALL_IS_DOORWAY` state. Door and wall state changes can therefore discard the whole cache.
- Relevant route events are coalesced, but the resulting live replan and all FVI misses run synchronously from `escort_route_monitor_completion()` on the game thread.
- The canonical route is held only in native static memory. It is rebuilt on level load and does not survive process or level reloads.
- Launcher and checked-in mission metadata contains display-oriented route steps, but the game does not consume it as a route cache. It also omits firing activation positions needed by Guide-Bot.
- The visibility-key equality includes projectile clearance radius, but its hash currently omits that field. This remains correct because equality is checked, but increases collision risk.

## Implementation contract

- Cache only the canonical end-of-level route. Do not persist player position, explored state, door animation state, or live Guide-Bot paths.
- Use the existing Android `versionCode` as the cache format and behavior version. A build reads only `route-cache/<versionCode>/`; no second schema number is introduced.
- Identify an entry with the game variant plus the canonical route snapshot's topology, progression, trigger, and object hashes. This makes renamed or replaced mission archives safe without relying on filenames.
- Store every projected route field, including activation, aim, and label positions. Validate all counts, hashes, endpoint fields, and fixed-size strings before accepting a record.
- Write through PhysicsFS into the game's app-private write directory. Checksum the complete fixed-size record and delete a failed write; truncated or partially replaced records are rejected and regenerated.
- On a hit, skip `route_planner_plan_view` completely. On a miss or invalid record, run the normal planner and save only a successful projected result.
- Keep live end-of-level replans correct. They may reuse the canonical analysis only through an explicit planner operation that rebases cached semantic steps against the current snapshot and performs short pathfinding plus one validation of a cached firing position. Any mismatch falls back to the full planner.
- Unexplored and arbitrary-segment requests remain uncached because their endpoint and automap/player state are transient.
- Expose hit, miss, write, rejection, live-reuse, and live-fallback counts through introspection for automated verification.

## Work phases

### Phase 1: cache format and build identity

- [x] Pass Gradle's computed `versionCode` to both native game targets.
- [x] Add a small host-testable cache module with bounded serialization, strict validation, and deterministic filenames.
- [x] Add focused tests for round trip, wrong build/hash rejection, truncation, corruption, and activation/aim preservation.

### Phase 2: canonical route integration

- [x] Load after the canonical snapshot is built and the cheap level summary is scanned.
- [x] Bypass semantic planning and FVI sampling on a valid hit.
- [x] Save successful, checksummed canonical plans on a miss and delete failed writes.
- [x] Fix the visibility hash to include projectile clearance radius.

### Phase 3: gameplay rebase

- [x] Rebase the cached end-of-level sequence onto current progression in the game adapter.
- [x] Use current key/reactor/wall state plus the existing objective-matched event boundary to select the first pending step.
- [x] Validate the selected shoot position once; for a switch-face canonical endpoint, validate and use the source segment center.
- [x] Fall back to the existing full planner whenever the cached sequence cannot be proven compatible with current state.
- [x] Leave unexplored and explicit segment routes on the existing planner.

### Phase 4: observability and cleanup

- [x] Add cache and rebase counters plus build/key information to route introspection.
- [x] Count cache rejection and I/O failures without failing level load.
- [x] Keep old version directories isolated and unread. Cleanup remains app-data lifecycle policy because the cache is split across the D1 and D2 PhysicsFS write roots, and no launcher-wide cache root was introduced.

### Phase 5: verification

- [x] Run scoped code quality on all touched files.
- [x] Run D1 and D2 route planner/cache tests and Windows builds.
- [x] Build the Android debug APK for all three ABIs with JDK 21.
- [x] Run an emulator integration twice on Obsidian level 2: cold run wrote one entry; warm run hit it without a rewrite.
- [x] Complete the blue-key and trigger-4 objectives and verify the correct next objective with four live reuses, zero live fallbacks, and one physical visibility query instead of 29,410 misses.

## Verification results

- D1 and D2 Windows debug builds passed.
- D1 and D2 `test_route_analysis_cache`, `test_level_metadata_scan`, and `test_route_snapshot` passed.
- Android `assembleDebug` passed for arm64-v8a, armeabi-v7a, and x86_64.
- Cold Obsidian level 2 run: `misses=1`, `writes=1`.
- Warm Obsidian level 2 run: `hits=1`, `misses=0`, `writes=0`, `live_reuses=4`, `live_fallbacks=0`.
- Warm visibility work after the trigger completion: one cache entry and one miss, down from 74,456 entries and 29,410 misses in the uncached full replan path.
- Maintained integration script passed with explicit cache/reuse assertions.

## Recommendation

Add a native route-analysis cache under app-private storage with a directory keyed by the existing Android `versionCode`. Within that version directory, key entries by game, mission/archive content fingerprint, level file fingerprint, and canonical topology/state hash. Persist the canonical route and validated trigger firing candidates, including activation and aim positions. Load it during level setup, then keep live progression and short pathfinding in memory. Revalidate only the selected firing candidate against current dynamic wall state instead of repeating corpus-style candidate discovery during a gameplay frame.

Do not persist transient live routes or use mission names alone as cache identity. Delete older version directories opportunistically at launcher startup; no independent schema-version mechanism is needed while `versionCode` is part of the path.
