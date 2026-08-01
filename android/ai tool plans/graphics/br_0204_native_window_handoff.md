# BR-0204 native window handoff

## Goal

Retain each Android native window for the full EGL handoff and synchronize surface lifecycle state across UI and render threads.

## Plan

- [x] Trace native-window publication, borrowing, EGL creation, recreation, and pause state
- [x] Define a reference-counted generation snapshot API under one synchronization contract
- [x] Update EGL consumers to release snapshots after surface creation or failure
- [x] Add focused lifecycle and ownership regression coverage
- [x] Run scoped formatting, Android build, and relevant emulator lifecycle validation
- [x] Archive BR-0204 with dated resolution evidence per the adversarial review process

## Verification

- Scoped Android C, C++, PowerShell, and JSON5 code quality passed.
- `:app:assembleDebug` passed for arm64-v8a, armeabi-v7a, and x86_64.
- `test_launch_to_automap.json5` passed for D1 and D2 with two consecutive background/resume cycles and two generation-bound EGL recreations in each game.
- Scoped `git diff --check` passed.
