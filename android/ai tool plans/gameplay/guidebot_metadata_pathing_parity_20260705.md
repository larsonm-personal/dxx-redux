# Guidebot Metadata Pathing Parity 2026-07-05

Goal: ensure recent pathing fixes are applied consistently to metadata generation and in-game guidebot routing.

Plan:
- [done] Compare metadata route planning and guidebot runtime route planning entry points
- [done] Identify where priority maintenance and trigger/shootable path logic lives
- [done] Move shared behavior into common pathing code or mirror it with tests if sharing is impractical
- [done] Validate metadata scan tests and relevant host/game builds

Notes:
- Metadata generation already calls the shared `level_metadata_scan_level` route builder from JNI and the host headless analyzer.
- In-game D2 guidebot routing consumes `level_metadata_get_state()->route_steps`, so ordered-key rollback and trigger/shootable route steps remain shared.
- Added a metadata-only runtime refresh before Android guidebot default goal selection so live key, wall, and trigger state is reflected when choosing the next route objective.
- Added scanner support for the live `WALL_DOOR_OPENED` flag so a mid-level refresh treats already-opened trigger doors as passable.

Validation:
- `.\android\run-code-quality.ps1 -Fix -Paths @('d2\main\escort.c','d2\main\secretarea.h','d1\main\secretarea.h','android\app\src\main\cpp\shared\secret_area_game_adapter.c','android\app\src\main\cpp\shared\level_metadata_scan.c','android\app\src\main\cpp\shared\level_metadata_scan.h','android\tests\test_level_metadata_scan.c','android\ai tool plans\gameplay\guidebot_metadata_pathing_parity_20260705.md')`
- `cmd.exe /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x86 && "C:\local\android-sdk\cmake\3.31.6\bin\cmake.exe" --build buildd1 --target test_level_metadata_scan --config Debug && "C:\local\android-sdk\cmake\3.31.6\bin\cmake.exe" --build buildd2 --target test_level_metadata_scan --config Debug'`
- `.\buildd1\maths\test_level_metadata_scan.exe`
- `.\buildd2\maths\test_level_metadata_scan.exe`
- `.\run-windows-build.ps1 -Target d2`
- `.\run-windows-build.ps1 -Target d1`
