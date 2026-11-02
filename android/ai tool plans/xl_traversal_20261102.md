# Original Pumo traversal and progression - November 2, 2026

Cutoff: November 2, 2026 10:00 America/Los_Angeles (18:00 UTC)
Cleanup and handoff reserve begins at 09:40 (17:40 UTC)

## Baseline and preservation

- Refreshed fork cmake: 8548c6da777283d11db4b832458a70555229d82b
- PRs 278-293 are merged; PR293 exact-head workflows all passed
- New branch xl_playtest_138_guidebot_pilot targets cmake
- Preserve the existing gameplay-first plan addendum and user stash
- Published baseline remains temp/pumo-final-playtest-20261101.zip
- No user devices or processes are owned by this goal
- Receipts: temp/xl-traversal-20261102

## Work sequence and acceptance

1. Time-box initial GuideBot feasibility to two hours from 22:18 November 1
   Audit original staging, XL actor/trigger/teleport/exit semantics, then attempt
   one explicit authored route in the existing rendered runner
2. Prefer a disposable native profile if the XL rendered build supports it;
   otherwise provision an owned Android emulator and retain identity evidence
3. Record build and asset hashes, objective trace, last actor state, terminal
   classification and visual evidence. Assisted companion routing does not
   establish normal-player combat or earned campaign progression
4. Pair unsupported planner steps with normal-player activation and persistence
   probes. Select concrete original doors, switches, exits and hub returns
5. Fix evidenced gameplay defects, preserve original bytes and useful visuals,
   run relevant validation and publish coherent fork-only stacked PRs
6. Commit before replacement builds, verify embedded IDs and exact APK, preserve
   saves/settings and earlier bundles, and deliver useful verified improvements
7. Maintain tested/untested boundaries and exact-head CI while continuing work

## Repeating final deliverable

Write a fresh self-contained handoff with changes and reasons, artifact IDs,
hashes and launch steps, supported/untested boundaries, stack order/PR targets/
heads, validation/pending CI, preservation/resource cleanup, ranked next work
with reasons, and a copyable continuation prompt referencing that handoff.
Carry all objective constraints and this repeating requirement into the prompt.
Use an explicit user-filled cutoff placeholder if the next cutoff is unknown.
Do not automatically start the following goal

## K: PMC boss combat and difficulty-dependent vulnerability

Normal starter-laser probes confirm front-hit rejection, rear damage, and a boss
retiring from six required objectives to five without starting the countdown.
The successful kill probe lost the player; sustained-fire and early-retreat
attempts also lost aim under combat. Preserve those limits rather than calling
this a surviving encounter or earned progression.

A separate source audit found that pinned XL ai/aiinit.cpp::InitRobotsForLevel
sets the vulnerable-spot dot threshold to 1/4 - difficulty/8. Our D2 variable
remains zero at every difficulty. Add a native collision-decision regression
across all difficulties, demonstrate the mismatch, then initialize the XL
threshold while retaining stock behavior. Check save/load and stock isolation.
Continue the original normal-input boss probe on the exact resulting APK; no
mission edits, granted weapons/health, forced damage or controlled exit dispatch.

## L: Deliver the difficulty-corrected combat build

Published `temp/pumo-combat-playtest-20261102.zip` from committed source
`f1e73f5fd38a836515bf98ea369dbe1c2a788d7d`, version 28800. The six embedded
D1/D2 ABI IDs and installed APK hash match. APK SHA256 is
`762670c549ded1050e107701d4b938f32d9b6a288b07790790d0ab4ad2c65118`;
ZIP SHA256 is `c02f5ca373f823669468e55629f9c88ba1e3054b65f0f2a22ffec51f02e47dd1`.

The exact APK passes 99 warm steps and 17 fresh-process steps. Boss health is
1112.387451171875 before/after the cold restore; the player survives and all six
boss objectives remain without countdown. All 24 backed-up profile files match
after installation, and the protected test restores identical hashes. Cold
frame inspected. The README and manifest distinguish this local normal-input
coverage from earlier c638 PMC3/opening-gate receipts, controlled native tests,
GuideBot assistance, human traversal and physical-device testing.

Retain the final, reactor and boss-definition bundles unchanged. The next
independent original-mission probe should exercise switch 10 and its locked
door through a safe room approach, then audit the authored master-switch links.
A prepared disposable-device probe uses the engine-exported door plane and
normal laser/thrust input. It has not run yet. Continue to avoid exit dispatch
as a substitute for earned traversal, and preserve the failed combat probes.

## M: Qualify original inner switches and the red-key forcefields

The safe-room switch-10 probe passes its locked-door negative control, normal
unlock shot and physical flight beyond the door plane. Original master switch
11 opens walls 124/125 but locks doors 16/97, while dispatching the authored
texture, message and audio children. Master 12 opens forcefield faces 84-87 and
dispatches the disable action targeting ambient hum objects 140/141. Normal
shots activate both masters, with player actor 0 / shot 1 and linked children
shot 0. No direct trigger dispatch is used.

Next, prove physical crossings and the adjacent blastable wall, then run a fresh
scenario with no red key so normal flight through the opened fields earns the
pickup. Preserve warm/cold gate and key states with the protected owner. Keep
fixed-pose local interactions distinct from connecting-route traversal. Treat
linked ambient-disable dispatch as dispatch evidence; audible stop persistence
is not exposed by the current introspection and must not be claimed from it.

The fresh protected inner-gate scenario now passes 88 warm and 23 cold steps
on the published f1e73 APK. It earns the red key through normal field crossing,
retains seven objectives, and preserves gate/key state across warm and cold
loads. Both backup hash lists match and the cold frame was inspected. This is
additional same-APK coverage; the combat ZIP remains immutable. No engine or
original-mission changes were needed for these interactions.

## N: Normal PMC1 escape boost and destination persistence

M is committed as fd28ff0d930c77520b2b409168049770c880dc9c in PR306,
targeting L/PR305. N starts from M. PR303 now has 14 successful checks and one
skip; PR304-306 have no observed failures and still have running checks.

The authored escape is a useful next gap because the rendered companion could
reach an XL exit but could not activate it. Pinned XL DoExit has no universal
boss/reactor-clear condition. The remaining objective count controls countdown,
not whether the local player may contact this enabled destination-12 exit.

An owned checkpoint probe now confirms normal thrust from segment 585 through
the opened/blasted passage and all 12 boosts to exit 46. The player remains
alive and reaches the rendered completion menu; normal Enter/briefing advances
to PMC2. Forward input is released after the first boost; no later pose changes steer
the flight.
The protected fresh scenario will earn the reactor and master/blast gates,
record the exit trace/frame, cold-load the preflight checkpoint and repeat the
flight before checking PMC2 save/load. This remains bounded local traversal,
not full mission navigation or all-boss combat. Published APK/assets stay exact.

N's fresh protected escape passes 105 warm / 24 cold steps in 200.475 / 94.078
seconds, using PIDs 14984 and 16296. Both protected hash lists have SHA-256
35982d56014782fa8fc80fd92beb4867971d9ddc3257f3eff71edbcd886f8477.
The completion and restored PMC2 frames were inspected. Dedicated receipts are
in xl-original-pmc-escape after correcting the initial output precedence. No
engine change was needed.

## O: Repair repeatable original PMC2 selectors

N is 7c29fb85abd7d597a36d0953e0562269e74b721f, PR307 targeting M/PR306.
O starts from N on xl_playtest_152_pmc2_selectors. Normal shots exposed a real
progression issue: selecting 13, 14, 15, then 13 again leaves door 122 locked.
The collision path destroys permanent switch panels on their first hit. Pinned
XL preserves those textures and their lights, while still accepting the hit.

The focused native regression fails before the fix and passes afterward across
static/animated/destruction-animation panels and four orientations, on both XL
layouts. Existing texture/save controls also pass. Next, commit the minimal
runtime fix with a protected original selector scenario, build version 28810,
verify all embedded IDs, install without removing pilot/save/settings files,
then validate the exact APK. Publish a new immutable bundle promptly after that
meaningful progression improvement is verified. Keep the current combat bundle.

## P: Deliver the verified selector build

O/PR308 is 8b80ef220dbda2d9481c21611084ef21f5413808. Version 28810 was built
from that commit, all six embedded library IDs match, and the installed APK hash
matches 89df3369e09897fbce528d6ae4825c7d9c58d02e30ca55741e99c3347fc49069.
All 25 backed-up pilot/save/settings files survived installation unchanged.

The original selector replay now passes 60/60 steps. The fresh protected scenario
passes 175 warm and 59 cold steps, including physical field/door crossings and
further setting changes after loading. Both protected hash lists match, and the
cold frame was inspected. The immutable pumo-selector-playtest-20261102.zip is
published, SHA-256 e4b1b6d698131956664a9d96fe3fffbdaeaf4a6e7cdb91a36b6447dd18c1094b.
Previous bundles remain intact. Old serialized broken panels are not migrated;
start a new game or use an unfired checkpoint on this build.

Next, qualify PMC2's normal contact-operated escape door and four-boost route.
Trigger 38 is an open portal contact at 2158/2, not a shot switch. It opens
locked door 152 at 2159/2, which also starts boost 67. Boosts 68/69/70 lead to
exit 71, destination 13. Use normal contact from before the portal and preserve
that distinction from fixed local setup or full mission traversal.

## Q: Normal PMC2 escape and PMC3 destination persistence

Extend the normal PMC1 escape with PMC2's necessary field selector,
contact-operated escape door, ordinary doors and four-boost exit. This closes the gap between working selector
controls and reaching the next level through authored player contact. Preserve
local-positioning limits and compare cold replay against the preflight save.
The exploratory exact 28810 APK passes 44 flight steps and nine normal advance
steps. PMC3 reactor/ending exploration is separate pending reusable qualification.

The focused protected scenario passes 155/155 warm steps in 259.425 seconds and
23/23 cold steps in 110.810 seconds. Game PIDs 31506 and 1101 differ.
The final restored destination is PMC3 (level 13), alive with one reactor objective.
Before/after profile and resource hash lists match, SHA-256
`26697009f39fe779bf7c98c8fd4f93060dd0c6ebfb8bdaa8c5481ed1ac3dd17b`.
Receipts and inspected completion/destination frames are in
`temp/xl-original-pmc2-escape`. These supplement the immutable selector bundle;
that published folder and ZIP were not changed.

## R: PMC3 teleport access, reactor and authored ending

Start from Q/PR310 at 0f1df9069edbfe2ba22d27ae10ef90570afaadbd. Preserve the
original earned PMC3 checkpoint before exploring. First qualify master 17's
normal shot through its grate, the closed/open portal wall and outward/return
teleport contacts. Then cover the ordinary exit-corridor door, reactor combat,
countdown persistence and explicit ending screens. This fills access and terminal
progression gaps left by fixed reactor/exit approaches. Keep local positioning
separate from full navigation, and do not alter content or grant objectives.

Exploratory exact 28810 results already establish normal reactor destruction
(14 steps), warm countdown restore/physical exit (11 steps) and a fresh-process
countdown restore/physical exit/normal ending-menu return (18 steps). The final
ending must use explicit screen advances: generic skip_briefing continues past
the main menu and cleanly exits the engine. Build a reusable protected scenario
from verified actions, retaining original resources and the user's other devices.

Protected qualification passed 215/215 warm steps (403.278s) and 22/22 cold
steps (51.892s), PIDs 5025/7369. Completion and main-menu frames were inspected.
Before/after profile and resource hashes match:
1e41ad8c1791a2978e26bea22293902736c40331439a8949a9cf2ec1e5403129.
Receipts: temp/xl-original-pmc3-finale. No content/runtime changes or new APK.

## S: PMC3 opening access

Start from R/PR311 at 3c7ea117a3470873f4132710edb5ceaffd8e42f7.
The exported original level graph places masters 10/11 below grates adjoining
initially accessible rooms 355/617. Probe ordinary laser activation from above
those grates before considering later switches 9 and 8. The graph is setup
guidance, not physical traversal evidence. Never position inside an inaccessible
switch chamber to claim earned access. Preserve the original earned level-13
checkpoint and use only the owned disposable profile.

The one-unit grate segment 355 cannot contain the ship. Corrected firing poses
use full-sized rooms 205/467; normal shots activate masters 10/11 and open the
shaft grates. The combined exploratory sequence passed 74/74 (113.137s), covering
both closed-wall controls, connecting switch 9, unlock switch 8 and warm load.
The first protected run missed master 11 through a grate bar. A two-unit offset
in room 467 passed a fresh shot and a checkpoint reload repeat. The protected
run is being repeated because its longer setup exposed this aiming sensitivity.

The corrected protected run passed 226/226 warm (363.149s) and 26/26 cold
(48.283s), PIDs 12742/14786. Both endpoint frames were inspected. Before/after
profile/resource hashes match:
9985ea381758a99808306501eb7e312511cdf735588016dd1cc2b9ef921c4c40.
Receipts: temp/xl-original-pmc3-opening. The original profile was restored.
Scoped quality and both catalogs pass (427 top-level, 524 support scripts).

## T: PMC3 blue-key access

Start from S/PR312 at 2b841f0f498a6ecc321f8b65dadec7c7820fd877.
The minimal exported-graph sequence after the opening checks is switch 6,
switch 7, contact door 0, then switch 1. Qualify the corresponding locked-door
controls and normal flights before collecting the original blue key from its
adjoining passage. Test the contact-trigger/remote-door corridor as continuous
flight, since independently placing the player beyond each door would miss its
timing condition. Keep fixed local approaches separate from connected navigation.
Preserve the opening checkpoint and qualify key/door state after warm and cold
loads if this probe succeeds. No runtime change is presumed.

The exploratory chain passed 89/89 (74.804s), with a separate 15/15 warm key
restore and keyed-door flight (42.086s). Protected qualification passed 325/325
warm (429.243s) and 14/14 cold (46.895s), PIDs 16488/18861. Both endpoint frames
were inspected. The protected ship survived with only 0.714 shields; do not
claim robust combat or extend the encounter without ordinary resource planning.
Before/after profile and resource hashes match:
671dd638ec9ed2de34f0949bdc7d8307636a1dcd04bb63abebf6d31e9eec296c.
Receipts: temp/xl-original-pmc3-blue. Quality and catalogs pass (427/526).

## U: PMC3 resources and gold-key access

Start from T/PR313 at 665f237e54fcec5833d2e05153e0efd75c2ce4bd.
The protected blue chain's narrow survival margin makes ordinary resource
planning necessary before extending it. Check original shield pickups 27 and 15
from adjoining rooms 1565 and 718, with normal flight and consumed-item checks.
Both are in the already accessible area; do not grant shields or reset pickups.
An initial selection of objects 10/14 used the wrong item ID: they are energy,
not shields. That normal pickup trace is retained separately; the corrected
selection follows powerup.h (energy 1, shields 2).
Then probe the original gold-key alcove from its exterior passage, test the
gold door without/with its key, and qualify save/load if successful. Preserve
the earned blue checkpoint and keep all fixed approaches explicit.

Protected qualification passed 380/380 warm (500.105s) and 16/16 fresh-process
steps (44.977s), PIDs 27449/30186, on published 28810. Both rendered endpoints
were inspected, with 74.643/75.630 shields. Profile/resource hash lists match
540570408b017dfe61d079cd48019843665bfb4ceb4993d5ea59242e5ed571dc.
Receipts: temp/xl-original-pmc3-gold; starting profile restored. No runtime or
mission-content change was needed. Catalogs passed 427/529.

Two earlier extended runs missed opening switches before reaching gold work.
Their normal firing sweeps were fixed on ancestor S and propagated through T,
now 646452d8239c0f101b317c939951ea5a2a7fb431. The combined reload check passed
23/23 (53.164s), and the complete 380-step run covers both corrected approaches.
Historical S/T protected receipts retain their original counts. The next gap is
switch 3 behind a bent grated shaft: audit and try the original guided-missile
pickup and normal steering, never place the player beyond the grates to pass it.

## V: bounded normal guided-missile flight

Start from U/PR314 at c79dcb506304accbbfde71a25c239c9c3713d37c. Original switch 3
is beyond a bent grated shaft and is not player-accessible in the verified key
state. Original guided item 80 is reachable from adjoining room 2126; its ID40
is a single missile, not a four-pack. Corrected normal pickup/selection passed
16/16 (44.957s). An earlier incorrect four-missile expectation is retained as a
test error, not a gameplay defect. Local staging and a closed-grate flight check
passed 13/13 (4.566s); normal movement also consumed original shield16 in1048.
The saved launch approach is1049, with one missile selected and switch3 unfired.

Current introspection follows the ship and cannot observe the guided projectile.
Add a read-only D2-only guided_missile object with validated live object/signature,
position, velocity, forward direction and ownership. No steering shortcuts or
gameplay rules change. Compile all six Android engine/ABI libraries, commit before
building, verify embedded IDs and the installed APK hash, and preserve all26 owned
profile files. Then exercise ordinary missile fire, steering and release/impact
with rendered evidence. Try the authored bent route for roughly one hour; retain
partial/unsupported observations separately from validated gameplay defects.
No player placement beyond the grate, equipment grants or forced switch dispatch.

Build28820 from fbabcf01fbb24b737896eea354bc1147f44b3ac5 passed all six engine/ABI
builds and embedded-ID checks; installed APK SHA-256
6e29ef901f2a510a8e6eeec6f85cc3954690b33162b6ab9a034fe9aeabf4188d matched and all26
profile files were unchanged. The original cloak and nearby threats were handled
through ordinary pickups and bounded laser bursts. Cleared first-grate flight
passed8/8 (2.591s), with active missile position/velocity and a rendered GUIDED
view. The bend, clean release and switch3 remain partial/unqualified. Corridor
steering was observed, but shield-stability guards failed; do not count those
scripts as passes. See evidence/pmc3-guided-pilot.md for exact boundaries and the
preserved cleared checkpoint. PR315 remains draft for follow-up qualification.
