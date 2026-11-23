# Pumo S21 execution, November 23

## Human report and first unresolved action

The reported phone was a Samsung SM-S931U, Android SDK 37, Play internal build
31950, Rookie and touch. In Mineral Valley level 2, Crauphel Scilab, the game
exited after a missile shot. The last sampled viewer room was 129. The missile
type, exact aim, and whether a cold save/load repeats the exit remain unknown.
The original private logs and their hashes are recorded in
[the phone feedback](pumo_phone_feedback_20261121.md). The green-on-green text
was on a briefing page; its page and line remain unidentified. This touch shot
is the **first unresolved gameplay action**. A fixed-pose emulator shot and two
protected S21 automated shots before and after quickload stayed in control;
those inputs do not reproduce the reported aim or prove the human issue closed.

The S21 diagnostic route used the exact original Pumo v1.1 archive, Rookie,
640 x 480, and returned 35/35 actions on both speed-pipeline runs. Profile
hashes were restored. A score-to-next-briefing progress bar was shown on the
device. The last-briefing-page-to-flight bar and contrast of the unidentified
green page still need physical play on the ordinary package.

The original Mineral Valley briefing script has a plausible contrast
candidate: the mainframe page beginning “And there it is!” requests bright
green foreground (`96 255 96`) over a picture with green displays. The text
box may overlap that imagery. The reporter has not identified this as the page
they saw, so the ordinary-kit direction asks for its words or a screenshot.

## Loading speed

The isolated diagnostic APK version 32219 overlaps decoding and hashing for at
most two already-read scoped images at a time, with an 8 MiB per-image limit.
The S21 serial reference was version 32217. Crauphel prebriefing resource
checking fell from 9,199 ms to 7,556 and 7,428 ms on two protected runs, gains
of 1,643 and 1,771 ms. Crauphel quickload scoped-image preparation fell from
5,234 ms to 3,542 and 3,591 ms. The two runs retained source-order bindings,
and the original Pumo texture audit checked hashes and decoded pixels. These
are native log phase times, not human stopwatch times. See
[the scoped pipeline report](xl_pumo_s21_scoped_pipeline_20261123.md).

The latest ordinary test kit is version **32321**, package `com.dxxredux.app`,
APK SHA-256 `732981f93d7d30a4bb4039f0d237d269c83411ae70ddd1389797c4b9cc665bb7`.
It built for ARM64, ARMv7 and x86_64. Its 11 earned checkpoint files and the
APK match the local manifest. This build adds a dark backing panel, amber fill
and white label to the Android loading indicator after the prior S21 capture
showed the bar was present but difficult to see on the score art. Scoped
Android quality, ktlint, the layout unit test and the three-ABI build passed.
The protected S21 visual check awaits the device display: two initial attempts
failed during import while the screen was off or showing NotificationShade;
profile hashes restored after each. No gameplay conclusion was drawn from
those attempts. The prior ordinary version 32320 was installed and hash-verified
on the S21. That ordinary package had no active test pilot or manual save, so
the checkpoint install dry run stopped at its documented prerequisite before
writing. The private kits and saves remain in ignored `temp/`. After the
earlier phone experiments, the S21's `stay_on_while_plugged_in` setting was
restored to its original value 0 and read back as 0.

## Original XL census and cluster comparison

The frozen integrated Windows baseline is **1,534/1,654** advertised starts at
120 unassisted simulation frames; 120 missed. The POG layer projected
1,549/1,654; the original Anthology trigger correction adds one; the original
Chaos Theory reactor correction adds three; Kore adds one; the two Playground
XL entries add two; an inert Anthology powerup action adds one; and the
byte-identical Playground HOG in the separate levelpack distribution adds
one; Bay32's resolved source identity adds one. A current-head targeted rerun
of **all 120 exact previously missed IDs**
measured **25/120** new passing starts and **95/120** still blocked. This
supports **1,559/1,654**, with **95/1,654** remaining, provided the 1,534
prior passes still pass. It is **not a complete 1,654-start census rerun**.
The rerun preserved original distributions, advertised ordinals and unassisted
inputs. Its catalog SHA-256 is
`304660efeaa0626593cdfd652fb30302592679aa2066ad28b02b0267088fd531`;
the route engine SHA-256 is
`677bdad06afc0b8bace92e6026b01bbe2b8a3c4acb2f2c70013fcc966a6a3594`.
All 25 passing receipts attest 120 frames. Alternate distributions and missing
advertised members remain separate. [Targeted rerun evidence](../XL_compat/evidence/gameplay-remaining-120-20261123.json).

The byte-identical original Chaos Theory XL archive, SHA-256
`5cbe34d9ff56bace946f1ec5c51c3dfdd3f24da2bddc2c165f6e3b16f657c73e`,
was replayed at all six ordinals with unchanged inputs. The baseline passed
levels 1, 3 and 4; the candidate passed all six. Levels 2, 5 and 6 had invalid
reactor wall links. Modern XL inspection now records and omits those links
from the classic shadow, preserving valid order and the source archive.
The 120-frame start check does not establish reactor destruction or door
operation. [Original receipts and authored links](../XL_compat/evidence/gameplay-chaos-reactor-links-20261123.json)

The exact-original Playground XL eight-entry archive replay initially passed
six entries. Its nested `missions/diedel/XL/playgdxl.mn2:1` was blocked by
particle subtype 20, which the pinned XL renderer maps to ordinary smoke. Its
root `playgdxl.mn2:1` had a separate invalid-segment wall placeholder that
pinned XL marks inactive before linking. After the two scoped fixes, all eight
entries loaded and ran 120 frames. The original Kore archive also advanced
from 0/1 to 1/1 by ignoring a robot-definition loot dependency that pinned XL
cannot spawn. These starts do not establish multiplayer operation, a full
flight or visual parity. [Kore evidence](../XL_compat/evidence/gameplay-kore-inert-robot-drop-20261123.json),
[Playground subtype evidence](../XL_compat/evidence/gameplay-playground-smoke-fallback-20261123.json),
[Playground wall evidence](../XL_compat/evidence/gameplay-playground-invalid-wall-20261123.json).

The original Anthology three-level cluster advanced from 2/3 to 3/3. Its
level 1 has a damage-only spawn action on powerup object 118; ordinary pickup
cannot fire the action, and the pinned XL spawn path cannot gate without a
boss record. Autoplay powerup actions remain blocked. The levelpack's separate
Playground descriptor uses a HOG byte-identical to the fixed root Playground
HOG. Replaying that exact levelpack descriptor and its sibling passed 2/2,
recovering one more frozen-census entry. The frozen baseline and this current
replay are separate-head measurements. [Anthology evidence](../XL_compat/evidence/gameplay-antho-inert-powerup-spawn-20261123.json),
[levelpack Playground evidence](../XL_compat/evidence/gameplay-levelpack-playground-equivalence-20261123.json).

Three exact-original multiplayer archives, `biohzrd.zip`, `emtombz.zip` and
`etombz.zip`, still fail at level load with multiple segment requirements; the
first archive reports functions 9 and 10 plus 22 other requirements. The
original `pmanicd2.zip` reports invalid segment flags and 44 other
requirements. These spot checks did not change the 1,654 denominator.

Bay32's exact levelpack ordinal resolved as `missions/BAY32.rdl` instead of
the advertised basename `bay32.rdl`. The route proof now hashes that source
through PhysFS. The original archive member, staged member and engine-read
source all have SHA-256
`e43093837215f2a57b14fba9838321e02b2e49057737b3cda9de2f60a5530dd8`.
Corrupting only the staged copy made the same engine command fail at level
load. Repeating the entire 120-miss cohort changed only Bay32, from a strict
identity failure to a 120-frame pass. It is included among the 25 gains.
[Bay32 identity audit](../XL_compat/evidence/gameplay-bay32-resolved-identity-20261123.json).

Follow-up inspection did not change the measured 95. Levelpack Chaos Theory
level 4 is a **classic 7/31** level with two stationary, uncontrolled type-20,
ID-1, render-12 lightning objects. The existing source-owned lightning path
only binds objects after translated extended-level loads; classic levels pass
through `prepare_classic_member` and the stock loader. The pinned XL reader
uses those objects, so clearing the classic admission error alone would lose
their behavior. The separately passing multiplayer archive is a different
distribution. The next implementation needs a source-owned classic effect
load and an original-cluster replay.

The split-Anthology OOF blocker is also more specific than its first error.
Six of its seven original model files overstate the TXTR chunk length by
53–105 bytes. The pinned XL parser follows decoded records for known chunks;
a bounded local decoder experiment reached `GPNT` on all six, while the
seventh still exposes `GRND`. Authored gun points and animation chunks need
runtime qualification before these models can be admitted. The exploratory
decoder change was removed and all four OOF-blocked starts remain in the 95.
[Original model hashes and chunk audit](../XL_compat/evidence/gameplay-anthology-oof-audit-20261123.json).

The levelpack Sphere level 1 HXM contains 27 replacement robot records.
Robot 2 names model 17,524 and gun submodel 21, beyond native tables. A
private derived copy correcting those two fields passed a standalone
structural validator; the exact original still fails. Robot 2 is absent from
the static level 1 objects, robot containment and matcen flags inspected, but
that does not prove it cannot be spawned at runtime. No replacement was
silently dropped and no Sphere start was credited.
[Original HXM and level hashes](../XL_compat/evidence/gameplay-sphere-hxm-audit-20261123.json).

### Remaining first blockers

The targeted 120-case rerun measured the following current first blockers.
It totals **95/120 prior misses**, or **95/1,654** in the integrated denominator
provided the prior 1,534 passes remain passing. The texture fixes exposed an
effect-object and lightning requirement in levelpack Chaos Theory level 4 and
additive wall rendering plus 14 other requirements in its level 6.
Of the 34 missing advertised members, **27** are FFYL ordinals advertised by
`missions/single/ffyl.mn2` in `levelpack.7z`; that archive contains the
descriptor but no matching FFYL HOG or RL2 members. A separate `FFYL.rar`
exists on the build machine and remains a separate distribution, so none of
those 27 was credited as a same-distribution engine pass.

| First-blocker category                         | Remaining / 1,654 |
| ---------------------------------------------- | ----------------: |
| Advertised level file missing                  |                34 |
| Light index bounds                             |                16 |
| Segment functions                              |                 6 |
| Lightning effects                              |                 5 |
| Fluid behavior                                 |                 4 |
| Non-hexahedral geometry                        |                 4 |
| OOF model decoding                             |                 4 |
| Robot fire killed the idle pilot               |                 4 |
| Robot definitions                              |                 3 |
| Segment ownership and groups                   |                 3 |
| Unsupported level format                       |                 3 |
| Wall types                                     |                 3 |
| Custom base definitions                        |                 2 |
| Additive wall rendering and other requirements |                 1 |
| Effect object types and lightning              |                 1 |
| Segment flags                                  |                 1 |
| Timeout                                        |                 1 |
| **Total**                                      |            **95** |

## Local validation and fork integration

The speed layer passed the Windows D2 headless-route build, universal Android
Debug build, original Pumo native mod-data test, texture dependency audit and
two protected 35/35 S21 diagnostic runs. Linux CI exposed a standard-library
include collision with Redux's legacy `min`/`max` macros; the speed PR now
shields its includes and was restacked before the reactor PR. The reactor layer
passed `xl_inventory_test`, the Windows route build and the entire six-level
original archive replay. No game asset, profile, save or private phone log was
committed.

All PRs are ready-for-review, open, based on the immediately preceding branch,
and registered in [stack 720](https://github.com/larsonm-personal/dxx-redux/stack/720):

| PR                                                             | Head        | Scope                          | Checks at handoff      |
| -------------------------------------------------------------- | ----------- | ------------------------------ | ---------------------- |
| [#726](https://github.com/larsonm-personal/dxx-redux/pull/726) | `3ce8db68b` | Score-to-briefing progress     | 36 success, 7 skipped  |
| [#727](https://github.com/larsonm-personal/dxx-redux/pull/727) | `32cb83d50` | Texture hash/preflight         | 36 success, 7 skipped  |
| [#728](https://github.com/larsonm-personal/dxx-redux/pull/728) | `4b894760c` | Level model reuse              | 20 success, 20 skipped |
| [#729](https://github.com/larsonm-personal/dxx-redux/pull/729) | `16195fcc1` | Anthology open-wall target     | 16 success, 21 skipped |
| [#730](https://github.com/larsonm-personal/dxx-redux/pull/730) | `afca85be6` | Scoped-image pipeline          | 17 success, 20 skipped |
| [#731](https://github.com/larsonm-personal/dxx-redux/pull/731) | `1415b51ea` | Reactor wall links             | 29 success, 13 skipped |
| [#732](https://github.com/larsonm-personal/dxx-redux/pull/732) | `e05d62d4a` | Kore inert robot loot          | 30 success, 12 skipped |
| [#733](https://github.com/larsonm-personal/dxx-redux/pull/733) | `6b4eb9b0a` | Playground smoke fallback      | 34 success, 9 skipped  |
| [#734](https://github.com/larsonm-personal/dxx-redux/pull/734) | `cb3b7796a` | Playground invalid wall        | 34 success, 9 skipped  |
| [#735](https://github.com/larsonm-personal/dxx-redux/pull/735) | `f25069202` | Anthology inert powerup action | 37 success, 6 skipped  |
| [#736](https://github.com/larsonm-personal/dxx-redux/pull/736) | `3a0ac3e1a` | S21 report and 120-case census | 37 success, 6 skipped  |
| [#737](https://github.com/larsonm-personal/dxx-redux/pull/737) | `a1209084b` | Bay32 source-identity proof    | CI pending             |
| [#738](https://github.com/larsonm-personal/dxx-redux/pull/738) | `0de66ae13` | Loading indicator contrast     | CI pending             |

Fork workflow admission can skip private XL qualification below the native
stack tip. The private smoke on #729 actually ran sampled and expanded
readiness and passed. On #737, the private workflow admitted and completed
both the fixed sample and expanded 64-case readiness jobs; its Linux tests
were still running at this checkpoint. The #734 Windows build was cancelled
after its test steps passed in [run 45180936407, attempt 1](https://github.com/larsonm-personal/dxx-redux/actions/runs/45180936407/attempts/1);
the targeted [attempt 2](https://github.com/larsonm-personal/dxx-redux/actions/runs/45180936407/attempts/2)
succeeded. Final head checks and admission evidence must be updated here
after active runs complete. Leave all PRs open for review.

## Handoff

Start with a physical report on the latest ordinary kit using
[the playtest directions](../XL_compat/PUMO_PLAYTEST.md). Keep kit version with
each report and record phone, Android version, difficulty, input method,
episode, checkpoint, last successful action and save/load result. Keep the
original issue save. See [the next tranche](xl_pumo_next_tranche_20261123.md)
for the exact continuation and a paste-ready kickoff.
