# GQR-0048 complete CD attempt budget plan

Date: 2026-08-12

## Objective

Close `GQF-0061` by carrying one attempt-owned output-byte, entry,
memory, cancellation, and free-space budget through every CD data track,
ISO-to-HFS fallback, and nested SOW extraction. Preserve transactional cleanup
and make exact-limit success and one-over rejection executable contracts.

## Scope

- Branch-added Android/native extraction code and its tests/build registration
- Standalone `extract_cd` composition and Android `DiscImportBridge` composition
- No `d1/` or `d2/` edits
- Do not change STi2 method-15 decoder internals owned by `GQR-0038`
- Do not edit the canonical quality ledger or the next-ten orchestration plan

## Work plan

- [x] Read repository instructions, `GQF-0061`, its durable evidence, and callers
- [x] Add a reusable native attempt-budget contract with test-sized limits
- [x] Thread one budget through ISO, HFS fallback, and SOW extraction APIs
- [x] Carry one budget across standalone CLI and Android multi-track composition
- [x] Add exact-limit, one-over, cancellation, memory-release, and cleanup coverage
- [x] Run focused native and Kotlin tests
- [x] Run the full extraction suite and Android ABI builds as feasible
- [x] Run scoped quality checks and prove zero `d1/` or `d2/` edits
- [x] Record final changed paths, metrics, validation, and limitations here

## Completion evidence

- One `dxx_extract_attempt_budget_t` now owns aggregate output bytes, entries,
  live memory, cancellation state, and free-space checks
- The same object crosses every ISO track, ISO-to-HFS fallback, mapped STi2
  installer, direct HFS fallback, and nested SOW archive in both CLI and Android
- Exact output/entry/memory limits pass; one-over reservations fail without
  changing state; memory release and sticky cancellation are covered
- Kotlin composition coverage proves one object identity across two tracks and
  nested post-processing, and proves staging cleanup
- Focused Windows native targets built: `test_extract_limits`, `test_cue_iso`,
  `test_sow_integrity`, `test_sti2`, and `extract_cd`
- Focused CTest passed 4/4: extract limits, CUE/ISO, SOW integrity, and STi2
- Focused Kotlin `CueDataTrackExtractionTest` passed 11/11
- Scoped clang-format, ktlint, CMake formatting/lint, and BOM checks passed
- Android `assembleDebug` reached native compilation but was blocked by the
  unrelated concurrent `shared/secretarea.c` unterminated `#ifdef __ANDROID__`
  at line 2697, before completing arm64-v8a; other ABIs were not attempted
- This item made zero `d1/` or `d2/` edits. Concurrent workers have unrelated
  edits there, so repository-wide `git diff --name-only -- d1 d2` is not empty


## Standalone ISO production composition reopening, 2026-10-09

Status: reopened TODO under existing GQF-0061/GQR-0048. Prior completed CUE/CLI/native contract, focused validation and Android build limitation above are preserved

- Exact imported evidence: GQ2-CHUNK-0021 installer disc extraction and JNI diagnosis 20261009, SHA256 fc376bad64accfd9f9d449e79e4bcaffef93a2f885355a995ba5e14882183409
- Frozen GQ2/current extractIsoDiscContent invokes production importIsoImageFromPath lambda with no shared attempt. That lambda omits DiscImportBridge.extractIsoImageFiles attempt, creating one default object; subsequent postProcessImportedDiscFiles omits its attempt, creating another. Complete-attempt output/entry/cancellation counters reset between primary and nested extraction
- Static counterexample: primary A<=L and nested B<=L independently satisfy per-attempt output ceiling while A+B>L. No media/resource/security execution or physical exhaustion claim; prior CUE object-identity fixture does not call this adapter

Required continuation:

- [ ] Trace every standalone ISO UI/path production caller and callback lifetime, preserving staged publication and descriptor closure
- [ ] Own one attempt in extractIsoDiscContent and pass it through primary callback and nested postprocess; preserve CUE/CLI continuity
- [ ] Coordinate pending-exception native-only unwind and typed cancellation with BR-0021/BR-0044; no later JNI state store under first exception
- [ ] Verify real production object/counter continuity, combined bytes/entries exact/one-over, sticky cancellation, nested failure, no fallback/publication and owned cleanup; avoid duplicate arithmetic-only or supplied CUE stub acceptance
- [ ] Verify ordinary ISO/SOW completion and staged rollback; execute deferred allocation/resource/fault acceptance only when user scope permits
- [ ] Reconcile current SOW-volume extension and final caller generation before closing all composition paths again

Diagnosis only: no implementation/test change or fresh runtime/build credit

## CUE fixture proof and production acceptance, 2026-10-09

GQ2-CHUNK-0054 reads all17 assigned CUE fixture hunks and complete current338 lines, actual picked/path callback and JNI ISO/HFS state mapping scopes. New same-instance test mutates state[0] itself and observes0/1/3 across callbacks. Preserve this routing contract; it does not prove native byte/entry/memory/cancel enforcement, standalone ISO composition or sourceName managed publication.

- [ ] Validate actual production/JNI state continuity across primary ISO, HFS fallback and nested SOW with small injectable exact/one-over limits, sticky cancellation, no publication on failure, prior-byte preservation and owned cleanup
- [ ] Cover real standalone ISO callback/postprocess with one attempt; current adapter still resets defaults despite CUE test
- [ ] Keep existing sector-specific arithmetic/overflow, CUE order, ordinary rollback and cancellation fixtures; cover full peak-live source/stage/output/backup generations under existing composition/storage owner
- [ ] Coordinate BR0021/0044 exception-safe native state unwind and0033 publication without duplicate fixture-only implementation

Diagnosis only; no code/tests/runtime/media/resource probes or status changes. Historical focused11/11 remains historical.
