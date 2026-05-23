# Multiplayer Bugfix Round 3

## Bug 1: MPDIAG in separate log file from Kotlin events -- DONE
- Root cause: :game process has its own NetLog singleton, creates a new timestamped file
- Fix: Pass log file path from SetupActivity to MainActivity via intent extra "netlog_path"
- Added NetLog.initAppend(context, filePath) to open an existing file for appending
- Added NetLog.currentFilePath() getter
- SetupActivity passes currentFilePath in the intent, MainActivity uses initAppend
- Files: NetLog.kt, SetupActivity.kt, MainActivity.kt

## Bug 2: Network overlay still shows "disconnected" -- DONE
- Root cause: overlay reads MatchmakingStateHolder.state in :game process, which defaults to DISCONNECTED
- The matchmaking service runs in the main process and never updates the :game process state
- Fix: In MainActivity.onCreate(), when mp_mode is set, seed the game-process MatchmakingStateHolder
  with status=CONNECTED so the overlay shows the correct status
- File: MainActivity.kt

## Bug 3: Stale games on matchmaking server (shows "3 games" with zero actual) -- DONE
- Root cause 1: game_ended() never called when lobbies in Starting/InGame state are removed
  during disconnect cleanup, LeaveLobby, or D13 implicit-leave paths. Only called on
  MatchResult and relay-limit abort. This causes current_in_game counter to never decrement.
- Root cause 2: cleanup_stale_lobbies had a 4-hour timeout, way too long
- Root cause 3: build_active_game_list() had no freshness check
- Fixes:
  - Added game_ended() calls in disconnect cleanup, LeaveLobby, D13 implicit-leave, and
    cleanup_stale_lobbies reap paths -- all check lobby state before calling
  - Reduced reap timeout from 4 hours to 5 minutes, based on last_state_update or created_at
  - Added freshness filter to build_active_game_list(): excludes games with no state update
    in 5+ minutes (uses last_state_update or created_at)
- Files: server/src/ws_handler.rs, server/src/stats.rs
