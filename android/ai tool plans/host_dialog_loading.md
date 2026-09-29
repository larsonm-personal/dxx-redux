# Responsive host dialog and shared mission catalog

- Trace host dialog work and identify synchronous scans and repeated save checks
- Introduce a shared background catalog loader and immutable per-load snapshot; reuse it for mission selection and save compatibility without retaining stale catalogs across imports/file-set changes
- Load save lists/checks off the UI thread, show loading/error/retry states, and prevent launching with unfinished or obsolete data
- Use the shared catalog in LAN resume and mission compatibility consumers
- Add timing diagnostics and focused device coverage for slow loading, cancellation/retry and selection changes; run relevant scanner/save tests and Android builds

Finding: CreateGameDialog scans mission choices on IO already, but reads save manifests and checksums during composition. Each save warning calls MissionScanner.scan again for D2. The confirm click repeats the scan. LAN quick-host also calls the scanner on its main coroutine.

## Completed

- Added MissionCatalog as the common snapshot/lookup interface. Its UI entry point scans on Dispatchers.IO; worker callers can scan an explicit file set. Snapshots include both modes, so changing mode only filters the existing list
- CreateGameDialog (LAN and online) loads the catalog and co-op save options asynchronously. Save checks and confirm-time validation reuse the same snapshot instead of scanning once per save. Loading and errors are visible, Retry starts a new attempt, Cancel stays active, and Host is disabled until preparation completes
- Shared MissionLoadState resets immediately for changed inputs, discards cancelled results and reloads after Activity resume. LAN and online host-lobby save checks also use it. Backend host/start checks invoked from those UI callbacks run on IO
- LAN quick-host and mission compatibility use the shared catalog interface. Each new operation resolves the current file set, avoiding an indefinitely stale global cache after import or enable/disable changes
- Added catalog/save-option timings to launcher diagnostics

## Verification

- Scoped mixed formatting passed; git diff --check passed
- Android debug and instrumentation APKs built successfully, including native D1/D2 libraries: android/temp/host-dialog-build.txt
- 36 JVM tests passed: MissionScannerManagedArchiveTest (8), MissionLevelAdmissionTest (3), MultiplayerResumePrefsTest (25). New snapshot test verifies mode filtering, stable existing snapshots and fresh content after disabling a mission
- android/tests/test_host_dialog_loading.ps1 passed on emulator-5554: real Compose host dialog with a blocked background scan, responsive main-thread heartbeat, visible loading, cancellation, injected failure, successful Retry and no extra scan on mode changes. Log: android/temp/host-dialog-device.txt
- The test initially used Android's text-search API, which did not find Compose virtual nodes; direct accessibility-tree traversal corrected the harness and the test passed
- android/tests/test_coop_save_compatibility.ps1 passed: incompatible save preserved, both peers warned, and Start fresh clears the restriction. Log: android/temp/host-dialog-save-compatibility.txt
- On the emulator's installed data, final diagnostics reported catalog loading at 107 ms and co-op save options at 72 ms, both on a worker thread. These are not timings for the user's physical device or mission collection
- Existing unrelated music-control edits and android/outstanding_bugs.md were not changed by this task
