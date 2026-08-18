# Remove pre-release music compatibility

## Plan

- [x] Clarify the pre-release compatibility rule in copilot instructions
- [x] Audit the music/save changes for compatibility-only code
- [x] Remove prior Android save-metadata version support and its tests/docs
- [x] Run scoped formatting, builds, tests, and the save-source emulator regression

## Result

- Added an explicit instruction against compatibility code for pre-release Android save, config, and data formats
- Removed the minimum supported metadata version, range checks, legacy music interpretation branch, compatibility test, and compatibility documentation
- Retained only the current version 6 metadata path

## Validation

- Scoped code quality passed
- Android focused unit tests, all native ABIs, and debug APK assembly passed
- D1 Windows build and 33/33 CTest tests passed
- D2 Windows build and 40/40 CTest tests passed
- Audio lifecycle regression passed 9/9
- Save-source emulator regression passed 28/28
