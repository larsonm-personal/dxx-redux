# Host Metadata Regeneration Repair 2026-07-05

Goal: make `regenerate_all_mission_metadata_host.ps1` stage mission archives the same way the game expects so the headless metadata analyzer can resolve mission levels.

Plan:
- [done] Trace host helper staging, mission descriptor lookup, and headless runtime mount behavior
- [done] Fix archive staging so descriptors and their mission HOG/data files remain discoverable by the engine
- [done] Improve failure reporting so missing-level warnings do not hide the root command/setup context
- [done] Validate with focused failing ZIPs

Notes:
- The host helper copied `.msn/.mn2` descriptors into `missions/`, but left sibling `.hog` files at the temp stage root.
- Both D1 and D2 mission loaders mount add-on HOGs from `missions/<mission>.hog` when the descriptor is found under `missions/`.
- Missing mission HOG mounts caused normal levels inside those HOGs to appear missing, making nearly every add-on fail.

Validation:
- Patched staging function creates `missions/VESTA.HOG` and `missions/vesta.hog` for `Vesta.zip`.
- `.\buildd1\main\dxx-redux-d1-headless-metadata.exe -hogdir game_data\extracted\d1 mac extracted -extra-dir <patched Vesta stage> -mission VESTA -secretarea-json-out <temp json>` exited 0 with `SECRET-AREA-DUMP OK`.
- Patched staging function creates `missions/vignett2.hog` for `vignett2.zip`.
- `.\buildd2\main\dxx-redux-d2-headless-metadata.exe -hogdir game_data_to_copy_to_emulator\temp -extra-dir <patched vignett2 stage> -mission vignett2 -secretarea-json-out <temp json>` exited 0 with `SECRET-AREA-DUMP OK`.
- `.\android\run-code-quality.ps1 -Fix -Paths @('android\helpers\regenerate_all_mission_metadata_host.ps1','android\ai tool plans\testing\host_metadata_regeneration_repair_20260705.md')`
