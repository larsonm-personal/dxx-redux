# Level metadata native safety study

## Goal
- Study how the level metadata native bridge can analyze malformed files without crashing the Android launcher
- Preserve useful failure details for the UI and exported diagnostics

## Plan
- [x] Inspect existing launcher native bridge and crash logging patterns
- [x] Inspect native crash handler and breadcrumb/report flow
- [x] Identify isolation options for malformed level analysis
- [x] Recommend result schema, UI behavior, and diagnostics
- [x] Recommend tests for malformed files and crash containment

## Findings
- An in-process JNI bridge is not safe enough for arbitrary third-party level files.
  - Kotlin `try/catch` can handle bad JSON or `UnsatisfiedLinkError`, but not SIGSEGV/SIGABRT/native memory corruption.
  - C++ `try/catch` does not catch signals.
  - The engine loader has many `Error()` and `Assert()` paths around mine, object, robot, texture, and support-file loading.
  - On Android, `Error()` writes a `crash_error_<pid>.txt` report and exits the process.
- The existing crash system is useful and should be reused.
  - `DxxReduxApp` initializes xCrash for Java/native/ANR tombstones in `files/tombstones`.
  - `CrashLog.installNativeHandler` initializes native breadcrumb storage after `System.loadLibrary`.
  - `crash_breadcrumb` writes an in-memory ring and `crash_breadcrumbs_latest.txt` so breadcrumbs can be backfilled into xCrash tombstones.
  - The launcher already lists and exports tombstones/crash error files from Advanced Settings.
- The manifest already uses a separate `:game` process for the game activity. Level metadata analysis should use the same isolation idea with a new short-lived worker process.
- Android builds both `dxx-redux-d1` and `dxx-redux-d2`, so the worker can load the correct library for the request.

## Recommended Safety Model
- Add a `LevelMetadataAnalysisService` declared with `android:process=":levelmeta"` or similar.
- Make the service one-shot:
  - launcher writes request JSON and a per-request status/result path under app-private cache or files
  - launcher starts/binds the service with the request id/path
  - service loads `dxx-redux-d1` or `dxx-redux-d2`, calls `CrashLog.installNativeHandler`, runs native analysis on `Dispatchers.IO` or a service worker thread
  - service writes a final result JSON atomically, then exits the worker process voluntarily
- Treat process death as a normal possible outcome:
  - if the final result file exists and says `status: "ok"` or `status: "failed"`, show it
  - if the process dies without a final result, report `status: "crashed"` using the last status checkpoint, recent tombstone/crash_error files, and breadcrumb snapshot
  - if it hangs, timeout, unbind/stop the service, and report `status: "timeout"`
- Do not use `siglongjmp` or signal handlers to keep the launcher process alive. Process isolation is the recovery boundary.

## Input Hardening Before Native Load
- Stage all SAF and ZIP constituent inputs into an app-private analysis workspace.
- Enforce ZIP staging rules:
  - normalize paths and reject zip-slip paths
  - cap total extracted bytes
  - cap file count and per-entry size
  - only extract files needed for the selected analysis
- Preflight obvious format errors in Kotlin before native load:
  - HOG magic and entry table plausibility
  - mission descriptor parse errors or missing referenced files
  - direct level extension/game mismatch
  - missing base game support files
- Keep full paths out of UI-facing errors where possible; use display names, sizes, and request ids. Full paths can go to diagnostic logs.

## Native Result Contract
- Native should return structured JSON for recoverable failures instead of calling `Error()` whenever it can detect the problem locally:
  - `status`: `ok`, `failed`, `partial`, `crashed`, or `timeout`
  - `request_id`
  - `game`
  - `source`
  - `levels`
  - `problems`
  - `diagnostics`
- Each level row should allow partial failure:
  - level number/name/file
  - counts when loaded
  - `status`
  - `problems`
- The worker should update a small status checkpoint before each risky stage:
  - initializing runtime
  - mounting archive/support files
  - loading mission descriptor
  - loading each level file
  - scanning objects/secrets/matcens/energy centers
- Native breadcrumbs should mirror those status stages, for example:
  - `levelmeta req=<id> game=d2 begin`
  - `levelmeta mount source=<name>`
  - `levelmeta load level=-1 file=<name>`
  - `levelmeta scan level=-1 objects=<n> segments=<n>`

## Launcher Failure Reporting
- The dialog should keep the launcher alive and show:
  - a concise human message, such as "Analysis crashed while loading secret level -1"
  - completed rows if a partial result exists
  - the failed stage and file/level from the checkpoint
  - a "Crash report saved" line when a recent tombstone or `crash_error_*.txt` was associated
- The launcher can associate crash reports by recording analysis start time and then scanning `CrashLog.listCrashFiles` for reports modified after that time.
- On crash or timeout, do not cache the failure as a stable metadata result. Cache only successful results and maybe short-lived failed results for the open dialog session.

## Tests
- Unit tests for Kotlin staging and preflight:
  - invalid HOG magic
  - truncated HOG entry header
  - ZIP path traversal
  - oversized ZIP entry/total size
  - descriptor with missing referenced level/HOG
- Integration test for worker isolation:
  - run analysis on a fixture that produces a clean structured failure
  - assert SetupActivity remains alive and the dialog receives `failed`
- Crash-containment test:
  - add a debug-only native test hook or intentionally crashing test request in the `:levelmeta` process
  - assert SetupActivity remains alive
  - assert result is `crashed`
  - assert a tombstone or crash error file appears with level metadata breadcrumbs
- Timeout test:
  - debug-only sleep/hang request
  - assert the launcher reports `timeout` and remains usable
