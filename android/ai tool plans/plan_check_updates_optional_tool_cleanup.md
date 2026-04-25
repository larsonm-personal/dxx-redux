# Check Updates Optional Tool Cleanup

## Goal
Determine whether DOSBox-X, StuffIt, and any other odd updater rows are still needed,
then clean up android/get_deps/check-updates.ps1 and related config/scripts so the
table reflects only meaningful actions.

## Status
- [x] Audit current usage of DOSBox-X and StuffIt across scripts, tests, and docs
- [x] Identify other updater rows that are optional, stale, or otherwise misleading
- [x] Apply the smallest cleanup that matches current usage
- [x] Re-run safe updater validation and confirm the table output

## Notes
- DOSBox-X and StuffIt remain available through their manual helper scripts, but are no longer shown in check-updates because they are not part of the normal Android build, test, or import bootstrap
- No other updater rows were cleaned up in this pass because the remaining rows still map to active build, lint, runtime, or import workflows