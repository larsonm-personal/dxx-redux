# Plan: Android lint cleanup 2026-05-23

## Goal

- Clear the current scoped `clang-format` and `ktlint` failures reported by `android\run-code-quality.ps1`

## Steps

- [x] Inspect the reported lint ranges and identify the smallest style-only edits
- [x] Patch the Kotlin function-signature, `when`, indent, and chain-continuation issues
- [x] Run scoped `clang-format` on the four native files from the report
- [x] Re-run focused `ktlint` and `clang-format` checks
- [x] Update this plan with the final result

## Result

- Updated `ControllerConfigPage.kt` and `TouchOverlayView.kt` with style-only fixes for the reported `ktlint` violations
- Applied `clang-format` to `jni_main.c`, `android_profile.c`, `android_profile.h`, and `rbaudio_bin.c`
- Direct `ktlint` on the two edited Kotlin files now passes with no output
- `clang-format --dry-run --Werror` on the four native files now passes with no output
- A clean rerun of `android\run-code-quality.ps1` re-confirmed the C/C++ and Kotlin sections as passing before later PowerShell analyzer stages stopped producing output, so the user-reported lint items are resolved

## Follow-up 2026-05-24

- [x] Inspect the current `run-code-quality.ps1` failures and locate the smallest local fixes
- [x] Fix the PowerShell analyzer issues in `check-updates.ps1` and `get_7zip.ps1`
- [x] Apply the pending format-only fixes for the reported C and shell files
- [x] Re-run focused checks for the touched files and record the result

## Follow-up 2026-05-24 Result

- Fixed the reported `PSUseConsistentIndentation` slice in `android/get_deps/check-updates.ps1`
- Fixed the `${commandName}` interpolation in `android/get_deps/get_7zip.ps1` so `PSScriptAnalyzer` no longer reports `InvalidVariableReferenceWithDrive`
- Applied the repo formatter flow to `android/app/src/main/cpp/extract/fingerprint_audio.c`, `android/app/src/main/cpp/extract/iso9660_reader.c`, and `android/get_deps/get_linux_build_prereqs.sh`
- A scoped rerun of `android/run-code-quality.ps1` over the five reported files now passes cleanly, including `clang-format`, `PSScriptAnalyzer`, `UTF-8 BOM lint`, `shellcheck`, and `shfmt`