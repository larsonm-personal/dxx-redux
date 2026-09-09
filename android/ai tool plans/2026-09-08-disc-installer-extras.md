# CD and installer extras

## Intent

Import recognized D1/D2 base data through the existing base-data handling and own the remaining playable content from one CD or installer as one Levels/Mods entry, with CD audio handled separately

## Work

1. Trace native extraction, base-file hoisting, dependency retention, and installer publication
2. Preserve mission directory relationships and consolidate CD/installer extras through shared publication
3. Add integration coverage for bundled HOG and loose-level missions, dependencies, base separation, audio separation, and repeat imports
4. Run scoped quality checks, relevant builds/tests, and Anniversary media validation

## Status

- CD and installer publication now share one extras owner, with base files and CD audio outside it
- Hoisting is restricted to recognized base filenames and GOG audio; custom missions retain source directories
- ISO and installer extraction retain loose levels and mission companions; Inno and Mac package extraction preserve nested paths
- Android D1/D2 mission loading mounts the selected descriptor directory for loose levels and assets, then unmounts it when the mission closes
- Host integration suite: 51 tests passed, including native Anniversary extraction with 287 descriptors, all four loose newlevel missions, and reimport state
- Native extraction suites: extension policy, CUE/ISO, GOG fd, and Mac package tests passed
- Real media extraction: Anniversary ISO, D2 Windows GOG installer, and D1 Mac package passed
- x86_64 debug APK built successfully for both games
- Device integration passed all 28 steps: native Anniversary import produced one owner, D1 was ready, Mad Decorator loaded from `missions/newlevel`, and the test restored the default set and deleted its temporary set
- Scoped mixed-language formatting/lint and final whitespace checks passed

## Validation entry points

- `android/tests/test_disc_content_import.ps1 -AnniversaryExtracted temp/disc-extras-anniversary`
- Add `-Device -Serial emulator-5554` to run the native Anniversary import and launch test against the installed APK
- Native fixture creation: `android/tests/build/RelWithDebInfo/extract_cd.exe "game_data/CD images/Descent Anniversary (ISO)/descent_anniversary.iso" temp/disc-extras-anniversary`

## Boundaries

- This extends supported CD/installer extraction; it does not add new archive/installer formats or recursively unpack arbitrary nested ZIPs
- Directory structure preserves same-named missions and assets from different source folders; duplicate mission names on compilation discs are retained
- `android/outstanding_bugs.md` explicitly prohibits agent edits and is unchanged
