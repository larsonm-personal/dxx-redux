# Public loaded-asset gameplay fixtures

These probes use the pinned public D1 1.4 PC shareware and D2 1.0 PC demo packages already admitted by `setup_shareware_test_data.ps1`. They do not use retail files or embedded saves.

## Pickup and autoselect

```sh
pwsh -NoProfile -File android/helpers/setup_shareware_test_data.ps1 -Game both -Download
xvfb-run -a pwsh -NoProfile -File android/tests/test_pickup_autoselect_parity.ps1 \
  -DataEdition shareware \
  -NativeExe "$PWD/buildd1-save/main/test_upstream_compat" \
  -ImportedExe "$PWD/buildd2-save/main/test_upstream_compat" \
  -Output "$PWD/temp/public-pickup"
```

The existing 2112-case matrix is unchanged: 4 ordering configurations, 11 pickup scenarios, 4 firing/delay policies, both once-only settings, both consumed-allowance states, and 3 pending selections. Native D1 and imported D1-in-D2 must match every acquisition, release and follow-up state: consumption, inventory, selected/pending weapon, flags, laser level, energy/ammo, rearm delay, firing count and Fusion charge. Pickup/selection must not advance SIM RNG. The native quad identity also passes real file/memory and opposite-endian runtime-record restoration; ordinary D2 must reject that native-only identity.

An ordinary D2 demo control completes the same matrix after the engine leaves the D1 bank. All three traces require the exact complete case-key sequence. `-D2Baseline` remains available for an independently retained ordinary D2 trace.

This is a controlled corridor fixture with loaded asset tables. It calls actual pickup and selection code, but synthesizes geometry and inventory. Fusion/Plasma logical inventory states do not establish artwork or playable pickup availability in the shareware levels. The asset report records selected mission, bank counts, and the actual clip/sound mappings. It is not a no-warp traversal or input replay.

Each run uses a new directory, only pinned files are staged, and output cannot overlap a supplied data directory. Evidence includes binary hashes, public package/file identities, stdout/stderr, source profiles and comparison report. The small host-only rejection test validates the report contract; it is not runtime gameplay coverage.

## Retail compatibility

The default remains `-DataEdition retail`, with explicit `-D1Assets` and `-D2Assets`. Existing `-Mode pickup`, `selection`, and `profile` remain available; public mode is deliberately limited to `pickup`. Retail runtime validation still needs the caller's retail data and is not claimed by the public CI job.
