# Collecting crash dumps for the menu-close crash

## Background

There is a reproducible crash when closing 2-level-deep menus after editing
autoselect orderings in the launcher.  The exact root cause is not yet pinned
down.  This document describes how to capture useful diagnostics when
reproducing the crash by hand.

## Reproduction steps (from user report)

1. Fresh emulator (or force-stop the app)
2. Launch D2, create a pilot (or accept an existing one)
3. Close the game (Quit from main menu)
4. Edit D2 autoselect order in the launcher, save
5. Open the game again
6. Navigate to Options -> Secondary autoselect ordering -> Escape -> Escape
7. Crash occurs ~1 second after the second Escape

## Emulator setup

Use a debug APK (`assembleDebug`).  The debug build includes the
introspection API and native symbol tables required for useful stack traces.

### Create a lightweight AVD (one-time)

```bash
# From Android SDK dir (D:\local\android-sdk on this machine)
sdkmanager "system-images;android-34;google_apis;x86_64"
avdmanager create avd -n CrashDebug -k "system-images;android-34;google_apis;x86_64" \
    --device "Nexus 5X" --force
```

### Start the emulator

```bash
emulator -avd CrashDebug -no-snapshot-save -gpu swiftshader_indirect
```

Or use the existing AVD:
```bash
emulator -avd Nexus5X_Light_1 -no-snapshot-save -gpu swiftshader_indirect
```

Wait for `adb devices` to show `emulator-5554  device`.

### Install the debug APK

```bash
cd d:\local\dxx-redux\android
$env:JAVA_HOME = "D:\local\jdk-21"
.\gradlew :app:assembleDebug
adb install -r app\build\outputs\apk\debug\app-debug.apk
```

### Push game data (if fresh AVD)

```bash
./push_game_data.sh
```

## Capturing the crash

### Method 1: Helper script (recommended)

```powershell
.\android\collect_crash.ps1
```

This script:
1. Clears logcat
2. Reminds you to reproduce the crash
3. Waits for you to press Enter after the crash
4. Collects logcat, tombstones, introspection state, and gamelog.txt
5. Writes everything to `temp\crash_report\`

### Method 2: Manual collection

Open two terminals.

**Terminal 1** -- continuous logcat to file:
```powershell
adb logcat -c
adb logcat > temp\crash_logcat.txt
```

**Terminal 2** -- reproduce the crash, then collect data:
```powershell
# After the crash happens:

# 1. Native crash tombstone (most useful -- has stack trace with symbols)
adb shell "ls -t /data/tombstones/ | head -1" | ForEach-Object {
    adb shell cat /data/tombstones/$_ > temp\crash_tombstone.txt
}
# If permission denied, try:
adb bugreport temp\bugreport.zip

# 2. Game log (engine-side logging)
adb shell run-as com.dxxredux.app cat files/gamelog.txt > temp\crash_gamelog.txt

# 3. Last introspection dump (if available)
adb shell run-as com.dxxredux.app cat files/introspect.json > temp\crash_introspect.json

# 4. Stop logcat in Terminal 1 (Ctrl+C)
```

## What to provide

Bundle these files and provide them back:
- `crash_logcat.txt` -- full logcat from before the crash
- `crash_tombstone.txt` -- native crash tombstone (has the stack trace)
- `crash_gamelog.txt` -- engine log
- `crash_introspect.json` -- last game state before crash (may be stale)
- The APK commit hash (run `git rev-parse --short HEAD`)

The tombstone is the most useful artifact -- it contains the full native
stack trace with function names and line numbers (since we use debug builds).

## What the diagnostics look like

A native crash in logcat looks like:
```
Fatal signal 11 (SIGSEGV), code 1 (SEGV_MAPERR), fault addr 0x6c
    in tid 1234 (Thread-2), pid 5678 (xredux.app:game)
```

The tombstone will have a full backtrace:
```
backtrace:
    #00 pc 001234ab  /data/app/.../lib/x86_64/libdescent2.so (ogl_filltexbuf+91)
    #01 pc 00123def  /data/app/.../lib/x86_64/libdescent2.so (ogl_loadtexture+374)
    ...
```

## Known bugs found so far

Two bugs were found and fixed during investigation:

1. **Use-after-free in window_close()** (d1/d2 arch/sdl/window.c):
   `d_free(wind)` was called before the `EVENT_WINDOW_CLOSED` callback,
   meaning the callback received a freed pointer.  Fixed by reordering:
   callback first, then free.

2. **Stale g_menu_scale_active** (d1/d2 main/newmenu.c): The Android
   menu-scale flag was not cleared in `EVENT_WINDOW_CLOSE` handlers for
   `newmenu_handler` and `listbox_handler`, leaving stale scaling state
   after menu close.

3. **Double-launch SIGSEGV** (jni_main.c, SetupActivity.kt): Pressing
   Home (instead of Quit) and then re-launching from the launcher created
   a second game thread in the same process, corrupting shared globals.
   Fixed with a static `g_game_running` guard in JNI and an
   `isGameProcessAlive()` check in the launcher.

These fixes are applied but may not be the specific crash in the user's
reproduction steps (#7 above).  The crash still needs further investigation.
