## Goal

- continue the next small d1/d2 diff minimization batch after the shared `auto_net` extraction
- prefer a nearby Android-only `net_udp.c` helper that is still duplicated in `d1/main` and `d2/main`

## Plan

- [completed] identify one small duplicated `net_udp_welcome_player` slot-selection and reconnect-prep seam with a clean shared-helper boundary
- [completed] extract that seam into the existing shared net helper surface with minimal call-site change
- [completed] run focused validation on the touched slice
- [completed] update the plan with the outcome

## Outcome

- extracted `android_net_udp_select_welcome_player_slot()` into `android/app/src/main/cpp/shared/net/net_udp_android.c` to centralize the duplicated open-slot and oldest-disconnected replacement selection logic used by `d1/main/net_udp.c` and `d2/main/net_udp.c`
- extracted `android_net_udp_prepare_reconnect_player()` into the same shared helper file to centralize reconnect address refresh and the Android-only direct-connection reset while keeping HUD, score, and object-sync side effects local in each game
- updated both `net_udp_welcome_player()` call sites to use the shared helper return codes from `android/app/src/main/cpp/shared/net/net_udp_android.h`
- validated with `android\gradlew.bat :app:externalNativeBuildDebug --console=plain`
- validated with `run-windows-build.ps1 -Target both`
- validated with `android\run_quick_tests.ps1` -> 14 passed, 0 failed, 0 timed out, 1 skipped for budget, total `00:02:44`

## Next Candidate

- revisit the remaining duplicated `net_udp_welcome_player` observer-join and object-sync setup path, or step one hop later in `net_udp.c` to the next identical helper-sized branch that still differs only by surrounding D1/D2 wiring