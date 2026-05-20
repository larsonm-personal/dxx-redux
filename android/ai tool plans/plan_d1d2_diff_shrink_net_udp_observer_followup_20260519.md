## Goal

- continue the next small `net_udp.c` diff-minimization tranche after the shared welcome-player slot-selection extraction
- keep the work local to the remaining duplicated observer-join and welcome-sync setup in `net_udp_welcome_player`

## Plan

- [completed] extract the duplicated observer-join setup into the shared net helper surface without touching the D2-only `WaitForRefuseAnswer` behavior
- [completed] extract the duplicated welcome-sync scheduling tail into the shared net helper surface with minimal call-site change
- [completed] run focused validation on the touched slice
- [completed] update this plan with the outcome and next candidate

## Outcome

- added `android_net_udp_prepare_observer_join()` in `android/app/src/main/cpp/shared/net/net_udp_android.c` to centralize the duplicated observer-slot selection, observer record update, and object-send flag setup while leaving the local packet copy, HUD message, and `net_udp_send_objects()` call in each game
- added `android_net_udp_begin_welcome_sync()` in the same shared helper file to centralize the duplicated welcome-sync scheduling tail after the player slot is chosen, including `player_tokens`, `Network_send_objects`, `Network_send_objnum`, and `LastPacketTime`
- updated both `net_udp_welcome_player()` implementations to route those duplicated assignment blocks through the shared helper surface while keeping the D2-only `WaitForRefuseAnswer=0` line local
- corrected the new helper signature to use `uint *player_tokens` so the extraction does not add a new pointer-sign warning on Android builds

## Validation

- `android\gradlew.bat :app:externalNativeBuildDebug --console=plain`
- `android\stop-stale-formatters.ps1`
- `android\run-code-quality.ps1 -Fix -Paths android/app/src/main/cpp/shared/net/net_udp_android.h, android/app/src/main/cpp/shared/net/net_udp_android.c, d1/main/net_udp.c, d2/main/net_udp.c`
- `android\gradlew.bat :app:externalNativeBuildDebug --console=plain`
- `run-windows-build.ps1 -Target both`
- `android\run_quick_tests.ps1` -> 13 passed, 0 failed, 0 timed out, 2 skipped for budget, total `00:03:24`

## Next Candidate

- the remaining `net_udp_welcome_player` duplication is now mostly the shared rejection preamble plus the tiny local packet-copy and HUD/send wrappers, so the next nearby candidate is either a shared reject-decision helper for the endlevel/busy/level/full checks or the next identical helper-sized branch later in `net_udp.c`