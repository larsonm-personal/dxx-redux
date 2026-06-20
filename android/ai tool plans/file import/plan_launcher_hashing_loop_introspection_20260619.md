# launcher hashing loop introspection

Goal: add enough setup introspection to prove why the launcher repeatedly
hashes imported game files and distinguish manifest duplicates, missing
entries, size mismatches, and files disappearing during hashing.

Plan:

1. [done] Identify the launcher hash decision point.
2. [done] Add manifest diagnostics that explain stale-file decisions.
3. [done] Expose the diagnostics through setup introspection.
4. [blocked] Validate with a fresh launcher run and inspect
   `setup_introspect.json`.

Notes:

- The shell runner in this agent session is still unable to start PowerShell
  commands, so validation will need a fresh terminal unless the process-launch
  issue clears.
- The intended debug surface is `com.dxxredux.SETUP_INTROSPECT`, not screenshots.
- `AssetManifest.staleFileDiagnostics()` reports missing manifest entries,
  size mismatches, and duplicate manifest entries that were canonicalized.
- `SetupHashDiagnostics` records the live hash pass and is included as
  `hashing` in setup introspection.
- `setup_introspect.json` now also includes `asset_manifest_diagnostics`.
- Tried to run the live probe from this agent session, but the shell tool still
  fails before executing command bodies. Even `Write-Output ok` exits with
  `-1073741502`, so adb/Gradle cannot be launched from this process yet.
- Added `android/helpers/probe_setup_hashing_loop.ps1` to automate launcher
  polling and fail when the same stale file appears across repeated hash passes.
  Attempting to run it from this session also exits immediately with
  `-1073741502`.
- Updated the probe to auto-select the first online adb device when `-Serial`
  is omitted and to fail clearly when no device is online.
- Updated the probe again to match the repo convention: it now dot-sources
  `test_helpers.ps1` and calls `Start-EmulatorIfNeeded` for the primary AVD
  when no adb device is online.
- Probe startup exposed a helper bug: `adb push` progress output while writing
  seeded `assets.json` was treated as a terminating `NativeCommandError` under
  `$ErrorActionPreference = Stop`. The manifest writer now temporarily uses
  `Continue` around those raw adb native calls, matching nearby helper code.
- Probe then exposed a PowerShell binding issue in its adb wrapper. The probe
  now uses a probe-local `Invoke-ProbeAdb -AdbArgs @(...)` wrapper instead of
  positional remaining arguments, avoiding collisions with imported helpers.
- The probe now builds `assembleDebug` by default, installs/provisions the APK
  on the selected emulator even if it was already online, and fails explicitly
  if `setup_introspect.json` lacks the new `hashing` diagnostics.
- Probe output from a live run showed repeated `size_mismatch` for
  `descent.hog` and `descent.pig`, while the current file alternated between
  `DESCENT.HOG`, `descent.hog`, and `descent.pig`. Diagnosis: case-variant
  duplicate files on ext4 were being hashed under one lowercase manifest key,
  causing the manifest size to toggle between physical files.
- Patched `AssetManifest` to choose one canonical disk file per lowercase game
  filename, preferring the exact lowercase file. SetupActivity now uses the
  normal `findStaleFiles()` API, which returns only canonical files.
- Follow-up cleanup removed the temporary setup hashing introspection and the
  one-off adb probe script. The durable fix now lives in `AssetManifest`, with
  unit tests covering duplicate manifest entries and case-variant disk files.
