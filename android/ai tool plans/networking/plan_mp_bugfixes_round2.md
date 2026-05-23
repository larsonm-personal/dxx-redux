# Multiplayer Bugfix Round 2

## Bug 1: Disconnect button layout in portrait mode -- DONE
- Disconnect was crammed against right edge in portrait mode on the connected screen
- Split into 3 rows: [Refresh Lobbies | Create Lobby], [Disconnect], [Friends | LAN]
- File: MultiplayerScreen.kt (ServerBrowserContent)

## Bug 2: Stale gameLaunchInfo triggers auto-host from previous session -- DONE
- gameLaunchInfo was never cleared after consumption, only in disconnect()
- LobbyScreen's LaunchedEffect fires immediately if non-null from previous session
- Caused phantom game launches with old parameters
- Fix: Clear in launchMultiplayerGame() after consumption, and in leaveLobby()
- Files: SetupActivity.kt, MatchmakingService.kt

## Bug 3: Port 42424 bind error (regression) -- DONE (root cause is bug #2)
- Host engine binds INADDR_ANY:42424 via udp_open_socket
- Stale gameLaunchInfo triggers duplicate game launch, first one binds port
- Added MPDIAG logging at bind error paths (with errno) for future diagnostics
- Files: d1/main/net_udp.c, d2/main/net_udp.c (added errno.h, MPDIAG at errors)

## Bug 4: Delete All Logs deletes in-progress log file -- DONE
- deleteAllLogs() called closeLog() then nuked entire directory
- After that, writer was null and logging was dead for the session
- Fix: Track currentFile, skip it in deleteAllLogs instead of closing
- File: NetLog.kt

## Bug 5: Zero MPDIAG in net log file -- DONE
- MPDIAG JNI bridge (android_net_log -> netLogFromNative -> NetLog.log) is wired
- Previous session's NetLog.init(this) fix in MainActivity.onCreate ensures writer is open
- Added MPDIAG calls at socket creation failure, bind failure, getaddrinfo failure
- Bug #4 fix ensures delete-all doesn't kill active logging mid-session
- Files: d1/main/net_udp.c, d2/main/net_udp.c
