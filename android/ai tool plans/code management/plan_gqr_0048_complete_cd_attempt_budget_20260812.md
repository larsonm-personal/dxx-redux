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
