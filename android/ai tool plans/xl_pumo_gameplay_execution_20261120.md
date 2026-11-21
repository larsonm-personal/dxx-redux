# Pumo gameplay tranche, November 20

## What the pilot reached

The Mineral Valley pilot flew from the hub through Majha to Metillis. From the earlier blue-key save the ship earned gold in room 948. It had three lives, 20.69 shields and score 35,360. Android loaded this desktop export, wrote its own save, then restored both keys in a fresh process. A developer-enabled Android build recorded 25 frames from the earned checkpoint. The ordinary desktop players matched the recording on both Windows and Linux. An extra Vertigo installation produced a fingerprint rejection on both hosts. The matching file set used retail D2, the XL resources and Pumo's own mission definitions. [Save diagnosis](../XL_compat/evidence/metillis-portable-gold-save-20261120.md) / [Replay evidence](../XL_compat/evidence/metillis-device-input-20261120.json)

The same pilot reached room 328 with 1.34 shields. On ordinary Android, enemy fire killed the ship, the explosion completed, and pressing Space respawned it in room 126 with two lives, 100 shields, 100 energy and both keys. Android saved the new ship and restored it in a fresh process. From this save the desktop pilot flew through room 1017, collected the red key in room 1257 and reached score 39,920 with 55.14 shields. The Windows and Linux player snapshots matched at both stops. The pilot used ordinary movement, combat and pickups throughout.

On Kartsal Motivation, hitting a heavy stationary robot had stopped a connected PMC1 run. The corrected collision calculation let the same ship continue through the encounter and activate the switches in rooms 326, 331 and 235. Switch 4 unlocked wall 93 on the opposite side of the door. Wall 92 on the pilot's side kept its authored lock, as in the original engine. Two exploratory shots from room 236 failed to operate the remote switches. Neither attempt changed the mission or shipped player controls.

## Interpreting the census

The starting ledger identifies the 130 advertised levels that missed the previous 120-frame startup test. It separates content missing from its distribution, cases where the original engine rejected the mission, unverified original behavior, and confirmed Redux differences. The ledger also explains why a successful startup leaves campaign objectives and multiplayer matches open. [Plan](xl_pumo_gameplay_tranche_20261120.md) / [Case ledger](../XL_compat/evidence/gameplay-deficits-20261120.json)

The reactor reader now handles a signed negative target count in Speed. Ordered POG replacement rows brought original Dogfight and QToxic entries through startup. Modern blue/red team room markers stay in the XL source and translate safely for ordinary singleplayer. Four additional advertised starts passed the Windows and Linux probes after the latter change. They are two copies of Monsterball Arena, the pinned WFC20 and the local WFC-D3. The remaining ten entries in that 14-case comparison retained other blockers. Speed now reaches its waypoint controllers and lightning effects. [Reactor](../XL_compat/evidence/speed-reactor-targets-20261120.md) / [Team room cohort](../XL_compat/evidence/team-room-starts-20261120.json)

## Fork integration

The planning, reactor reader, POG decoder and launcher import layers merged into the fork in PRs 689, 690, 692 and 693. The stationary-robot fix merged in PR 694. Dot's launcher settings, search and D-pad integration had already landed in PR 552. The nightly reporting and Gradle fixes had already landed in PRs 685 and 686.

PR 695 exposed a separate CI error on a retry. The first hosted Android LAN job failed Wi-Fi routing before launch. The second executed and passed the selected two-player owner. The final job reconciled the stale first artifact because both attempts uploaded the same artifact name. Local reconciliation against the hosted plan rejected artifact 14678290014 and accepted 14678223670. PR 697 passes the public Android producer's artifact ID through the reusable workflow to the gate. The checks for the exact target, expected test and terminal result still apply.

The fork's workflow deliberately skips private XL qualification below the native stack tip. In run 44836942738 the admission log recorded that decision for PR 695 after PR 694 merged. The top layer owns the cumulative private asset test. Label-only three-player tests, optional evidence exports, and scheduled-only lanes have distinct triggers.

## Earned reactor and Kartsal door

The Mineral Valley pilot replenished shields in rooms 990, 991 and 1006 and earned an extra life in room 997. From room 1202 it fired on reactor object 204 in room 1215. Reactor health fell from 400 to −10. The 90-second countdown started. The ship held all three keys, 43.62 shields, four lives and score 50,000. Windows and Linux matched at each stop. [Checkpoint identities](../XL_compat/evidence/metillis-earned-reactor-20261120.json)

On Kartsal the player can aim through the grate in room 236 and hit the unlocked side of wall 93 with an ordinary laser. After sprinting around the corridor the ship passed the thin doorway into room 1781. This put the next switch bank within reach. The earlier attempts to shoot switches 5 and 47 from the grate remain failed attempts. The door shot itself passed independently on Linux.

## Both Metillis exits and the next two mines

From the earned reactor checkpoint, the main pilot flew 39 portals back to exit trigger 10. Windows and Linux agreed on the resulting Iquaxel state in room 260 with four lives, 100 shields, 100 energy and score 51,150. The ordinary Android APK imported the same mission and restored it, wrote a new quicksave and passed seven cold-load steps. On Iquaxel an ordinary laser destroyed wall 36 from six units away. Flying across switch 3 in room 382 opened the path to the next area.

On the other branch the pilot picked up the cloak, passed the guards and traversed the four portals from room 1183 to room 1189. The secret exit entered Coldrom in room 228 with four lives and score 53,740. For Kartsal, moving sideways before firing let the pilot hit master switch 15 from room 357, operating child triggers 47 and 48. The resulting 538 reachable rooms include the corridor to the next switch. [Checkpoint and device identities](../XL_compat/evidence/pumo-earned-exits-20261120.json)

Hosted LAN run 44837498928 exercised PR 697 on a real failed-job retry. The first Android attempt failed before the game with artifact 14679349574. The second produced artifact 14680093267 under the same name. The gate selected that attempt and passed. [Hosted run, attempt 2](https://github.com/larsonm-personal/dxx-redux/actions/runs/44837498928/attempts/2)

[Next plan](xl_pumo_next_tranche_20261120.md) / [Replacement kickoff](xl_pumo_next_kickoff_20261120.txt) / [Human playtest directions](../XL_compat/PUMO_PLAYTEST.md)
