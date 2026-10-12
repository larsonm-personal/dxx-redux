# Secret Area Baseline

The runner has two independent data profiles. Both execute the same native metadata
dump and compare the complete output, including secrets, route steps, level
statistics, music, and mission metadata.

- `retail` is the default and uses `secret_area_base_game_baseline.json`, covering
  all 30 levels in each full game
- `shareware` uses `secret_area_shareware_baseline.json`, covering all seven D1
  shareware levels and all three D2 demo levels

## Public shareware coverage

Prepare the pinned public PC packages and run the existing baseline test:

```powershell
pwsh -NoProfile -File android/helpers/setup_shareware_test_data.ps1 -Game both -Download
pwsh -NoProfile -File android/tests/test_secret_area_baseline.ps1 -DataEdition shareware -BuildBeforeRun -RequireAssets
```

The setup helper verifies the package and extracted file hashes from
`game_data/test_data_manifest.json` and `android/tests/fixtures/demo_import_oracles.json`.
The sources are D1 `desc14sw.exe` and D2 `d2demo10.zip`, also offered by the Android
launcher. It extracts them into `game_data/shareware/d1` and `game_data/shareware/d2`
without running the DOS installers. The runner itself is offline and accepts only
the pinned files, with no retail-data fallback.

The native build directories must already be configured. Alternatively pass
`-D1Exe` and `-D2Exe` for existing `dxx-redux-*-headless-metadata` binaries. Explicit
`-D1DataDir` and `-D2DataDir` overrides must match the same shareware hashes. Use
`-Game d1` or `-Game d2` to compare only that engine against the corresponding entry
in the two-game fixture.

## Regeneration

Generate the shareware fixture from both native binaries:

```powershell
pwsh -NoProfile -File android/tests/update_secret_area_baseline.ps1 -DataEdition shareware -RequireAssets
```

Generate the full retail baseline with legally supplied retail assets:

```powershell
.\android\tests\update_secret_area_baseline.ps1 -RequireAssets
```

Updates require both games to avoid replacing the shared fixture with a partial
campaign. Unchanged regeneration preserves the fixture bytes and modification time.
Shareware generation never replaces the retail fixture.

Review scanner algorithm changes before updating either file. The fixtures flag
changes in generated secret counts, ordering, entry walls, labels, and segment
membership as well as the other native metadata fields. No game assets are checked
in with these generated JSON fixtures.
