# Weapon Autoselect D-pad Reorder Plan - 2026-06-02

## Goal
Fix controller navigation on the weapon autoselect editor so focus is ready immediately, vertical list navigation repeats with the existing shared rate, and grabbed items cannot move focus sideways into the other list.

## Steps
- [x] Read project instructions and inspect the autoselect editor plus shared D-pad helpers
- [x] Extend the shared vertical D-pad repeat helper to allow custom repeated movement
- [x] Update the autoselect editor to seed focus reliably, use the shared repeat helper for row focus and grabbed-item movement, and consume horizontal D-pad while grabbed
- [x] Add focused unit coverage for grabbed-item movement boundaries
- [x] Run Android Kotlin tests or the narrowest available verification

## Verification
- [x] `.\gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.AutoselectReorderScrollTest`
- [x] `.\android\run-code-quality.ps1 -Fix`
