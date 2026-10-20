# Mac editions and replacement imports

User scope: recognize mixed default assets and offer replacement, support D1
MacPlay in D2, and use the Mac D2 disc as a D1-in-D2 Guide-Bot source

## Fixtures

- D2 Mac: game_data/CD images/d2 mac/Descent_II.bin and its CUE
- D2 PC: game_data/gog installers/setup_descent_2_1.1_(16596).exe
- D1 Mac: game_data/CD images/Descent - Mac macplay
- Preserve the user's edits to android/outstanding_bugs.md

## Work

- [x] Reproduce Mac Guide-Bot preparation with isolated real fixtures and diagnostics
- [x] Implement Mac optional resource decoding and verify models, textures and audio
- [x] Implement D1 Mac resource preparation, including sound bank and edition admission
- [x] Verify D1 Mac level launch, gameplay, presentation and sounds in D2
- [x] Trace demo, Mac CD and GOG import publication and filename precedence
- [x] Detect incompatible base-game assets and offer transactional replacement or a new set
- [x] Cover existing mixed sets and preserve unrelated missions, saves and settings
- [x] Add and run integration coverage using the actual media sequence
- [x] Run scoped code quality, relevant native/JVM builds and Android checks

## Initial evidence

Imports currently merge paths. D1 Mac editions are explicitly rejected by the
native admission policy. The optional D2 bitmap reader does not enable Mac
palette-index conversion. The Mac D2 fixture has HAM version 3 and PC-identical
sound banks; its precise Guide-Bot failure still needs reproduction

Run host asset tests in an explicit scratch working directory. Supplying CLI
arguments disables the test executable's automatic fixture-directory isolation

## Findings and implementation

- Actual Mac PIG filenames are lowercase; GOG PIG filenames are uppercase.
  Android can retain both, letting the Mac file mask the imported PC file.
  Publication now replaces all case variants of base filenames transactionally
- The launcher audits case conflicts, demo/retail coexistence, and known HOG/PIG
  edition conflicts. It offers an inventory for confirmation before clearing
  only that game's base files and links. Original linked files are retained
- Version labels are byte revisions, not full distribution inventories. Mac D2
  shares PC-labeled HAM and sounds, so those labels cannot establish a conflict
- Retail Mac D1 needs implicit high-resolution cockpit dimensions and its
  descent.rsrc sound bank. Both are now supported in D1-in-D2; resource bytes
  also participate in save compatibility identity. Mac shareware stays unsupported
- Isolated Mac D2 Guide-Bot preparation succeeds with the supplied media.
  Its optional bitmap reader needed Mac 0/255 palette conversion; diagnostic
  logs now identify the exact optional source files selected at runtime

## Validation

- Focused JVM import/audit tests passed, including actual Mac and GOG fixtures
- Windows D1 and D2 upstream compatibility CTests passed
- Native real-media source and save tests passed, including all 98 Mac samples,
  cockpit dimensions, changed/missing optional sources and resource identity
- Android debug build passed for supported architectures
- Scoped mixed-language quality checks and both automation catalog checks passed
- Full emulator integration passed with the original CDs and specified GOG EXE:
  demo/Mac detection, D1-in-D2 level 1, deployed Mac Guide-Bot, audible effects,
  GOG case replacement, deliberate legacy conflict cleanup, and clean reimport
- Runtime introspection reports all 3 cockpit frames, 213 object bitmaps and
  98 sounds applied; one released Guide-Bot is present
- Cleanup preserved sentinel pilot/save/mission/music files and the D1 PIG.
  The final clean GOG audit reports no conflict reasons
- Evidence: temp/mac-edition-support/device/result.json and phase-specific
  audit/result/log files. The runner restores the previously active file set
- APK: android/app/build/outputs/apk/debug/app-debug.apk includes arm64-v8a,
  armeabi-v7a and x86_64. Runtime testing used emulator-5554, not the RP4 Pro

The gameplay harness needed string-valued wait predicates and optional existing
pilot selection. The shared test runner now honors PreserveState for launcher
save cleanup so this integration can actually verify that saves survive
