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
