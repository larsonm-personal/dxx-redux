# Plan: Centralize Debug Logging

## Goal
Merge `android_net_log.c` and `debug_log.c` into a single `android_log.c`.
All logging that ends up in launcher-visible debug logs should go through
`debug_log(int category, const char *fmt, ...)` with an enum for the log type.

## Current State
Two parallel logging paths both write to `debuglogs/*.txt`:
1. `debug_log(category, fmt, ...)` -> `debugLogFromNative(int, String)` -> `DebugLog.log()`
2. `android_net_log(cat_str, msg)` -> `netLogFromNative(String, String)` -> `DebugLog.logNetwork()`

COOPLOG macro duplicated in 5 files. MPDIAG macro duplicated in 2 files.

## Changes

### C side
- [x] Create `android_log.h` = merged `debug_log.h` + `android_net_log.h` + COOPLOG macro
- [x] Create `android_log.c` = merged `debug_log.c` + `android_net_log.c` (single `debug_log()` impl)
- [x] Delete `debug_log.c`, `debug_log.h`, `android_net_log.c`, `android_net_log.h`
- [x] Update `CMakeLists.txt`: replace both source entries with `android_log.c`
- [x] Update `jni_main.c`: include `android_log.h` instead of `debug_log.h`
- [x] Update d1/d2 `console.c`: include `android_log.h` instead of `debug_log.h`
- [x] Update d1/d2 `ogl.c`: include `android_log.h` instead of `debug_log.h`
- [x] Remove COOPLOG macro from d2/state.c, d1/state.c, d2/multi.c, d1/multi.c, d2/coop_save.c
- [x] Replace `#include "android_net_log.h"` with `#include "android_log.h"` in all files
- [x] Update MPDIAG in d1/d2 net_udp.c to use `debug_log(DLOG_NETWORK, ...)` instead of `android_net_log()`
- [x] Update mpdiag_pkt_dump to use `debug_log(DLOG_NETWORK, ...)`
- [x] Update coop_indicator_lines.c to use `debug_log(DLOG_NETWORK, ...)`
- [x] `debug_log_categories.h` stays unchanged (already has the enum)

### Kotlin side
- [x] Remove `netLogFromNative()` from MainActivity.kt
- [x] Move MatchmakingStateHolder.appendLog() into `debugLogFromNative()` for NETWORK category
- [x] Remove `DebugLog.logNetwork()` from DebugLog.kt

### Validation
- [x] Build passes
- [x] Linters pass
