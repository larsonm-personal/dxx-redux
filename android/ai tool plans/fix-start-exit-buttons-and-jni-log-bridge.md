# Fix START/EXIT Buttons and JNI Log Bridge

## Problem 1: START GAME and EXIT buttons on select players screen do nothing

### Root Cause
When the select players menu closes (via Enter or ESC), `window_close()` sends
`EVENT_WINDOW_ACTIVATED` to the main menu. The main menu handler calls
`check_auto_net()`, which sees `auto_host_pending` is still nonzero (it's only
cleared by the auto-start path in `net_udp_start_poll`, never by manual start/exit).
This re-enters `net_udp_auto_host` -> `net_udp_start_game` -> `net_udp_select_players`,
opening a NEW select players menu recursively. The second `net_udp_start_game` call
resets `N_players=0`, causing the client to briefly disappear then rejoin.

### Fix
Separate `auto_host_pending` (consumed by `check_auto_net`) from a new static flag
`net_auto_start_when_full` (used by `net_udp_start_poll` for auto-start). Clear
`auto_host_pending` as soon as we enter `net_udp_select_players`, preventing re-entry.

Files modified:
- d2/main/net_udp.c
- d1/main/net_udp.c

## Problem 2: No C-level logs in exportable debug logs

### Root Cause
MPDIAG and net_log_comment write to logcat/debug log files. The user's
"exportable debug log" is the Kotlin `NetLog` system (`filesDir/netlogs/netlog_*.txt`).
There is no JNI bridge from C to Kotlin NetLog.

### Fix
Create a JNI bridge function that C code can call to log to the Kotlin `NetLog` system.
Add a C-callable wrapper `android_net_log(category, message)`, backed by JNI calls to
`NetLog.log()`. Integrate with MPDIAG macro.

Files modified:
- android/app/src/main/cpp/android_input.c (or new file)
- android/app/src/main/java/com/dxxredux/app/multiplayer/NetLog.kt
- d2/main/net_udp.c (MPDIAG macro)
- d1/main/net_udp.c (MPDIAG macro)
