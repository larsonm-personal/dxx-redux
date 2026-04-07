# Plan: Guidebot in Multiplayer Coop

## Phases

### Phase 1: C-side core (escort.c, multibot.c, multi.c, collide.c)
- [x] Add `Escort_owner_player` global + init/reset in escort.c
- [x] Add `MULTI_ESCORT_OWNER` packet type in multi.h, send/receive in multi.c
- [x] Change 3 guard clauses in escort.c to allow coop + owner-only
- [x] Lock companion ownership in multibot.c (multi_can_move_robot, multi_do_claim_robot)
- [x] Extend companion invulnerability to all coop levels in collide.c
- [x] Add companion-flare guard in collide_player_and_weapon()
- [x] Add owner adoption in multi_make_player_ghost / disconnect path
- [x] Add ownership trigger: detect when Buddy_allowed_to_talk transitions 0->1
- [x] Non-owner menu message in do_escort_menu()
- [x] Guard do_escort_frame in ai.c to only run for owner in coop

### Phase 2: Android overlay + touch
- [x] JNI: nativeGetEscortOwnerPlayer(), nativeIsEscortOwner() (with D1/D2 guards)
- [x] CoopStatsOverlay.kt: guidebot owner indicator line
- [x] TouchOverlayView.kt: hide Guide radial for non-owners in coop
- [x] Wire providers in MainActivity.kt

### Phase 3: Build + test
- [x] Android build (assembleDebug) -- passes with 0 errors
- [x] Code quality linters (clang-format, ktlint) -- clean on modified files
- [x] Integration test (test_launch_to_automap) -- PASS, 37/36 steps

## Status
- All phases: COMPLETE
