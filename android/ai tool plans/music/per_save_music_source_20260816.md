# Per-save music source

## Goal

Keep each save's exact music source authoritative while using bundled music by default for newly opened soundtrack-bearing missions and pilot music only as a fallback.

## Plan

- [x] Trace save, resume, new-mission, and in-game music application paths
- [x] Store and restore the exact save source with the smallest metadata extension
- [x] Keep new soundtrack-bearing missions defaulting to bundled music while making saves authoritative
- [x] Add focused metadata, structural, and device regression coverage
- [x] Run scoped formatting, Android tests/builds, D1/D2 builds/tests, and emulator music checks

## Constraints

- Preserve unrelated user changes
- Keep D1 and D2 hooks symmetric
- Keep the obsolete blanket preference UI removed
- Do not add launcher-side parsing of pilot file formats

## Result

- Android save metadata version 6 reserves music value 4 for mission-bundled MIDI while retaining the existing trailer layout
- Loading a save restores the engine music type and mission flag together, then restarts playback through the same path used by the picker
- The pilot remains the default for unsaved content, and importing or re-enabling a soundtrack-bearing mission still selects its bundled music by default

## Validation

- Scoped code quality passed
- Android music policy unit test, audio lifecycle regression, all native ABIs, and debug APK assembly passed
- Full Android unit suite passed 841/842; the existing SevenZip native initialization environment failure remained in `ModManagerMissionZipTest.missionRarImportsReetusAndStagesAtMissions`
- D1 Windows build and 33/33 CTest tests passed
- D2 Windows build and 40/40 CTest tests passed
- `test_music_save_source_restore_d2.json5` passed 28/28 steps on the emulator
