# Route metadata background precompute implementation

## Goal

Remove synchronous route-planning stalls from level loads, preserve useful work
across app updates and interrupted analysis, precompute installed content with a
bounded launcher worker, and keep Guide-Bot behavior predictable and deterministic.

## Phases

- [x] Replace app-version cache invalidation with explicit native cache generations
- [x] Add native cache readiness, probing, and safe monotonic publication
- [x] Split live-game cache preparation/loading from expensive planning
- [x] Enqueue and install live-game results without blocking level start
- [x] Gate D2 Guide-Bot route requests on validated next-waypoint readiness
- [x] Keep multiplayer and input-demo route activation deterministic
- [x] Add one global bounded launcher coordinator with recent-save prioritization
- [x] Analyze one level per launcher job and persist viewer/scheduler state
- [x] Persist corruption-safe reusable partial analysis work
- [x] Add profiling/game diagnostics and failure/retry reporting
- [x] Add native, Kotlin, and integration regression coverage
- [x] Run scoped formatting, unit tests, D1/D2 builds, and completion audit

## Invariants

- Kotlin schedules native work but never reimplements route keys or route records
- Heavy planning never runs concurrently with live simulation in the game process
- D1 and D2 background analyzers never run concurrently
- Cache records are accepted only after exact key, generation, checksum, and bounds validation
- A complete record is never replaced by a less complete record
- Async completion cannot change multiplayer or input-demo behavior mid-level
- Starting or resuming gameplay never waits for metadata calculation

## Validation

- Scoped code quality passed for all changed route metadata files
- Focused LevelMetadata and RouteMetadata Kotlin tests passed
- D1 and D2 route cache native tests passed
- Android debug APK built for arm64-v8a, armeabi-v7a, and x86_64
- Windows D1 and D2 host builds passed through `run-windows-build.ps1`
- Emulator launcher resume started one isolated worker, published generation `g1`
  route records, persisted scheduler state, and wrote resumable visibility chunks
- Emulator launcher-to-D2-level transition completed while precompute was active
- The existing automap integration continued to a later cold-metadata assertion;
  objective labels are intentionally unavailable until the async route is ready
