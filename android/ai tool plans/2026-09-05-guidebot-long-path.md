# Long GuideBot paths and traverse motion

1. Preserve the legacy signed-byte path cursor and save/network layout by retiring consumed prefixes of active forward GuideBot paths before the cursor overflows
2. Build and run original FirstStrike level 5, including a deterministic repeat
3. Compare the four primary missions against their existing results
4. Investigate traverse bobbing separately; do not relax collision checks or broadly alter D1 robot behavior to smooth the camera

## Findings

- Retiring consumed prefixes at cursor 120 preserves geometry and the signed-byte save/network layout
- Overflow-only primary run: FirstStrike improves from 22/30 to 29/30, adding levels 5, 7, 11, 13, 14, 18, and 23; no previously successful level regresses in the 88-level core set
- FirstStrike 21 now fails with "live Guide-Bot route has no actionable goal" after gold, instead of timing out
- Disabling move_towards_outside for objective routes was tested separately and rejected: it caused regressions including Castaway 2 and several base-campaign levels
- Bobbing remains unresolved; retain the existing geometry until a narrower motion fix is validated
- Evidence directories: android/temp/long_path_primary_20260905 and android/temp/long_path_centered_20260905

## Final validation

- Final Windows D2 build succeeds without the previous waypoint const-qualifier warnings
- All 47 native CTests and scoped code-quality checks pass
- New test_guidebot_firststrike_long_path.ps1 passes two identical runs through the exit (4516 frames, 75 rounded seconds)
- Final 88-level core run repeated twice: no nondeterminism or infrastructure failures; no previously successful level regressed
- Updated FirstStrike.simulation.json to 29 ok / 1 failed; the other three mission simulation files are byte-for-byte unchanged
- Final report: android/temp/long_path_final_20260905
