# Original PMC1 inner switches and red-key access

These checks fill gaps between the earlier local switch/key probes. They keep
the original mission bytes and use ordinary laser, thrust and collision pickup
conditions. Fixed poses choose each local approach; they do not establish the
connecting route or human traversal.

| Control                       | Before normal activation                                                 | Result and physical crossing                                                                                                                                     |
| ----------------------------- | ------------------------------------------------------------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Switch 10, segment 634 side 0 | Door 137 remains locked when shot                                        | The switch unlocks 136/137; room-1532 fire/thrust crosses their z=-15.1493 plane to z<-21                                                                        |
| Master 11, segment 690 side 3 | The closed passage at wall 125 blocks thrust                             | A shot locks doors 16/97 and opens walls 124/125; thrust crosses x=-165.9305 to x<-175                                                                           |
| Blastable walls 122/123       | Both retain their unblasted flags                                        | Ordinary starter-laser hits set both blasted flags, then thrust crosses x=-185.9305 to x<-195                                                                    |
| Master 12, segment 657 side 2 | The first red-key forcefield blocks thrust and the player has no red key | A shot opens all four field faces 84-87; normal flight crosses the fields at z=503.8507 and z=543.8507, collects the authored key between them and reaches z>550 |

The masters are not treated as generic door-open commands. Master 11 dispatches
children 20/21/22/23/24/34, including the authored door locks. Master 12 dispatches
children 50/51. Parent traces record player actor 0 and shot 1; linked children
record actor 0 and shot 0. Child 51 targets ambient hum objects 140/141 according
to the original inventory and runtime dispatch. The current API does not expose
their enabled state, so audible stop/persistence is not claimed from this trace.
Texture, message and named-sound child dispatch is likewise distinct from a full
visual/audio acceptance check.

The fresh scenario enters PMC1 through normal hub laser/thrust, keeps its reactor
and six bosses alive, and ends with seven objectives and no countdown. It saves
in segment 534, moves away and loads normally. A fresh process then loads that
checkpoint and flies through the unlocked door, opened/blasted passage and red-key
chamber again. Lock/open/blasted states, master operation and the earned key must
persist. No direct exit or trigger dispatch, weapon/health grant or mission edit
is used.

Run `test_xl_pog_runtime.ps1 -PmcInnerGates -PmcPlaytest -OriginalHub -MissionFlow`
with the same complete original resources as the other Pumo scenarios. The
existing protected owner backs up/restores pilots, saves, settings and resources,
checks the AVD identity and disables emulator recovery. Scripts are
`test_xl_pumo_inner_gates.jsonc` and `test_xl_pumo_inner_gates_cold.jsonc`.

## Exploratory evidence

On exact APK `f1e73f5fd38a836515bf98ea369dbe1c2a788d7d`, version 28800:

- Switch 10, locked-door control and flight: 30/30 steps, 36.321 seconds
- Master 11 normal shot and linked door states: 15/15 steps, 31.536 seconds,
  including ordinary cold Load Game
- Master 12 normal shot and four field faces: 11/11 steps, 1.523 seconds
- Physical field/passage flights and normal blastable-wall combat: 24/24 steps,
  10.449 seconds, followed by a saved checkpoint

The exploratory pilot already held the red key. Only the fresh protected
scenario can establish pickup through the newly opened fields. Paused endpoint
frames are retained under `temp/xl-traversal-20261102`. Master 11's message had
expired by capture, so the frame does not prove readable message presentation.

The combat ZIP remains unchanged; additional checks on the same APK have separate
receipts. Physical devices, human controls, full boss combat, connected traversal
and earned PMC exits remain unqualified.

## Fresh protected scenario

On the same exact f1e73 APK, the fresh scenario passes 88/88 warm steps in
133.796 seconds and 23/23 fresh-process steps in 37.638 seconds. PIDs 12394 and
13300 differ. The player starts without the red key, is blocked by the field,
then earns the key by normal flight through the opened chamber. Cold flight
ends alive in segment 546 at z=578.9136, with the key, seven objectives and no
countdown. The cold frame was inspected.

Protected before/after hash lists both hash to
`ed0dbeeb0b5766fcfd5a559428500b38e13d6471ebfcc7f3ac074c1458b78950`.
Full receipts are in `temp/xl-original-pmc-inner-gates`. These checks were run
after publication of the combat bundle; its files and hashes remain unchanged.
