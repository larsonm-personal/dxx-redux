# Guidebot Coop Resume Desync Fix - 2026-05-26

## Goal
- Apply likely fixes for guidebot desync after cooperative save restore

## Plan
- [x] Read the nearby multiplayer robot ownership interfaces
- [x] Fix coop metadata lookup when Android metadata is appended after it
- [x] Restore companion robot ownership and local control slot after loading
- [x] Run focused checks or build validation if practical

## Validation
- Ran `git diff --check`
- Ran `.\run-windows-build.ps1 -Target d2`
- Ran `.\gradlew.bat :app:assembleDebug` from `android` with JDK 21
