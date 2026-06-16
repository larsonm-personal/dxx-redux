# Net Failure Log Analysis - 2026-06-15

## Context
- User provided two exported debug logs in `game_data/logs_net_failure`.
- Sequence: player B hosted and A joined successfully, then player A hosted and B joined with an error dialog and corrupted textures on B.
- User reports no visible behavior or graphics changes after the previous attempted fixes.

## Plan
- [x] Inventory the two log files and identify which belongs to A or B
- [x] Extract session boundaries, launch direction, dump reasons, and level texture signatures
- [x] Compare the successful and failing netgames for level, mission, palette, PIG, and texture signature differences
- [x] Summarize likely causes and next instrumentation or fix candidates without changing behavior
- [x] Patch Android coop autosave restore path/authority mismatch

## Findings
- `debuglog_20260615_105831.txt` is Player32. `debuglog_20260615_220821.txt` is Player68.
- Working run: Player68 hosted level 2, Player32 joined level 2. Both devices had matching level texture signatures through normal level load and multiplayer prep.
- Failed run: Player32 hosted level 2 and armed restore slot 7 from the Android save-set autosave. Host then tried legacy `Players/coopsave.mg7`, did not find it, and skipped restore.
- In the same failed run, Player68 joined and applied its own local legacy `Players/coopsave.mg7`. Its `Textures[]` signature changed from the normal `072fc81c` post-prep value to `940020e2` after restore, while segment texture IDs stayed `00201614`.
- That explains the client-only corrupted visuals better than a level file or palette choice: the peer restored a different/stale local bitmap/effect state while the host did not restore the authoritative save.
- Fix direction applied: Android coop autosave restore now mirrors the save-set path used while arming, and non-host peers skip autosave-slot restore packets instead of loading local autosaves.
