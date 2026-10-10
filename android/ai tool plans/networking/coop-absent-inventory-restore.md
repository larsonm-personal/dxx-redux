# Persistent absent-player inventory and restore crash

- [x] Trace the supplied crash and exercise dropped-player lobby restore using paired emulators
- [x] Initialize unmatched restored ships before enabling physics in shared D1/D2 code
- [x] Recover saved absent inventories after coordinated restore, using the same host-authoritative path as late join
- [x] Retain ten recent absent identities across saves without changing the existing disk layout
- [x] Exercise disconnect, host exit autosave, process restart, lobby restore, repeated save/load and late rejoin
- [x] Run scoped formatting, native builds and automation catalog checks

Evidence: build 24540 client aborted in d2/main/physics.c:398 at
2026-10-08 23:49:23.267 because PF_USES_THRUST was set with zero drag.
The client was absent in the host save. The shared remapper skips that ship,
then state.c enables flight without initializing its mass/drag. Saved absent
records are loaded but only late-join object sync calls inventory recovery.

Keep v13's sixteen on-disk record spaces for existing saves; limit the live
remembered table to ten identities, newest departure last. No time expiry.
Use the existing recovery ledger to avoid duplicating dropped or claimed gear.
Delay recovery until restore is complete so a client's world load cannot erase
the authoritative inventory packet. Preserve current identity matching.

The cache refreshes departure order for an existing identity, retains an old
record if a return is interrupted during restore, and prefers an active saved
record over any stale absent copy. Saving after changing mines strips obsolete
key flags from absent records. Remembered inventory delivery works without the
general QoL switch; recovery revisions still reject repeated deliveries.

Validation:

- Android debug x86_64 APK and ARM64 D1/D2 native libraries build successfully
- D1 and D2 CTest recovery, player-session, and save-format tests pass (six total)
- Recovery tests include a saved inventory grant with QoL disabled and rejection
  of the repeated grant
- Scoped code quality and both automation catalog checks pass
- Final paired D1-in-D2 level 5 regression passed all three complete cycles,
  including exact inventory and movement checks after each return
  (`temp/ghost-fixed-d2-clean.log`, exit 0)
- First full D1-in-D2 integration run recovered exact inventory on both cold
  lobby restores and the late join; the final host movement probe failed after
  robots killed the idle host. The fixture now clears robots before saving
- An exploratory old-build run exhausted its 90-second launch/restore timeout;
  it did not establish a reproduced assertion. Cold-return coverage allows 180
  seconds for mission preparation plus restore on these emulators

Regression command:

```powershell
pwsh android/tests/test_lan.ps1 -Game d2 -MissionFile descent -InitialLevel 5 `
    -GhostInventory -HostDevice emulator-5582 -JoinDevice emulator-5586 -SkipBuild
```

The scenario seeds plasma, six homing missiles, laser upgrades and quad lasers,
force-stops the client, waits for the host to detect the drop, uses the real
host exit autosave, restarts both processes, and verifies inventory and thrust.
It repeats two lobby restores before a third cold restore with an in-game join.

Follow-up: subtract gear collected after the owner leaves from persistent ghosts.
The ghost grant already reconciles the ownership ledger, but that ledger was
disabled with coop QoL off. Keep accounting active for every coop game so saved
ghost inventories cannot return collected gear, independently of the guidebot,
arrows and warp setting.

- [x] Remove the QoL dependency from shared ownership accounting
- [x] Add cold-restore coverage for collected weapons and partial missile pickup,
  including a delayed drop after the absent snapshot and both QoL settings
- [x] Extend the paired ghost regression to collect after disconnect, then check
  retained host ammo and reduced returning inventory across three save/rejoins
- [x] Run scoped formatting, D1/D2 native tests, Android build and paired regression

The first pickup run exposed that Abort Game never wrote a new coop autosave:
the runner restored the disconnect autosave from before collection. The native
log records collection at 07:51:13, but the newest history entry was 07:51:04.
Save on host leave before gear spew and network teardown, using coop_autosave's
existing transition guards. Require a new autosave slot in the paired test.

The next run collected a loose single missile automatically before the scripted
four-pack, correctly producing five on the host. Move the departing ship away
from spawn points and other ships before generating spew, using the existing
death fixture, so the four-plus-two assertion is deterministic.

Follow-up validation so far: six native D1/D2 recovery/session/save-format tests
pass, x86_64 APK and both ARM64 libraries build, and scoped formatting passes.
Rebuilding multi.c reports the existing D1 PlayerCfg.ObsChat array-address
warning at line 2349; the new exit hook introduces no compiler warnings.

The stricter exit assertion also caught Abort Game clearing Current_level_num
before multi_leave_game, correctly rejecting that late save. Route AUTO_ABORT
and AUTO_EXIT through coop_autosave in the shared save dispatcher, before the
single-player precheck and before the caller clears the mine. Retain the leave
hook for other shutdown paths. Cold client preparation also exhausted the
default initial-join timeout once; run with -TimeoutSeconds 240.

Final pickup regression passed all three complete cycles with QoL disabled,
including fresh exit saves, two lobby restores, an in-game return and movement
after each return (`temp/ghost-pickup-complete-d2.log`, exit 0, 08:42:17).
The host keeps four collected missiles and the returning player receives two;
neither world retains duplicate dropped objects. The flight fixture now moves
away from pickups and stops drift afterward. Weapon assertions require the
seeded weapons without rejecting legitimate extra pickups during play.

Six native recovery/session/save-format tests, ten persistence contract checks,
the x86_64 APK, both ARM64 engine builds, scoped formatting and both automation
catalog checks passed. Native recovery coverage also checks fully collected
weapons, delayed drop packets, and both QoL settings.

```powershell
pwsh android/tests/test_lan.ps1 -Game d2 -MissionFile descent -InitialLevel 5 `
    -GhostInventory -GhostInventoryPickup -NoCoopQol -TimeoutSeconds 240 `
    -HostDevice emulator-5582 -JoinDevice emulator-5586 -SkipBuild
```
