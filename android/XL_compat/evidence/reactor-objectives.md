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
six afterward, and no final countdown. It does not dispatch or qualify an exit.

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
keys, shields or completion state are granted. Android qualification is pending
on a committed replacement APK; do not describe the new scenario as passing yet.

All 14 supplied original Pumo levels have zero or one reactor. Multiple-reactor
firing state, non-ending negative boss flags, full six-boss PMC combat, the normal
PMC1 exit, human traversal and physical devices remain unqualified. Old saves
can retain the previously ghosted reactor; start a new game on the replacement
build to exercise the correction. Existing saves are preserved, not rewritten.

Receipts: `temp/xl-traversal-20261102/pmc-reactor-*`, `reactor-native/`, and
`reactor-native-test.log`. The previous published bundle remains available.
