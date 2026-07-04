# Indicator Line Weight and Coop Warp Availability Plan

## Goal
Make the guidebot/player indicator lines more readable and show the coop "Warp to" action in more valid moments.

## Steps
- [x] Locate the Android indicator-line rendering and coop warp availability logic
- [x] Increase guidebot/player line visual weight without changing unrelated overlay behavior
- [x] Relax the native coop warp availability gate while preserving coop/QoL/safety checks
- [x] Run scoped formatting and relevant Android build/tests

## Notes
- Keep D1/D2 native changes mirrored where coop warp code is shared
- Prefer Android-scoped changes and avoid changing desktop behavior
