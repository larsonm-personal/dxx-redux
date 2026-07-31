# Coop rewind round 5 log analysis

## Scope

Diagnose the supplied host/client log where the host remained on the waiting
screen and the client entered the level after the host left that screen.

## Plan

- [x] Isolate the rewind request, save transfer, restore, and level sync timeline
- [x] Match the logged events to the D1, D2, and shared coop implementation
- [x] Identify the most likely failure chain and the next useful diagnostics
- [x] Report findings without changing gameplay code

## Findings

- Rewind transfer 1 was queued successfully at 22:29:05.290
- The client acknowledged its receive buffer at 22:29:05.352
- Three later rewind attempts were correctly rejected because transfer 1 was
  still active; they did not cancel transfer 1
- Transfer 1 finished sending at 22:29:11.778 and the client acknowledged that
  it was entering apply at 22:29:11.841
- The host then restored successfully and entered the normal UDP level-sync
  barrier at 22:29:12.694
- The host remained in that barrier until 22:29:23.597, when level sync returned
  success after the waiting screen was manually bypassed
- This matches the reported client release: leaving the host waiting screen
  allows `net_udp_send_sync()` to run, which releases a client already waiting
  in `net_udp_wait_for_sync()`
- The missing event is a client `UPID_REQUEST` changing the host-side client
  slot from `CONNECT_WAITING` to `CONNECT_PLAYING`
- The first client request can race with and be discarded by the two
  `net_udp_flush()` calls around host level sync. The client should retry every
  two seconds, so the remaining likely branches are that retries were not sent,
  were routed incorrectly, failed reconnect authentication, or were received
  outside `NETSTAT_WAITING`
- The supplied file contains only the host Coop Desync log. A paired client log
  plus Network-category logs are needed to separate those branches

## Client log follow-up

- [x] Align the client transfer, restore, and level-sync events with the host
- [x] Confirm whether the client enters `net_udp_wait_for_sync()`
- [x] Determine whether readiness requests are absent, rejected, or misrouted
- [x] Record the paired-log root cause or smallest remaining uncertainty

## Paired-log root cause

- At 22:29:11.852 the client rejects the host-authored rewind image because
  its saved top-level coop callsign is `touch` while the client's callsign is
  `Player68`
- The client reports rewind status 3, which is
  `ANDROID_REWIND_STATUS_FAILED`, and never reaches `StartNewLevelSub()` or
  `net_udp_wait_for_sync()`
- The host has already received the client's pre-apply acknowledgement, so it
  assumes the client is entering restore, restores successfully, and waits for
  a level-ready request that the failed client can never send
- Authoritative coop save restore and level restart call
  `state_restore_coop_from_memory()`, which enables callsign remapping
- Multiplayer rewind instead calls `state_restore_from_memory()`, which
  explicitly disables that remapping. This is the direct code-path mismatch
  responsible for the client failure
- The immediate fix target is the authoritative rewind restore call: use the
  coop-aware memory restore path for `GM_MULTI_COOP`, in both D1 and D2 through
  the shared implementation
- A separate robustness improvement would acknowledge restore success or
  failure after applying, since the current apply-ready message is sent before
  the client knows whether its restore will succeed
- Status 3 is `ANDROID_REWIND_STATUS_FAILED`, including on the host. After the
  waiting screen is bypassed, the host reaches `coop_save: restored metadata`
  and then returns failure
- In the D2 restore code, the only failure return after that log and before the
  normal success return is a failed
  `coop_powerup_duplication_apply_pending()` validation. The saved rewind image
  contains ten per-player pickup records at the target point, so this is a
  second restore blocker that needs its own diagnostics or correction

## Implementation

- [x] Route authoritative multiplayer rewind through the coop-aware memory
  restore path
- [x] Fix restored per-player pickup validation against the restored object set
- [x] Add focused regression coverage for restored pickup-history validation
- [x] Run scoped code quality and relevant native tests
- [x] Run Android and Windows build verification
- [x] Record the final verification results

## Implementation notes

- Authoritative rewind now calls `state_restore_coop_from_memory()` while in
  `GM_MULTI_COOP`, preserving the established callsign-remapping behavior used
  by coop save restore and level restart
- Restored pickup history now remaps valid records by object signature and
  discards records whose object no longer exists, is no longer eligible, or is
  duplicated. Malformed identities and allocation failures remain hard errors
- A focused production-source test verifies signature remapping, stale and
  duplicate pruning, and rejection of malformed identities in both games. The
  coop restore dispatch is covered by the D1/D2 Android native builds

## Verification

- Scoped `android/run-code-quality.ps1 -Fix`: passed
- `run-windows-build.ps1 -Target both`: passed for D1 and D2
- D1 native tests: 27 of 27 passed
- D2 native tests: 31 of 31 passed
- Android `:app:assembleDebug`: passed for arm64-v8a, armeabi-v7a, and x86_64
- `git diff --check`: no whitespace errors; only pre-existing CRLF conversion
  warnings were reported
- Two-device coop rewind still requires an on-device confirmation run
