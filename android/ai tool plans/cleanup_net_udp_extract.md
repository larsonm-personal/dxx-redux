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

## Validation

- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- `android\stop-stale-formatters.ps1`
- `android\run-code-quality.ps1 -Fix`
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- `run-windows-build.ps1 -Target both`
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- `android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest --console=plain`
- `run-windows-build.ps1 -Target both`

## Next sub-tranche

- Next narrow 3.2 step: remove the concluded Android-only Category A PDATA diagnostics in `net_udp.c` while leaving the lower-volume warning and error paths from Category B in place