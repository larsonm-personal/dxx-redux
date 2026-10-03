# ZIP archive source limit

## Goal and bounds

The Retroid CI APK rejected a valid 32,561,908-byte combined D1/D2 ZIP because
the entire source was capped by the 16 MiB self-extractor preamble limit.

Choose a separate 512 MiB ZIP source ceiling. This provides roughly sixteen
times the tested bundle's size for game files, missions, and bundled audio
without allowing unbounded cache staging. ZIP sources are staged on disk with
a 64 KiB copy buffer, so the ceiling is not an allocation of that size in RAM.
It also matches the existing ZIP music staging source ceiling.

Retain the 16 MiB preamble limit, 512 MiB extracted entry limit, 2 GiB aggregate
extraction limit, 4,096-entry limit, 1,000:1 expansion limit, and 128 MiB bounded
read memory policy. The source ceiling includes the preamble and ZIP metadata.

## Plan

- [x] Separate source and preamble limits in the ZIP reader and mission probes
- [x] Keep launcher probes within the same source ceiling
- [x] Cover ordinary archives above 16 MiB, preamble bounds, source bounds, and cleanup
- [x] Run focused archive, mission, music, and extraction budget tests
- [x] Replay the exact archive that failed on the Retroid through the fixed reader
- [x] Run scoped code quality and record validation

## Validation

- Focused Gradle `:app:testDebugUnitTest` passed under JDK 21: 109 tests,
  zero failures, errors, or skips. Selected classes: `ArchiveInputStreamsTest`,
  `ExtractionLimitsTest`, `MissionZipTest`, and `MissionZipMusic*Test`
- Kotlin production and test compilation passed as part of that run
- New coverage extracts a normal ZIP above 16 MiB using the default source
  limit, verifies payload CRC and the following entry, and checks stage cleanup
- Mission candidate probing above 16 MiB now passes without a caller override
- Exact and one-over preamble boundaries pass; the oversized preamble fails
  specifically on the preamble guard rather than the whole-source guard
- Exact and one-over source bounds and forged-header cleanup pass using a
  reduced test-only ceiling to avoid staging 512 MiB for each boundary test
- Replayed the exact `test-data.zip` from the Retroid failure through the
  compiled reader's default limit and unchanged extraction budget. All eleven
  files matched the original game data by length and SHA-256: 32,561,908 source
  bytes, 55,434,143 extracted bytes, no staged file left behind
- Scoped mixed code quality passed for all seven changed files
- Scoped `git diff --check` passed

Evidence is under `android/temp/zip_source_limit_20261002/`: `jvm-tests.txt`
and `retroid-archive-replay.txt`. The replay driver is temporary host validation
code; no copyrighted game data was added to the repository.

This is a Kotlin importer change, with no native engine changes. A replacement
APK has not been built or installed, so the existing Retroid CI installation
still contains the old limit. Concurrent GPU portability work was left intact.
