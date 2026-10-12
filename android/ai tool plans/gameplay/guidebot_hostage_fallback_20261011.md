# Restore ordinary unreachable-hostage behavior

- [x] Replace enhanced hostage prerequisite planning with ordinary reachability and failure handling
- [x] Update the live engine regression to cover unreachable hostages with an existing red-key objective, reachable hostages, and no remaining hostages
- [x] Run scoped formatting, the D2 host build, and live navigation regression
- [x] Review other actionable evidence in debuglog_20261011_220042 (1).txt

The Android segment planner is disabled during gameplay. Its retained route can
be reused as an unrelated hostage prerequisite, producing the reported red-key
loop. Defer prerequisite planning and use the existing can't-reach response.
Keep serialized hostage fields for existing save/checkpoint layouts. Do not edit
android/outstanding_bugs.md or unrelated work in progress.

## Validation

- Scoped `android/run-code-quality.ps1 -Fix`: passed
- `run-windows-build.ps1 -Target d2`: passed, no new warnings in changed files
- `android/tests/test_guidebot_live_navigation.ps1 -NoBuild`: passed all three
  scenarios twice with identical results, including all hostage failure,
  stale-objective, no-planning, reachable-hostage, and rescue assertions
- Native regression evidence:
  `android/temp/guidebot_live_navigation/run_ec8045b07d18466b81865b2944cb83ab`
- Device coop reproduction was not run

## Other log findings

- At 22:03:32.498 and 22:03:44.695, Vertigo level 2 (`d2xlvl02.rl2`)
  single-player restores fail validation at `stage=effects`, at the same file
  offset. The `[COOP DESYNC]` entries record the return-to-menu mechanism; they
  are not evidence of a multiplayer desync. The existing mod-save compatibility
  work is relevant, but this log does not identify the rejected effect field
- From 22:02:16 to 22:03:13, 21 worst-frame introspection samples take 34-86 ms
  (59 ms mean). The measured stage includes `android_mod_usage_flush`, which in
  the logged build rehashes loaded sound data and serializes the report at up
  to once per second. That is a concrete suspect, not a separately timed root
  cause. Existing work gates reporting on the game-log category; finer timing
  would distinguish report generation, state collection, and file writes
- The repeated guidebot resets and short-path wandering accompany the stale
  hostage prerequisite. Do not infer an independent movement defect until
  retesting with ordinary unreachable-hostage behavior
