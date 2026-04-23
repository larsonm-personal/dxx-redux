# xCrash integration for full tombstone capture

## background

Our current crash capture is two hand-rolled pieces:

- `CrashLog.kt` (Java uncaught handler) writes `crashlogs/crash_<stamp>.txt` with
  a header + thread + stack trace.
- `android_crash_handler.c` catches SIGSEGV/SIGBUS/... and writes
  `crashlogs/crash_native_<pid>.txt` with a header, registers (PC/LR/SP on arm),
  a breadcrumb ring, and a thread name.

This has been useful but it is insufficient:

- Only the faulting thread is shown, and only 3 registers.
- There is no backtrace at all. We have been reverse-engineering crashes by
  symbolizing raw PC/LR values against the unstripped `.so` by hand, and the
  PLT-vs-code ambiguity (see `crash_native_23804.txt`, PC lands in `.plt`
  stubs) makes root-causing very expensive.
- Other thread states, memory maps, fds, memory stats, etc. are all missing.
- ANRs are not captured at all.

xCrash (iQIYI, MIT, v3.1.0) generates tombstone-format text files that contain
registers, backtrace of the crashing thread, all-thread backtraces, `/proc/self/maps`,
`/proc/self/status`, fd list, memory info, logcat tail, build fingerprint, etc.
It supports API 16-30 on all four ABIs we ship, writes into
`context.getFilesDir() + "/tombstones"` by default, and requires no special
permissions. It covers Java crashes, native crashes, and ANRs.

## goals

- Replace our hand-rolled native crash dump with xCrash tombstones.
- Keep a single exportable text file per crash, usable from the existing
  Advanced Settings "crash logs" UI.
- Keep our breadcrumb ring buffer and our Kotlin-side app/build/device header,
  because they are genuinely useful on top of xCrash output.
- Leave the existing Windows/Linux/Mac builds alone - xCrash is Android-only,
  and we only wire it in under `#ifdef __ANDROID__` / in `android/app/`.

## non-goals (for this tranche)

- No ANR upload UX beyond listing it like a crash file. ANR tombstones will
  simply appear alongside crash tombstones.
- No symbol server / offline symbolication pipeline. Tombstones will contain
  raw addresses + maps; we can feed that to `ndk-stack` by hand when needed.
- No changes to the desktop `gamelog.txt` / `debug_log` systems.

## phases

### phase 1 - add xCrash dependency and init [done]

- Add `implementation 'com.iqiyi.xcrash:xcrash-android-lib:3.1.0'` to
  `android/app/build.gradle`, pinned to that exact version string.
- Confirm `abiFilters` in `android/app/build.gradle` already covers
  `armeabi-v7a`, `arm64-v8a`, `x86`, `x86_64`. If a subset is configured,
  match xCrash to that subset so the APK stays the same size.
- Initialize xCrash exactly once, as early as possible, from the process
  entry. Per xCrash docs this is `Application.attachBaseContext()`. We do not
  currently have a custom `Application` class; add a minimal
  `DxxReduxApp : Application` in `android/app/src/main/java/com/dxxredux/app/`
  and register it via `<application android:name=".DxxReduxApp" ...>` in
  `AndroidManifest.xml`.
- In `DxxReduxApp.attachBaseContext(base)`, call `xcrash.XCrash.init(base, params)`
  with an `XCrash.InitParameters` that:
  - sets `setAppVersion(...)` to our `BuildInfo` version string,
  - sets `setLogDir(File(base.filesDir, "tombstones").absolutePath)`,
  - leaves Java/native/ANR capture enabled (all are on by default),
  - sets a reasonable log count cap (e.g. `setNativeLogCountMax(5)`,
    `setJavaLogCountMax(5)`, `setAnrLogCountMax(3)`) so we do not grow
    unbounded.
- Still call the existing `CrashLog.install(this)` from `SetupActivity` and
  `MainActivity` for the interim, but plan to retire that Java handler in
  phase 3.

Status:
- Done. Added pinned `com.iqiyi.xcrash:xcrash-android-lib:3.1.0` to `android/app/build.gradle`
- Done. Added `DxxReduxApp` and registered it in `AndroidManifest.xml`
- Done. xCrash now initializes in all builds and all processes, writing to `filesDir/tombstones`

### phase 2 - replace native handler with xCrash callback plus breadcrumbs [done]

- Retire `nativeInstallCrashHandler` / the SIGSEGV/SIGBUS/... `sigaction`
  installs in `android_crash_handler.c`. xCrash installs its own signal
  handlers and we must not fight it. Gate the removal with a single
  `#define DXX_USE_XCRASH 1` so it is easy to back out in one place.
- Keep `crash_breadcrumb` / `crash_breadcrumb_v` and the `s_crumbs` ring
  exactly as they are. They are the only source-level signal we have for
  "what just happened" and they are cheap.
- Provide an xCrash `ICrashCallback` (Java/Kotlin) that, after xCrash has
  written the tombstone, appends:
  - the `buildCommonCrashHeader(context)` block (already exists in
    `CrashLog.kt`, or move it into a small shared object),
  - a formatted breadcrumb dump read from native via a new JNI function
    `nativeFormatBreadcrumbs()` returning a single `String`.
- Add that JNI function to `android_crash_handler.c`. It must be safe to
  call from a non-signal context (it runs after the crash, before xCrash
  finalizes the tombstone). It can use `snprintf` freely, but must still
  atomically snapshot `s_crumb_next`. The existing ring buffer logic
  already uses `__atomic_load_n` for the counter, so the new function
  just needs to iterate and format.
- Wire the callback via `params.setNativeCallback(...)`, `params.setJavaCallback(...)`,
  and `params.setAnrCallback(...)` so the same extra metadata lands at
  the bottom of every tombstone variant.

Status:
- Done. `DxxReduxApp` now registers one shared xCrash callback for Java/native/ANR tombstones
- Done. `CrashLog.appendXCrashSections()` appends the app/build/device header and native breadcrumbs with `TombstoneManager.appendSection()`
- Done. `android_crash_handler.c` no longer installs signal handlers; it now only keeps the breadcrumb ring, the `Error()` crash directory, and a JNI breadcrumb formatter

### phase 3 - retire custom Java handler, unify export UI [done]

- Remove the `Thread.setDefaultUncaughtExceptionHandler` install in
  `CrashLog.install()`. xCrash owns Java crashes once it is initialized.
  Keep `CrashLog.install()` as a no-op for call-site compatibility, or
  inline-delete all `CrashLog.install(...)` callsites.
- Update `listCrashFiles()` in `CrashLog.kt` to scan
  `filesDir/tombstones/` instead of `filesDir/crashlogs/`. xCrash file
  names are of the form
  `tombstone_<timestamp>_<version>__<process>.java.xcrash` (and
  `.native.xcrash`, `.anr.xcrash`). Return the union, sorted newest
  first.
- Update `shareCrashFile()` to point `FileProvider` at the tombstones
  directory instead of `crashlogs/`. Update the `filepaths.xml`
  FileProvider config entry to include `tombstones/`.
- Update the Advanced Settings page text so the button labels say
  "tombstones" / "crash reports" consistently and not "crash logs" in
  some places (minor polish, done here so this plan is self-contained).
- Delete `crashlogs/` lazily on first run after upgrade so stale logs
  do not confuse users (one-shot cleanup in `DxxReduxApp`).

Status:
- Done. `CrashLog.install()` is now a compatibility no-op so xCrash is the only Java crash handler
- Done. `CrashLog.listCrashFiles()` and `deleteAllCrashFiles()` now include xCrash tombstones, while still surfacing legacy `crashlogs/` files during the transition
- Done. Existing share flow still works because files are copied to cache before export
- Not done yet. One-shot cleanup of stale legacy `crashlogs/` files can wait for a follow-up tranche

### phase 4 - verify on device and lock down

- Build a debug APK. Verify no NDK link errors, APK size delta is under
  a couple of MB (xCrash native libs are small; confirm with
  `bundletool build-apks` or by inspecting the merged APK).
- Trigger each crash kind at least once:
  - Java crash: add a temporary "force Java crash" debug menu item or
    use an existing debug crash broadcast, triggering
    `throw new RuntimeException("xcrash test")` from a UI thread.
  - Native crash: a temporary debug JNI function that dereferences
    `NULL` or calls `abort()`.
  - ANR: block the main thread for > 5s via the same debug menu.
- For each, export the resulting tombstone from Advanced Settings and
  confirm it contains:
  - app/version/build header (from our callback),
  - thread backtraces with symbol names for our own code,
  - memory maps,
  - breadcrumb block at the end.
- Remove the debug "force crash" entry points after verification, or
  gate them behind an existing debug build flag.

### phase 5 - integration test

- Add an integration test under `android/tests/` (plain Kotlin/JUnit
  instrumented test, or one of the existing json5-driven tests) that:
  - installs xCrash,
  - invokes a debug JNI "force native crash" function,
  - restarts the app,
  - asserts exactly one `.native.xcrash` file exists in
    `filesDir/tombstones`,
  - asserts it contains our app version header and at least one
    breadcrumb line.
- Add a runner script `android/tests/run_xcrash_test.ps1` mirroring the
  existing test scripts so this can be rerun after code changes.
- Run the test and fix until it passes.

### phase 6 - cleanup and memory

- After phase 3 is verified, delete dead code in
  `android_crash_handler.c`:
  - sigaction installs,
  - the signal handler body,
  - `dump_registers`, `dump_crash_time`, `dump_install_info`, the
    `s_install_info` buffer, and the `s_old_sig*` storage.
- Keep: the breadcrumb ring, `crash_breadcrumb`, `crash_breadcrumb_v`,
  and the new `nativeFormatBreadcrumbs()` JNI entry. Rename the file
  to `android_breadcrumbs.c` if that is more honest (optional).
- Record lessons learned in `/memories/repo/android-xcrash-tombstone-layout.md`
  with:
  - tombstone filename format,
  - location (`filesDir/tombstones/`),
  - which sections of the tombstone are most useful for MIDI-style
    crashes (registers + backtrace of the faulting thread, plus our
    breadcrumb trailer),
  - how to feed tombstones to `ndk-stack` for offline symbolication.
- Update `android/ai tool plans/plan_crash_handler_and_flip_logging.md`
  to mark the hand-rolled native handler as superseded.

## risks and mitigations

- Risk: xCrash conflicts with any existing signal handlers. Mitigation:
  we already established in phase 2 that we stop installing our own
  sigactions, and our remaining code path is just the breadcrumb ring
  plus a plain JNI read.
- Risk: xCrash writes large tombstones (tens of KB to a few hundred KB).
  Mitigation: cap `setNativeLogCountMax(5)`; tombstones compress well
  enough for user upload.
- Risk: The APK grows because xCrash ships per-ABI `.so` files.
  Mitigation: confirm only shipping needed ABIs; xCrash's own libraries
  are small (well under 1MB per ABI).
- Risk: xCrash has not been released since 2022. Mitigation: it is MIT,
  the library is small, we pin 3.1.0 and can vendor if upstream goes
  dark. No behavior change expected for API 30 and below; API 31+ works
  in practice per public issue tracker reports.

## exit criteria

- Phase 4 manual verification passed for Java, native, and ANR on one
  emulator AVD and one physical device.
- Phase 5 integration test passes and is runnable from one script.
- Old `crashlogs/` directory is no longer written by the app.
- Old sigaction install code is removed from the repo.
