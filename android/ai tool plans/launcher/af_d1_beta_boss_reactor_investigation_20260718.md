# AF D1 Beta Boss/Reactor Metadata Investigation

## Goal

Determine whether the regenerated boss-to-reactor route changes in `af_d1_beta.json` are caused by the large-level metadata optimization, and identify the exact correctness issue without modifying checked-in metadata or planner behavior during the investigation.

## Phase 1: Reproduce and isolate

- [x] Identify every changed level and route field in the regenerated metadata
- [x] Reproduce `af_d1_beta.zip` with the current D1 headless analyzer
- [x] Compare current planner decisions with the pre-optimization planner behavior

## Phase 2: Explain the decision

- [x] Trace boss and reactor reachability, visibility, distance, and tie-breaking inputs
- [x] Determine which optimization or cache rule changes the selected objective
- [x] Check whether Uneasy4 output parity missed this route-planner edge case

## Phase 3: Report

- [x] Record the root cause, affected scope, and recommended correction
- [x] Update this plan with reproduction evidence and any remaining uncertainty

## Phase 4: Host/emulator data parity fix

- [x] Make host regeneration select D1 base data using the emulator's pinned DOS hashes
- [x] Report the selected D1/D2 data directories and validated hashes
- [x] Add focused regression coverage for rejecting mismatched candidate data
- [x] Run scoped code quality, PowerShell tests, and a focused AF D1 Beta regeneration
- [x] Confirm the regenerated metadata matches the checked-in boss routes

## Results

- The regenerated file changes Cefispar Mine and Firehand Basin. Both replace the final boss objective with a reactor objective and adjust the final route distance and travel time.
- This is not caused by the large-level progress, timeout, or route visibility optimization. Every retained host-regeneration artifact from July 16 and July 17 already contains the same reactor results, before those changes were made. Emulator artifacts contain boss results.
- A controlled run using the same current D1 executable and the same extracted mission reproduced the full difference from only the base D1 data directory:
  - Mac D1 data: Cefispar reactor segment 235, Firehand reactor segment 309, exactly matching the new file.
  - DOS D1 data: Cefispar boss segment 235, Firehand boss segment 314, exactly matching the checked-in file, including travel distances.
- Temporary headless diagnostics confirmed that both levels contain robot ID 23. The DOS `descent.pig` marks it as a boss, producing boss objects 21 and 168. The Mac `descent.pig` does not mark it as a boss, so route planning correctly falls back to each reactor.
- The host helper resolves D1 data by filename and currently prefers `game_data/extracted/d1 mac extracted`. Emulator automation provisions the pinned DOS retail `descent.hog` and `descent.pig` hashes from `Get-StandardGameDataDeps`. The two regeneration paths therefore analyze different robot definitions.
- Recommended correction: make host regeneration resolve the same pinned DOS D1 data hashes as emulator regeneration, and report the selected data directory and hashes. Do not accept the reactor metadata update if checked-in metadata is intended to describe Android gameplay.
- The temporary diagnostic source change was removed after collecting the evidence. No planner behavior or metadata file was modified by this investigation beyond the user-generated `af_d1_beta.json` already present at the start.

## Fix results

- Moved the 11 pinned standard game-data dependencies into `standard_game_data.ps1`, which is now shared by emulator test provisioning and host metadata regeneration.
- Host regeneration resolves required D1 and D2 files by case-insensitive filename and SHA-256 hash. It skips filename-compatible data from another release and fails if no exact pinned set exists.
- The helper reports the selected D1/D2 directories and every validated required hash before analysis begins.
- The resolver regression test proves a mismatched first candidate is rejected, a later exact candidate is selected, hashes are returned for reporting, and an all-mismatched candidate list fails.
- Full host regeneration selected `game_data_to_copy_to_emulator/temp` for both games, passed 109 of 110 archives with one expected size skip, and had zero failures. `af_d1_beta.json` returned to the checked-in DOS boss routes with no remaining diff.
- Correct DOS data exposed 21 other stale D1 metadata files that had been generated from Mac robot definitions. Those regenerated files now describe the same boss objectives and routes used by Android.
- Scoped PowerShell code quality passed. Resolver, JSON normalization, travel-time, and shared dependency tests passed. Both D1 and D2 Windows CMake builds passed.
