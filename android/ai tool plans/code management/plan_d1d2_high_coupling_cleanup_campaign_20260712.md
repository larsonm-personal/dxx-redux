# D1/D2 high-coupling cleanup campaign

## Goal

Process the remaining high-value D1/D2 diff-minimization candidates with the fixture and integration coverage required for their higher coupling. Defer the input-demo direct-command policy until every non-demo-breaking tranche is complete and validated so demos are re-recorded only once from a clean final state.

## Existing work to preserve

- Preserve the completed low-risk diff-minimization campaign and its shared helpers
- Preserve unrelated route-planner, guidebot, mission metadata, and outstanding-bug work
- Keep desktop D1/D2 and Android D1/D2 behavior supported throughout
- Keep D1/D2 file formats and game-specific policy in engine-owned C code

## Ordered tranches

### H01. Playsave transactional repair and format fixtures

- [x] Refresh the live D1/D2 launcher-bridge layout audit against current code
- [x] Add byte-accurate D1/D2 PLR and PLX fixtures covering supported versions and layout variants
- [x] Add short-file, malformed-section, oversized-PLX, and injected-write-failure coverage
- [x] Introduce atomic shared I/O and bounded PLX rewrite primitives
- [x] Correct layout/version handling through compact per-game descriptors kept next to canonical engine format knowledge
- [x] Extract common launcher bridge mechanisms and minimize inherited `playsave.c`/header churn
- [x] Build and run host and all-ABI Android coverage; JNI callers compile against the repaired bridge

H01 evidence:

- D1 fixtures cover versions 4-8, shareware and registered v7/v8 layouts, and both 8-byte D1X variants
- D2 fixtures cover versions 17-24 structural boundaries and byte-swapped v24 headers
- Atomic failure fixtures cover write, sync, and replace failures without original-file mutation
- PLX fixtures preserve a 65,537-byte payload, unknown keys, outer `[end]` placement, and unterminated target-section repair
- Windows D1/D2 builds and all six focused executables pass
- Android native builds pass for arm64-v8a, armeabi-v7a, and x86_64

### H02. Kconfig launcher control-layout descriptors

- [ ] Capture D1/D2 launcher-setting mappings in table-driven fixtures before movement
- [ ] Define compact game-specific override descriptors
- [ ] Move pair/default construction into shared code without changing indices or defaults
- [ ] Run controller comparison, keyboard defaults, and launcher roundtrip coverage

### H03. Host migration and disconnect coverage

- [ ] Reconcile existing host-migration plans and current implementation
- [ ] Add deterministic host-loss/disconnect coverage before extraction
- [ ] Separate common election/reset/ownership mechanics from D2-only ownership transfer
- [ ] Validate two-emulator host loss, reconnect, object ownership, and guidebot authority

### H04. Private newmenu rendering state

- [ ] Re-audit newmenu/listbox residuals after the shared scaled-render callback work
- [ ] Extract only callbacks that are smaller than the duplicated bodies
- [ ] Keep private layout, selection, and hit-testing policy local where abstraction would increase inherited churn
- [ ] Run menu scale, readability, touch, pause, and listbox coverage

### H05. D2 classic-demo serialization

- [ ] Snapshot representative classic-demo JSON output fixtures
- [ ] Separate serialization from private parser state through a compact immutable record/snapshot boundary
- [ ] Preserve parsing, mount, error, and output ordering semantics
- [ ] Validate exact normalized JSON output and failure cases

### H06. Input-demo direct-command policy and demo refresh

- [ ] Begin only after H01-H05 are complete and fully validated
- [ ] Define shared command iteration and explicit D1/D2 policy adapters
- [ ] Accept input-demo format/behavior compatibility breaks where they simplify the correct engine boundary
- [ ] Run deterministic record/replay and final-state coverage
- [ ] Re-record affected regression demos once from the final clean implementation
- [ ] Regenerate and verify associated state and RNG traces

## Validation gates for every tranche

- Run scoped code quality and `git diff --check`
- Build all configured Android ABIs
- Build Windows D1 and D2
- Run focused host fixtures before emulator tests
- Run emulator tests sequentially with cleared logcat and file-backed output
- Record exact before/after inherited-file metrics and update the candidate catalog

## Current status

- [x] Campaign order established with demo-breaking work last
- [x] H01 playsave transactional repair and format fixtures complete
- [ ] H02 Kconfig descriptor audit in progress
