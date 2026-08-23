# Quick save and load in multiplayer

## Plan

- [x] Trace quick-save and quick-load input handling and multiplayer restrictions in D1 and D2
- [x] Implement synchronized host-only co-op quick save and load for both games
- [x] Add a persistence contract regression check for synchronized co-op routing
- [x] Run scoped formatting, Android and Windows builds, and relevant tests
- [x] Leave `android/outstanding_bugs.md` unchanged as required by its file header

## Verification

- Android `:app:assembleDebug`: passed for all configured ABIs
- Android arm64 native rebuild after formatting: passed
- Android `QuickSaveLoadActionTest`: passed
- Windows D1 and D2 build: passed
- D2 CTest suite: 43/43 passed (D1 has no registered CTest tests)
- Python state persistence contracts: 9/9 passed
