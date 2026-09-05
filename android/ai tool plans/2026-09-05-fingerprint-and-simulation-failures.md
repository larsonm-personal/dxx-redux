# Fingerprint and simulation failures

1. Fix omitted MissionDir under strict mode and test source selection
2. Initialize headless player defaults and reproduce Don't Panic's secret-level pickup
3. Verify focused runs and record remaining batch failures without masking them

## Verification

- Fingerprint failure was null MissionDir.Count under inherited strict mode
- Added null guard and tests for omitted, null, empty and explicit source directories
- Real Castaway soundtrack extraction/fingerprinting passed: 10 files, zero errors, temporary output, AcoustID disabled
- Headless initialization now calls new_player_config instead of leaving weapon ordering and other player defaults zeroed
- Don't Panic secret 1 and D1-to-D2 Conversion level 9 both completed twice with matching results
- Junine failure was segment -1 escaping failed waypoint localization during smoothing; retaining the original unsmoothed waypoint avoids invalid collision input
- Junine level 8 returned controlled simulation timeouts in two matching runs
- Windows D2 build, 47 CTest tests, reusable player-defaults test, fingerprint budget/source tests and scoped quality checks passed
- Checked-in regression artifacts were not regenerated

## Remaining

- Destination Saturn level 15 still has an asset/mission-loading mismatch
- Six outer process timeouts remain from the original run
- No blanket timeout increases or error suppression applied
