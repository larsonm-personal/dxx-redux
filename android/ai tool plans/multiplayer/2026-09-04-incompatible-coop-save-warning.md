# Reject incompatible co-op saves before hosting

- [x] Trace save selection, metadata reader, and LAN/online host launch paths
- [x] Expose native compatibility validation and retain visible saves with warnings
- [x] Block incompatible selected saves at launch and reject restores before level loading
- [x] Add focused regression coverage, run scoped quality, Android and Windows builds/tests
- [x] Record results and limitations

## Results

- Save selection retains incompatible entries, displays the warning, and disables Host for the selected incompatible save
- LAN and online lobby start paths revalidate the selected file, including service entry points used by automation
- D1 and D2 reject incompatible metadata before loading a level; failed restores no longer report successful completion
- Shared native format probe checks the current metadata version, footer, payload header, bounds, and checksum; the existing restore reader remains responsible for game-specific payload semantics
- No old-format conversion or migration added

## Validation

- Android debug APK and internal AAB built successfully
- JVM tests: 996 passed; an initial music-cache publication failure passed on retry without changes to that code
- Windows D1 and D2 builds passed; native tests: 46 passed, including the co-op metadata format test
- Scoped mixed-language formatting and lint passed
- API 36 emulator integration test passed on the final debug APK: incompatible save retained, host start blocked with warning, Start fresh clears the restriction
- Emulator UI hierarchy confirmed the save row remains selectable, the warning is visible, and the Host control is disabled
- Test fixtures removed/restored after testing; no APK installed on the physical phone and no internal bundle uploaded

Reusable emulator test: `android/tests/test_coop_save_compatibility.ps1 -Serial emulator-5554`
