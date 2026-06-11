# Reactor shootable route metadata

## Goal
- Fix false `target unreachable or blocked by unsupported door` reports for levels where the reactor can be shot through a transparent/render-past wall from a reachable segment.

## Steps
- [x] Inspect the scanner view callbacks and game adapter doorway APIs.
- [x] Add a general shootable-reactor check without mission-specific special cases.
- [x] Expand the shootability check to sample deterministic interior firing points within physically reachable segments, not only segment centers.
- [x] Regenerate Plutonian Shores metadata and confirm levels 1 and 5 no longer report partial route failures.
- [x] Run focused formatting/tests for the touched files.

## Verification
- Ran `buildd2\main\dxx-redux-d2-headless-metadata.exe -hogdir game_data_to_copy_to_emulator\temp -extra-dir temp\plutonia_coop_start_stage -mission plutonia -secretarea-json-out temp\plutonia_secretarea_reactor_shootable_after_sample.json`.
- Plutonian Shores level 1 `Infiltraitor`: `travel_status=ok`, `travel_targets_reached=6`, `travel_targets_total=6`.
- Plutonian Shores level 5 `Subterranea`: `travel_status=ok`, `travel_targets_reached=4`, `travel_targets_total=4`.
- Full Plutonian Shores dump has zero remaining `target unreachable` or `unsupported door` travel problems.
- Rebuilt `dxx-redux-d2-headless-metadata` and `dxx-redux-d1-headless-metadata`.
