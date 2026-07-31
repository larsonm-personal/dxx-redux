# Master Regression Data Regeneration

- [x] Identify the canonical wrappers for CD extraction regressions, fingerprint/AcoustID data, and mission level metadata.
- [x] Add one `android/` master script that invokes those wrappers and stops at the first failure.
- [x] Preserve ISO-based CD extraction/regression behavior when a directory also contains a fingerprint-only cue sheet.
- [x] Add focused tests for stage selection and stop-on-error behavior.
- [x] Run the focused PowerShell tests and scoped code-quality checks.

Follow-up from the first full run:

- [x] Diagnose the ten retail-disc extraction failures.
- [x] Move the bounded 4096-entry ISO catalog to heap storage instead of the native thread stack.
- [x] Accept the zero-length root identifier used by the supported Quartzon media.
- [x] Add coverage for a 1025-entry catalog and zero-length root identifiers.
- [x] Rebuild and run the native ISO tests.
- [x] Re-run forced extraction across all CD fixtures.

Follow-up from the second full run:

- [x] Diagnose the disappearing Mac `*.sti2.tmp.*` measurement race.
- [x] Ignore only `FileNotFoundError` when a measured output vanishes after enumeration.
- [x] Add deterministic coverage for the disappearing-file race.
- [x] Re-run the bounded-extractor tests and forced d2 Mac extraction.

Follow-up from the first complete Android regression run:

- [x] Confirm the ten failures used the old installed APK and all stopped at `import_failed`.
- [x] Make the canonical CD wrapper build and reinstall the current debug APK before device tests.
- [x] Add stage coverage requiring the fresh-APK option.
- [x] Run focused wrapper tests and affected Android extraction regressions.

Follow-up from the latest full regeneration attempt:

- [x] Isolate the most recent invocation and classify its terminal failure.
- [x] Audit generated CD, fingerprint, AcoustID, and mission metadata diffs for regressions.
- [x] Trace suspicious output changes to their responsible generator behavior.
- [x] Report evidence-backed causes and separate fixes from safe regenerated output.

Fixes from the latest full regeneration attempt:

- [x] Restart the emulator before the full extraction suite and after infrastructure failures.
- [x] Allow successful file-only evidence to replace stale failures for non-launchable media.
- [x] Preserve valid cached AcoustID metadata when a refresh returns no usable result.
- [x] Verify automation scripts after ADB staging and retry the spec after infrastructure failure.
- [x] Add focused regression coverage for all four behaviors.
- [x] Run scoped tests, quality checks, and affected integration regressions.

Follow-up from the ewithin mission soundtrack run:

- [x] Stop double-counting nested archive containers and their contents against one declared-size budget.
- [x] Retain per-container declared-size checks and bound concurrently retained output.
- [x] Add nested-archive regression coverage.
- [x] Re-run the ewithin fingerprint stage and fingerprint-data wrapper.

Regression timestamp hygiene:

- [x] Treat the header timestamp as the spec-generation date, not the latest test-analysis date.
- [x] Preserve the existing timestamp for shared-writer updates unless the generator explicitly supplies a new one.
- [x] Add coverage for semantic no-ops and test-result updates.
- [x] Remove all timestamp-only regression-file changes from the current diff.

Validation:

- `android/tests/test_regenerate_all_regression_data.ps1`
- `android/tests/test_extract_all_cds_batch.ps1`
- `android/tests/test_generate_regression_specs.ps1`
- `android/tests/test_cd_regression_runner.ps1`
- PowerShell parser checks for all touched scripts
- Scoped `android/run-code-quality.ps1 -Fix` and check-only pass
- `git diff --check`
- Native ISO/CUE tests: 66/66 passed
- Android `assembleDebug`: passed with JDK 21
- Forced CD extraction: 34 succeeded, 0 failed
- Bounded-extractor Python tests: 6 passed
- Bounded ZIP/child-process PowerShell tests: passed
- d2 Mac bounded extraction stress check: 5/5 passed
- Fresh-APK Android regressions: Anniversary ISO, Definitive Collection USA Disc 1, and Quartzon USA passed
- Affected Android regression subset: 8/8 passed after the Logitech automation-staging rerun
- CD regression evidence after affected reruns: 33 passed, 1 intentional Android demo-launch skip, 0 failed
- AcoustID preservation audit: 0 unchanged-fingerprint cached music labels lost
- Forced 25-archive music regeneration: passed; failed/filtered lookups preserved cached metadata
- Known-disc semantic audit: 68 to 69 discs, 722 to 723 tracks, 37 validated recording IDs added
- ewithin mission soundtrack: 66 tracks extracted and fingerprinted
- Mission ZIP wrapper: 113 processed, 10 with audio, 0 failed
- Forced regression-spec regeneration: timestamp-only targets remained byte-for-byte unchanged
- Regression diff hygiene audit: 0 timestamp-only regression-file changes
