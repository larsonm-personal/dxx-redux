# KCXF2 Level 3 Shootable Pathing Plan

## Goal
Analyze KCXF2 level 3's pseudo-reactor and late shootable progression path, then improve metadata and in-game guidebot pathing so they agree as closely as possible.

## Tasks
- [x] Read project instructions and inspect current metadata/guidebot pathing code.
- [x] Regenerate or inspect KCXF2 level 3 metadata route and identify missing shootable steps.
- [x] Trace how the in-game guidebot chooses goals versus metadata route-step analysis.
- [x] Patch the shared route logic so pseudo-reactor and final shootable triggers are represented.
- [x] Validate with focused metadata tests and generated output, plus code quality checks.
- [x] Update this plan with findings and validation.

## Notes
- User reports KCXF2 level 3 has a shootable pseudo-reactor that reveals a switch below it, then progression to exit.
- Current guidebot route chooses a different path and omits the last couple shootables.
- Metadata and guidebot route logic should be kept as close as feasible.
- The failing route stopped at trigger 26 with a dependency loop on trigger 22 for side 616:0.
- Root cause: route passability treated a side as blocked unless the first listed opener for that side had fired. KCXF2 level 3 has multiple openers for the same side, so a later reachable opener should satisfy the side even when an earlier opener is self-dependent.
- Fix: route edge passability now checks all opener walls for both sides of an edge and accepts the edge if any opener trigger in the route has fired.
- Regenerated KCXF2 metadata now routes level 3 as trigger 26 -> trigger 2 -> trigger 22 -> trigger 21 -> exit and reports route_status ok.

## Validation
- `cmd.exe /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x86 && "C:\local\android-sdk\cmake\3.31.6\bin\cmake.exe" --build buildd2 --target test_level_metadata_scan --config Debug'`
- `.\buildd2\maths\test_level_metadata_scan.exe`
- `cmd.exe /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x86 && "C:\local\android-sdk\cmake\3.31.6\bin\cmake.exe" --build buildd2 --target dxx-redux-d2-headless-metadata --config Debug'`
- `.\buildd2\main\dxx-redux-d2-headless-metadata.exe -hogdir "game_data\CD images\Descent II (USA) (v1.1)\data_tracks\d2data" -extra-dir "temp\kcx_f2_rm" -mission KCXF2RM -secretarea-json-out "temp\kcxf2_level3_after_multi_opener_fix.json"`
- `$env:JAVA_HOME='C:\local\jdk-21'; .\gradlew.bat assembleDebug --no-daemon`
- `.\android\helpers\run_mission_zip_batch.ps1 -Pattern KCXF2RMv11.7z -MetadataOnly -Install -OutDir "android\temp\mission_zip_batch\kcxf2_level3_route_fix" -TimeoutSeconds 900`
- `.\android\run-code-quality.ps1 -Fix -Paths @('android\app\src\main\cpp\shared\level_metadata_scan.c','android\tests\test_level_metadata_scan.c','game_data\mission_files\KCXF2RMv11.json','android\ai tool plans\gameplay\kcxf2_level3_shootable_pathing_20260705.md')`
- `cmd.exe /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x86 && "C:\local\android-sdk\cmake\3.31.6\bin\cmake.exe" --build buildd2 --target test_level_metadata_scan --config Debug && buildd2\maths\test_level_metadata_scan.exe'`
