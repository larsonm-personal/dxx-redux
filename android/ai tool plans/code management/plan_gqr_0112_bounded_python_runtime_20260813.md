# GQR-0112 Bounded Python Runtime Plan

## Scope

- Remediate `GQF-0125` in branch-added extraction scripts and tests
- Remove command-name and `PATH` selection from bounded extractor launch
- Keep inherited `d1/` and `d2/` files unchanged

## Runtime policy

- Prefer the installed repository-pinned Python 3.12.8 Mac oracle runtime on Windows
- Verify the repository runtime tree against `tool_versions.conf` before execution
- Permit an explicit cross-platform runtime only when the caller supplies its literal path and SHA-256 through parameters or the dedicated environment pair
- Require the selected runtime to report the pinned Python version and its own canonical executable path
- Fail closed for missing, damaged, mismatched, ambiguous, or unsupported runtimes
- Never fall back to `python`, `python3`, or `py` command lookup

## Work phases

- [x] Add runtime resolution and admission to `bounded_extraction.ps1`
- [x] Route `Invoke-BoundedExtractor` through the admitted runtime using argument arrays
- [x] Add focused tests for hostile `PATH`, missing and damaged runtimes, explicit overrides, spaces, and platform policy
- [x] Run focused PowerShell and Python regression tests
- [x] Run scoped code quality and record final metrics

## Validation target

- Windows repository runtime succeeds after tree, version, and executable identity checks
- Hostile `PATH` entries are never invoked
- Missing or damaged repository runtime fails before bounded extraction starts
- Explicit overrides require a valid digest and exact Python 3.12.8 identity
- Paths containing spaces remain literal arguments
- Non-Windows hosts require an explicit admitted runtime until a pinned repository package exists

## Completed validation

- `test_bounded_python_runtime.ps1` passed hostile `PATH`, repository provenance, explicit SHA-256, missing path, missing digest, damaged digest, wrong version, non-Windows policy, and spaced-path launch cases
- `test_bounded_extraction.ps1` passed bounded ZIP and four verified installer-package cases
- `test_run_bounded_extractor.py` passed 6 supervisor resource-limit tests under the pinned isolated runtime
- `test_extract_all_cds_batch.ps1` and `test_extract_all_gog_batch.ps1` passed
- Scoped `run-code-quality.ps1 -Fix` passed for both changed PowerShell files
- `git diff --check` passed

## Diff metrics

- `android/helpers/bounded_extraction.ps1`: 107 insertions, 10 deletions
- `android/tests/test_bounded_python_runtime.ps1`: 109 new lines
- Inherited `d1/` and `d2/` changes: zero
- All three owned files are printable ASCII without a UTF-8 BOM

## Current diagnosis reconciliation, 2026-10-09

GQ2-CHUNK-0037 retains DONE GQR-0112/FIXED GQF-0125 runtime identity repair. Current implicit policy also supports the repository-pinned Linux x64 tree; original Windows-only and explicit-only non-Windows validation target above is historical. Current test checks actual copied repository tree plus unexpected-module rejection and spaced explicit override. No fresh runtime or tests executed in this diagnosis tranche.

Complete setup/metadata request supervision remains distinct: identity probe and Kotlin CLI build/selection run synchronously outside extractor-child deadline. Coordinate finite preflight/CLI deadline, capped response and cancellation with existing BR-0582/GQR-0212 host owners; preserve hash/version/canonical path validation and DONE extractor process/output repairs. See plan_br_0018_host_mission_archive_admission_20261009.md for actual host expansion continuation; no tool-identity reopening.
