# Nightly source reconciliation

Source snapshot: `e12e0bcf4c2fe6e3edd0302f75dd5c4666e9bb70`. Policy SHA-256: `ab1fb9ab736f23a05d0330a4b3b2236695d996e4aed63113f004a60575633489`.

This is an admission map, **not executed coverage or a claim that every existing test is supported**. The machine-readable [per-path map](nightly_source_reconciliation.json) contains every one of the 1,239 discovered candidate files, its classification, exact selection IDs, known owner, and reason. It also lists all 91 textual CMake registration mentions separately.

## Counts

The policy has **158 selectable IDs**. Files and selections are different units: shareware D1/D2 variants produce multiple selections, while a single Gradle selection discovers many files.

- child-of-admitted-owner: 205 candidate files
- directly-selectable: 126 candidate files
- excluded: 6 candidate files
- still-unmapped: 881 candidate files
- support/non-test: 21 candidate files

The 346 existing direct master owners reconcile as {"directly-selectable": 54, "excluded": 6, "still-unmapped": 286}. Two additional input-demo matrix wrappers are separate candidates and are not silently treated as all child variants covered.

CMake mentions: {"child-of-admitted-owner": 86, "still-unmapped": 5}. Runtime `ctest --show-only=json-v1` and result receipts, not textual mention counts, define actual registrations. The shared engine template expands differently for D1 and D2.

## Limits and next work

- Unmapped is an explicit audit backlog. It does not mean licensed, physical-only, unsupported, or passed. In particular, merely mentioning retail media in a helper is insufficient grounds to exclude it.
- Declared support JSONC/PowerShell files whose owners are not admitted stay unmapped: their assertions are not hidden as non-tests.
- Gradle source-set mapping includes possible fixture/helper classes; only actual discovered test results earn leaf coverage.
- Public shareware admission covers exactly the listed game variants. It does not cover retail versions, extended graphics/multiplayer matrices, or published-package variants.
- Full extractor and native owners may be admitted before runtime dependencies/assets are available. Setup failures remain retryable; licensed-media/internal skips remain visible exclusions, never passing leaves. The optional light extractor profile is separate and is absent from this pinned source commit.
- Six files retain the master runner's explicit manual exclusion. No other candidate was labelled unavailable merely from keyword inference.
- Hosted full native/extractor/audio qualification is pending. Local qualification of the adapter was 17 contract tests, 4 real Python tests and two repeated light-profile runs; these do not qualify all admitted selections.

## Remaining unmapped candidate files

The complete machine-readable backlog is intentional: no broad completion claim is appropriate while it is nonempty. Reasons and declared owners are in the linked JSON.

The full list is the `files` array filtered to `classification == "still-unmapped"` in [nightly_source_reconciliation.json](nightly_source_reconciliation.json). Each entry has its exact path and concrete audit reason.

- instrumentation: 11 unmapped candidate files
- jsonc: 410 unmapped candidate files
- ps1: 372 unmapped candidate files
- py: 80 unmapped candidate files
- sh: 8 unmapped candidate files

## Unmapped CMake registration projects

- `android/tests/fluidsynth_quality/CMakeLists.txt`
- `android/tests/fm_quality/CMakeLists.txt`
- `android/tests/pcm_ring/CMakeLists.txt`
- `android/tools/etc2tool/CMakeLists.txt`
