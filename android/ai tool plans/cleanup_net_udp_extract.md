# Phase 3 Cleanup: net_udp Extraction

## Goal

Start phase 3.2 by extracting the smallest low-risk `net_udp.c` helper slice into
shared native code before moving on to the larger diagnostics and host-migration
paths.

## Scope

- android/app/src/main/cpp/shared/net/net_udp_android.h
- android/app/src/main/cpp/shared/net/net_udp_android.c
- d1/main/net_udp.c
- d2/main/net_udp.c
- d1/main/CMakeLists.txt
- d2/main/CMakeLists.txt

## Work items

- [x] Confirm the address/identity helper slice is the smallest safe first extraction
- [x] Move the shared sockaddr and player-identity helpers to `shared/net`
- [x] Replace the duplicated D1 and D2 helper bodies with thin wrappers
- [x] Wire the shared helper source into both D1 and D2 UDP targets
- [x] Run validation and record the result
- [x] Pick the next 3.2 sub-tranche after the first validated extraction

## Guardrails

- Keep all call sites in `net_udp.c` unchanged outside the extracted helper bodies
- Preserve the Android-only callsign fallback for disconnected players
- Keep the helper usable in host builds so the Windows validation path stays intact
- Do not widen this first 3.2 step into PDATA logging cleanup or host rebind logic yet

## Result

- The first 3.2 helper extraction is now validated on Android and Windows host builds
- The shared header had to include the real `_sockaddr` owner context, and the host targets had to expose `${CMAKE_CURRENT_SOURCE_DIR}` so the shared source could include `multi.h`, `player.h`, and `net_udp.h`
- The final shared helper keeps the original Android fallback behavior exactly by preserving the `!connected` check from the D1 and D2 local implementations
- On the current branch, most of the originally cataloged Category A PDATA TX/RX counter blocks were already gone before this pass, so the next real low-risk logging slice was narrower than the candidate note suggested
- The follow-up 3.2 logging trim removed the per-player `read_sync` address dump and the generic `net_udp_listen` heartbeat summary while keeping the lower-volume `pdata=0 / mdata>0` warning path intact
- The next follow-up 3.2 logging trim removed the success-path `[ANDROID]` sync and level-transition `net_log_comment(...)` lines in `wait_for_sync`, `send_sync`, `request_poll`, and `level_sync` while keeping failure and warning paths intact
- That trim briefly left a stale D2 `send_sync` `logbuf` declaration behind; removing it restored the slice to the same warning profile as the surrounding branch state
- The next helper extraction moved the duplicated Android `net_udp_rebind_for_hosting` body into `shared/net/net_udp_android.c` and reduced the D1 and D2 copies to thin wrappers with a local MPDIAG adapter
- The shared helper kept the existing control flow and log strings intact by taking the socket state, open and close callbacks, and a preformatted message logger from the local wrappers
- The next helper extraction moved the duplicated Android `mpdiag_pkt_dump` body into `shared/net/net_udp_android.c` and reduced the D1 and D2 copies to one-line wrappers, leaving all TX and RX call sites unchanged
- This helper stayed Android-only in the shared file, so the host builds kept the same compile surface while still linking the shared translation unit cleanly
- The previously queued PDATA RX or TX counter cleanup turned out not to be a logging-only slice on the live branch because that state still drives `Netgame.players[player].loss`, so it stayed out of this pass
- The next follow-up trim instead removed the remaining success-path `MPDIAG(...)` chatter in `read_sync` and `send_sync`, including the `CONNTYPE[...]` sync-transition logs, while keeping the actual connection-state writes plus warning and failure paths intact
- The next follow-up trim removed the remaining success-path object-sync `MPDIAG(...)` chatter in `send_objects` and `read_object_packet`, while keeping truncated-packet checks, overwrite warnings, retry logs, and sync-failure diagnostics intact

## Validation

- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- `android\stop-stale-formatters.ps1`
- `android\run-code-quality.ps1 -Fix`
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- `run-windows-build.ps1 -Target both`
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- `run-windows-build.ps1 -Target both`
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- `android\stop-stale-formatters.ps1`
- `android\run-code-quality.ps1 -Fix`
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- `run-windows-build.ps1 -Target both`
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- `android\stop-stale-formatters.ps1`
- `android\run-code-quality.ps1 -Fix`
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- `run-windows-build.ps1 -Target both`
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- `android\stop-stale-formatters.ps1`
- `android\run-code-quality.ps1 -Fix`
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- `run-windows-build.ps1 -Target both`
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- `android\stop-stale-formatters.ps1`
- `android\run-code-quality.ps1 -Fix`
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- `run-windows-build.ps1 -Target both`
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- `android\stop-stale-formatters.ps1`
- `android\run-code-quality.ps1 -Fix`
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- `run-windows-build.ps1 -Target both`
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- `android\stop-stale-formatters.ps1`
- `android\run-code-quality.ps1 -Fix`
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- `run-windows-build.ps1 -Target both`

## Next sub-tranche

- Next narrow 3.2 step: trim the remaining success-path `verify_objects` `MPDIAG(...)` lines while keeping the `FAIL packet_loss` and `FAIL missing_players` diagnostics intact, then revalidate before touching broader log-wrapper work