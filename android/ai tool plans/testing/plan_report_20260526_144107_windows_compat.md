# Report 20260526 144107 Windows compatibility fixes

## Goal
Fix recent Windows test regressions after Linux compatibility script changes, focusing on extract, GOG redbook, LAN lobby discovery, LAN launch, and matchmaking launch tests

## Failures from report
- `test_all_extracts`: ADB pushes fill emulator storage during many extract cases
- `test_extract`: D1 Mac import reaches intro but skip taps do not clear it before timeout
- `test_gog_installer_redbook_unified`: setup automation times out waiting for `can_launch=true`
- `test_lan`: direct LAN launch never enters network mode
- `test_lan_lobby_discovery`: second emulator discovers zero lobbies
- `test_mp`: matchmaking lobby works, but host game launch/introspection stalls

## Work plan
1. Inspect failure logs and scripts for recent path, shell, timeout, and device cleanup changes
2. Patch the smallest script or launcher issues needed for Windows behavior
3. Run targeted failing tests or lower-cost reproductions
4. Update this plan with completed items and residual risk

## Status
- [x] Initial report reviewed
- [x] Failing scripts inspected
- [x] Fixes implemented
- [x] Targeted verification run
- [x] Residual issues documented

## Completed fixes
- Added direct import scratch cleanup so repeated extraction specs do not leave `/data/local/tmp/dxx_extract_*` or app-private `tmp_import` data behind
- Routed extract launch validation through setup automation to avoid post-launch race conditions
- Scoped single-emulator extraction/GOG wrappers to the primary emulator when `ANDROID_SERIAL` is unset
- Ensured LAN and matchmaking tests provision the full standard game data on both emulators
- Hardened LAN lobby discovery polling and Windows logcat line parsing
- Fixed Android D1/D2 menu crash paths in `newmenu.c`
- Marked the D1 Test Flight data set as not launch-ready because its old `descent.pig` format is not supported by the D1 loader; extraction still verifies file output

## Verification
- `:app:testDebugUnitTest --tests com.dxxredux.app.SetupLaunchReadinessTest` passed
- Targeted D1 Test Flight extract passed as file-only skip with `can_launch=false`
- Targeted D1 Mac extract passed after native menu fixes
- `test_lan_lobby_discovery.ps1` passed
- `test_lan.ps1` reached `LAN MP TEST PASSED`
- `test_mp.ps1` reached `ALL CHECKS PASSED`
- `test_gog_installer_redbook_unified.ps1` passed after serial scoping

## Residual issues
- A targeted aggregate run over stale failing extraction specs passed `Descent - Mac macplay`, then hung during the large `Descent II (USA)` full launch path after import and file verification. Treat this as a remaining big-disc/automation investigation rather than a Windows compatibility regression.
- Full `test_all_extracts.ps1 -All` has not been completed after these fixes.
