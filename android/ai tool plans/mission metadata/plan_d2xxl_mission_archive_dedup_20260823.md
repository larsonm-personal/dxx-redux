# D2X-XL mission archive analysis continuation

Goal: extend the first-pass sort in `C:\Users\first last\Desktop\d2x-xl levels` by checking archive importability and distinguishing genuinely different missions from metadata-only or packaging-only variants.

- [x] Recover and summarize the first-pass manifest, notes, and sorted archive sets.
- [x] Inventory `.zip` and `.7z` layouts and compare them with the launcher's supported mission-import structure.
- [x] Compare likely near-duplicates by normalized archive contents, HOG member inventories, hashes, and mission metadata, starting with Bahagad.
- [x] Record actionable conclusions: importable as-is, repack needed, genuinely different, duplicate, or unresolved.
- [x] Mark this plan with completed work and report any remaining verification or implementation work.

Result: see `d2xxl_mission_archive_analysis_20260823.md`. Remaining work is to
choose the retained variants, repack them without D2X-XL cache data, convert any
desired extended `D2X` HOGs, and run the final selected corpus through the
Android emulator import batch.
