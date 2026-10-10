# GQR-0106 streaming mission-music source budget plan

## Goal

Preserve exact source sizes and one nested expansion budget while cataloging or materializing music from streaming ZIP/DXA and HOG containers

## Constraints

- Keep all product changes in branch-added Android files
- Reuse `ExtractionBudget` and `MissionMusicCatalogBudget`
- Preserve data-descriptor entry sizes discovered during streaming
- Treat zero or negative compressed metadata conservatively so ratio accounting cannot be disabled
- Thread one budget through nested ZIP/DXA and HOG work
- Bound MIDI materialization and honor cancellation
- Do not run Gradle until the root agent grants the serialized slot

## Work

- [x] Audit streaming ZIP/DXA, HOG, catalog, staging, and MIDI paths
- [x] Implement exact source-size and cumulative nested-budget propagation
- [x] Add descriptor/unknown-size, expansion, nesting, MIDI, cancellation, and ordinary regression tests
- [x] Run scoped formatting and static checks
- [x] Run focused Gradle tests after the serialized slot is granted
- [x] Report changed paths, diff metrics, validation, and remaining risks

## Results

Streaming DXA entries are now consumed before their data descriptors are trusted, so catalog tracks retain exact expanded sizes and unusable compressed metadata cannot bypass final ratio validation. One attempt-owned `ExtractionBudget` now covers catalog DXA/HOG expansion, compressed-audio staging, nested DXA-to-HOG work, and bounded MIDI reads. MIDI byte materialization is capped at 64 MiB and all extraction-budget operations honor thread interruption

Product changes are limited to branch-added Android Kotlin in `ExtractionLimits.kt`, `MissionMusicCatalogBudget.kt`, `MissionZipMusic.kt`, and `MissionZipMusicStageManager.kt`. Tests were extended in `ExtractionLimitsTest.kt`, `MissionMusicCatalogBudgetTest.kt`, and `MissionZipMusicStageManagerTest.kt`. No `d1/` or `d2/` file changed

Validation passed:

- Focused Gradle command covering all three suites completed successfully in 8 seconds after the final warning cleanup
- `ExtractionLimitsTest`: 13 tests, zero failures or errors
- `MissionMusicCatalogBudgetTest`: 11 tests, zero failures or errors
- `MissionZipMusicStageManagerTest`: 15 tests, zero failures or errors
- Scoped code-quality formatting and lint passed for all owned paths
- Repository `git diff --check` passed

Coverage includes real streaming data descriptors, exact and one-over cumulative expansion output, expansion-ratio rejection, zero and negative compressed metadata, shared nested DXA/HOG budgets, oversized MIDI rejection before reading, cancellation, sequential attempts, and ordinary top-level/nested staging and MIDI regressions

## Diagnosis reopening, 2026-10-09

Status: reopened; prior completed selected-entry implementation and historical validation above remain valid at their recorded scope

Nonselected DXA descriptor entries in actual staging/read helpers are drained by ZipInputStream.closeEntry without shared actual-byte/cancellation accounting or final resolved-size/ratio validation. Staged subclass overrides close only. Compressed-source/central-directory bounds do not establish expanded-work limits. Immutable source-only evidence: GQR-0106 nonselected streaming entry budget diagnosis reopening 20261009, SHA256f27b2fc073e18c410d596f74103ea3e461a8eee0b5a8633fa9cea8be602f2dfa in general_code_quality_evidence_ledger_20260811.md

- [ ] Carry one production budget through every seek/drain, including skipped and directory entries, before explicit/implicit closeEntry
- [ ] Reconcile final descriptor sizes and usable compressed ratio after accounted EOF; check cancellation between chunks
- [ ] Preserve selected payload/HOG exactness,64MiB MIDI admission and intended nested-work accounting without double charges
- [ ] Validate real staging and MIDI read routes with skipped-before-selected descriptor entries, selected-first/last, nested HOG, exact/one-over per-entry/aggregate/ratio, interruption and no partial publication/cleanup
- [ ] Use injectable small limits or controlled stream seams instead of huge media/resource probes; deferred fault/security/resource execution requires later authorized tranche

No code/test edits or fresh execution in diagnosis tranche. GQR0107 leases and source-generation identity remain separate owners

## Catalog producer follow-up, 2026-10-09

GQ2-CHUNK-0029 extends the existing reopened streaming root to MissionZipMusic.scanDxa. Unsupported-extension and directory descriptor entries closeEntry outside actual-byte/EOF size-ratio accounting. ExpansionBudget ordinary IOException can pass into per-source malformed-container catch while independent valid music remains, so a null result from a sole rejected DXA fixture does not prove whole-attempt rejection

- [ ] Use accounted skips/drains in catalog producers as well as selected staging/read routes; validate resolved descriptor sizes/ratios and check cancellation
- [ ] Propagate typed expansion-policy/cancellation rejection past optional-source catch, preserving ordinary malformed optional-source isolation and existing GQR0105 shared work/retained accounting
- [ ] Extend small injected-limit acceptance to rejected nested container plus valid peer music in both orders, direct/extracted routes, unsupported/directory members and exact/one-over totals/ratios; require no mixed cache/sidecar publication

No implementation or fixture execution here. Preserve original selected-path and cardinality repairs and historical validation. Full report imported as GQ2-CHUNK-0029 mission extraction generation music catalogs and names diagnosis 20261009

## Shared streaming consumer coordination, 2026-10-09

GQ2-CHUNK-0030 named direct SetupFileImport238-348 registers every entry and accounts selected writes, then invokes closeEntry331 on every member. Preserve DONE GQR0090 selected/SOW continuity repair; analogous nonselected actual/descriptor drain acceptance must be coordinated with this existing reopened streaming helper root. Source staging can repeat zero reads without cancellation checks. Future shared policy must cover source staging, producer and skipped consumer work with typed cancellation and actual accounting, preserving separate current fixed source/preamble caps and legacy-name compatibility. No runtime or code/test change here

## Discriminating MIDI admission fixture, 2026-10-09

GQ2-CHUNK-0055 assigned stage fixture declares64MiB+1 but uses missing.zip. Removing size guard119 still returns null at source existence121, so existing assertion does not independently prove pre-read size rejection. Preserve actual selected64MiB implementation and historical results; no status/root change.

- [ ] Add ordinary valid source and small injected read limit/open-read observation with exact/one-over and valid control, so bypassing guard cannot pass through an unrelated missing-source rejection
- [ ] Retain exact selected returned bytes and test actual direct/extracted/nested routes; no64MiB fixture allocation solely to prove admission
- [ ] Keep existing skipped descriptor/catalog drain and typed policy acceptance, including unsupported padding before selected tracks and independent valid peers in both orders

Diagnosis only, no guard mutation, product/test edit or probe. Coordinated plan_gq2_0055_music_fixture_continuation_20261009.md preserves0093/0094/0102/0107 ownership.
