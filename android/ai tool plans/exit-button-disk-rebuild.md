# Exit button in error state + disk space + emulator rebuild

## Issue 0: Exit button doesn't work when Error() fires during init

### Root cause
- `Error()` in d2/misc/error.c calls `exit(1)` which kills the entire process
- The post-main() cleanup in jni_main.c (Activity.finish() + Process.killProcess()) never runs
- atexit handlers may delay process death, leaving a frozen splash screen
- The exit button's JNI callback (`nativeMetaAction`) fails because native side is dying

### Fix
1. [x] Add `android_finish_and_exit()` in jni_main.c that finishes the Activity via JNI then calls _exit(1)
2. [x] Declare it in android_crash_handler.h (already included by error.c)
3. [x] In d2/misc/error.c and d1/misc/error.c, call android_finish_and_exit() instead of exit(1) on Android
4. [x] In ExitButtonView callback (MainActivity.kt), wrap JNI call in try-catch with fallback to finish()+killProcess()

## Issue 1: Emulator disk space + rebuild option

### Fix
1. [x] Add `-Rebuild` switch to Run-Emulator.ps1
2. [x] Add interactive prompt if -Rebuild not specified: "Delete and rebuild emulator? (y/N)"
3. [x] If rebuild: call avdmanager delete + create_light_avds.ps1 logic

## Issue 1b: GOG installer disk space check

### Fix
1. [x] Before extraction in GogImportDialog, check setDir.usableSpace vs total file sizes
2. [x] Warn user if not enough space, block extraction

## Build and lint
- [ ] Build with gradlew assembleDebug
- [ ] Run run-code-quality.ps1 --fix
