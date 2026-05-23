# Crash Reporting System

## Goal
Add crash reporting that captures Java exceptions and native signals (SIGSEGV, SIGABRT, etc),
writes crash files to `filesDir/crashlogs/`, and shows them in the Advanced Settings page
with export buttons -- mirroring the existing network logging UI.

## Components

### 1. CrashLog.kt (new: `android/app/src/main/java/com/dxxredux/app/CrashLog.kt`)
- Singleton object, similar to NetLog
- `install(context)` -- sets Java UncaughtExceptionHandler, calls native signal handler install
- `listCrashFiles(context)` -- lists crash_*.txt files in crashlogs/ dir, newest first
- `shareCrashFile(context, file)` -- copies to cache, serves via FileProvider (same as NetLog)
- `deleteAllCrashFiles(context)` -- deletes crashlogs/ contents
- Java handler writes thread name + full stack trace to `crashlogs/crash_YYYYMMDD_HHmmss.txt`
- After writing, chains to the old default handler so Android still reports the crash normally
- Max 20 crash files (prune oldest)

### 2. android_crash_handler.c (new: `android/app/src/main/cpp/shared/android_crash_handler.c`)
- Signal handler for SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL
- Uses ONLY async-signal-safe functions: open, write, close, _exit
- Writes crash info to a pre-computed path (set during init)
- Captures signal number, faulting address (from siginfo_t), and basic thread info
- `android_crash_handler_init(const char *crash_dir)` -- installs handlers
- Called from Java via JNI in CrashLog.install()

### 3. AdvancedSettingsPage.kt changes
- Add CrashReportsSection() composable between NetworkLogging and DangerZone
- Same pattern: list files, export buttons, delete all

### 4. SetupActivity.kt changes
- Call `CrashLog.install(this)` in onCreate(), after NetLog.init()

### 5. CMakeLists.txt changes
- Add `shared/android_crash_handler.c` to both d1 and d2 target_sources

### 6. file_paths.xml changes
- Add `<cache-path name="crashlog_exports" path="crashlog_exports/" />`

### 7. Import crash protection
- Wrap scope.launch in the "Import All" button handler with try-catch
- Log any exception to CrashLog before re-throwing

## Non-goals
- Full native stack unwinding (would need libunwind, too heavy)
- Breadcrumb/session tracking
- Automatic upload
