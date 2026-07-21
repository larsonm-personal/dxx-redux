# Temp artifact retention research

- [x] Inventory existing test and build cleanup helpers and ignore rules.
- [x] Trace the producers and consumers of recurring temp directories, especially mission metadata runs.
- [x] Measure current artifact generations and disk usage without modifying them.
- [x] Classify reusable caches separately from disposable run outputs and identify unsafe cleanup cases.
- [x] Recommend a helper-script interface, retention rules, safety checks, and tests.
- [x] Report findings without implementing or deleting files.

## Findings

- `android/temp/mission_zip_host_metadata` contains 34 timestamped runs totaling 107.88 GiB. Keeping the newest run would reclaim 103.19 GiB. The runs are diagnostic snapshots and are never read as caches by later runs.
- The host metadata helper extracts archives into `raw`, then copies flattened inputs into `stages`, so large inputs can occur twice per completed run. There are 33 `ewithin-xl.zip` copies totaling 16.21 GiB.
- `android/build-outputs` contains 663 internal, 11 debug, and 64 release timestamped AABs plus one `app-universal.apk`, totaling 42.49 GiB. Keeping the newest AAB in each variant family and the unversioned APK would reclaim 42.26 GiB. Deploy and install helpers intentionally select the newest AAB.
- `temp/input_demo_determinism_matrix` contains 99 timestamped runs totaling 2.53 GiB. Keeping the newest would reclaim 2.47 GiB.
- Old default-generated files in `temp/test_reports` account for another 0.32 GiB. Strict timestamp grouping is required because the same directory also contains manually named diagnostic subdirectories.
- The conservative first implementation can reclaim about 148.24 GiB from these four explicitly recognized families.
- Preserve `buildd1`, `buildd2`, `android/app/.cxx`, `android/app/build`, `android/.gradle`, dependency downloads, emulator/AVD state, input data, and unrecognized or manually named temp paths. These are incremental caches, required inputs, or ambiguous user artifacts.
- Default behavior should be a dry-run inventory. An explicit `-Apply` switch should remove only strict allowlisted patterns, retaining one newest generation per family by default.
- Before applying, resolve and validate every target beneath its exact managed parent, refuse to run while a related producer/deploy/install process is active, skip reparse points, and preserve recently modified candidates as an additional race guard.
- Tests should use a synthetic repository tree and verify preview mode, `-Apply`, per-family retention, incomplete runs, unknown-name preservation, cache preservation, path containment, and idempotence.

## Implementation

- [x] Add a preview-first cleanup helper with strict managed-family discovery.
- [x] Add explicit apply, retention, recent-file, process, containment, and reparse-point safeguards.
- [x] Cover managed directory, packaged AAB, and flat report-file retention with fixture tests.
- [x] Perform static parsing and scoped code-quality checks without executing the cleanup helper.
- [x] Document usage and verification limits.

The helper is `android/helpers/clean-old-artifacts.ps1`. It previews by default and requires `-Apply` before it can remove anything. The fixture test is present but was intentionally not executed at the user's request. Static PowerShell parsing and scoped code-quality checks passed for both new scripts.

## Class-based redesign

- [x] Inventory every recurring temp/build artifact naming and lifecycle pattern produced by repository scripts.
- [x] Define reusable artifact classes, derived family keys, and per-class retention counters.
- [x] Replace path-specific cleanup discovery with scratch-role and timestamp-convention discovery.
- [x] Bound discovery so stable workspaces, caches, source checkouts, and deeper trees are preserved.
- [x] Replace the fixture test with class, family, retention-counter, location-independent, and safety coverage.
- [x] Perform static parsing and scoped code-quality checks without running cleanup.

The recurring producers fall into timestamped generation directories, timestamped output files, stable workspaces, stable singleton outputs, and reparse points. The first two classes accumulate and receive independent keep counters. The remaining classes are counted and always preserved. Family keys are derived from the parent directory and a filename template with its timestamp and build version normalized, so new timestamped producers do not require cleanup-script edits.

Default scratch roots are discovered by role from `temp*` and `build-outputs` directories directly below the repository and Android roots. Discovery examines direct children and one collection level. This covers current run buckets, reports, packages, warnings, and debug logs while avoiding recursion into cached checkouts, build trees, emulator state, or other stable workspaces. Callers can add or replace roots with `-Roots` without changing the implementation.

The redesigned helper was not run against either the repository or a fixture, following the user's instruction. Static PowerShell parsing and scoped code-quality checks passed.

## Fixture isolation

- [x] Move the cleanup test fixture outside the repository checkout.
- [x] Make synthetic artifact names and extensions explicitly test-only.
- [x] Preserve unconditional cleanup and static verification without executing the test.

The test now creates a uniquely named fixture below the operating-system temp directory. Synthetic paths use `TEST_ONLY` labels and `.test-output` extensions instead of plausible project package or report names. The entire fixture is removed in `finally`, and no test artifact is created within the checkout.

## Empty-class counter fix

- [x] Guard byte aggregation for artifact classes with zero eligible items.
- [x] Guard total aggregation for a completely clean workspace.
- [x] Add static verification without executing cleanup.

PowerShell emits no `Measure-Object` result for an empty pipeline under this strict-mode access pattern. Both aggregations now start at zero and call `Measure-Object` only for non-empty collections. Static parsing and scoped code-quality checks passed.

## Producer-side retention

- [x] Reconfirm all tracked producers that create accumulating timestamped artifacts.
- [x] Add one shared producer retention entry point with a default keep count of five.
- [x] Invoke retention from package, mission, test-suite, determinism, and warning producers.
- [x] Ensure producer calls scope cleanup to their own artifact families.
- [x] Add static and fixture coverage without cleaning repository artifacts.

`retain-recent-artifacts.ps1` accepts the artifacts created by the current producer run, derives their family identities through the class-based cleanup engine, and keeps the newest five generations of only those families. This prevents a warning-log run from rotating unrelated test results that happen to share the same `temp` root. Repository-external custom outputs and non-timestamped custom mission batch directories are left alone.

Producer hooks now cover timestamped AAB packages, emulator and host mission metadata batches, full and quick test reports/logs/result directories, determinism matrix runs, and Android/MSVC warning logs. The emulator metadata wrapper is covered through its call into `run_mission_zip_batch.ps1`.

All changed PowerShell files pass parser checks and the repository's scoped code-quality pass. The deletion fixture was extended to verify the default five-generation rotation, but neither it nor either cleanup entry point was executed, following the user's instruction.
