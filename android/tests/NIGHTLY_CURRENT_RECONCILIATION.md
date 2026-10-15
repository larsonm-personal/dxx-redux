# Current nightly coverage accounting

The [historical 158-selection snapshot](NIGHTLY_COVERAGE_RECONCILIATION.md) remains unchanged. Its 881-file pending-admission count described that earlier snapshot, not the current owner audit.

The current [machine-readable reconciliation](nightly_current_reconciliation.json) separates three questions:

1. **Source ownership:** which existing dispatcher or assertion owner includes a candidate? The [ownership map](nightly_source_ownership.json) retains declared JSONC/PowerShell ownership even when its fixture is unavailable. Source-set membership does not prove that a JVM class contains runnable tests.
2. **Runnable admission:** which exact reviewed profile IDs are selected by the current policy? Public/shareware, retail, device, graphics, and game variants are not interchangeable. Every one of the original 346 master owners has a source-backed prerequisite decision in the [148-owner host audit](HOST_OWNER_AUDIT.md) or [198-owner Android audit](ANDROID_OWNER_AUDIT.md). Audited-but-unselected is an explicit eligibility decision or remaining adapter/provisioning requirement; see each linked audit for its concrete reason. It is not an unexplained audit backlog.
3. **Executed evidence:** only per-run receipts and discovered leaf results establish execution. Policy admission and source ownership never imply a passing test. Setup failures, interruptions, internal missing-media skips, and unsupported target capabilities retain distinct outcomes.

## Regeneration

Run `python3 android/tests/reconcile_nightly_coverage.py` after an admission freeze. The generated file records the exact policy SHA-256, selection count, all 1,239 original candidate paths, all 346 audited owners, and all 91 textual CMake mentions. Re-running without input changes produces identical bytes. New infrastructure source files added after the pinned inventory are not silently included in these historical counts.

The `files` array filtered to `classification == "still-unmapped"` is the complete remaining path-level mapping list. Known child ownership with an unselected fixture stays `child-of-audited-unselected-owner`; it is not discarded as non-test support. A child of a selected profile is a source relationship only: actual child execution and profile-specific exclusions must still be established by receipts.

Two legacy FM CMake mentions use sources tested through the full extractor, but their separate legacy configuration is not runtime-qualified. The PCM-ring CMake owner needs its existing NDK/ADB wrapper. These are recorded separately rather than claiming every CMake configuration ran.

## Runtime evidence at the expansion freeze

Hosted results reported by the integration run establish 1,187 JVM test passes with four skips, 71 D1 native CTest passes, and successful strict-fingerprint assertions. The first full extractor run had 51 passing registrations, one skip, three partial media exclusions, and three SoundFont-related failures. The verified SoundFont setup fix was then exercised in [run 38795934448](https://github.com/larsonm-personal/dxx-redux/actions/runs/38795934448) at `b2eae695b6d8e4540448f352321e6e3e2debbe7b`: all three SoundFont tests passed, yielding 54 fully passed registrations, one skip and three partial-media exclusions. These counts are separate units and must not be added to source-file or selection counts. They do not qualify every admitted owner, D2 native runtime, or all Android/device profiles.

Exact current admission and unresolved-path counts appear in the generated JSON. No all-tests-supported or all-tests-passed claim is made while mappings or runtime evidence remain incomplete.
