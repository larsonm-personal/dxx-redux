# GQR-0096 RAR Enumeration Admission Plan

## Goal

Bound native RAR catalog enumeration before item and output-projection
materialization by applying one attempt-owned entry, work and live-memory
policy while each native item is inspected

## Constraints

- Keep product changes in branch-added Android launcher files
- Do not edit `d1/` or `d2/`
- Preserve terminal policy failures and capability-only host fallback
- Reject filtered and projection-amplified catalogs within the same attempt
- Do not edit the canonical quality ledger or root campaign plan

## Phases

- [x] Trace the native enumeration, projection validation and fallback boundary
- [x] Add incremental attempt-owned catalog admission and collision validation
- [x] Cover exact and one-over entry, work and memory limits with focused tests
- [x] Cover long names, common prefixes, filtered amplification, cancellation and cleanup
- [x] Run focused tests after the serialized Gradle slot is granted
- [x] Run scoped code quality and record exact diff and validation metrics

## Initial finding

`extractRarWithSevenZipBinding` maps every native item into a retained item
list, maps that list again into an output-projection list, and only then calls
`ExtractionBudget.registerEntry`. Native item count, filtered paths, path
storage, normalization and projection collision structures can therefore grow
without the attempt limits that govern extraction

## Result

- `RarCatalogAdmission.kt` owns native-item enumeration and admits each item
  before retention using one entry budget, a 65,536-unit catalog-work budget
  and the shared 128 MiB live-memory ceiling
- Native items, including entries filtered for blank paths, consume work as
  they are visited. Path length, normalization and each projected prefix also
  consume work before their corresponding operations
- Path, item, projection and collision-key storage is reserved incrementally
  and released on success, cancellation, policy rejection or source failure
- The same `ExtractionBudget` registers declared entry size and ratio while
  enumerating and is then carried into payload extraction for actual bytes
- Canonical path and collision policy is applied while each item is admitted,
  eliminating the second projection list and its normalized/grouped copies
- `IOException`, `CancellationException` and
  `ArchiveOutputValidationException` remain terminal under the typed RAR
  fallback dispatcher

## Validation

- One focused Gradle invocation passed `RarCatalogAdmissionTest` and
  `RarFallbackPolicyTest`: 18 tests, 0 failures, 0 errors and 0 skipped
- The run compiled main and test Kotlin and completed successfully in 24s
- Direct ktlint checks passed all five affected Kotlin source and test paths
- Scoped repository code quality passed for `ArchiveFiles.kt`
- Repository-wide `git diff --check` passed
- New product, test and plan files are printable ASCII without a BOM
- The new admission component is 213 lines, its focused test is 163 lines and
  this durable plan is 65 lines
- No `d1/` or `d2/` file changed

There are no implementation or validation blockers

## Pre-admission timestamp census reopened, 2026-10-09

Existing GQF-0109/GQR-0096 reopened OPEN/TODO. Preserve incremental payload catalog admission and historical18 focused tests above. ArchiveFiles ExtractedReadableArchive248 calls readRarModificationDates291-309 before dispatcher249 and bounded extraction enumeration396. Separate traversal/buildMap retains native names/dates without shared work/entry/live-memory/cancellation admission; retained dates overlap later extraction catalog. Optional runCatching does not bound successful work and suppresses terminal failure meaning

- [ ] Reconcile complete RAR facade date and extraction acquisition; collect stored timestamps through admitted visitation or an explicitly shared attempt instead of an unbounded preliminary census
- [ ] Charge filtered visits, name normalization, date strings and metadata-map retention before operations; include simultaneously retained metadata and extraction catalog within one attempt
- [ ] Preserve genuinely absent dates and stored-date provenance without swallowing terminal policy/cancellation or inferring dates from extracted filesystem output
- [ ] Cover real facade date path plus extraction with small exact/one-over work/entry/retained bounds, long paths, filtered/undated entries, cancellation and failure; require early stop, released ownership and no output on rejection
- [ ] Preserve canonical collision validation, declared/actual extraction accounting and safe capability fallback; coordinate host metadata reader budget with GQR0212 and source-generation/reader ownership with existing owners

Exact report imported as GQR-0095 and GQR-0096 RAR residual diagnosis reopening 20261009 SHA2563ece54eabe7d4b8d90e943cf7e96fe4e07907f34a5dd6825240493afae02d3e7. Historical no-blocker statement applied to completed narrower implementation. Current completion requires these follow-ups; no code/test execution or deferred probes here
