# Original PMC2 normal escape

The permanent selector fix restores access to the ordinary escape route. This
scenario qualifies the remaining local gates and authored exit without dispatching
an exit or trigger, granting equipment, or changing mission content.

## Authored conditions and normal activation

Master 16 is shot from its adjoining upper room and opens forcefields 120/121.
The player flies across the field at segment 2068 side 2. Contact with master 39
at 2072/3 dispatches the original escape message/sound. Ordinary laser fire and
thrust open and cross doors 128 and 150. Fixed approaches between these local
checks do not establish connected human navigation through the whole route.

Trigger 38 at 2158/2 is an open portal contact, not a shot switch. It opens
locked door 152 at 2159/2. Before contact, firing at that door leaves it closed.
From the center of room 2156, forward thrust contacts 38, crosses the unlocked
door and engages boost 67. Input is then released; engine physics carries the
player through boosts 68, 69 and 70 to exit 71 at 2234/2. Its value 851968
selects PMC3, level 13. There are no pose changes during that escape flight.

## Exact-build exploratory evidence

Published selector APK 8b80ef220dbda2d9481c21611084ef21f5413808, version 28810,
SHA-256 89df3369e09897fbce528d6ae4825c7d9c58d02e30ca55741e99c3347fc49069:

- Normal local gate and escape flight: 44/44 steps in 46.237 seconds
- Master 16 records actor 0, shot 1; contacts 39, 38, 67-71 record actor 0, shot 0
- Level 12 Complete endpoint: segment 2239, position
  [53.24751281738281, -801.3291015625, 12.147506713867188], alive,
  shields 86.9648, zero objectives, boost unlocked
- Normal completion-menu advance to PMC3 and save: 9/9 steps in 33.819 seconds
- Completion frame inspected; receipts are pmc2-escape-flight-* and
  pmc2-escape-next-* under temp/xl-traversal-20261102

The preflight profile archive has SHA-256
45a17c0b0472f662c815273f391da683e0a53dd76882a2e621a0f0217f191883.
It preserves 25 pilot/save/settings files on the owned disposable AVD.

## Repeatable protected owner

Use test_xl_pog_runtime.ps1 with -Pmc2Escape -PmcEscape
-PmcReactorPlaytest -PmcSwitchPlaytest -PmcPlaytest -OriginalHub -MissionFlow
and the complete original resources. The owner protects the profile, verifies
AVD identity and disables automatic emulator recovery. The warm scenario saves
before the contact-operated escape, then records the completion trace/frame.
The cold scenario loads that checkpoint in a fresh process, repeats normal
contact and boost travel, advances to PMC3 and checks its save/load.

This scenario exercises the selector needed to open the escape field; the full
setting-change matrix remains in the separate selector scenario. Its cold branch
specifically qualifies escape replay and destination persistence. Full navigation,
human controls,
physical ARM devices and PMC3 combat/navigation are outside this test's claim.

The cold replay originally reached Level 12 Complete but failed a final
last-actor assertion for boost 67: original robot 171 (type 37), placed by the
mission in segment 2159, followed through and replaced that diagnostic slot.
The test now checks the player-owned boost state and active player lock at
initial engagement. The last-actor slot can already belong to a following robot
within the same polling interval, so it is not the player-state authority.
Completion still requires all boost contacts and player-owned exit
71; it does not require the mutable boost last-actor slots to remain unchanged.
This preserves direct evidence of normal player activation without treating a
later valid robot contact as a gameplay failure.

## Protected qualification result

The focused protected scenario passes 155/155 warm steps in 259.425 seconds and
23/23 cold steps in 110.810 seconds. Game PIDs 31506 and 1101 differ.
The final restored destination is PMC3 (level 13), alive with one reactor objective.
Before/after profile and resource hash lists match, SHA-256
`26697009f39fe779bf7c98c8fd4f93060dd0c6ebfb8bdaa8c5481ed1ac3dd17b`.
Receipts and inspected completion/destination frames are in
`temp/xl-original-pmc2-escape`. These supplement the immutable selector bundle;
that published folder and ZIP were not changed.

An earlier launch stalled during graphics initialization and recorded an automation
broadcast ANR before the scenario began. Profile restoration matched exactly.
Four subsequent menu launches and three clean quit/relaunch cycles passed; the
stall did not reproduce. Diagnostics remain under pmc2-escape-interrupted-* and
clean-relaunch-* in temp/xl-traversal-20261102. No speculative runtime fix was made.
