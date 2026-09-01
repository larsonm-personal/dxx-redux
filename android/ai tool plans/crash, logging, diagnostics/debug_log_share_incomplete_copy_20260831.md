# Debug log share incomplete copy

## Goal

Identify and fix the Android share path that makes Google Drive report
"incomplete copy" for exported debug logs while the Save path succeeds.

## Plan

1. [done] Inspect the two supplied logs and trace debug-log publication,
   FileProvider access, share intents, and active-writer behavior
2. [done] Implement the smallest Android-specific reliability fix and verify
   the centralized single- and multi-URI share contract
3. [done] Run scoped formatting, tests, Android build verification, and mark
   this plan complete with findings

## Findings

- The supplied files were found directly under Downloads rather than in the
  quoted `Downloads/debuglog` directory
- Both logs show successful immutable cache publication before the Drive error;
  the second published all 8,854,744 bytes with ample free storage
- Share intents put each URI only in `EXTRA_STREAM`. Android URI grants apply to
  intent data and `ClipData`, and chooser propagation depends on those grantable
  fields. The shared URI was therefore absent from the explicit grant payload
- All launcher share paths now retain `EXTRA_STREAM` for receiver compatibility
  and also attach every shared URI as `ClipData` with read permission

## Verification

- Scoped Kotlin formatting and code-quality checks passed
- `:app:testDebugUnitTest :app:assembleDebug` passed, including Kotlin
  compilation, all three configured native ABIs, the JVM suite, and APK assembly
- `git diff --check` passed
