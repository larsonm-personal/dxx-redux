# Original PMC1 opening gates

The opening scenario fills a normal-input gap before the later switch/key and
reactor probes. It enters PMC1 through the normal hub laser/thrust route, then
uses fixed poses for three local switch/door interactions. Original mission
bytes, weapons, keys and objectives are unchanged.

| Switch                | Normal activation and resulting gate                                                          | Crossing                                                                    |
| --------------------- | --------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------- |
| 0, segment 331 side 5 | A laser shot unlocks walls 20/21; shooting the locked door first must fail                    | Room 240 approach, then fire and thrust beyond x=-103; door plane x=-108.93 |
| 1, segment 326 side 4 | A laser shot opens wall faces 4/5/6/7                                                         | Thrust from segment 253 into 262                                            |
| 4, segment 235 side 4 | A laser shot unlocks only authored target wall 93; its locked-door negative control must fail | Room 339 approach, then fire and thrust beyond x=-315; door plane x=-309.93 |

Each switch records player actor 0 and shot 1. The opposite door face 92 retains
its lock flag: the original switch lists only 93. This agrees with pinned XL
revision `2576a98b5de310e3ac7c9777df2677d1627b8bae`,
`main/trigger.cpp::CTrigger::DoUnlockDoors`, which clears only each listed wall's
lock/key state. Opening the door still animates both faces. The test does not
invent a reverse-side unlock.

The warm scenario saves at segment 331, moves away and loads the checkpoint.
The cold scenario starts another app process, uses the ordinary Load Game menu,
checks all three operated switches, the open wall faces and the asymmetric lock
state, then fires and flies through the first unlocked door. Open/closed motion
may retain the opened bit; the lock-bit assertions allow that transition.

Use `android/tests/test_xl_pog_runtime.ps1 -PmcOpeningGates -PmcPlaytest
-OriginalHub -MissionFlow` with the same complete original resources as the
other PMC scenarios. The existing protected owner backs up and restores assets,
pilots and settings, disables emulator recovery and verifies AVD identity before
cleanup. Opening-gate coverage runs separately from reactor combat so each
scenario has its own starting player state.

## Direct evidence and limits

On exact APK `c63885a409055938330680fc63bf6bdabea1cbdd`, version 28790:

- Two opening switches and gate crossings: 54/54 steps, 80.457 seconds
- Fresh-process load of that checkpoint plus the single-target unlock and
  crossing: 30/30 steps, 41.077 seconds
- Rendered paused endpoints inspected; player survived both probes without
  cheats or invulnerability

The final protected scenario passes 76/76 warm steps in 126.672 seconds and
15/15 fresh-process steps in 36.852 seconds. Native PIDs 2906 and 3845 differ.
The cold flight crosses beyond the first door to segment 315, with 45 shields;
face 92 remains locked and 93 unlocked. The cold frame was inspected. Protected
before/after hash-list SHA256 values both equal
`f93fbc713287ad1411ec837163fc5a3577c3bb94b3c4ff6dd4ec2b205ce3db8e`.
Full protected receipts are in `temp/xl-original-pmc-opening-gates`. The two
automation catalogs pass with 427 top-level entries and 511 support scripts.

The first door approach failed because `face_view` selected the one-unit-thick
segment 318 while the player radius is 4.735 units. The switch had correctly
cleared both locks, but the ship and its gun origins did not fit that approach.
The corrected room-240 pose and door-plane crossing pass. That failure is probe
placement evidence, not a gameplay defect. A short retry also failed its menu
precondition after the standalone helper restarted the app; the fresh replay
earned the switch activations again.

Receipts, exact poses, terminal states and frames are under
`temp/xl-traversal-20261102/pmc-opening-*`. The task-owned checkpoint archives have
build/hash provenance and must not replace a user's pilots. These are local
activation, weapon contact, crossing and persistence checks. They do not qualify
the connecting route through every gate, full PMC combat, earned campaign
completion, human controls or physical ARM devices.
