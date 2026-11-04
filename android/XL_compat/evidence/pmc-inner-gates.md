# Original PMC1 inner switches and red-key access

The old seven-objective result used a Vertigo robot extension. Original Pumo
VHAM defines type76 as model20, strength20, zero guns and boss_flag0; all12
VHAM robot records (66-77) have boss_flag0. The original PMC1 therefore has one
reactor objective, not a reactor and six type76 bosses. The current gate asserts
that original definition and one remaining objective. This changes the probe,
not mission bytes or engine objective accounting.

The historical checks below fill gaps between the earlier local switch/key probes. They keep
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

The historical substituted-resource scenario enters PMC1 through normal hub laser/thrust, keeps its reactor
and six bosses alive, and ends with seven objectives and no countdown. It saves
in segment 534, moves away and loads normally. A fresh process then loads that
checkpoint and flies through the unlocked door, opened/blasted passage and red-key
chamber again. Lock/open/blasted states, master operation and the earned key must
persist. No direct exit or trigger dispatch, weapon/health grant or mission edit
is used.

Run `test_xl_pog_runtime.ps1 -PmcInnerGates -PmcPlaytest -OriginalHub -MissionFlow`
with complete original PumoMods and without VertigoHam. The owner now rejects
Vertigo substitution for this gate. The
existing protected owner backs up/restores pilots, saves, settings and resources,
checks the AVD identity and disables emulator recovery. Scripts are
`test_xl_pumo_inner_gates.jsonc` and `test_xl_pumo_inner_gates_cold.jsonc`.

## Historical substituted-resource exploratory evidence

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

## Historical substituted-resource protected scenario

On the same exact f1e73 APK, the fresh scenario passes 88/88 warm steps in
133.796 seconds and 23/23 fresh-process steps in 37.638 seconds. PIDs 12394 and
13300 differ. The player starts without the red key, is blocked by the field,
then earns the key by normal flight through the opened chamber. Cold flight
ends alive in segment 546 at z=578.9136, with the key, seven objectives and no
countdown. The cold frame was inspected.

Protected before/after hash lists both hash to
`ed0dbeeb0b5766fcfd5a559428500b38e13d6471ebfcc7f3ac074c1458b78950`.
Historical receipts are preserved in
`temp/xl-overnight-20261104/prior-inner-gates`. These checks were run
after publication of the combat bundle; its files and hashes remain unchanged.

## Full-original Android qualification, November 3 2026

The retained APK29471 (source `ec7715ffa5cd34865b701aa2152ee42f2a51b018`,
package `com.dxxredux.app`, SHA256
`da070ebaacf95ea9be2f95fbd630ef5db1ccea0e24cf9108bbb6822c96a0661d`)
passes the corrected original-resource gate on owned Android14 software emulator
DxxPhoneFog20261102. No APK was built or installed for this check. All six retained
native IDs are `dxx-build:ec7715ffa5cd34865b701aa2152ee42f2a51b018:29471`.

- Warm:88/88,76.356seconds, run `c17172396049446dad7da989b61a2282`
- Fresh process:23/23,18.539seconds, run `87a763dc929a4a8ba339f46664a67e29`
- Game PIDs7570 and8248 differ; menu Load Game restores the checkpoint
- Final position:segment546,(21.0695,-850.2141,581.5607), alive, red key held,
  one remaining reactor objective, countdown inactive, cheats disabled
- Protected before/after manifests both SHA256
  `160d7bc5c74593e3db73d3ac46baed653d55096c34dd403cb87cafe3af5ef253`

Receipts and inspected cold frame: `temp/xl-original-pmc-inner-gates`.
Read-only VHAM record audit: `temp/xl-overnight-20261104/original-vham-robots.json`.
The native reader's125-byte extension weapons and480-byte robot layout were used;
all12robot record sentinels equal0xabcd. No mission bytes were edited. The first
old seven-objective assertion failed as expected; a subsequent added float
assertion needed numeric comparison rather than string20 versus20.0. Both failed
runs and their exact restoration receipts remain preserved separately.

Resources are the original Pumo HAM/HXM/VHAM/ASE/OOF tree, original camera addon,
unchanged HOG SHA256 `6e3e723e549897188a8fdc19db8b04630de9d428d2b31f8e3f48af108915b043`
and MN2 SHA256 `45b4138e0a0d49ab8ee09334b7cfa97c0ec75ac6c6b6406b21eba28ddaa19cfd`.
The protected legacy staging route reports mission_asset_owner=base; this result
does not independently qualify launcher UI import/catalog ownership. Fixed poses
select local approaches; normal shots, thrust, pickup and save/load do the work.
Full connected traversal, reactor/escape, later campaign, phone controls and
rendered/audio fidelity remain unqualified by this gate. Historical type76 boss
combat is evidence for the substituted fixture only.
