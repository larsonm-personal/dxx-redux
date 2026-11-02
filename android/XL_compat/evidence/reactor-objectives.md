# Reactors beside XL bosses

The ordinary PMC1 reactor probe exposed a legacy initialization defect: the
authored reactor (object 101, segment 677) was changed to an invisible ghost
because the level also contains six bosses. On APK
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

The earlier probe expectation that destroying PMC1's reactor alone must start
countdown was incorrect. The replacement expects seven objectives before firing,
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
The player survived with 42 shields; all six original bosses remained alive,
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
