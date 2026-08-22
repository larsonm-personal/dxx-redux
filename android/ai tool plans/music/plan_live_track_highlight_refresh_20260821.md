# Live track highlight refresh plan

Goal: keep the open in-game track viewer synchronized when playback automatically advances to another track.

- [x] Trace native automatic track advancement and panel refresh triggers
- [x] Add diagnostic logging for viewer-visible track changes
- [x] Add focused regression coverage for the refresh lifecycle
- [x] Implement a lightweight live refresh while the panel is attached
- [x] Run scoped code quality, focused tests, and Android CMake/build verification
- [x] Record validation results and mark the plan complete

Validation:

- `MusicOverlayPollingTest` and `MusicOverlaySourcesTest`: passed
- Scoped `run-code-quality.ps1 -Fix`: passed
- `:app:assembleDebug`: passed with CMake builds for arm64-v8a, armeabi-v7a, and x86_64
- `git diff --check`: passed
