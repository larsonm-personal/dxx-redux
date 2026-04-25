# Check Updates CMake SDK Managed

## Goal
Detect the Android SDK bundled CMake version in android/get_deps/check-updates.ps1,
show that it is SDK-managed, and stop offering it as a separate update.

## Status
- [x] Update CMake installed-version detection to read from the Android SDK layout
- [x] Mark the CMake row as SDK-managed so no standalone update is offered
- [x] Re-run safe no-prompt validation and confirm the CMake row behavior

## Notes
- CMake is detected from dependency_base\android-sdk\cmake\<version> and displayed as sdk-managed instead of receiving target or install update actions