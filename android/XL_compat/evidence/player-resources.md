# Shield-damage and energy-drain wall actions

XL type-17 and type-18 wall actions now use a separate resource implementation in
the shared binary. Redux keeps its ordinary dispatch. Admission remains based on
detected requirements, with direct object resource actions and independent
mission/resource requirements still gated.

## Semantics and limits

Reference: arbruijn/d2x-xl 2576a98b5de310e3ac7c9777df2677d1627b8bae.
The implementation follows main/trigger.cpp and CShipEnergy's Set/Update for the
default D2 ship profile:

- Shields subtract fixed units, after the reference float conversion. Energy
  subtracts a percentage of current energy. Negative values replenish
- Nonzero changes clamp the result to raw -1 through 200 fixed units. A zero
  integer delta leaves the current value untouched, including an over-cap value
- Exactly zero shields remains alive. Negative shields starts the engine's
  ordinary death sequence. Already-dead players skip resource changes while
  action flags still advance
- These are direct authored changes, so combat invulnerability and protected
  segments do not suppress them. They do not call combat damage or invent its
  palette/audio response. The native object shield mirror follows the changed
  player value, with ordinary death-camera initialization retained
- Serialized links are inert and retained; they are not treated as geometry.
  Existing master, delay, one-shot, permanent and byte-flag handling applies.
  Alternate leaves these two action types unchanged

The default profile uses scale 1 and a 200-unit cap. Optional XL ship scaling,
automatic recharge and multiplayer are not enabled or qualified by this tranche.
Direct object resource actions remain unsupported: the reference indexes the
geometry-trigger table with an object-trigger index. Existing immediate object
masters dispatch the actual linked geometry leaf and its own value; the native
control verifies this explicitly rather than reproducing that cross-table bug.
Delayed object descendants retain their separate lifecycle gate.

The port uses a wide intermediate and final clamp for extreme inputs where the
reference's float-to-fix narrowing or signed addition is undefined. This is a
defined arithmetic extension, not a reference comparison at those undefined
bounds. No new value-based admission exception is introduced.

## Source and native controls

tests/reference_player_resources.cmake hash-pins main/trigger.cpp,
objects/player.cpp and include/player.h. The two player files have SHA256:

- bab2ca407a6ac51993fb1194b785caa8101fe0a8a5896bd8a010cf68e07c09e5
- 4e134411347c2dee70f22c7b60c63717390496022964a77c1eb8dc5b5ff15cf4

It compiles the actual resource switch cases, CShipEnergy::Set and inline Update
against default-cap/scaling, storage, recharge-timer and death adapters. All 840
defined-range comparisons pass with MSVC and warning-free Linux GCC, including
the original authored values, fractional resources, zero deltas, caps, negative
values, float precision boundaries and dead players. Ten separate extreme-value
controls check the defined arithmetic extension. This is a leaf/state comparison,
not a full reference-engine run or optional recharge qualification.

The asset-free inventory contract checks all three format profiles, raw values
through INT32_MIN/MAX, inert negative targets, browser status and retained direct
object rejection. The actual-loader xl_player_resources control verifies real
wall activation, safe-volume/invulnerability bypass, wall and nonplayer immediate
masters, replenishment, flags, inert target persistence, exact delayed execution
after disk restoration through stock, zero/negative shield boundaries and dead
player handling. All 24 focused native regressions pass in 127.70 seconds. All
12 private-policy checks and both catalogs pass. The private original receipt
requires both new resource tests.

## Derived fixture and Android acceptance

The resource fixture derives from the existing trigger-flag Neptune fixture. It
replaces eight wall actions, adds a protected volume and disables the ten inherited
object controls. It is not original Neptune. The supplied Dinter survey contains
three shield actions (3 and 201 units) and one 5-percent energy action; those
original requirements motivated this chunk, but Dinter retains other blockers.

Fixture HOG SHA256:
683ec40c043d113f5c5983482f3f22f37bfc2d3cd9893d3e34a7f5fe66be856e

Descriptor SHA256:
96a8e2da8a41be5a24c989974fc8bb72b464b616929d4b41a64e651c322ea17c

The normal Android APK passes all 62 steps on emulator-5554: authored changes,
safe-volume combat rejection followed by direct action damage, percentage drain,
replenishment/cap, flags, delayed execution, two asynchronous checkpoint
roundtrips, a different-difficulty stock transition, disk restore, lethal
continuation and dead-player resource preservation. Native checks additionally
cover invulnerability, nonplayer immediate masters and the exact zero boundary.
Protected player/save/configuration and matching mission files have identical
before/after hashes. The app is force-stopped after restoring them. Controlled
poses are used; there is no autonomous navigation.

Run ID: 2fe836ea929b4f61828472b8aa9e8b93, elapsed 117364 ms.
Acceptance APK SHA256:
47238b5a2027631b369a5eac3f51319a5ba930e8cf4bfade2015aa66c881b3e1

Reproduce with android/tests/test_xl_player_resources_runtime.ps1 and explicit
-Serial, -Hog and -Descriptor. It owns the matching JSON script. Catalogs contain
396 top-level entries and 451 support scripts. Logs are under
temp/xl-player-resources-20261027; Android receipts and protected-file hashes are
under temp/xl-player-resources-android. No new original mission or full campaign
qualification is claimed.
