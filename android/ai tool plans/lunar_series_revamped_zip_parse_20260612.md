# Lunar Series Revamped ZIP parse failure

## Goal
- Re-run `game_data/mission_files/Lunar Series Revamped.zip` with the newest build.
- Determine whether the failure is already fixed or still reproducible.
- If still failing, inspect the ZIP/import/metadata artifacts and patch the responsible parser or test path.

## Plan
- [x] Build or confirm a fresh debug APK
- [x] Run focused mission ZIP analysis for `Lunar Series Revamped.zip`
- [x] Inspect current and prior artifacts if it fails
- [x] Identify the parse/import failure cause
- [x] Apply the smallest fix and rerun the focused test

## Notes
- Full batch launch test still times out, but metadata-only analysis passes with the newest build.
- The real metadata result showed `level_name` as replacement characters because these RDL headers store four `0xFF` bytes as the title.
- Updated native metadata serialization to ignore unusable non-text names and fall back to the level filename stem.
- Updated the batch helper so a later launch-smoke failure no longer overwrites a saved metadata result with `{ "failure_text": "failed" }`.
- Latest focused full-batch output still reports failed because the game-side script stalls at the in-game assert, after metadata analysis has already passed.

## Launch Smoke Follow-up
- [x] Inspect the game-side assert automation implementation
- [x] Reproduce or isolate why Lunar stalls at step 24
- [x] Patch the assertion path or script behavior so it reports pass/fail
- [x] Run focused Lunar full-batch verification
