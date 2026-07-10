# KCXF2 guidebot hidden wall route

## Goal
Fix the guidebot route selection after KCXF2 red key so the metadata hidden wall step is used before the boss instead of reporting the boss as unreachable.

## Plan
- [x] Inspect metadata route generation for hidden wall steps and KCXF2 output
- [x] Inspect guidebot route step satisfaction and next-goal selection
- [x] Patch the smallest runtime or metadata logic issue that skips the hidden wall step
- [x] Add or update a focused regression check
- [x] Run the focused test or the closest host-side validation available

## Result
- Fixed hidden wall route-step satisfaction so closed or opening hidden doors are not treated as passable just because the player can open ordinary doors.
- Renamed the metadata/browser route label to "Shoot hidden wall".
- Renamed the metadata browser activation title to "Shoot hidden wall" for existing generated rows.
- Updated the guidebot instruction for this route step to "shoot this hidden wall".
- Added `test_kcxf2_guidebot_hidden_door_next.json5`.
- Verified with Android native build, APK build, and focused emulator automation.

## Text refinement
- [x] Rename hidden wall route labels to `Shoot hidden wall`
- [x] Shorten guidebot route instruction to `shoot this hidden wall`
- [x] Update metadata browser activation copy to `Shoot hidden wall`
- [x] Update the focused KCXF2 hidden-wall regression expectations
- [x] Run scoped validation
