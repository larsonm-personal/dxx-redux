# Regression regeneration recovery failure diagnosis

## Plan

- [x] Inspect the failed stage report and mission ZIP batch artifacts
- [x] Trace each emulator recovery to its first concrete failure
- [x] Compare with nearby successful or failed runs and identify the likely cause
- [x] Report findings and recommend the smallest validation or fix

## Findings

- The terminal recovery-limit exception is secondary. The initiating failures are
  `SetupActivity did not become ready` and earlier launcher automation timeouts.
- `Wait-SetupActivityReady` broadcasts `SETUP_INTROSPECT` every 800 ms.
- Each introspection calls `MidiEnumerationBridge.enumerateTracks`, which scans the
  D1/D2 HOG MIDI catalogs and converts HMP data to compute durations.
- Android recorded repeated broadcast ANRs for `SETUP_INTROSPECT`, beginning before
  the first late-batch automation timeout. ANR traces show one coroutine inside
  `hmp2mid_mem` and later introspection coroutines waiting on `nativeDataLock`.
- The recovery path restarts ADB and reinstalls the APK, but accepts an emulator as
  healthy based on shell/package checks. It therefore repeats the expensive polling
  pattern instead of removing the cause.
- A preparation failure consumes two recovery counts: one retry inside preparation,
  then one post-failure recovery. Three consecutive affected ZIPs therefore exhaust
  the configured limit of five calls.

## Recommended fix direction

- Make setup readiness use a lightweight probe that does not enumerate MIDI tracks.
- Prevent overlapping setup introspection jobs or cache/omit expensive music catalog
  data from routine readiness polling.
- Add ANR/process-state diagnostics to readiness timeouts so the initiating failure
  is preserved in batch artifacts.

## Implementation plan

- [x] Add a lightweight setup introspection mode for readiness polling
- [x] Coalesce overlapping setup introspection broadcasts
- [x] Add focused regression coverage for the lightweight and single-flight paths
- [x] Run scoped formatting, tests, and Android compilation

## Implementation verification

- Scoped code quality checks passed
- `SetupIntrospectionPolicyTest` passed
- `:app:assembleDebug` passed
- Lightweight setup introspection completed in 282 ms and wrote only the setup
  readiness marker
- A metadata-only, no-regression-JSON batch run of the previously failing `mna.zip`
  passed in 26 seconds
