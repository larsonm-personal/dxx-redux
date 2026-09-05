# Parallel headless regression regeneration

## Phase 1: Inventory and constraints

- [x] Identify every host-only regeneration stage with independent work items
- [x] Document shared files, scratch directories, and result aggregation hazards
- [x] Choose a conservative automatic worker-count policy with an override

## Phase 2: Shared concurrency support

- [x] Add a reusable process-pool helper that does not open visible consoles
- [x] Keep status output and result retirement in the parent PowerShell process
- [x] Preserve deterministic artifact ordering and atomic checked-in writes

## Phase 3: Parallel runners

- [x] Parallelize headless Guide-Bot level simulations
- [x] Parallelize Windows-native mission metadata archive analysis
- [x] Apply the helper to any other safe host-only regeneration stage found
- [x] Keep headed and emulator-backed paths serial

## Phase 4: Verification

- [x] Add focused concurrency, failure, ordering, and worker-count tests
- [x] Compare serial and parallel outputs byte-for-byte on representative inputs
- [x] Run scoped code quality checks and relevant regression tests
- [x] Run Windows and Android builds plus D2 CTests if native sources are affected
- [x] Review the final diff for unrelated changes

No native, Kotlin, or Android source files were changed for this work, so platform builds and D2 CTests were not required
