# Metl154 tranche 9: launcher debug controls surface

## Scope

This tranche starts Part C.1 and the remaining launcher-facing part of C.2 from
`metl154_postfix_cleanup_and_debug_harness.md`:

- add a small Debug Options section to the launcher graphics page for the
  merged-wall tooling that already exists in native code
- gate the in-game Video Info merged-wall controls behind a launcher-backed
  preference instead of showing them unconditionally
- persist and apply the surfaced legacy texmerge experiment and merged-wall
  logging preferences when the game process starts or resumes

Out of scope for this tranche:

- new native debug flags that do not already exist
- route overlays, supertransparency highlights, or new snapshot capture types
- the Part D crosshair capture work and the Part E graphics harness

## Required validation

1. `android\run-code-quality.ps1 -Fix`
2. `cd android; $env:JAVA_HOME='c:\local\jdk-21'; .\gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
3. `android\run_test.ps1 -ScriptName test_merged_wall_snapshot_regression.json5 -Game d2 -Install`

## Work items

- [x] Add launcher-backed merged-wall debug options to `GraphicsSettingsPage`
- [x] Gate the Video Info merged-wall controls behind the launcher preference
- [x] Apply the persisted logging and legacy-texmerge preferences on game create/resume
- [x] Re-run validation and update this file with findings

## Findings

- `GraphicsSettingsPage` now exposes a small launcher-backed Debug Options
  section for the already-landed merged-wall tooling instead of leaving those
  controls discoverable only from the in-game overlay.
- The launcher now persists three useful knobs without inventing new native
  debug flags: whether the Video Info overlay should show its merged-wall debug
  controls, whether the Graphics and Texture debug-log categories used by the
  merged-wall diagnostics should be enabled together, and whether the legacy CPU
  texmerge experiment should be forced on game launch or resume.
- `MainActivity` now re-syncs Kotlin debug-log category state and applies the
  persisted merged-wall experiment preference when the game process starts and
  when it resumes after returning from `SetupActivity`, so launcher changes take
  effect without manual native toggles.
- `VideoInfoOverlay` now hides the merged-wall debug row unless the launcher
  preference enables it, reducing always-on debug clutter in the normal Video
  Info view.

## Validation result

- `android\run-code-quality.ps1 -Fix`: passed
- `cd android; $env:JAVA_HOME='c:\local\jdk-21'; .\gradlew.bat :app:assembleDebug :app:testDebugUnitTest`: passed
- `android\run_test.ps1 -ScriptName test_merged_wall_snapshot_regression.json5 -Game d2 -Install`: passed with
  `ASSERT_PASS: "merged_wall_snapshot.covers" contains "rock331"`,
  `ASSERT_PASS: "merged_wall_snapshot.faces[0].route" == "old_texmerge"`, and
  `ASSERT_PASS: "merged_wall_snapshot.faces[0].decision_reason" == "tri13_face1_stock_64x64"`

## Status

- [x] In progress
- [x] Validation complete