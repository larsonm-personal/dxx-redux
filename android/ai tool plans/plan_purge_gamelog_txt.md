# Plan: Purge gamelog.txt, redirect to in-app debug logging

## Goal
Eliminate gamelog.txt as a concept. All con_printf output that currently goes to
gamelog.txt should instead go through the existing debug_log() system under a new
"Game Logs" category (DLOG_GAME). On non-Android builds, gamelog.txt continues
to work as before (no behavior change).

## Architecture

Two independent logging systems exist today:
1. **con_printf** (console.c): writes to in-memory buffer, stdout, logcat (Android),
   gamelog.txt via PHYSFS, introspection ring buffer. Always active.
2. **debug_log()** (debug_log.c): per-category enabled flags, JNI bridge to Kotlin
   DebugLog, writes timestamped lines to debuglogs/ directory. Currently only used
   for texture loading diagnostics.

After this work, on Android, con_printf will call debug_log(DLOG_GAME, ...) instead
of writing to gamelog_fp. The gamelog_fp path will be compiled out on Android.

## Phases

### Phase 1: Add DLOG_GAME category
- [x] debug_log_categories.h: add DLOG_GAME=3, bump DLOG_COUNT to 4
- [x] DebugLogCategory.kt: add GAME=3, bump COUNT to 4, add "Game Logs" label
- [x] debug_log.c: add "GAME" to category_tags array

### Phase 2: Redirect con_printf on Android
- [x] d2/main/console.c: include debug_log.h, replace gamelog_fp write block with
  debug_log(DLOG_GAME, ...) under #ifdef __ANDROID__. Guard gamelog_fp declaration,
  con_close, con_switch_log with #ifndef __ANDROID__
- [x] d1/main/console.c: same changes

### Phase 3: Guard GameLogSplit/GameLogTimeStamp on Android
- [x] d2/main/gameseq.c: wrap GameLogSplit block with #ifndef __ANDROID__
- [x] d1/main/gameseq.c: same
- [x] d2/main/game.c: wrap GameLogSplit con_switch_log(NULL) with #ifndef __ANDROID__
- [x] d1/main/game.c: same

### Phase 4: Update scripts
- [x] android/test_helpers.ps1: replace gamelog.txt collection with debug log file
  collection (list and cat files from debuglogs/ directory)
- [x] android/collect_crash.ps1: replace gamelog.txt collection with debug log file
  collection

### Phase 5: Update documentation
- [x] .github/copilot-instructions.md: update gamelog.txt reference
- [x] android/ai tool plans docs: update gamelog.txt references
- [x] d2/main/state.c, d2/main/coop_save.c: update comments mentioning gamelog.txt

### Phase 6: Build, lint, verify
- [x] Build d2 (cmake)
- [x] Run code quality linter
- [ ] Verify on emulator that toggling "Game Logs" slider produces debug log output

## Files NOT changed (intentional)
- ChangeLog.txt, d2/CHANGELOG.txt: historical records, leave as-is
- d1/d1x-default.ini, d2/d2x-default.ini: gamelog options still relevant for
  non-Android builds
- d1/misc/args.c, d2/misc/args.c, d1/include/args.h, d2/include/args.h: GameLogSplit
  and GameLogTimeStamp args still relevant for non-Android builds
- d1/main/inferno.c, d2/main/inferno.c: help text for -safelog still relevant for
  non-Android builds

## Notes
- con_printf still writes to logcat on Android (the __android_log_print block is kept)
- con_printf still writes to the introspection ring buffer (console_ringbuf_add)
- con_printf still writes to the in-memory con_buffer for the in-game console
- The only thing removed on Android is the PHYSFS gamelog_fp file write
- debug_log(DLOG_GAME) goes through JNI to DebugLog.kt which writes timestamped
  lines to debuglogs/ -- the same system as Network/Graphics/Texture logs
- The "Game Logs" slider will appear automatically in Advanced Settings because
  DebugLoggingSection() iterates 0 until DebugLogCategory.COUNT
