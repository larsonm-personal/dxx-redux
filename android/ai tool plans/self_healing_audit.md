# Self-healing audit

Reviewed 2026-09-28 at `b2e5c617`.

## Outcome

Recovery exists in several subsystems, but it is not yet a consistent app-wide guarantee. The main gaps are incomplete teardown, recovery loops that can stop recovering, and old asynchronous work being allowed to modify a new attempt.

Six concrete code findings follow. These are static findings, not newly reproduced device failures. The earlier logs establish the orphaned-engine problem and unsupported-save failure; they do not establish that the other findings caused those sessions' symptoms.

This audit changes documentation only. `android/outstanding_bugs.md` remains untouched.

## Scope and completed plan

- Traced game launch/lifecycle, LAN discovery/start, Internet matchmaking/proxy, and mission transfer ownership and retries
- Sampled file import/publication, soundfont download cancellation, and native metadata worker recovery
- Inspected existing recovery tests and correlated relevant paths with the supplied logs
- Recorded priorities, recovery behavior, and targeted failure-injection tests

This is a high-level Android audit, not exhaustive coverage of engine networking, the matchmaking server, every import format, or every UI action. No builds or device tests were run for this documentation-only review.

## Proposed principle

A repeated user action must either start a fresh attempt or visibly explain why an existing healthy operation must finish first. A stale flag, abandoned socket, dead worker, or obsolete callback must not silently prevent progress.

For operations that can fail:

1. Give each attempt an identity and an owner
2. Before retry, retire the previous attempt's resources and reset its deadlines and transient state
3. Cancellation must stop the actual work: close blocking I/O, await termination where practical, and retire an isolated native process when necessary
4. Only the current attempt may publish state, consume a result, or finish cleanup of current resources
5. Use bounded startup and no-progress deadlines, with visible failure and a usable retry action
6. Preserve valid user data and healthy sessions; discard only invalid or abandoned temporary state

Unsupported save data is a persistent input problem. Recovery should explain it and offer another save or an explicit fresh start, rather than repeatedly retrying the same load or silently discarding the user's selection.

## Findings

### 1. High: orphaned game cleanup is limited to multiplayer launch

Evidence:

- `SetupActivity.kt:2309,2350`: `prepareMultiplayerGameProcess()` retires a raw `:game` process when no returnable Activity marker exists, then waits up to two seconds for exit
- `SetupActivity.kt:524,538,2598`: recorded-demo and ordinary/resume launches prepare files and launch without that cleanup
- `MainActivity.kt:3109,3139`: Activity destruction cancels startup work and clears the marker, but an already-running native thread can survive
- `jni_main.c:287`: the next engine start in that process is rejected and its Activity is finished

Failure sequence: a game Activity finishes while its native engine remains alive; the user then starts a single-player game, resumes a save, or starts a recorded demo. These launch paths can reuse the orphan and encounter the same rejection previously seen in multiplayer.

The earlier client log establishes the underlying lifecycle failure: `debuglog_20260927_214219.txt` records Activity destruction at 21:44:49, native join activity afterward, and three subsequent startup rejections. The current multiplayer preflight addresses that route only.

Recommended change: make process readiness a shared launch preflight for all new engine sessions, preserving the existing distinction between a returnable game and an orphan. Do not kill a healthy game simply because another launch was requested. A PID/marker alone also does not establish responsiveness; add an acknowledgement/deadline if recovery from a hung, still-present Activity is required.

Test: extend `android/tests/test_lan_orphan_restart.ps1` to retry via ordinary Play, Resume, and recorded-demo launch. Assert a fresh PID, successful engine admission, and no loss of an existing healthy returnable session.

### 2. High: a failed LAN socket recovery can disable further recovery

Evidence:

- `lobby/LobbyService.kt:745`: `openSocket()` first calls `closeSocket()`, which cancels the scope containing the watchdog
- `LobbyService.kt:763,813`: binding happens before the replacement watchdog starts
- `LobbyService.kt:914`: `recoverTransport()` catches an open failure and sets `socketRefreshNeededOnResume`, but does not schedule another foreground attempt or publish a user diagnostic
- `LobbyService.kt:307`: `startDiscovery()` returns immediately while `_isDiscovering` is true

Failure sequence: discovery is active, the watchdog attempts recovery, and rebinding fails. Discovery remains marked active, but there is no running watchdog/receiver to repair it. Repeating `startDiscovery()` does nothing. A later background/resume cycle or explicit full stop/start can recover, but the same foreground action is not sufficient.

Recommended change: keep the recovery supervisor independent of the socket scope, or schedule bounded reopen retries from an owner that survives teardown. Roll back partially acquired resources on open failure. Publish a recoverable error, and have an explicit retry verify transport health rather than trust the discovery flag.

Test: inject a bind failure during watchdog recovery, then remove it without backgrounding the app. Verify discovery recovers, or reports failure and succeeds on Retry. Assert exactly one receiver/watchdog and no leaked multicast lock.

The supplied logs' missing broadcast reception does not prove this failure occurred; they do not establish a failed socket rebind.

### 3. High: old matchmaking callbacks can tear down a newer connection

Evidence:

- `multiplayer/MatchmakingService.kt:247`: `connect()` asynchronously closes the old WebSocket and immediately installs a replacement
- `MatchmakingService.kt:826,838`: `onOpen` and `onMessage` update global connection state without checking socket identity
- `MatchmakingService.kt:855,884`: `onClosed` and `onFailure` unconditionally clear the global socket, shut down the current proxy, clear lobby state, and schedule reconnect

Failure sequence: connection B replaces A; a late close/failure callback from A arrives after B has opened. A's callback can null B's service reference and dismantle B's session. This affects reconnect/disconnect cycles, not just simultaneous button presses.

Related ownership gap: STUN and connectivity checks cancel previous jobs without waiting for retirement. Their broad exception handlers and state updates are not guarded by attempt identity (`MatchmakingService.kt:660,714,730,756,801`). Old work can publish failure/fallback results or clear job state after a new attempt starts.

Recommended change: attach a connection/session generation to callbacks, authentication, STUN, and probes. Reject obsolete results before touching shared state. Handle cancellation separately from operational failure, close owned blocking resources, and reset the retry budget for an explicit new user attempt while preserving the budget across automatic retries.

Test: connect A, replace it with B, then deliver A's delayed close, failure, message, and auth/probe result. B must remain connected with its own proxy and lobby. Repeat after exhausting automatic reconnect attempts.

### 4. High: cancelling a mission download does not stop its file writer

Evidence:

- `multiplayer/MissionTransferService.kt:149,189`: download replacement/cancellation only calls `Job.cancel()`; it neither closes the client socket nor waits for completion
- `MissionTransferService.kt:288,449`: receive, verification, and finalization use synchronous I/O without cancellation checks in their loops
- `MissionTransferService.kt:300`: the connecting `Socket(address, port)` constructor has no application connect deadline; the 15-second read timeout is set afterward
- `MissionTransferService.kt:460,566`: attempts for the same content share a partial file and sidecar; the writer truncates to its chosen offset
- `lobby/LobbyService.kt:1438`: local download callbacks check content revision, but not the current lobby/session and transfer attempt identity

Failure sequence: a download is cancelled/replaced, including by a duplicate or delayed transfer grant. The old blocking download can continue while the replacement starts. Both can write/truncate the same partial file, finalize an import, or publish progress. Hash validation can detect bad bytes, but does not prevent concurrent writers or obsolete completion state.

Recommended change: retain and close the active socket on cancellation, bound connect time, and serialize ownership of a partial file until the old writer retires. Add cancellation checkpoints through verification/finalization and fence callbacks with session plus attempt identity. Keep the existing verified-chunk resume behavior.

Test: use a slow server, cancel/retry during a read, inject a duplicate grant, and leave/rejoin the same mission. Assert one writer, no old completion callbacks, valid resumed hashes, and bounded cancellation even when the connection stalls.

### 5. Medium: failed game launch does not consistently undo launch state

Evidence:

- `lobby/LobbyService.kt:1668`: host `gameStarted` is set and clients receive START before the local launch completes
- `SetupActivity.kt:2312,2325,2341`: preflight failure clears launcher preparation, but does not reset the hosted lobby's in-game state
- `SetupActivity.kt:498,503`: a caught `startActivity()` failure clears preparation and reports an error, but leaves `mpGameLaunching` set
- `SetupActivity.kt:2297,2789`: that guard rejects later launch events until `onResume()` resets it; a caught launch failure need not cause an Activity transition
- `SetupActivity.kt:2807`: normal lobby rollback relies on returning through `onResume()`

Failure sequence: host launch fails after START was committed, leaving clients trying to connect and discovery advertising an in-game session. In the caught Activity-start failure case, the host's next launch event is also ignored by the stale guard.

Recommended change: use one idempotent launch-failure path to clear owned launch guards, retire newly created proxy/resources, reset the host lobby, and publish failure to both sides. Keep a launch/session identity so cleanup from an old attempt cannot undo a new one. Preflight before committing START where possible; later failures still need a rollback/abort protocol.

Test: inject host preflight failure and Activity-start failure after Start. Without navigating away, retry and reach the lobby successfully; clients should receive a useful failure indication instead of waiting for an absent engine.

### 6. Medium: proxy setup/worker failure can remain only a log message

Evidence:

- `multiplayer/LocalhostProxy.kt:93,107`: `addPeer()` catches a local-port bind failure, logs it, and returns no failure result
- `multiplayer/MatchmakingService.kt:200,222,231`: `createProxy()` still publishes the proxy and reports creation
- `SetupActivity.kt:2409`: the client proceeds toward engine launch using the expected loopback endpoint
- `LocalhostProxy.kt:128,264,461`: unexpected worker exit or terminal socket errors log/exit without a service-level failure callback

Failure sequence: the proxy port cannot bind, or a forwarding loop dies. The launcher can proceed as though the route exists; the user experiences an engine join timeout rather than an actionable transport failure.

Recommended change: make proxy creation return success only after required listeners bind. Propagate unexpected forwarding termination to the session owner so it can release resources and expose a retry. Decide explicitly when a running engine can reconnect versus when it must return to the lobby; blindly recreating sockets may change peer addressing.

Test: occupy the required proxy port, attempt Join, then release it and retry. Also close an active proxy socket unexpectedly. Verify visible failure, complete teardown, and a clean next attempt.

## Additional design gap to test

Launch preparation has no overall or no-progress deadline once the multiplayer launch event is received: its request timeout is removed at `SetupActivity.kt:2301`. The preparation dialog cannot be dismissed (`SetupActivity.kt:2977`), and native startup waits on playlist preparation (`MainActivity.kt:2139`). Blocking storage/provider work could therefore leave a spinner with no recovery action. This review did not establish a specific hung provider or playlist call in the supplied sessions. Add fault injection before choosing deadlines or process boundaries; a coroutine timeout alone does not stop blocking JNI or Java I/O.

## Existing recovery worth preserving

| Area                    | Mechanisms reviewed                                                                                                                                                              | Remaining validation boundary                                                                      |
| ----------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------- |
| LAN                     | Watchdog, resume socket refresh, join retries, leases, periodic discovery queries and remembered-host probes                                                                     | Watchdog itself must survive a failed reopen; physical broadcast delivery remains a separate issue |
| Native metadata         | Request ownership, progress deadlines, cancellation marker/grace period, isolated worker termination, non-cancellable cleanup (`LevelMetadata.kt:1590,2439`)                     | Exercise interruption during result publication and immediate retry                                |
| File copying/import     | Unique temporary sibling, expected-size check, fsync, atomic publication, finally cleanup (`SetupFileImport.kt:740`); startup temporary/marker cleanup (`SetupActivity.kt:3022`) | Sampled paths only; blocked document-provider I/O still needs cancellation testing                 |
| Mission extraction      | Temporary extraction, content identity checks, invalid-cache rejection/rebuild (`MissionZipExtractionStore.kt:76,157`)                                                           | Process death during publication should be tested end to end                                       |
| Soundfont downloads     | Cancellation closes the HTTP call; connect/read/total deadlines; download separate from activation (`SoundfontDownloads.kt:53,66,134`)                                           | Do not assume this contract already applies to other downloads                                     |
| Mission transfer resume | Per-chunk verification and rollback to last valid boundary, covered by `MissionTransferResumeTest.kt`                                                                            | Existing integrity tests do not establish cancellation or concurrent-writer safety                 |
| Engine exits            | Normal native completion retires the process; fatal error reporting returns to the launcher (`jni_main.c:395,619`)                                                               | Activity destruction before native completion remains the gap in finding 1                         |

The 22:34 logs show both engines rejecting D1-in-D2 save version 31, not evidence that resetting networking would make that save load. Current save preflight and host/client warning propagation address that input failure and should remain separate from transport recovery.

## Suggested implementation order and acceptance bar

1. Fix shared launch readiness and launch rollback, together with the LAN recovery supervisor
2. Fence matchmaking generations and make mission-transfer cancellation stop actual I/O
3. Propagate proxy failures and add launch progress deadlines informed by fault injection

Extend the existing orphan/discovery and metadata/import runners rather than creating a large new test framework. Add a controlled transport harness where delayed callbacks and duplicate grants are needed.

For each regression: make attempt A fail, retry B immediately in the same app session, deliver any late A result, and verify B succeeds or reports a bounded, actionable error. Assert no extra engine processes, sockets, writers, or stale launch guards. Preserve valid saves and verified partial content. Log attempt identity, recovery reason, cleanup result, and deadline expiration so a user-exported log can explain the recovery.
