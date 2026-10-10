# GQR-0087 ZIP prompt admission plan

## Scope

- [x] Trace ZIP staging, structure validation, callers, tests, and prior finding evidence
- [x] Confirm the defect: a four-byte local-header marker lifts the 16 MiB preamble gate before any ZIP fields are validated
- [x] Remove marker-based lifting and hard-cap one-shot ZIP staging at 16 MiB plus one probe byte
- [x] Add focused forged-marker, malformed-structure, exact-limit, one-over-limit, valid-boundary, and temporary-file cleanup tests
- [x] Run focused JVM tests, scoped code quality, direct test formatting, and repository diff checks
- [x] Record exact files, metrics, validation, and remaining limitations for the parent ledger owner

## Constraints

- Keep product and test changes in branch-added Android Kotlin files
- Do not edit inherited D1 or D2 files, the canonical quality ledger, or the parent campaign plan
- Preserve concurrent extraction, networking, routing, and metadata work in the shared worktree
- Retain complete central-directory validation after bounded staging

## Outcome

- Product change: `ArchiveInputStreams.kt` no longer raises the staging ceiling after a raw `PK 03 04` marker. The one-shot `InputStream` path stages at most 16 MiB and consumes at most one additional probe byte before rejection, then validates EOCD, central-directory, and matching local-header structure before exposing entries
- Compatibility decision: one-shot ZIP and self-extracting inputs above 16 MiB are now deliberately unsupported. Supporting larger sources safely requires a seekable or reopenable source API that can validate trailing structure before large staging
- Tests: `ArchiveInputStreamsTest` now covers a coherent forged local header followed by unbounded garbage, exact and one-byte-over source limits, malformed structure, successful extraction, source close, and temporary-file cleanup
- Focused Gradle result: 61 tests passed across `ArchiveInputStreamsTest`, `MissionZipTest`, `MissionZipMusicTest`, and `MissionZipMusicStageManagerTest`
- Quality: scoped production wrapper passed; direct ktlint formatting passed for the test path that the wrapper did not discover; `git diff --check` passed
- Metrics: production `+10/-24`, focused tests `+130/-0`, inherited D1/D2 effect `+0/-0`
- Native/assemble validation was not run because this Kotlin-only item compiled through Gradle while unrelated native extraction files were under concurrent modification

## Current fixed source and preamble policy reconciliation, 2026-10-09

GQ2-CHUNK-0030 reviewed frozen512MiB source staging and current2GiB default, with separate16MiB preamble bound. Historical16MiB total-source compatibility wording above no longer describes current code. Raw markers do not raise either fixed source policy and complete structural validation still precedes exposing entries, so existing marker-bypass root remains FIXED/DONE. No fresh source/resource execution. GQR0088 complete EOCD candidates and GQR0089 emitted central-set identity remain open; larger staging is not proof of a prevalidated seekable admission path

## Current ZIP fixture evidence, 2026-10-09

GQ2-CHUNK-0054 reads all8 frozen fixture hunks, complete375-line current fixture and229-line production helper. Preserve fixed staging ceiling and separate preamble bound, local-only reserved bit15 compatibility and supported-stream flag mismatch rejection. Post-frozen CP437/UTF8 and512MiB stored-entry source fixture match current2GiB whole-source policy, but were not executed in diagnosis. Exact payload length/CRC and close/failure stage checks are useful; no fresh large-source acceptance.

Existing GQR0088 complete EOCD candidate selection and0089 central/sequential entry equivalence remain pending. Complete staging progress/cancellation/deadline stays with GQR0212, including count0 reads. Retain bounded small injected source-limit controls; no diagnosis resource probe or reopening completed marker-bypass repair. Historical61-test evidence is not a current execution.
