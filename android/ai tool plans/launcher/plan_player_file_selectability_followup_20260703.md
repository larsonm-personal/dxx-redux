# Player File Selectability Follow-up Plan

## Goal
Make the D1/D2 in-game pilot list and launcher multiplayer callsign list hide Android coop/internal player files consistently.

## Steps
- [x] Inspect `plr_is_selectable()` in D1 and D2 and compare it with coop/autosave file creation
- [x] Fix D1/D2 filtering so sentinel or internal player files are excluded from the in-game list
- [x] Keep launcher callsign scanning aligned with the native rules where practical
- [x] Add or update focused tests if the helper logic can be isolated
- [x] Run scoped formatting and relevant build/tests

## Notes
- Keep player-file format logic in the C `playsave.c` files
- Mirror D1 and D2 changes because pilot selection code is duplicated
- Launcher already excludes `coopsave.plr`; native now also rejects the sentinel and malformed names before binary validation
- Reused the launcher scanner test for the Kotlin side; the new native filename gate is coupled to PhysFS/player-file validation and was verified through the Android debug build
