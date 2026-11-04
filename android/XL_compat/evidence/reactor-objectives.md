# Reactors beside XL bosses

The historical PMC1 reactor probe with substituted Vertigo robot definitions
exposed a legacy initialization defect: the
authored reactor (object 101, segment 677) was changed to an invisible ghost
because the fixture supplied six bosses. Original Pumo supplies gunless props
for those type76 objects instead. Mixed reactor/boss controls below remain
relevant to XL semantics, but are not original Pumo combat qualification. On APK
`888ded31795e2074c46566c688ac813902a60cce`, normal laser fire consequently could
not hit a reactor. The probe timed out at step 72; introspection showed an empty
reactor list. Original HOG, descriptor and mod bytes were unchanged.

## Source semantics and fix

Pinned XL revision `2576a98b5de310e3ac7c9777df2677d1627b8bae`:

- `main/gamedata.cpp`, `DefaultGameplaySettings`: multi-boss mode defaults on
- `objects/reactor.cpp`, `InitReactorForLevel`: XL multi-boss levels retain
  reactors alongside bosses and count each reactor plus each level-ending boss
- `objects/robot.cpp`: positive boss flags set `bEndsLevel`; currently admitted
  Redux XL boss definitions have positive flags 21 through 28
- `objects/boss.cpp`, `CBossData::Remove`, and `ai/aiboss.cpp`,
  `DoBossDyingFrame`: a required boss counts until its death roll finishes
- `objects/reactor.cpp`, `DoReactorDestroyedStuff`: final countdown and reactor
  wall links wait for the remaining required count to reach zero; reactor object
  actions still run before final completion, including an authored countdown

Redux now keeps the reactor on an active XL level. Completion uses live reactor
objects and the existing saved per-boss death state, avoiding a separate counter
that could drift across loading. A reactor death still dispatches its object
actions and records the dead reactor, but does not start final countdown while
required bosses remain. Stock level initialization retains its prior behavior.

For that historical substituted fixture, expecting PMC1's reactor alone to start
countdown was incorrect. Full-original Pumo has one objective, so its ordinary
reactor destruction does start countdown. The replacement expects seven objectives before firing,
six afterward, and no final countdown. That reactor scenario does not dispatch
or qualify an exit. The later [normal escape probe](pmc-escape.md) separately
qualifies the authored boost exit. XL DoExit does not require this countdown
objective count to reach zero; these are separate semantics.

## Validation and boundaries

The native derived-fixture controls exercise lethal combat, independent boss
death rolls, both reactor-first and bosses-first orders, intermediate disk
save/restore, and final countdown only after completion. Six completion samples
are checked by the unmodified pinned XL completion function, compiled with
isolated state adapters. Existing boss spawn, malformed-save and stock isolation
controls remain in the same runner. These are controlled tests, not a campaign
playthrough.

The protected Android mode is `test_xl_pog_runtime.ps1 -PmcReactorPlaytest
-PmcSwitchPlaytest -PmcPlaytest -OriginalHub -MissionFlow`, with the same original
resource arguments as the switch/key probe. It appends a normal switch-14 shot
and reactor laser attack to the earned hub/key/switch route, then checks warm and
fresh-process persistence. Fixed poses select bounded interactions; no weapons,
keys, shields or completion state are granted.

The exact universal APK from `c94341f823c0c498ae3dc824cf4663f8fddf4d8d`, version
28780, passed 81/81 warm steps in 187.759 seconds and 15/15 cold steps in 34.769
seconds. Normal starter laser fire destroyed the reactor in 10.972 seconds.
The player survived with 42 shields; all six fixture bosses remained alive,
and countdown stayed inactive. Warm load restored the saved segment 664.
Fresh-process PIDs were 15671 then 16886; the restored ship also flew through
the previously opened switch gate. The cold rendered frame was inspected.

All six D1/D2 libraries across ARM64, ARMv7 and x86_64 embed the source ID above.
APK SHA256: `3221c31bac49e9dfa1a9ada6af63888858854c1f1f09c0790cc207f4ab8c5117`.
The installed APK hash matches. Installer checks preserved all 20 backed-up
pilot/save/settings files, and the protected scenario restored identical
resource/player hash lists. Device evidence uses the owned x86_64 host-GPU
emulator `DxxTraversal20261102`, not physical ARM hardware.

The same APK passed the existing PMC3 continuity scenario (115/115 in 233.938
seconds) and fresh-process scenario (21/21 in 47.561 seconds). Native PIDs changed
from 18056 to 19526; the player/85-actor checkpoint matched and restoration hashes
were identical. Its rendered frame was inspected. These broader scenarios use
controlled exits and fixed poses, so they do not establish earned campaign
completion. Previous-APK receipts remain in
`temp/xl-traversal-20261102/baseline-pmc3-cold`.

Follow-up audit: PMC3 retains eleven boss-state entries originating from base
definitions, but its current definitions exclude them from required objectives;
the final snapshot correctly reports the remaining reactor only. Registration
currently precedes `load_level_robots`, which applies per-level replacements.
Audit newly introduced HXM bosses and the registration order before claiming
general custom-boss lifecycle coverage. PMC1's six active required bosses are
explicitly verified above.

All 14 supplied original Pumo levels have zero or one reactor. Multiple-reactor
firing state, non-ending negative boss flags, full six-boss PMC combat, the normal
PMC1 exit, human traversal and physical devices remain unqualified. Old saves
can retain the previously ghosted reactor; start a new game on the replacement
build to exercise the correction. Existing saves are preserved, not rewritten.

Receipts: `temp/xl-traversal-20261102/pmc-reactor-*`, `reactor-native/`, and
`reactor-native-test.log`. The previous published bundle remains available.

## Original Pumo reactor, November 3 2026

The owner also accepts `-PmcReactorPlaytest -PmcInnerGates -PmcPlaytest
-OriginalHub -MissionFlow` with complete original PumoMods and no VertigoHam.
It appends `test_xl_pumo_original_reactor.jsonc` to the original inner-gate route:
normal hub entry, master shots, blastable wall and red-key pickup by flight,
then switch14 and ordinary starter-laser fire at reactor101 (segment677).
The separate original suffix requires its authored750shields before combat,
one objective before firing, zero afterward, active countdown, survival and
ordinary warm quick-save/load. No kill, completion or inventory state is granted.
`test_xl_pumo_original_reactor_cold.jsonc` menu-loads the checkpoint in a fresh
process, retaining reactor destruction, key, switch14 and active countdown.

Exact retained/installed APK29471 SHA256
`da070ebaacf95ea9be2f95fbd630ef5db1ccea0e24cf9108bbb6822c96a0661d`, package
com.dxxredux.app/source `ec7715ffa5cd34865b701aa2152ee42f2a51b018`, passes106/106
warm steps (102.283seconds,run `6b2cdb510aa54f448f286692423f4300`) and7/7cold
steps (11.904seconds,run `43cc701a49774ae58172b1fe05a5bbfb`). Game PIDs9379 and10179
differ. Final segment664,33.3056shields,73.0505energy, reactor shields-8.5,
zero objectives and an active90-second countdown with87seconds remaining.
The original Pumo HAM/HXM/VHAM/ASE/OOF and camera/effects resources are the same
as the [qualified inner-gate run](pmc-inner-gates.md). No APK build or install.

Protected manifests both SHA256
`160d7bc5c74593e3db73d3ac46baed653d55096c34dd403cb87cafe3af5ef253`.
Receipts: `temp/xl-original-pmc-reactor`. Prior substituted receipts are preserved
in `temp/xl-overnight-20261104/prior-reactor`. This uses owned Android14 software
emulator DxxPhoneFog20261102 and protected legacy staging, not physical hardware
or independent launcher UI qualification. Fixed poses choose local approaches;
connected navigation and escape remain outside this reactor-only result.
