# Fix Stuck-at-Launcher and Related Regressions

## Status: COMPLETE

---

## Problem 1: Resolve-GameDataDeps stat regression  [x]
`stat -c '%s %n' *` via `sh -c` through `adb shell run-as` has quoting/compat
issues on Android's toybox. Result: device file listing always empty, all 11
files re-pushed every run. Files may not actually land because the stat command
execution path (sh -c nesting) can corrupt the adb shell session.

Fix: replace `stat -c` with `ls -la` parsing. `ls -la` reliably returns size +
filename per line on Android. Parse with regex.

## Problem 2: Stuck-at-launcher detection  [x]
After `Start-GameWithRetry` reports "Game started", tests sit for full timeout
with no automation progress because the game engine found no data files (or is
stuck at launcher for other reasons). No early failure detection.

Fix: in `Watch-AutomationResult`, after ~15s with no automation file AND no
logcat progress, check `setup_introspect.json` (launcher state). If launcher
is still visible, fail immediately with descriptive message.

## Problem 3: test_mp emu2 game data  [x]
`Start-EmulatorIfNeeded` boots emulator-5556 but doesn't install APK or push
game data. The test then fails at the game-data verification step.

Fix: after boot, install APK and push game data to the new emulator.

## Problem 4: test_bot_client stall  [x]
Welcome bundle loop expects 5 messages (AUTH_OK, MOTD, SERVER_STATUS,
LOBBY_LIST, FRIEND_LIST_RESP) but server only sends 4 (no MOTD).
`Receive-WsMessage` with no timeout blocks forever on the 5th.

The test is also interactive ("Ctrl+C to quit") -- not suitable for automated
test menu. 

Fix:
- Break welcome loop on FRIEND_LIST_RESP (the terminal welcome message)
- Add `TimeoutSeconds` param with 30s default
- Create action: create lobby, wait up to timeout, report
- Exit 0 on successful auth+create, exit 1 on failure
