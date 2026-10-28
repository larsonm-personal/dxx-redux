# Immediate wall-trigger teleport evidence

The later [trigger flag tranche](trigger-flags.md) supersedes the 32/64 restriction
below with action-specific semantics. Object events also have their separate
[lifecycle evidence](object-triggers.md); this original tranche remains scoped.
The later [boost tranche](speed-boosts.md) adds the source's missing speed-60
normalization before wall teleport rotation. Its 252 updated source comparisons
supersede the original velocity results below; object teleports retain motion.

## Scope

Detected type-14 wall actions are implemented in the shared OpenGL D2 binary for
singleplayer. Valid segment/side links, random destination selection, position,
orientation, velocity rotation, actor filtering, repeat/one-shot behavior and
saved disabled state are covered by the controls below. There is no production
mission-name or hash exception

The immediate tranche is extended by [scheduler support](trigger-scheduler.md):
absolute serialized times 100 through 900000 and permanent/alternate/autoplay
flags now have runtime and persistence controls. Orientation/silent flags,
object-attached triggers, speed boosts, spawn-setting actions and multiplayer
remain rejected. Other level, capacity and
asset requirements still apply. Software-renderer XL admission is unchanged

This tranche does not qualify another original mission or campaign. Surveyed
original teleport missions retain additional blockers. Entombed is fully decoded
but also needs modern repair-center and team-room semantics. Most other candidate
archives stop earlier at unsupported object layouts. Entombed also has 110
non-neutral owner/group records and a level-specific HXM; Glass House has an
uninterpreted object-trigger tail and its own HXM. Both remain rejected. The
[candidate inventory](wall-teleport-candidates.json) retains complete decoded
blocker groups, capacity checks, source hashes, asset declarations and parse
failures without treating unparsed requirements as absent. See the
[candidate comparison and roadmap](../../ai%20tool%20plans/mission%20metadata/xl_wall_teleports_20261025.md)

## Source behavior

Pinned XL revision: `2576a98b5de310e3ac7c9777df2677d1627b8bae`

- `main/trigger.cpp`: wall dispatch, random link, orientation/velocity transform,
  one-shot flags, time-based delay detection and secret-exit sound
- `main/segment.cpp`: authored wall dispatch and float-accumulated segment center
- `objects/updateobject.cpp`: physics crossings include nonplayer objects and
  continue through the recorded crossing list after relocation
- `include/vecmat.h`, `maths/fixc.cpp`, `maths/tables.cpp`: default-format rotation,
  normalization, truncation and trigonometry

The reference selects `rand() % links`; Redux uses its simulation RNG stream with
the same modulo selection. Identical random sequences across engines are not
claimed. The reference's positive asin endpoint for either sign at unit magnitude
is retained, including the subsequent orientation direction correction

The runtime gate uses loaded XL source state, independent of menu/rendering
profile state. Eligible nonplayer actors record crossed segments; stock actors
retain the original path. Non-delayed XL teleport times are not decremented by
the stock trigger timer. Required secret-exit sound and player-appearance clip
dependencies are checked before loading

## Native evidence

`xl_inventory_contract` covers all three supported layouts (15/33, 17/36,
23/40), invalid destination indices, delayed-action thresholds including INT_MIN,
supported immediate actions and continued rejection of boost requirements

`xl_wall_teleports` uses original Neptune geometry with explicitly derived
teleport records. It verifies:

- Wall entry and direct dispatch, actor-type filtering and dead-player refusal
- 64 immediate repeats exercising both destinations; one-shot and disabled state
- Nonplayer physics crossing through a real shared portal, from 0 through 15 to 92
- Post-arrival movement that hits a solid wall and remains inside valid geometry
- Immediate-time preservation alongside the unchanged stock trigger countdown
- Serialized source admission and multiplayer rejection
- Save, switch to stock, restore source mission, pose and one-shot disabled state

The independent reference executable compiles hash-pinned extracted XL functions,
including `TriggerSetOrient` and `TriggerSetObjOrient`, with storage adapters and
the original trigonometric tables. It links no Redux math. All 252 sampled runtime
orientation/velocity results match exactly: six segments, six sides, seven input
velocities, eight distinct normals including two non-axis directions. It tests
the supplied normals, not an independent end-to-end geometry-loader comparison

Fixture source HOG SHA256:
`4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303`

Derived HOG SHA256:
`b3c793340be3cfbc9f1281a8f55d378281da05576044cbaef9870e84cc69756b`

Derived descriptor SHA256:
`322b0be65efef22c34ba0f82398dbaef50a0455f2efdb7cb9e691e11c8fff293`

Original source bytes are checked before and after the native run. The derived
fixture replaces an exit action; it is deliberately not a playable-campaign
qualification fixture

## Android evidence

Runner: `android/tests/test_xl_wall_teleports_runtime.ps1`

The first normal-APK emulator-5554 run passed 36/36 steps (run
`b0e240089b834e6684c0981960c01ad6`): ordinary mission selection, actual engine
trigger dispatch, destination range, refusal after one-shot use, quick save/load,
stock transition and restoration back into the source mission. The runner backs
up and verifies restoration of touched app-private state

The final APK after generic source-state and required-asset checks passed 36/36
again, run `f99312ddb7364f1eaac31c8ff918c8e1`. APK SHA256:
`d60c0f73fab5e47e6a9033f90157c5f5154367466ffb20fdbe06b86b9e8bc5e0`

The final native selection passed all ten tests: inventory, teleport dispatch and
reference comparison, Neptune inventory/layout/features/color precedence,
Observatory inventory/descriptor and modern water/no-damage controls. Both
automation catalog checks passed, and scoped formatting/lint passed

Emulator 5556 is not used. Native math checks do not establish exact
Android numerical parity; the Android script verifies the listed runtime outcomes

## Limits

No autonomous navigation or uninterrupted playthrough is claimed. Optional XL
warp graphics are outside this tranche; the stock appearance effect and
secret-exit sound are used. Sound is dispatched through the engine, but this is
not a listening comparison. Interactions with unsupported boosts, object events,
delays and extended flags remain blocked rather than silently approximated

The numerical comparison uses default XL math format 2. Alternate math modes,
classic demo recording/playback and guidebot route planning through teleport
links are not qualified by these controls
