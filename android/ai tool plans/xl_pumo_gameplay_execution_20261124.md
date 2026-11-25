# Pumo and D2X-XL gameplay execution, November 24, 2026

The original Pumo v1.1 episodes both have connected ordinary-input Windows
runs. Mineral Valley ran levels 1 through 9 and reached the level 10 farewell.
The optional route went from level 4 through Coldrom, level 14, and returned to
level 5. Kartsal ran 1, 11, 12 and 13. This session followed the reported phone
issues, measured new compatibility gains on the same Windows distributions,
and compared a matched Android scene with the local D2X-XL install.

The personal fork was fetched. Work continued from the verified evening stack
tip `67d743590` after inspecting 34 worktrees and the open fork PRs. Stack #746
starts at `cmake`.

## Pumo phone feedback and loading

Both copies of the playtest report came from Play internal 31950 on an
SM-S931U with Android SDK 37, Rookie difficulty and touch. The preserved log
identifies Crauphel Scilab,
Mineral Valley level 2, with room 129 the last sampled location. The player
reported an exit after firing a missile. The log recorded a fatal projected
lightning extent. The exact missile, aim landmark and save/load outcome remain
unknown. The player also reported hard-to-read green text on a briefing page.
The page remains unidentified. [Current phone directions](../XL_compat/PUMO_PLAYTEST.md)

On the physical Galaxy S21 SM-G996U with Android 15, ordinary kit 32333 passed
35/35 scripted Rookie checks with the original archive and eight raw addons.
It fired a Concussion near room 129 and repeated the shot after quickload.
The protected profile was restored. The same ordinary kit reached **Thanks
for playing this first episode** in 10/10 steps from `mineral-ending` and
**MD Vertex9** in 14/14 steps from `kartsal-final-checkpoint`. It includes 11
verified earned checkpoints.
The fixed camera, scripted inputs and checkpoint starts establish those
specific actions. The original human touch approach and shot are still open.
[Ordinary S21 recipe and results](../XL_compat/evidence/pumo-s21-ordinary-32333-20261124.json)

Earlier protected S21 runs verified that the native loading bar stayed visible
during level music startup with **Show progress bars** enabled. The ordinary
32325 recording showed moving bars across all four observed transitions and
played the original mission Ogg tracks with **Mission zip** selected. The
reporting build played the base D2 MIDI tracks with that selection.
[Ordinary S21 mission music and progress evidence](../XL_compat/evidence/pumo-s21-mission-ogg-overlap-20261123.json)

On S21 diagnostic build 32332,
prebriefing time went from 7.14 to 5.88 seconds when advancing to Crauphel.
Audio preparation fell from 2.24 to 1.00 seconds. The cold hub still took 6.91
seconds before its briefing. Crauphel quickload audio took 1.91 seconds, down
from 2.21 seconds. The scoped-image phase took 3.34 seconds during the warm
transition and became the next target. Windows checked byte-identical native
WAV sources and mixer output across all 14 original levels at 44.1 and 48 kHz.
[Source and timing comparison](../XL_compat/evidence/pumo-s21-audio-preparation-20261124.json)

The Android 14 emulator comparison of diagnostic APKs 32332 and 32335
completed the same 35-step archive route on both. The hub-to-Crauphel
transition reused 19 of 144 decoded images after rereading and verifying
their sources. The image phase measured 1.82 seconds before and 1.90 after,
and the total prebriefing interval measured 4.10 and 3.63 seconds. Quickload
rebuilt all 144 images. These are single timing observations under the same
recipe. An earlier 32332 attempt lost ADB during the transition, leaving the
in-guest restore unverified. The retry used the same APK and passed with
before and after profile hashes. The failed receipt and original host copy
were kept. [Scoped texture reuse](../XL_compat/evidence/pumo-scoped-texture-reuse-20261124.json)

Diagnostic build 32337 held the decoded texture owner across Android save
preflight. The same protected 35-step route passed, including firing again
after quickload. Crauphel reused 144/144 images after rereading their source
bytes and verifying identity and dimensions. Its image phase took 1.06
seconds, down from 1.54 on build 32335. Request to active flight took 7.40
seconds, down from 8.24. One run per APK established these timings. The
cold hub still decodes its 80 images from source.
[Android quickload receipt](../XL_compat/evidence/pumo-quickload-texture-owner-20261124.json)

The S21 captured the candidate green page beginning **And there it is!** with
the original closing artwork. Ordinary 32328 draws a script-colored outline
around XL briefing text. The current kit includes that behavior. Capture the
first words or a screenshot if low contrast returns. [Candidate page](../XL_compat/evidence/pumo-briefing-green-candidate-20261123.json)

## Windows compatibility

The fixed catalog has 1,654 starts, split between 543 local and 1,111 pinned
distribution entries. Each readiness run uses the advertised ordinal, 120
unassisted frames, a 60 Hz tick rate and seed 1. The original measurement was
1,534/1,654 and the evening stack reached 1,563/1,654. The Anthology repeat
reached 1,567/1,654 with all four original OOF witnesses gained. The copied
Windows executable initially lacked its runtime DLL search path. The failed
batch was retained and the identical executable and case plan completed after
the runtime path was corrected. [Anthology full repeat](../XL_compat/evidence/gameplay-census-anthology-20261124.json)

- Anthology now starts the three split levels and the standalone level. All
  seven replacement models matched their running original registrations, with
  22 muzzle slots and 66 fixed-point components checked. The work qualified
  exact texture chunk bounds, authored muzzle vertices, weapon batteries and
  static submodel properties. [Anthology cluster](../XL_compat/evidence/gameplay-anthology-oof-20261124.json)
- The two Glass House distributions now pass their colored-wall starts. The
  running original supplied palette, opacity and doorway results for both and
  the older Lunar Outpost layout. After the wall change Lunar Outpost reached
  object trigger 0, type 16. A native doorway control tested collision,
  visibility, transparent rendering and save/load. [Colored walls](../XL_compat/evidence/gameplay-colored-walls-20261124.json)
- Seven of eight team-room and repair-center witnesses now pass. The original
  ran all eight. Three 23/40 pinned multiplayer distributions and four legacy
  distributions started in solo readiness mode. Prophecy ordinal 3 advanced
  past repair centers 691 through 693 to its lightning effect at object 186.
  The authored signed groups and team ownership are preserved in the loaded
  data. [Team rooms and repair centers](../XL_compat/evidence/gameplay-legacy-team-rooms-20261124.json)
- Lunar Outpost reached 120 unassisted Windows ticks with the original's two
  stationary CAMBOT bindings. Its first blocker after camera admission was an
  authored 16-frame POG strip referenced by 11 static colored walls. Keeping
  the alias and excluding those walls from animation clocks resolved it.
  Both Glass House distributions and Warp Shock ordinal 6 still start.
  [Lunar Outpost and its controls](../XL_compat/evidence/gameplay-lunar-outpost-cameras-20261124.json)
- Boiling Point reached 21/21 starts across the local archive and both pinned
  distributions. The pinned 23/40 and local 27/40 copies of **Control** each
  have seven unreachable light indices. Six fall outside valid face geometry.
  The remaining one is on an untextured reciprocal portal with neither face
  targeted. Redux keeps the raw light delta table and rejects out-of-range
  slices for live sources. Alien Frontier ordinal 1 exercises that rejection.
  [Boiling Point and live-source control](../XL_compat/evidence/gameplay-boiling-point-light-indices-20261124.json)
- Prophecy ordinal 3 now starts and reaches 120 unassisted ticks. The original
  kept both zero-lifetime lightning owners but created no emitters even with
  lightning enabled. In the same control Steel created two emitters and
  Amalthea created 85. Redux preserves the dormant owners and their eligibility
  as targets. Seven of eight Prophecy starts pass, and the first level reaches
  the existing experimental fluid gate. Amalthea advances past lightning
  object 1129 to its robot generator capacity and negative flicker delay.
  The two Speed distributions still lead with unsupported waypoint controllers
  and also require live lightning work. Both Steel distributions retain their
  earlier active owner rejection. The native fixed-target lifecycle passed
  creation, activation and save/restore checks.
  [Prophecy and live lightning controls](../XL_compat/evidence/gameplay-prophecy-dormant-lightning-20261124.json)
- The local 19/40 Pyromaniac-D2 distribution now reaches 120 unassisted
  Windows ticks. The original preserved the reserved bits on all 245 segments,
  including 48 with nonzero reserved bits. The pinned ambient oracle passed
  all 256 flag bytes. The 16-case Windows repeat included both Pyromaniac-D2
  distributions, both Prophecy distributions and Sclair. Fifteen starts
  passed and the first local Prophecy level stopped at the fluid gate. The
  first original Pyromaniac attempt stopped at an optional-data warning.
  The same engine and archive ran using the established windowed configuration.
  [Pyromaniac-D2 flag and start receipts](../XL_compat/evidence/gameplay-pmanic-d2-segment-flags-20261124.json)
- A read-only Alien Frontier audit covered all 676 indices across the 15
  pinned levels. Fourteen levels contain overruns. The encoded unsigned
  interpretation finds 331, and the original reader's signed-byte behavior
  finds 332. All 332 reference overruns are on textured faces. Four are
  targeted by authored light actions in ordinal 9, `lakshmid.rl2`. Treating
  the bytes as a classic count would make those four fit, while breaking 59
  rows in the currently valid ordinal 6. All 15 ordinals reached 120
  unassisted Hotshot iterations in the original. The same Windows census
  started ordinal 6 and rejected the other 14 packed tables. The next
  witness is light-off trigger 18 on segment 691, side 2 of ordinal 9.
  [Alien Frontier index and action audit](../XL_compat/evidence/gameplay-alien-frontier-light-index-audit-20261124.json)

## Android and D2X-XL presentation

The reference is the local D2X-XL install running the pinned source at
`2576a98b5de310e3ac7c9777df2677d1627b8bae`. The fixed Crauphel scene is level 2,
room 105. Both PMC signs use the same nine authored phases of `misc067#0.tga`.
All 18 texture fingerprints matched at the two qualified resolutions, and the
S21 saw all nine live phases in a 147-step protected diagnostic.
[Texture and live S21 phase evidence](../XL_compat/evidence/pumo-crauphel-reference-presentation-20261124.json)

The second capture matched fixed position, orientation, a 640 x 480 world
viewport and trilinear filtering. An early emulator attempt timed out at the
graphics confirmation and reverted its filter. The next attempt completed
158 steps after accepting the graphics change. Numeric gamma 0 was also an
unequal control. D2X-XL displays it at 25% brightness and Redux uses it as
neutral. The final reference uses gamma 3, which is 100% in D2X-XL. The match
put the surrounding walls and ceiling below one RGB value of mean absolute
error per channel. The sign regions alone remained about 57 red, 50 green and
33 blue values brighter on Android. The reticle and HUD use the Android
capture path and are outside those scene measurements.

The reference default and an explicitly disabled control had wall coronas off.
Enabling them in the original illuminated both PMC signs. Suppressing only
Redux's wall glow in a protected 158-step emulator diagnostic reduced the
median crop error from 18.55, 16.45 and 10.58 to 0.52, 0.94 and 0.44 RGB values
over all nine matched phases. The optional enabled original had a stronger
and wider glow in the same scene. [Matched lighting controls](../XL_compat/evidence/pumo-crauphel-lighting-comparison-20261124.json)

Build 32338 tested the new **XL Wall Light Glow** checkbox in the game menu.
The default run passed 42/42 steps, saved the unchecked preference and drew
zero wall coronas. The enabled run passed 44/44 steps, saved the checked
preference and drew the glow. Both runs restored the protected profile. The
captured default frame differed by at most 2.74 RGB values per channel across
the signs from the closest original phase. The first menu test used the wrong
array selector and stopped at the checkbox assertion. The same APK passed
after the test used the supported selector. [Wall glow menu and scene](../XL_compat/evidence/pumo-crauphel-wall-glow-menu-20261124.json)

The original Konflict, Neptune and base D2X-XL corona checks passed 36/36,
68/68 and 31/31 steps with glow enabled. The local runner first sent its game
automation before the launcher handshake. The launcher-driven retry reached
the engine and exposed Neptune's obsolete negative control, which expected a
multiplayer-only object to reject a solo start. The fixture now tests an
unsupported object type. It checks that rejection before writing the control
archive. The corrected Android run verified that rejection and completed the
original Neptune start, rendering, save/restore and stock transition.
Both local harness failures were preserved.

The enabled wall-glow comparison then used the original mission glare image,
the wall's four corners and the source's ten-unit depth fade. Diagnostic
32340 passed 171 steps on the Android 14 emulator and captured all nine
phases. The median corridor error against the enabled original fell from
13.20, 12.29 and 8.80 to 1.49, 1.83 and 1.08 RGB values. Both sign regions
were within 1.46 mean RGB values per channel across the phase medians.
The surrounding glow still differs by up to 8.96 RGB values in the narrow
regions beside the signs. An initial emulator attempt lost ADB before frame
capture. Its protected profile copy was verified before the read-only guest
closed. The same diagnostic APK passed in a fresh guest.
[Authored wall glare comparison](../XL_compat/evidence/pumo-crauphel-authored-wall-glare-20261124.json)

The final runtime APK, 32341, passed 42/42 steps with glow disabled and 44/44
with it enabled. The disabled capture matched the earlier default frame
hash. Both checks saved the settings and restored the protected profile and
guest display settings. An ADB loss during the first enabled check happened
before the menu test. The same APK passed on a fresh read-only guest, with
500 textured draws. The S21 was locked for these later diagnostic runs, so
the paired measurements used Android 14 on the isolated emulator.

## Full Windows census

The repeat through source `4d3ec26f349cdbf7322ea6e1159cf1fa35c4ceda`
completed in 4,093 seconds. Windows started 522/543 local and 1,054/1,111
pinned cases, for 1,576/1,654. The two Glass House and seven team-room cases
account for the nine gains from 1,567. This repeat had no losses. Seventy-eight
cases remain in that snapshot. The running original was observed in 42 of
those cases. Thirty-one were rejected at mission selection, three had empty
advertised-member lookup and two lack verified simulation.
[Full Windows census and remaining inventory](../XL_compat/evidence/gameplay-census-team-rooms-20261124.json)

The full repeat through Lunar Outpost and Boiling Point used source
`1bef0480fbf7ba2b7832cf45a5922a8866fdd5ca` and completed in 3,601 seconds.
Windows started 523/543 local and 1,056/1,111 pinned cases, for **1,579/1,654**.
Lunar Outpost and both copies of Boiling Point ordinal 2 are the three gains
from 1,576. No previously passing case was lost. This is 45 additional starts
over the original 1,534/1,654. The subsequent #758 commit revises the host
test's original-data expectations. Of the 75 remaining cases, 39 ran in the pinned original,
31 were rejected by the original mission loader, three had an empty original
member lookup and two have unverified original simulation.
[Full cameras and light-index census](../XL_compat/evidence/gameplay-census-cameras-and-lights-20261124.json)

The latest complete census ran the zero-lifetime lightning change at source
`1853bbbea5f79bc4c3513a64d6ddf3f309c1ad90` in 3,634 seconds. It recorded
524/543 local starts and 1,056/1,111 pinned starts, for **1,580/1,654**.
Prophecy ordinal 3 was the only gain from 1,579. All previously passing cases
passed again. The production source matches the published Prophecy layer
after the intervening Neptune fixture rebase. Of the 74 remaining cases,
38 ran in the original, 31 were rejected at mission selection, three had an
empty advertised-member lookup and two have unverified original simulation.
[Complete Prophecy census](../XL_compat/evidence/gameplay-census-prophecy-lightning-20261124.json)

The later Pyromaniac-D2 cluster resolved one of those 74 entries. The tracked
remainder is 73, including 37 with observed starts in the original. The flag
layer has a 16-case comparison, and the Prophecy snapshot is the latest full
census. [Later targeted result](../XL_compat/evidence/gameplay-pmanic-d2-segment-flags-20261124.json)

## Alien Frontier light layout, later November 24 session

The personal fork was fetched again. The local `cmake` tip was
`2fe3b82ab`; all prior stack PRs through #763 had merged, and there were no
open fork PRs. The #763 public headed replay admission and Linux tests passed
on its final head. The new branch starts from that `cmake` tip. Workspace
cleanup was previewed and run through `android/clean-workspace.ps1` with the
active runs and authentic assets excluded, restoring more than 50 GiB free.

The pinned Alien Frontier HOG was unchanged. Its fifteen 15/34 members have
676 light indices. Fourteen members divide their delta tables exactly once
when the third record byte is read as the count; the fifteenth, Alarlof,
has a valid packed table and fails the separate-byte test. Redux requires
valid faces, nonzero bounded ranges, exact coverage and no overlaps before
using the alternate interpretation. It continues to reject malformed
tables that fail those conditions.

Pinned D2X-XL with read-only light-index logging started all fifteen
unchanged ordinals at Hotshot for 120 unassisted frames. It did not reject
the live out-of-range source during those idle starts. Pre-fix Redux with
the same unchanged HOG started only ordinal 6; a diagnostic reencoding of
608 index rows admitted the other fourteen in that older runtime. A separate
bounds-checked original action copy kept
lighting method 0 and dispatched ordinal 9 light-off trigger 18 from
segment 691, side 2. It changed face lighting, but the packed source at
segment 692, side 1 declared 5,696 deltas from offset 2,991 in a 3,860
entry table; only 869 fit. The five targeted packed sources declared
18,944 entries and the bounded audit applied 2,798. The complete byte-count
diagnostic applied all 592 declared deltas and also changed face lighting.
No unbounded original traversal was attempted.

Redux loaded and ticked all fifteen advertised ordinals at the same Hotshot
input, 120 frames, 60 Hz and seed 1. The native original-data check loaded
ordinal 9 with 57 sources and 3,860 deltas, then trigger 18 darkened the
five source masks and trigger 16 restored them. No new blocker appeared
within those 120 frames; full authored traversal remains unmeasured.
The Android diagnostic version 32530 loaded the same unchanged ordinal 9
member as a derived single-level mission and captured its opening chamber
on emulator-5588. The matching local D2X-XL windowed capture shows the same
central rock, side panels and lower blue object. This is presentation at
spawn, not a gameplay parity claim. The images and raw runs stay local.
[Byte-count audit and bounded action receipt](../XL_compat/evidence/gameplay-alien-frontier-byte-count-20261124.json).

The focused source comparator, inventory contract and native 15/34 runtime
checks passed 3/3 after formatting. The Windows D2 build and the isolated
Android diagnostic APK built. The full exact-source Windows census is running
to replace the previous 1,580/1,654 snapshot; the targeted Pyromaniac and
Alien Frontier gains imply 1,595/1,654 and 59 remaining if all prior passes
hold. Of that tracked remainder, 23 had observed starts in the pinned
original. These are provisional denominators until the complete run ends.

| First blocker after the two targeted gains | Tracked cases out of 59 |
| ------------------------------------------ | ----------------------: |
| Advertised level file missing              |                      34 |
| Robot fire killed the idle pilot           |                       4 |
| Fluid behavior                             |                       4 |
| Non-hexahedral geometry                    |                       4 |
| Unsupported level format                   |                       3 |
| Custom base definitions                    |                       2 |
| Robot definitions                          |                       2 |
| Waypoint controllers                       |                       2 |
| Lightning effects                          |                       2 |
| Robot generator capacity                   |                       1 |
| Timeout                                    |                       1 |

The 59 comprise 31 original mission rejections, three empty advertised-member
lookups, 23 observed original starts and two original simulations not verified.

The S21 ordinary 32333 kit remains installed on the SM-G996U with Android 15.
The user is not available to perform manual testing now. The original Rookie
touch missile, aim landmark and saved-approach repeat therefore remain open,
as do the three human-visible loading intervals. Earlier fixed-camera and
scripted results above retain their distinct scope. The original issue save
was not touched in this session.

The Redux readiness harness starts at Hotshot. The first reference runs
inherited their runner's Rookie default. All 15 were repeated at Hotshot
with no steering or firing and reached
120 rendered iterations at the specified ordinal. The inspected OOF muzzle
and model records, colored walls and team-room records matched the earlier
captures in all 15. The two engine harnesses record their own clocks.
[Exact Hotshot reference repeat](../XL_compat/evidence/gameplay-reference-hotshot-20261124.json)

| First blocker                    | Full census, out of 74 | After Pyromaniac-D2 repeat, out of 73 |
| -------------------------------- | ---------------------: | ------------------------------------: |
| Advertised level file missing    |                     34 |                                    34 |
| Light index bounds               |                     14 |                                    14 |
| Lightning effects                |                      2 |                                     2 |
| Fluid behavior                   |                      4 |                                     4 |
| Non-hexahedral geometry          |                      4 |                                     4 |
| Robot fire killed the idle pilot |                      4 |                                     4 |
| Unsupported level format         |                      3 |                                     3 |
| Custom base definitions          |                      2 |                                     2 |
| Robot definitions                |                      2 |                                     2 |
| Waypoint controllers             |                      2 |                                     2 |
| Robot generator capacity         |                      1 |                                     1 |
| Segment flags                    |                      1 |                                     0 |
| Timeout                          |                      1 |                                     1 |

## Fork review stack

The prior ready-for-review PRs targeted `larsonm-personal/dxx-redux`, each
based on its immediate predecessor in [stack #746](https://github.com/larsonm-personal/dxx-redux/stack/746).
All through #763 have since merged into `cmake`; the current Alien Frontier
layer begins a new review branch on that tip.

The new source layer is [#764](https://github.com/larsonm-personal/dxx-redux/pull/764)
against `cmake`; this dated report and handoff is
[#765](https://github.com/larsonm-personal/dxx-redux/pull/765) against #764's
branch. Both are ready for review in
[stack #766](https://github.com/larsonm-personal/dxx-redux/stack/766).
Appending #764 to the fully merged #746 stack failed because its deleted
top branch could not serve as #764's base. The new stack keeps the required
predecessor order without resurrecting a merged branch.

| Layer                                           | Fork PR                                                        | Verified head                              | Passed | Skipped |
| ----------------------------------------------- | -------------------------------------------------------------- | ------------------------------------------ | -----: | ------: |
| Pumo resource and audio wait                    | [#751](https://github.com/larsonm-personal/dxx-redux/pull/751) | `86534a4e8081b81f9d5d2f0da9a084ee771085d6` |     34 |       9 |
| Anthology OOF model owners                      | [#752](https://github.com/larsonm-personal/dxx-redux/pull/752) | `7209dd60724a4752eb065811682e35aaaef6cc39` |     37 |       6 |
| Ordinary S21 kit and reference phases           | [#753](https://github.com/larsonm-personal/dxx-redux/pull/753) | `c0f857eb20eb49e3ec51937926843081f598635f` |     37 |       6 |
| Original colored walls                          | [#754](https://github.com/larsonm-personal/dxx-redux/pull/754) | `d878633fcc10c5f0f3b28f3467c70c7ee997d147` |     34 |       8 |
| Team rooms and repair centers                   | [#755](https://github.com/larsonm-personal/dxx-redux/pull/755) | `b5e429ad52d4999f430392efad224bf74417493f` |     37 |       6 |
| Verified decoded texture reuse                  | [#756](https://github.com/larsonm-personal/dxx-redux/pull/756) | `2f93568d4fb7b515777f7e450790e284dfe6b462` |     37 |       6 |
| Lunar Outpost cameras and wall strips           | [#757](https://github.com/larsonm-personal/dxx-redux/pull/757) | `ef91c47ab7cdab49f45d48dcceb49f437ec3babc` |     36 |       7 |
| Boiling Point dormant light indices             | [#758](https://github.com/larsonm-personal/dxx-redux/pull/758) | `0d5485863776fca972c07505575d6461f9d2e2c0` |     37 |       6 |
| Verified Android quickload textures             | [#759](https://github.com/larsonm-personal/dxx-redux/pull/759) | `20658fbf7cc20a30fb19f0be6ecdac37b7cb3a11` |     37 |       6 |
| XL wall glow setting and original visual checks | [#760](https://github.com/larsonm-personal/dxx-redux/pull/760) | `eb2aa07e4ac000b265197c76e62c10f282d70b67` |     36 |       7 |

The selected host, Android and JVM receipts show executed test rows on each
listed head. Each trusted XL qualification started all eight sample levels
and all 64 expanded levels. Where admitted, the public replay qualification
ran 34 headless and 34 headed native runs. The separately required imported
D1 flight gameplay and simulation RNG comparison passed. The broader paired
report has four optional cases outside that qualification.

On #751, public replay and the Rust server job were skipped by the stack
admission decision. On #754, the routine script matrix and server job were
skipped by the same `native non-tip or unrelated changes` decision. The
workflow syntax and automation catalog jobs still ran. The #757, #760 and
#762 server jobs recorded the same admission message. Common skips reflect
manual retained-receipt inspection, supplemental pinned Pumo staging,
special Release jobs, scheduled progress export and LAN timing export. The
first #752 Windows developer build failed later than the local release
build. The actor-aware muzzle helper still had fixture calls with numeric
model arguments. The exact head above includes the corrected fixture calls
and passed its public and trusted runs.

The first #758 trusted run reached the modern original probes and failed at
the old requirement that `control.rl2` be rejected. The same pinned archive
reproduced that assertion locally. The revised host test requires all seven
members to inspect, verifies Control's seven dormant indices, 128 retained
indices and 32,769 retained deltas, and runs the pinned source comparator on
the repaired level. The authentic archive comparison and all 56 Python
tests passed locally. [First trusted run](https://github.com/larsonm-personal/dxx-redux/actions/runs/45405705433)

The first #758 public workflow completed build, save integrity and replay
admission. The pull request's `cancel-in-progress` rule canceled its two
replay jobs when the corrected head started a new workflow. The canceled run
remains in the history. [First public run](https://github.com/larsonm-personal/dxx-redux/actions/runs/45405660931)

The corrected head ran the full public replay backends and the trusted
original test. The trusted host reached its original asset stage and reported
8/8 sample and 64/64 expanded starts. [Corrected public run](https://github.com/larsonm-personal/dxx-redux/actions/runs/45411644179), [corrected trusted run](https://github.com/larsonm-personal/dxx-redux/actions/runs/45411687173)

Quickload ran five hosted Linux tests, five Android tests and one JVM test.
Its trusted run passed 8/8 and 64/64 starts. Each public replay backend
passed 34/34 runs, with the required scoped D1 gameplay comparison passing
in the headed lane. [Public quickload run](https://github.com/larsonm-personal/dxx-redux/actions/runs/45417669326), [trusted quickload run](https://github.com/larsonm-personal/dxx-redux/actions/runs/45417713723)

The wall-glow setting ran five hosted Linux tests, five Android tests and one
JVM test. Its trusted suite passed 8/8 and 64/64. Its two public replay
backends passed 34/34 runs each and the required imported D1 gameplay
comparison passed. [Public wall-glow run](https://github.com/larsonm-personal/dxx-redux/actions/runs/45423909446), [trusted wall-glow run](https://github.com/larsonm-personal/dxx-redux/actions/runs/45423960207)

The Prophecy layer opened as [#761](https://github.com/larsonm-personal/dxx-redux/pull/761)
at `62bcd2beae10e9d1c506dc26fc579884cfe7ead3`. Five hosted Linux, five Android
and one JVM test ran and passed. The trusted suite passed 8/8 and 64/64.
[Public Prophecy run](https://github.com/larsonm-personal/dxx-redux/actions/runs/45429030649), [trusted Prophecy run](https://github.com/larsonm-personal/dxx-redux/actions/runs/45429067296)

The Pyromaniac-D2 layer opened as [#762](https://github.com/larsonm-personal/dxx-redux/pull/762)
at `013e1c8f97b68cb18391f86f8a40479fa20dff6a`. Five hosted Linux, five Android
and one JVM test ran and passed. Its trusted suite passed 8/8 and 64/64.
[Public Pyromaniac-D2 run](https://github.com/larsonm-personal/dxx-redux/actions/runs/45434119194), [trusted Pyromaniac-D2 run](https://github.com/larsonm-personal/dxx-redux/actions/runs/45434155321), [selected tests](https://github.com/larsonm-personal/dxx-redux/actions/runs/45434119290)
