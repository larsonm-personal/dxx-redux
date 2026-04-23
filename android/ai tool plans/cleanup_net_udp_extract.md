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
- The next follow-up trim removed the remaining success-path `verify_objects` `MPDIAG(...)` lines while keeping the `FAIL packet_loss` and `FAIL missing_players` diagnostics intact
- The next follow-up trim removed the remaining success-path `add_player ... added as player` `MPDIAG(...)` line while keeping the duplicate-callsign dump diagnostics intact
- The next follow-up trim removed a larger join-handshake bundle of success-path `MPDIAG(...)` lines in `net_udp_new_player`, `net_udp_welcome_player`, `net_udp_add_player`, and `net_udp_process_request` while keeping the duplicate-callsign dump, reconnect rejection, and `NO MATCH` diagnostics intact
- The next follow-up trim removed the remaining `game_connect` and `manual_join` success-path `MPDIAG(...)` chatter while keeping timeout, version-mismatch, illegal-port, socket-failure, DNS-failure, and explicit ESC-cancel diagnostics intact
- The next follow-up trim removed the remaining success-path `sync_token` and `game_info_token` `MPDIAG(...)` lines while keeping the `sync_token: MISMATCH ...` diagnostics intact
- The next follow-up trim removed the remaining packet-dispatch success-path `MPDIAG(...)` chatter for `rx GAME_INFO_REQ`, `tx GAME_INFO`, `rx UPID_GAME_INFO`, and `UPID_REQUEST` while keeping the `GAME_INFO_REQ check returned ...` diagnostics intact
- The next follow-up trim removed the remaining `request_poll`, `wait_for_requests`, and `level_sync` success-path `MPDIAG(...)` summaries while keeping the abort, disconnect, and failure diagnostics intact
- The next follow-up trim removed the remaining Android auto-flow success-path chatter in `auto_join`, `auto_host`, and `net_udp_start_game`, including request-progress and join-success `MPDIAG(...)` lines plus entry `net_log_comment(...)` lines, while keeping failed-open, failed-resolve, exit-button abort, version-mismatch, timeout, mission-not-found, and Player_num-adjustment diagnostics intact
- The next follow-up trim removed the remaining direct-connection recovery success-path chatter in `net_udp_timeout_check`, `net_udp_process_p2p_ping`, `net_udp_process_p2p_pong`, `update_address_for_player`, and `reattemptDirect`, while keeping timeout, illegal-player, pong-timeout, disconnect, and proxy-fallback diagnostics intact
- The same larger pass also removed the remaining routine join or game-info bookkeeping `con_printf(CON_DEBUG, ...)` lines for token-set success and player-address narration in `net_udp_new_player` and `net_udp_process_game_info`, while keeping token-mismatch and malformed-packet diagnostics intact
- The next helper extraction moved the duplicated direct-reattempt helper set into `shared/net/net_udp_android.c` and reduced the D1 and D2 copies of `net_udp_send_p2p_reattempt_direct`, `net_udp_process_p2p_reattempt_direct`, `resetProxy`, and `reattemptDirect` to thin wrappers with small local log adapters where the existing callback types differed
- The shared send helper preserves the live packet layout by serializing `connect_to_player` into the reattempt packet body, while the shared process and proxy helpers keep the local send callback, timer source, address update hook, and connection-status storage ownership unchanged

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
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- `android\stop-stale-formatters.ps1` or fallback direct stale-formatter process query when the helper hits null `CommandLine` entries
- `android\run-code-quality.ps1 -Fix`
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- `run-windows-build.ps1 -Target both`
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- fallback direct stale-formatter process query for `run-code-quality.ps1|clang-format|ktlint|PSScriptAnalyzer|shfmt|shellcheck`
- `android\run-code-quality.ps1 -Fix`
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- `run-windows-build.ps1 -Target both`
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- fallback direct stale-formatter process query for `run-code-quality.ps1|clang-format|ktlint|PSScriptAnalyzer|shfmt|shellcheck`
- `android\run-code-quality.ps1 -Fix`
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- `run-windows-build.ps1 -Target both`
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- fallback direct stale-formatter process query for `run-code-quality.ps1|clang-format|ktlint|PSScriptAnalyzer|shfmt|shellcheck`
- `android\run-code-quality.ps1 -Fix`
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- `run-windows-build.ps1 -Target both`
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- fallback direct stale-formatter process query for `run-code-quality.ps1|clang-format|ktlint|PSScriptAnalyzer|shfmt|shellcheck`
- `android\run-code-quality.ps1 -Fix`
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- `run-windows-build.ps1 -Target both`

## Next sub-tranche

- Next larger 3.2 step: extract the duplicated `net_udp_welcome_player` slot-selection and reconnect-reset helper cluster after `find_player_by_identity`, covering open-slot selection, oldest-disconnected replacement, reconnect address refresh, and Android connection-type reset while keeping the HUD, score, and object-sync side effects local, then revalidate before reopening smaller trims