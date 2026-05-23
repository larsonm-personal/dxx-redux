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