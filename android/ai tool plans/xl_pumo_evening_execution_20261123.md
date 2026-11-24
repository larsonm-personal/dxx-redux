# Pumo and XL evening execution, November 23

Started from the personal fork's merged `cmake` at `b579df413`. Pull requests
#726 through #743 were already merged. The evening opened a new stack with
immediate-parent branches. Original game assets, profile data, phone logs,
recordings and saves stayed in ignored local work directories.

## Phone feedback and loading

The first unresolved action is the original human missile shot in Mineral
Valley level 2, Crauphel Scilab near room 129. The initial report used Play
internal build 31950 on SM-S931U with Android SDK 37, Rookie and touch. The
last sampled room was 129 before `XL lightning projected extent exceeds the
source integer range`. The aim landmark, missile type and save/load result
are still unknown. The protected S21 comparison used SM-G996U on Android 15
and diagnostic builds
32322 and 32323. Each passed 35/35 scripted steps on the original archive,
including a room-129 Concussion shot, quickload and the same shot again. The
ordinary 32322 S21 run in the earlier tranche adds a fixed-view touch double
tap after automation released input. The reporter's exact steering and aim
still need a fresh physical-phone check. Keep the original issue save intact.

The earlier 32322 phone recordings show moving progress bars from score
advance to briefing and from the last briefing page into flight, including
**STARTING LEVEL MUSIC**. The original page beginning **And there it is!**
is a candidate for the green-on-green report. It rendered on diagnostic build 32326 with background
`mv14b.pcx`, six-bit foreground `(15, 63, 16)`, and fitting text with zero
glyph overlaps. The reporter's page remains unidentified.
[Candidate briefing capture](../XL_compat/evidence/pumo-briefing-green-candidate-20261123.json).

Diagnostic 32327 reproduced the page on the S21 with a four-sided edge
using the script's shadow color. The original background and green glyph
color matched, and the page fit with zero glyph overlaps. The paired frames
gained 1,303 dark pixels in the green monitor region. The surrounding
artwork control matched exactly. The run passed 21/21 steps and restored
the phone profile. Ordinary kit 32328 carries the outline and the same 11
checkpoint digests.

On the protected S21, the scoped texture change cut the storage phase from
1.25 to 0.87 s in the hub and from 0.60 to 0.38 s in Crauphel. Crauphel
quickload storage fell from 0.59 to 0.38 s. Prebriefing resource work in
Crauphel took 7.77 and 7.55 s, respectively. The two observations of the
postbriefing level load were 5.84 and 5.88 s. These are paired stage
measurements from one route per build.
[Scoped storage evidence](../XL_compat/evidence/pumo-s21-scoped-storage-20261123.json).

A second protected pair compared diagnostic builds 32323 and 32324 with the
same original archive and route. Starting music in the main level-load path
fell from 2.55 s to 0.02 s in the hub and from 1.30 s to 0.02 s in Crauphel.
The full postbriefing native load shortened from 8.31 to 5.45 s in the hub
and from 5.59 to 4.66 s in Crauphel. The native load portion of quickload fell
from 5.99 to 4.60 s. The worker had completed each of the three decodes when
playback asked for it. Both runs loaded the same tracks and passed 35/35
steps. The phone's original stay-awake setting was read back as 0.

Final diagnostic build 32326 repeated all 35 steps with the fallback path
present. Its native load times were 5.56 s for the hub, 4.58 s for Crauphel
and 4.57 s for the load phase inside quickload. Texture caching still took
3.92 s and 3.29 s on the two fresh entries. The resource check before the
briefing is a separate wait. First import also expands a cache of about
1.56 GiB. Keep the three loading intervals separate in phone reports.
[Ogg overlap evidence](../XL_compat/evidence/pumo-s21-mission-ogg-overlap-20261123.json).

The exact ordinary 32325 APK passed 35/35 with the original loose HOG. A
first run held the default 1920 x 1080 game canvas and stopped at the 640 x
480 assertion. The repeat set the intended canvas in the temporary profile.
The six original files were restored after each run. The 32325 kit contains
all 11 earlier earned saves, each with its previous digest.
[Ordinary S21 receipt](../XL_compat/evidence/pumo-s21-ordinary-32325-20261123.json),
[ordinary kit](../../temp/xl-pumo-phone-s21-20261123-32325/README.md).

In a separate protected S21 run, the same ordinary APK imported the original
7z and eight raw addons through the launcher picker. It played the original
mission Ogg tracks and passed the same 35 steps. The native load phases were
6.06 s for the hub, 4.52 s for Crauphel and 4.82 s during quickload. The worker
had completed each track when playback asked for it. The six original files
matched after restoration.

The ordinary archive recording shows the amber bar moving during all four
sampled transitions, from difficulty to the hub briefing, from that briefing
to flight, from the hub score to the Crauphel briefing, and from that briefing
to flight. The prebriefing bar changes position and the native-load bar
changes width. The older capture shows the distinct music label.
The same 32325 recording measured 7.25 s before the hub briefing and 7.09 s
before Crauphel. Level resources and scoped resources accounted for 4.94 s
and 2.27 s in the hub, then 3.60 s and 3.46 s in Crauphel. Those prepared
resources were reused after each briefing. Native texture caching then took
4.17 s in the hub and 3.22 s in Crauphel.

Both authored endings rendered on the same physical S21 with ordinary 32325
after cold loading earned Trainee kit saves. `kartsal-final-checkpoint` crossed the
final trigger from level 13 room 4238 and reached **MD Vertex9** in 13/13
steps. `mineral-ending` resumed into the level 9 exit, waited for **Level 9
Complete** to accept input, and reached **Thanks for playing this first
episode** in the level 10 closing briefing in 10/10 steps. Both original
profiles were restored. These checks used scripted controls at the saved
approaches. The connected ordinary-input Windows routes remain separate.
[Ordinary S21 recording and ending receipts](../XL_compat/evidence/pumo-s21-ordinary-32325-20261123.json).

## Display restoration

A fullscreen Windows diagnostic temporarily put the laptop panel in 640 by 480. Closing the test restored the panel to 2880 by 1800 and the other
display remained 3840 by 2160. The checked-in rendered readiness runner now
uses a window. Before and after the original Clminate renderer runs, both
Windows mode values matched. The S21 and both existing Android emulators had
their physical modes and no `wm size` or `wm density` overrides. Future
display-changing tests should record the current values, restore the exact
original values in cleanup and verify them before reporting completion.

## Original Windows qualification

The exact six-level `levelpack.7z` Chaos Theory cluster moved from 4/6 to 5/6
with Standoff's stationary lightning and then to 6/6 with Clminate's smoke
and supported additive closed walls. Each qualified start used 120
unassisted frames at 60 Hz with seed 1. Standoff's native Lightning slots
survived a cold saved route. Clminate's 11 enabled smoke owners also survived
cold save/load and automated continuation. A separate OpenGL observation
using the pinned smoke add-on captured all 11 live emitters and matched the
two cleared-particle control frames. That observation qualifies the smoke
view. A dedicated additive wall render test and ordinary playthrough are
still open. The exact archive and member hashes are in the receipts.
[Lightning](../XL_compat/evidence/gameplay-levelpack-chaos-classic-lightning-20261123.json), [smoke](../XL_compat/evidence/gameplay-levelpack-chaos-classic-smoke-20261123.json).

The frozen integrated baseline covers 1,534/1,654 starts. The first evening
head passed 27/120 original misses. The exact #747 combined Windows rerun
passed 28/120, with only Clminate changing relative to #744. Reruns used the
same catalog, selected case IDs, resources, frame budget and inputs. The
result supports 1,562/1,654 starts if the earlier 1,534 passes still hold.
This cohort exercised the 120 earlier misses.

With Flying Colors 2.0 then moved from 1/2 to 2/2 across its two distinct
original distributions. The standalone archive's classic 7/31 level has six
authored team rooms. The baseline tried to register the first team room as a
station and stopped with `Invalid station type 9 in fuelcen.c`. The pinned
source leaves the rooms without producers. The single-player candidate
completed 120 unassisted frames with the native classic load path, 120
windowed OpenGL frames, and an assisted cold save route from room 8 to room 187. The separate 23/40 levelpack release still passes. No further blocker
appeared in the startup or cold continuation.
[With Flying Colors evidence](../XL_compat/evidence/gameplay-wfc-classic-team-rooms-20261123.json).

The exact #748 comparison passed **29/120**, leaving **91/120**. The selected
IDs, inputs, identities and catalog hash match #747. The standalone With
Flying Colors row was the only outcome change. The #748 Windows candidate
binary SHA-256 is
`82785eec5268bc71ddba28169937a10dd5ae3375129311fa8808afadba4cc0aa`.
The 120-case receipt SHA-256 is
`202b2e18f9f099ff3249d6e6372101f3defba8b38ad5cf0d506acb64908b0173`.

The full repeat on #749 measured **1,563/1,654** starts, up from 1,534/1,654,
with 29 gains and zero regressions. The local catalog passed 514/543, up from
493/543. The pinned catalog passed 1,049/1,111, up from 1,041/1,111. The final
120-case subset matched #748 in outcomes, frame counts, XL state and selected
level filenames. The Windows executable SHA-256 was
`2eb019f52bd3119d02a4b214fc2a769f3e9b5c0a24a5ec6be03f7297106bf27b`.

The host session disappeared late in the pinned run. The recovery retained
1,584 usable per-case receipts, reran seven infrastructure errors and
completed 63 unexecuted cases on the same executable and inputs. The
interrupted rows and all logs remain in the private run directory. The
reconstructed aggregate SHA-256 is
`de1619095fc010756eabe49bd5c0688f2528c2ac00c72c6db890333eae252790`.
[Full Windows aggregate receipt](../../temp/xl-pumo-evening-20261123/windows-full-1654/integrated-summary-recovered.json).

### Remaining first blockers

| First observed blocker           | Count within 120 prior misses | Count within integrated 1,654 | Started and ran in pinned XL |
| -------------------------------- | ----------------------------: | ----------------------------: | ---------------------------: |
| Missing advertised level member  |                            34 |                            34 |                            0 |
| Light index bounds               |                            16 |                            16 |                           16 |
| Segment functions                |                             5 |                             5 |                            5 |
| Lightning effects                |                             5 |                             5 |                            5 |
| Fluid behavior                   |                             4 |                             4 |                            4 |
| Non-hexahedral geometry          |                             4 |                             4 |                            4 |
| OOF model decoding               |                             4 |                             4 |                            4 |
| Robot fire killed the idle pilot |                             4 |                             4 |                            4 |
| Segment ownership and groups     |                             3 |                             3 |                            3 |
| Unsupported level format         |                             3 |                             3 |                            2 |
| Wall types                       |                             3 |                             3 |                            2 |
| Robot definitions                |                             2 |                             2 |                            2 |
| Custom base definitions          |                             2 |                             2 |                            2 |
| Segment flags                    |                             1 |                             1 |                            1 |
| Timeout                          |                             1 |                             1 |                            0 |
| **Total**                        |                    **91/120** |                  **91/1,654** |                    **54/91** |

The pinned-engine observations from the frozen comparison count 54/91
that started and ran, 31 that rejected the mission, three with an empty
advertised-member lookup, and three with simulation still unverified.

Of the 34 missing advertised files, 27 belong to the levelpack FFYL
descriptor without matching members in that distribution. The pinned
original rejects or cannot find the other seven in their own distributions.
The alternate FFYL distribution retains its separate case identities. The
next runnable-original audit is the four Anthology OOF failures. The seven
inspected split-archive models are byte-identical to their counterparts in
`anthology.7z`. They have opaque vertex alpha and include authored
facing, glow, field-of-view and rotation properties. Six TXTR lengths exceed
the bytes consumed by the pinned reader. The models also carry original gun
points, animation tracks and weapon batteries.
[Missing member audit](../XL_compat/evidence/gameplay-missing-seven-audit-20261123.json), [OOF audit](../XL_compat/evidence/gameplay-anthology-oof-audit-20261123.json).

## Fork review and workflow admission

The ready-for-review branches target only `larsonm-personal/dxx-redux` and
form [stack #746](https://github.com/larsonm-personal/dxx-redux/stack/746).
Each base is the immediate predecessor. The PRs remain open for review.

| PR                                                                                 | Base    | Exact tested head                          | Fork checks          | Runs with retained receipts                                                                                                                                                                                                                                                 |
| ---------------------------------------------------------------------------------- | ------- | ------------------------------------------ | -------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [#744](https://github.com/larsonm-personal/dxx-redux/pull/744) Classic lightning   | `cmake` | `44eb8911db43e3c088ba6da3d8463a27d3e54f50` | 36 passed, 7 skipped | [Selected](https://github.com/larsonm-personal/dxx-redux/actions/runs/45288348342), [private XL](https://github.com/larsonm-personal/dxx-redux/actions/runs/45288374735), [public save and replays](https://github.com/larsonm-personal/dxx-redux/actions/runs/45288348426) |
| [#745](https://github.com/larsonm-personal/dxx-redux/pull/745) Texture storage     | #744    | `e2f26d48bde6f3967f72e1220eb54b9a2982d525` | 34 passed, 9 skipped | [Selected](https://github.com/larsonm-personal/dxx-redux/actions/runs/45293127604), [private XL](https://github.com/larsonm-personal/dxx-redux/actions/runs/45293149257), [public save admission](https://github.com/larsonm-personal/dxx-redux/actions/runs/45293127635)   |
| [#747](https://github.com/larsonm-personal/dxx-redux/pull/747) Classic smoke       | #745    | `e43512b923774ea11973beeea5e327578101a329` | 34 passed, 9 skipped | [Selected](https://github.com/larsonm-personal/dxx-redux/actions/runs/45293600721), [private XL](https://github.com/larsonm-personal/dxx-redux/actions/runs/45293664038), [public save admission](https://github.com/larsonm-personal/dxx-redux/actions/runs/45293600648)   |
| [#748](https://github.com/larsonm-personal/dxx-redux/pull/748) Classic team rooms  | #747    | `8cc60d89808be4cd4719f6e15c7ff2eea447bb4a` | 35 passed, 8 skipped | [Selected](https://github.com/larsonm-personal/dxx-redux/actions/runs/45297048002), [private XL](https://github.com/larsonm-personal/dxx-redux/actions/runs/45297080388), [public save admission](https://github.com/larsonm-personal/dxx-redux/actions/runs/45297047937)   |
| [#749](https://github.com/larsonm-personal/dxx-redux/pull/749) Mission Ogg overlap | #748    | `90c1af417cb506a2eb4810a3416f1b3f52411bc9` | 37 passed, 6 skipped | [Selected](https://github.com/larsonm-personal/dxx-redux/actions/runs/45299289843), [private XL](https://github.com/larsonm-personal/dxx-redux/actions/runs/45299320499), [public save and replays](https://github.com/larsonm-personal/dxx-redux/actions/runs/45299289856) |

The exact-head selected receipts for #744, #745, #747, #748 and #749 each record
five Linux host passes, five Android passes and one JVM pass. Their private
XL receipts record the executed retail XL fast owner and fixed and expanded
readiness at 8/8 and 64/64. #744 also ran headless and headed Linux replay
jobs successfully. The public replay gate on #745, #747 and #748 logged its
native non-tip or unrelated-change skip. Their Linux build and save-integrity
job and public replay admission controls passed.

The #749 replay gate admitted coverage and the exact-head Linux run passed
both jobs. Headless and headed reports each executed 34 native runs across
17 cases. The negative controls ran and passed. The headed report also
passed the required imported D1 flight pair for gameplay state and simulation
RNG. Its broader imported parity qualification remains open.
[Replay admission](https://github.com/larsonm-personal/dxx-redux/actions/runs/45299289856/job/138141346124),
[headless qualification](https://github.com/larsonm-personal/dxx-redux/actions/runs/45299289856/job/138141457533),
[headed qualification](https://github.com/larsonm-personal/dxx-redux/actions/runs/45299289856/job/138141457610).

On #745, the first selected Linux lane stopped during provisioning when
Gradle could not resolve `org.jetbrains.kotlin.jvm` 2.4.20. All five Linux
owners were recorded as infrastructure-blocked before execution. The
failed-job retry completed the five host tests at attempt 2 on the same
commit. The five Android and one JVM passes retain their attempt 1 receipts.
[Provisioning failure](https://github.com/larsonm-personal/dxx-redux/actions/runs/45293127604/job/138107148801), [successful rerun](https://github.com/larsonm-personal/dxx-redux/actions/runs/45293127604/job/138118947636).

The selected plans had no supplemental Pumo staging or developer Release
compile. The standalone developer Release PR job is restricted to its two
named branches. Retained online evidence inspection and public timing export
require their evidence run inputs. Scheduled or manual nightly progress
publishing is outside the PR admission condition. These workflow gates
explain the corresponding skipped jobs.
