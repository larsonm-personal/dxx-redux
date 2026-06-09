# Missing reactor travel note

## Goal
- Allow level metadata travel calculation to complete when a level has no reactor.
- Route from the last required/visited target directly to the nearest exit in those levels.
- Show `note: missing reactor` in metadata rows without presenting it as a warning.

## Plan
- [x] Inspect native metadata status/problem fields and launcher display mapping.
- [x] Patch the scanner to treat missing reactor as a note and continue to exit.
- [x] Patch table display so missing-reactor notes are quiet notes, not warnings.
- [x] Verify Obsidian, base D1/D2 metadata, formatting, and Android compile checks.

## Notes
- Obsidian improved from 3 of 18 non-ok travel results to 0 of 18.
- Magma V Outpost, Lapis Lazuli, and Syenite now report `travel_status=ok`, empty `travel_problem`, and `travel_note=missing reactor`.
- D1 and D2 base game headless dumps stayed at 30 of 30 travel-ok with no missing-reactor notes.
- Scoped code quality passed. Android `:app:compileDebugKotlin :app:externalNativeBuildDebug` passed.
