# Original Nautilus unused wall animation fields

Checkpoint on `xl_recovery_20261117_06`, immediate parent
`bf5806e578adbe1b2a9a40abdcf5428cf9e96f57` (#567), in stack 471.
This narrows dependency discovery to the fields the wall behavior consumes.
It does not normalize the original level or qualify campaign completion.

## Source and implementation

Original Warp Shock 2.0 level 4, Nautilus, has 28 wall records with clip -33:
24 closed walls (type 5) and four illusions (type 3). Those types do not consume
an animation clip. Native gamesave initialization, wall_toggle and the pinned
XL CheckAndFixDoors distinguish animated door/blastable behavior from these
types. The previous generic resource graph rejected the unused signed field.

ObjectDependencies::wall now requires a valid nonnegative animation for doors
and blastable walls, ignores negative unused fields on other types, and retains
the conservative dependency closure for their positive references. Raw bytes
and live clip values remain unchanged. Preview and existing live dependency
audits share this rule. Native runtime behavior and save formats are unchanged.
Existing opposite-door admission and runtime bounds remain in force.

The close-door controls cover all six nonanimated types with clip -33, reject
negative/out-of-range door and blastable clips, and still reject an invalid
positive auxiliary animation. The original-level optional CMake owner verifies
the authentic HOG/resource pins and input-tree identity, saves separate receipts
per attempt, and preserves the 120-second diagnostic watchdog.

## Local evidence

Private receipt root: E:/dxx-redux-builds/xl-recovery-20261117.

- warp4-walls-01: ordinary developer-OFF load/tick, 120 unassisted frames,
  PASS in 4.922 seconds. This is one frozen case, not a full 64-case score.
- wall-close-controls-01: PASS, including actual-source comparisons: 1620
  sphere/plane, 3072 close-door state/allocation, 336 obstruction and 660
  frame/flag/state/timer comparisons, plus native lifecycle controls.
- warp-wall-native-05: optimized original-level PASS. Captures 120 native frames
  with environmental input, replays them through the existing input-demo path,
  and compares full object/world/native/SIM state using the existing observer
  equivalences. The embedded replay result also matches. Controlled illusion
  transitions and a partial cloak saved across a stock-level transition pass;
  raw clips remain unchanged. This is a supplemental controlled lifecycle.
- rendered-warp4-01: PASS, 120 simulation and 120 rendered frames, 1045 original
  segments, Intel Arc 140T. Binary SHA256
  071fb4411b5fd73130f438e99d0d70eaa722f6f5363bf0111b82c29aa85b2dad.
- android-wall-save-01: rebuilt actual Android libraries; seven D1 and twenty D2
  native save cases PASS. Isolated device fixture, no APK installation or app
  save modification. This does not claim original Nautilus Android gameplay.
- Automation catalog PASS: 99 standalone JSON, 550 support scripts, 315
  standalone PowerShell tests. Runner catalog integration PASS.

Original HOG SHA256:
2054dca034ea9aac0d13c37a5867b18fa84d9b307b36702b56bcc9ed3a61b45c.
Original copied mission/mod tree SHA256 before and after native-05:
5e3b524a6fbd839834d054c52fe417bab87e02288ca9b056a60282bce6daf59c.
The runner checks authentic Vertigo and XL addon identities explicitly.

Failed receipts remain available. Native-01/02 compared independent live clocks;
native-03 omitted capture-side automap discovery and compared different recorder
boundaries. Native-04 exposed the fixture's exact one-second expectation: the
native decloak finishes strictly after one second. The final fixture asserts
both the exact boundary and its next frame. Debug-02 exposed a missing caller
pause before the fixture's second low-level save. No runtime clock/RNG fix,
observer exclusion, watchdog extension or readiness relaxation was used.

Revised fixture PASS: warp-wall-native-06 (optimized) and warp-wall-debug-03
(Debug/assertions), both including the exact decloak boundary and paired pause.
Their binary SHA256 values are respectively
48d0e554653d75a62301cbaf6b5460f71716ba0dce3599e33287eab84518bc23 and
253893e97926aad103e3a80d64dceac3b90ce6db9c0078bace552f5237bcf369.

The private CI fast owner does not currently stage this original
Warp package: its required close-door controls cover the dependency rule, while
the frozen expanded measurement covers ordinary original-level load/tick.

## Preceding checkpoint CI

#567 trusted run 44040272340 repaired the Linux JSON conversion failure.
Private Linux asset owner PASS: all 113 required controls; host elapsed 943.818
seconds. Original eight PASS. The retained receipt has no subzero_lifecycle key:
the trusted controller still invokes its reviewed runner, so the new supplemental
Subzero runner is locally exercised but is not enrolled in that CI measurement
until controller adoption. Candidate compilation and ordinary Subzero load/tick
are covered separately; do not claim its deeper original lifecycle ran in CI.
The separate expanded ten-minute job was incomplete: 54 passed, four failed,
six untested in 266.825 seconds. Its unchanged remaining-budget start guard
stopped the measurement; this is not 60/64 or a readiness percentage.
The last complete CI score remains #564's 59/64.

Artifacts: original-eight 14308838618, expanded 14309278113, host 14310157529.
Downloaded archive SHA256 values were checked against GitHub's artifact digests.
