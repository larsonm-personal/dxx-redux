# Action-specific XL trigger flags

Implemented wall and object actions retain all eight serialized flag bits.
Admission still checks action, target, owner, layout, resource and capacity
requirements independently. Object flag words above 255 remain rejected. This
does not qualify another original mission.

## Semantics

The pinned reference is arbruijn/d2x-xl
2576a98b5de310e3ac7c9777df2677d1627b8bae. In main/trigger.cpp, TF_SET_ORIENT
(32) is consumed only by DoSpeedBoost. That action remains unsupported; this bit
is inert for admitted actions, including wall and object teleports.

TF_SILENT (64) is not a global mute:

- Opening a forcefield suppresses SOUND_FORCEFIELD_OFF when set. Wall mutation
  and removal of both linked hums still happen. Closing a forcefield ignores
  this bit and creates the normal hum
- Illusion-off emits its explicit SOUND_WALL_REMOVED cue only when the bit is
  set. This inverted condition is intentional source behavior. Illusion-on has
  no corresponding cue. HUD suppression still uses TF_NO_MESSAGE (1)
- Other implemented actions ignore bit 64

The separate XL illusion handler changes the front and connected back wall,
retains stuck-object cleanup, and checks missing walls and exterior children.
Redux continues through its classic handler when XL is inactive. The pinned
CSegment::IllusionOff incorrectly looks up the back wall using the front side
number; the XL handler intentionally corrects this. The source oracle records
the discrepancy and the native test checks an actual differing-side pair. This
is not a claim of byte-for-byte reproduction of that bug or full wall parity.

## Source and native evidence

tests/reference_trigger_flags.cmake checks six working-tree source hashes and
compiles actual DoChangeWalls, DoIllusionOff, DoIllusionOn and segment illusion
methods. Its recorded engine/audio adapters compare 6,400 leaf cases across all
256 byte flag combinations, three wall actions, changed/unchanged/front/back
states, absent walls, duplicate links and non-forcefield materials. Cue policy,
forcefield mutations and hum-removal requests agree. MSVC and warning-free
Linux GCC pass. The dynamic-light branch, renderer, actual mixer and collision
mathematics are not compared by this adapter.

The actual-loader xl_trigger_flags control covers 0/32/64/96 action dispatch,
paired forcefield state and linked hum cleanup, unchanged-wall silence, both
illusion sides, one-shot and alternate behavior, a pending action restored
through stock at its exact remaining deadline, and actual lethal robot damage
and removal with owned byte flags. Missing-back and exterior-wall cases are
bounded. The inactive XL hooks explicitly return to Redux behavior. The existing
inventory contract preserves new bits and rejects wider object flag words.
The final strengthened control also checks identical teleport orientation,
velocity and simulation RNG for all four combinations, and verifies closing
forcefields retains hum even with bit 64. Both changed tests pass again after
the full regression sweep.

All 21 focused native tests pass in 131.01 seconds, including teleport, chain,
scheduler, object, generator, equipment, boss, effect and stock-transition
controls. All 12 private-policy tests and both automation catalogs pass. The
private original runner now requires xl_trigger_flags and
xl_trigger_flag_reference rather than allowing missing comparisons.

## Derived fixture and Android acceptance

The trigflag fixture derives from unchanged Neptune HOG
4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303 and the existing
object-event fixture writer. It has eight wall actions, a retail forcefield
material on wall pair 0/1, illusion pair 2/3, and one enabled robot-removal action.
The nine other owner records are disabled controls. It is not original Neptune.

HOG SHA256:
6f2adcc1834786068db52703c6ef9d842b5f7cc60797d706937b2b470daa489b

Descriptor SHA256:
f11d50656e8181b8bf104d306cebc0ab13546cf1ba2ff9bae988aabeb4711bc5

Android passes all 66 steps on emulator-5554. There is one actual forcefield-off
mixer attempt and two suppressed decisions; four actual illusion-off attempts and
one suppressed decision; and three started/stopped forcefield hums. All have zero
mixer failures. Three illusion cues completed and one was stopped by lifecycle
work. These are playback/lifecycle controls, not waveform comparisons. Both
asynchronous checkpoint roundtrips, a different-difficulty stock transition,
ordinary disk restoration and continued actions pass. Protected file hashes
match before and after. The app is force-stopped after restoration.

Run ID: 740c8ee3940c4025978b60971c0625d0, elapsed 121866 ms.
Acceptance APK SHA256:
200c2a82c844d8fb5a33a9d20306de185e87f178246a14e1e6a730527887b6d9

The reusable runner is
android/tests/test_xl_trigger_flags_runtime.ps1 with explicit -Serial, -Hog and
-Descriptor. It owns test_xl_trigger_flags_runtime.jsonc, backs up and restores
protected player/save/configuration and matching mission files, and records
actual mixer counters separately from cue-decision diagnostics. Controlled poses
are used throughout; no autonomous navigation is attempted.

Native/source logs are under temp/xl-trigger-flags-20261027. Android receipts
and protected-file comparisons are under temp/xl-trigger-flags-android.

Boost orientation, delayed object-master descendants, other actions/owners,
multiplayer, arbitrary wall/material behavior, full waveform equivalence and
original campaign qualification remain outside this tranche.
