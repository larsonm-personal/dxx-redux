# Metadata regeneration timeout repair

- Diagnose the Uneasy4 host metadata timeout from run_20260911_183250
- Reproduce with checkpoints and worker timing; fix the underlying timeout or scan issue
- Verify the repaired metadata stage and refresh affected results if necessary
- Compare simulation artifacts with earlier verified results to explain the reduced diff

Existing regenerated metadata/simulation edits and the precision-recovery fix belong to the current workspace; preserve them

## Findings and repair

- Original run `temp/regression_data_reports/run_20260911_183250`: metadata had 137 passes and one failure, Uneasy4.zip; all later simulation work was attempted
- Uneasy4 was terminated after 360 seconds waiting for stdout while its native checkpoint identified active route-target visibility work
- The worker helper now treats changing semantic checkpoint fields for the current request as activity; missing, malformed, stale-request, or unchanged checkpoints do not keep a stalled worker alive
- The default idle timeout stays 360 seconds; timeout logs now include the last accepted checkpoint
- An isolated original-limit Uneasy4 run passed in 141.6 seconds and generated metadata identical to the checked-in file by SHA-256
- The new integration fixture failed before the helper change and passes after it, including successful progress beyond the idle limit, rejected stale/unchanged/malformed checkpoints, timeout/crash diagnostics, and worker recovery
- Scoped code quality and git diff whitespace checks passed
- The deliberately short 30-second Uneasy4 stress probe advanced for 221 seconds, then correctly timed out during a visibility phase with no new checkpoint for 30 seconds under load; this supports retaining the production 360-second idle limit
- Full validation completed: 138 sources passed, zero skipped or failed, exit 0, in 855.2 seconds, using the original selection with eight workers and no baseline publication
- Uneasy4 passed in 554.5 seconds (the previous stdout-only watchdog would have terminated it at 360 seconds)
- All 138 newly generated metadata files are byte-for-byte identical to their workspace counterparts; no metadata publication is needed
- Full validation artifacts: `android/temp/metadata_progress_full_validation/summary.json`; log: `temp/metadata_progress_full_validation.log`

## Simulation diff investigation

Comparing native outputs from `20260911_162054` and `20260911_184848` yielded 2,549 matching native results (90 selected items without a native pair). Of 1,336 changed frame counts, 1,296 now equal HEAD again. Thus the reduced diff is primarily reproducible restoration of previous trajectories, not metadata failure deleting results

The full run has nine timeout-to-confirmed changes and five confirmed-to-timeout changes, a net four native completion gain. The five losses are outside the previous 529-level precision-recovery validation:

- Dimensions for Descent target 36, level 2, slayer02.rdl
- d2xxl_downloads/levelpack target 241, level 3, TUNNEL.RL2
- d2xxl_downloads/saturn level 3, TUNNEL.RL2
- Disint_Beta_3 level 2, 02egre.rl2
- reetus level 3, r1.rdl

Four of these restore the timeout status and exact frame count already in HEAD; slayer02.rdl differs from HEAD (previously ok). These remain routing follow-up cases, separate from this PowerShell metadata watchdog repair. Full comparison details are in `temp/regeneration_simulation_diff_comparison.json`
