# Warning and lint cleanup, 2026-07-14

## Scope

- Reduce current warnings reported by the repository's established C/C++, Kotlin, PowerShell, shell, and CMake checks
- Prefer branch-authored Android, build, and helper code
- Do not churn untouched original code under `d1/` or `d2/`; only consider legacy-tree files already changed relative to the appropriate upstream base
- Preserve the user's existing `android/outstanding_bugs.md` edit

## Plan and status

- [x] Read the repository instructions and identify the quality/warning helpers
- [x] Review prior cleanup documentation, helper behavior, branch diffs, and baseline output
- [x] Fix actionable findings without expanding the legacy source diff
- [x] Run scoped or full code-quality validation and relevant host/Android builds or tests
- [x] Record remaining intentional exclusions and final validation results

## Notes

- The working tree started with an unrelated modification to `android/outstanding_bugs.md`; leave it untouched
- The current branch is `cmake`; compare branch-authored legacy-tree changes against the best available merge base with `main` or `master`, and use `upstream` as additional context where useful
- Baseline Android build: 0 C/C++ warnings and 2 duplicate Kotlin warnings for deprecated `InputMethodManager.SHOW_IMPLICIT`
- Baseline full code-quality check: 24 Android native files need clang-format and 9 Kotlin findings are autocorrectable; PowerShell, BOM, shellcheck, shfmt, cmake-format, and cmake-lint passed according to `android/temp/run-code-quality.summary.json`
- The full fix pass found that PSScriptAnalyzer included ignored generated scripts under `android/temp`; exclude that artifact tree, and make unexpected wrapper-stage failures return a `runner` failure instead of a false success

## Result

- Applied clang-format to the 24 reported Android native files and ktlint autocorrections to `AssetManifest.kt` and `MissionZipMusicStageManager.kt`
- Removed the deprecated `InputMethodManager.SHOW_IMPLICIT` call flag and three additional Kotlin compiler findings found by a clean build
- Fixed the Android JNI Chromaprint decode pointer type warning
- Removed nine warning sites on lines owned by this branch in `d1/` and `d2/`: two unused multiplayer locals, five invalid RNG state probe calls, one missing rewind-file adapter, and one ambiguous dangling `else`
- Removed the UTF-8 BOM from `android/tests/input_demo_graphics_canary_helpers.ps1`
- Excluded ignored generated `android/temp` scripts from PSScriptAnalyzer and made unexpected code-quality stage errors fail the wrapper
- Left warnings whose exact sites blame to the `main` merge base unchanged, including the MSVC `weapon.c` return-path warnings; also left fetched LZMA SDK warnings unchanged

## Validation

- Full `android/run-code-quality.ps1 -Fix` passed all eight stages after the helper fixes
- Full non-mutating `android/run-code-quality.ps1` passed clang-format, ktlint, PSScriptAnalyzer, BOM lint, shellcheck, shfmt, cmake-format, and cmake-lint
- A clean Android `assembleDebug` completed for armeabi-v7a, arm64-v8a, and x86_64; the branch-owned legacy warnings were absent
- A follow-up warning build after the last Android native and Kotlin fixes reported 0 C/C++ warnings and 0 Kotlin warnings for all rebuilt files
- `android/tests/test_gradle_unit_tests.ps1` passed
- `run-windows-build.ps1 -Target both` built D1, D2, and their headless/metadata targets successfully
- `git diff --check` passed
