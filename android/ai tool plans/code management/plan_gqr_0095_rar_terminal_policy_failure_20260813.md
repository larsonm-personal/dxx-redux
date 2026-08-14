# GQR-0095 RAR Terminal Policy Failure Plan

## Goal

Keep native RAR policy, resource-limit, cancellation and integrity failures terminal while allowing host-tar fallback only when the native backend is safely unavailable or does not support the archive

## Constraints

- Keep all product changes in branch-added Android launcher and extraction files
- Do not edit `d1/` or `d2/`
- Preserve typed failure meaning through the complete RAR dispatcher and caller path
- Preserve safe capability fallback for unavailable or unsupported native backends
- Do not edit the canonical quality ledger or root campaign plan

## Phases

- [x] Trace native RAR dispatch, host-tar fallback and all callers
- [x] Introduce the smallest typed fallback decision boundary
- [x] Add focused tests for capability fallback and terminal policy, limit, cancellation and integrity failures
- [x] Run focused tests and scoped code quality
- [x] Compile the affected Android launcher code as feasible
- [x] Record exact validation and diff metrics

## Initial finding

`extractRarArchiveToDirectory` catches every native exception except `ArchiveOutputValidationException`, clears the destination and invokes host tar. This weakens failures raised by `ExtractionBudget`, storage checks, cancellation or native integrity checks because their exception type is not classified before fallback

## Caller trace

- `ArchiveFiles.open` constructs `ExtractedReadableArchive` for RAR input, which uses the dispatcher and deletes its temporary extraction root on terminal failure
- Direct Setup import stages its source in `extractRarContents`, calls the same dispatcher, reports the propagated failure and deletes its work directory in `finally`
- Mission metadata, mission import, music staging, mod management and setup archive browsing all reach RAR extraction through `ArchiveFiles.open`
- No caller adds another extraction backend after the dispatcher returns a terminal failure

## Validation

- `:app:testDebugUnitTest --tests com.dxxredux.app.RarFallbackPolicyTest --no-daemon --console=plain` passed all 8 focused tests and compiled main and test Kotlin
- Scoped repository code quality passed for the tracked product file; direct ktlint format and check passed for both the product file and new test
- `git diff --check` passed for the combined worktree
- All three owned files are printable ASCII without a BOM
- Owned product and test hunks add 165 lines and remove 13 lines: 61/13 in `ArchiveFiles.kt` after excluding the concurrent bounded-read hunks, plus the 104-line focused test
- The 37-line durable plan brings the owned total to 202 additions and 13 removals
- No `d1/` or `d2/` file changed
