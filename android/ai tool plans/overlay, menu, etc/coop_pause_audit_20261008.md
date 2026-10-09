# Co-op pause and menu audit

1. Review derived pause reasons, UI handoffs, control blocking, native menu event dispatch, co-op save/restore barriers, and session changes in both engines
2. Exercise host and client menus, quick-load confirmation/cancel, quick-save/restore, held controls, and continued network/game time in an isolated two-player session
3. Diagnose any failures with targeted logs and introspection before changing behavior; add reusable integration coverage for confirmed gaps
4. Build both engines, run scoped code quality and relevant tests, and document verified behavior and remaining limits

Initial review: ordinary Android/native menus are excluded from multiplayer simulation pause; input remains blocked. Co-op quick save/load is host-only and uses multi_send_save_game / multi_send_restore_game, including the restore transfer barrier. Check whether Controls is cleared when input becomes blocked and whether native menu drawing still permits network progress

## Confirmed control regression

Two-player D2 baseline (`android/temp/coop-pause-20261008-121101`) held Fire Primary, then opened the actual Kotlin quick-load dialog. Game time and remote packet sequence advanced, but `fire_primary_state` remained 4. ReadControls now ignores releases while input is blocked; android_pause_publish previously cleared Controls only when input resumed

The shared pause publisher now clears Controls on entry to a blocked state. It does not flush SDL events on entry, so pending native-menu navigation remains available. Resume retains the existing full input flush. This applies to both engines and all derived input blockers without changing multiplayer pause rules

Added `test_coop_pause.ps1` / `test_lan.ps1 -PauseMenus` for host/client dialogs, held fire, admin-tray handoffs, native Game/Options/Save/Load menus, advancing game time/network traffic, host quick save/restore, guest rejection, and restoring with the guest in a native menu

## Admin-tray handoff

Logs from the menu test showed native requests still queued with `modals=1`; the tray cleared that modal after its 200 ms close animation. Code review found the same revision invalidation risk as the quick-load dialog if engine dispatch is delayed. Native-action handoffs now close the tray synchronously; ordinary tray dismissal retains its animation

## Fixture corrections

- Fire Primary uses state bit 4, so assert nonzero rather than exactly 1
- Save Game begins by editing the description; cancel the edit before dismissing the menu
- `player_energy` changes only local state. Broadcast inventory with the existing recovery fixture and wait for host receipt before saving
- A guest's native menu remains open across an in-place host restore. Verify it remains live and navigable, then dismiss it explicitly
- Normal combat remains active in menus. Clear robots and separate the ships in the navigation fixture; a first D1 run lost its guest to combat

## Validation

- Final D1 co-op suite passed: `android/temp/coop-pause-20261008-122713`
- Final D2 co-op suite passed: `android/temp/coop-pause-20261008-122931`
- Final D1-in-D2 co-op suite passed: `android/temp/coop-pause-20261008-123524`
- Final D2 single-player pause integration passed, including the 86-step save/load fixture, actual confirmation, menu touch navigation, overlapping modals, graphics challenge, recovery, and Activity recreation: `android/temp/pause-state-20261008-123708`
- D1-in-D2 co-op graphics accept/cancel/timeout passed on both peers: `android/temp/graphics-multiplayer-20261008-123141`. The following save test correctly refused a dead guest, so the pause suite was rerun from a fresh session separately
- Final x86_64 and ARM64 CMake/APK builds passed without new compiler warnings
- QuickSaveLoadActionTest and OverlayVisibilityPolicyTest: 18 tests passed
- Scoped mixed-language code quality and both automation/master-runner catalog checks passed
- Final ARM64 build installed on the Retroid. Confirming the actual quick-load dialog restored the user's D1 demo in D2 from live difficulty 3 to saved difficulty 2, advanced the session, and acknowledged the request
- Retroid quick save remains byte-for-byte unchanged (SHA-256 `0c4c282651a649a1cdc8d10bc6cd3406608f5211c373d3a256ebdbcd0a54ae08`). Restored game left paused with the admin tray open; final snapshot is `android/temp/coop-pause-audit/retroid-paused.json`
