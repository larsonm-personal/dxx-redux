# Co-op hostage HUD counts

## Problem
The HUD treats every hostage absent from the mine and the local ship as lost
Remote onboard hostage counts are missing from normal ship-status updates

## Plan
- [x] Synchronize Android onboard counts through authenticated ship-status packets in D1 and D2, including pickup and death updates
- [x] Use the team onboard total for the shared co-op HUD
- [x] Add focused HUD regression coverage, format changed files, and build both games

## Validation
- Scoped `run-code-quality.ps1 -Fix -Paths @(...)` passed
- `run-windows-build.ps1 -Target both` passed
- `ctest --test-dir buildd1 -R "^test_hud_(counts|layout)$" --output-on-failure` passed (2/2)
- `ctest --test-dir buildd2 -R "^test_hud_(counts|layout)$" --output-on-failure` passed (2/2)
- The new CTest target is included in the existing `android/tests/test_native_host_unit_tests.ps1` runner
- `gradlew.bat -p android :app:externalNativeBuildDebug "-Pandroid.injected.build.abi=x86_64" --console=plain --no-daemon` passed for D1 and D2
- Builds reported existing warnings outside the changed logic; no new warnings were introduced
- Live multiplayer pickup/death verification has not been run

## Network format
- Android D1 protocol 30065: append onboard hostages at ship-status byte 53 (54 bytes total)
- Android D2 protocol 30068: append onboard hostages at ship-status byte 83 (84 bytes total)
- Desktop packet lengths and protocol versions are unchanged
- Android co-op peers need matching updated builds
