# Coop texture desync logging

## Goal
Add focused Android coop desync tick logging that can explain why textures are wrong on fresh coop starts.

## Plan
- [x] Find the existing coop desync tick logging path
- [x] Identify texture state that is likely to diverge during fresh starts
- [x] Add concise diagnostics guarded by the same coop desync logging condition
- [x] Run scoped formatting and available validation
- [x] Update this plan with outcomes

## Outcome
- Added `texdiag[...]` lines to the coop diagnostic tick in `coop_indicator_lines.c`
- Moved the diagnostic tick before the indicator-line early return so it logs even when no coop indicator line is drawn
- Armed the same diagnostic tick after fresh coop `StartNewLevel` completion in both D1 and D2 host/join paths
- Texture diagnostics now include texture-table, segment-texture, and player alternate-texture signatures plus GL texture counts and first invalid tmap sample

## Validation
- `android\run-code-quality.ps1 -Fix -Paths ...`: passed for the scoped Android C file
- `android\gradlew.bat :app:assembleDebug`: passed
