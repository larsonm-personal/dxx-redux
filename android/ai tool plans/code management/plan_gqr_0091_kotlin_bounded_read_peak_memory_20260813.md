# GQR-0091 Kotlin bounded-read peak-memory remediation

## Goal

Close `GQR-0091` / `GQF-0104` by applying the shared 128 MiB
peak-live-memory policy to every Kotlin `readBytesBounded` materialization
without changing inherited D1 or D2 files

## Plan

- [x] Trace the helper, all production callers, the frozen evidence, and the
  parallel native policy
- [x] Replace `ByteArrayOutputStream` growth and final-copy overlap with exact
  preallocation for trustworthy sizes and bounded segmented reads otherwise
- [x] Pass trustworthy expanded sizes from file and archive callers
- [x] Add allocation instrumentation tests for exact and one-over limits,
  segment boundaries, short and zero reads, allocation failure, cleanup, and
  maintained callers
- [x] Run focused JVM tests, scoped code quality, and an Android Kotlin compile
- [x] Record final paths, peak metrics, validation, and any blockers

## Constraints

- Keep all product changes in branch-added Android files
- Count every simultaneous helper-owned byte array against one 128 MiB ceiling
- Preserve exact byte results and reject dishonest declared sizes
- Convert allocation failure into an ordinary extraction error and release all
  helper-owned staging references on every failure
- Use reduced test-only memory ceilings rather than large fixtures

## Result

- `ExtractionLimits.kt` now mirrors native
  `DXX_EXTRACT_MAX_MEMORY_BYTES` with a 128 MiB combined-memory ceiling
- Trustworthy expanded sizes allocate the returned array exactly once, so an
  exact 128 MiB result peaks at 128 MiB with no growth or final copy
- Unknown-size reads use fixed segments and admit at most half the memory
  ceiling as payload, so segment storage plus the final exact array peaks at
  128 MiB. One additional byte is rejected before another allocation
- All maintained file, ZIP descriptor, nested HOG, and track callers pass a
  trustworthy expanded size when one is available. Unknown ZIP sizes retain
  the segmented path
- Allocation failures become `IOException` with their `OutOfMemoryError` cause
  and every helper-owned staging reference is released on failure

## Validation

- `ExtractionLimitsTest`: 11 tests, 0 failures, 0 errors
- Exact, one-over, two growth boundaries, short reads, zero-length reads,
  short input, forced allocation failure, cleanup, and existing budget callers
  pass with reduced 8-byte and 16-byte ceilings
- Android `compileDebugKotlin` and `compileDebugUnitTestKotlin` pass as part of
  the focused Gradle test run
- Scoped code quality passes for every touched Kotlin path
- Scoped `git diff --check` passes
- No D1 or D2 file was changed

There are no implementation blockers. An initial test run encountered shared
Gradle output contention and a concurrent test deprecation; both were cleared
before the successful terminal run
