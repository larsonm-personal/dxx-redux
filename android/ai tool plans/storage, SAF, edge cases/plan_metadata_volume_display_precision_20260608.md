# Metadata volume display precision

## Goal
- Show normalized mine volume with at most two significant figures in the Android launcher metadata table.

## Plan
- [x] Locate the normalized volume display formatter.
- [x] Patch the display formatter without changing stored metadata precision.
- [x] Run focused formatting and build checks.

## Notes
- Desired examples: `53x` instead of `53.1x`; one-decimal output remains useful from `1.1x` through `9.9x`.
- JNI and Kotlin fallback formatters now keep one decimal below `10x` and round values `10x` or larger to an integer with at most two significant figures.
- Verification: scoped code quality passed for the touched files, and `:app:compileDebugKotlin :app:externalNativeBuildDebug` passed.
