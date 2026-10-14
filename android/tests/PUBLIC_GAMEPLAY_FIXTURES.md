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

## Combat, contact and drops

```sh
xvfb-run -a pwsh -NoProfile -File android/helpers/test_d1_gameplay_rules.ps1 \
  -DataEdition shareware \
  -NativeExecutable "$PWD/buildd1-save/main/test_upstream_compat" \
  -ImportedExecutable "$PWD/buildd2-save/main/test_upstream_compat" \
  -Output "$PWD/temp/public-combat"
```

This public profile reuses the existing actual engine operations and their assertions. It adds public-asset runtime coverage rather than copying asset-free unit tests:

- 30 difficulty/invulnerability damage cases (force, scrape and radial damage) and 30 difficulty-sensitive resource pickups
- 32 motion cases, 8 robot contacts and 16 robot-pair intersections
- 36 resource drops, 4 Vulcan drop/acquisition cases and 27 robot-egg cases
- 44 robot blasts, 11 secondary explosions and 30 volatile projectile impacts
- 2160 controlled replacement-drop inventory decisions, including logical IDs without public artwork

D1 uses all 11 packaged robot definitions: 0, 1, 2, 3, 4, 14, 16, 17, 18, 19 and 20. The ordinary D2 control uses all 10 packaged definitions: 25, 30, 31, 33, 37, 38, 39, 40, 42 and 50. Model availability must match these exact sets; no unavailable robot is silently skipped. D2 contact/pair/egg actors are explicitly mapped to its own real models. The existing secondary-explosion helper excludes companion/thief actors, leaving 8 D2 cases; D2 also has 40 blast and 8 Vulcan/Gauss drop cases.

Laser, Concussion and Homing supply actual public impact-projectile physics, including both sides of D2's volatile-impact radius threshold. Mega projectile effects, Omega physical drops and quarter-damage resistant-boss classes remain retail-only. The public profile does not fabricate those absent assets. Native and imported traces must match exactly; the ordinary D2 control retains its trainee half-contact/quarter-blast reductions, exploding-contact rejection, bounce orientation, resource-drop suppression and egg RNG assertions.

All actors run in controlled corridor geometry with explicit state and timing. Source-profile evidence records the audited models, projectile IDs, mission and exclusions. It does not claim level traversal. The original broad retail `--gameplay-rules-trace` dispatch and default runner profile retain their original actor/projectile lists and assertions.
