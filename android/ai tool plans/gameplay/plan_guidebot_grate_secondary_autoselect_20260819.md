# Guide-Bot Grate and Secondary Autoselect Plan

## Scope

- Analyze the supplied Guide-Bot trace at the impassable grate
- Correct any failed or incomplete stalled-edge recovery behavior
- Trace secondary-fire weapon cycling against the configured autoselect threshold
- Ensure launcher application to all pilots is honored by gameplay
- Add focused regression coverage and verify host and Android builds

## Work

- [x] Correlate the grate trace with recovery state and generated paths
- [x] Identify the secondary-fire/autoselect state mismatch
- [x] Implement minimal fixes for both issues
- [x] Add or extend automated regression coverage
- [x] Run scoped formatting, host tests, Android build, and device validation

## Findings

- The trace first stalls on segment edge 540 to 607 and successfully replans through
  541, but then stalls on edge 541 to 606
- Recovery previously retained only one avoided destination, so avoiding 606 discarded
  607 and sent the Guide-Bot back to the first blocked grate edge
- Route recovery now retains both blocked destinations for the active objective and
  excludes both from subsequent path searches
- D1 and D2 gameplay both skipped over weapon order value 255 while autoselecting,
  even though the UI defines it as the Never Autoselect below separator
- Autoselect now stops at the separator in both games

## Verification

- Scoped code quality checks passed
- Autoselect order validation passed all 5 tests
- D1 and D2 Windows builds completed successfully
- D1 CTest passed all 33 tests and D2 CTest passed all 40 tests
- Android debug APK assembled successfully for all configured ABIs
- `test_obsidian_level6_guidebot_switch_grate.json5` passed all 44 steps on
  the Android emulator, including both avoided grate segments and departure from
  the second blocked approach
