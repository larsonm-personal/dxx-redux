# Pumo playtest and XL startup execution, November 21

The full Windows rerun passed 1,539/1,654 advertised starts, up from 1,534,
with every prior pass preserved. Binding packed POG wall strips to their
authored door clocks recovered five starts at 120 unassisted frames. The
original D2X-XL engine ran the same five archive, descriptor and ordinal
combinations. Both Pumo phone kits and all 11 supplied checkpoints matched
their manifests. Physical-phone feedback is pending.
[Case ledger](../XL_compat/evidence/gameplay-pog-wall-startup-20261121.json)

## Pumo on an ordinary phone

The first open physical-phone action is to load the 32030
`kartsal-final-checkpoint`, fly straight through the Black Hole checkpoint
and advance the score screen. On 32080, load `mineral-ending`, let the
moving ship enter the Umbriel exit and advance the level score and closing
briefing. The ordinary Android 14 emulator reached both authored endings
from those saves. Longer phone choices include 32080 `umbriel-blue` and
`mercury-arrival`. The original connected Windows flights used ordinary
inputs and retries from earned checkpoints. The Android runs used guided
checks, and the earlier fixed-position tests cover individual mechanics.
[Phone directions](../XL_compat/PUMO_PLAYTEST.md) /
[Kartsal ending](../XL_compat/evidence/pumo-kartsal-ending-20261120.json) /
[Mineral Valley ending](../XL_compat/evidence/pumo-mineral-valley-ending-20261121.json)

The local APK hashes matched versions 32030 and 32080. The four 32030 saves,
seven 32080 saves and the original Pumo v1.1 archive matched the recorded
digests. No phone report was available to reproduce. The intake form records
kit, phone, Android version, input method, difficulty, episode, checkpoint,
last successful action and the outcome after a manual cold save/load.

## Animated POG startup

Both Redux Windows runs used the same inventory and verified runtime
resources, difficulty index 2, seed 1 and 120 unassisted ticks at 60 Hz.
The original comparison used difficulty index 2 and 120 idle frames on
its own clock. The control reproduced 17/17 rejections at frame zero. The
candidate passed one of four pinned cases and four of thirteen additional
archive cases.

| Distribution | Descriptor and ordinal | Advertised level |
| --- | --- | --- |
| `levelpack.7z` | `missions/diedel/toxic-d3.mn2` 1 | `toxic-d3.rl2` |
| `boilpnt.7z` | `missions/single/boilpnt.mn2` 3 | `canyon.rl2` |
| `levels/lor-xl.7z` | `missions/single/lor-xl.mn2` 1 | `lor-xl.rl2` |
| `levels_multiplayer/cs-dust.7z` | `dust.mn2` 1 | `cs-dust.rl2` |
| `levels_multiplayer/toxic.7z` | `missions/diedel/d3/toxic-d3.mn2` 1 | `toxic-d3.rl2` |

Redux previously accepted packed strips through its texture-effect owner.
The original also marked matching wall clips for alternate playback. On
pinned Toxic-D3, source observation tied bitmap 2157 to clip 5 with six native
frames and a one-second authored duration. The level contains six door
faces using that clip. The fix reuses the owned wall clip planner, immutable
frame views and saved side phases for eligible XL layouts. Bitmap storage,
rendering and pixel collision select the same side phase. The change is
under `android/XL_compat`.

The candidate gets past bitmap 2329 in all three Pyrotech D3 cases, then
rejects bitmap 2344. The pinned original associates those bitmaps with wall
clips 26 and 27. The pinned level uses clip 26 on ten door faces and contains
zero wall records using clip 27. The retained retail HAM maps bitmap 2344 to
texture 664, which appears on zero serialized level sides. The next check is
whether an indirect dependency consumes the clip-27 replacement. The standalone
Catalyst original associates bitmap 2538 with clip 46, which has zero wall
records in that level. Pinned Standoff and Conclusion remain blocked at
their active door strips on native 7/31 geometry. The standalone Standoff
distribution contains 27/40 geometry and rejects bitmap 2538 after the
change. Its distribution keeps its own result.

## Toxic-D3 door crossing and cold restore

Both distributions started a fresh pilot in room 1. The Windows developer
probe used ordinary controls and automated steering through rooms 10, 143,
157, 156, 151, 148 and 180. The pilot flew into side 4 of room 180 to start
opening the clip-5 door, then saved while both faces were at stored phase
`-2`. It crossed to room 179 under thrust. The fresh-process run saved
again immediately after load and confirmed that both faces retained phase
`-2`. It then crossed to room 179 with the same game time and 100 shields.
Each distribution passed the 18-step fresh flight and four-step cold
continuation. These runs used difficulty index 0 and no position or view
assists. An earlier attempt omitted the fresh-pilot defaults. Another
requested an approach longer than the segment depth. Both attempts are
retained locally. The passing approach used four game units.
[Flight receipt identities](../XL_compat/evidence/gameplay-pog-wall-startup-20261121.json)

The first unresolved action on this door route is to repeat the room 180
crossing and mid-motion load on a rendered Android build and observe the
texture on both sides. Multiplayer and campaign completion need separate
runs. The next engine admission failure is pinned Pyrotech D3 ordinal 1,
which still stops at bitmap 2344 before the player can move.

## Remaining starts

The complete Windows candidate passed 1,042/1,111 pinned cases and 497/543
additional-archive cases. The same inventories previously passed 1,041/1,111
and 493/543. The five gains are the only changes in outcome or frame count
across the 1,654 cases. There are 115 remaining misses. Of those 115, 77 ran
in the original comparison, 31 were rejected by the
original mission selector, three had an empty advertised-member lookup, and
four had unverified original simulation. Thirty-one have a starting
alternate distribution. [Prior census](../XL_compat/evidence/gameplay-census-integrated-20261120.json) /
[Current cluster](../XL_compat/evidence/gameplay-pog-wall-startup-20261121.json)

| First obstacle in Redux | Remaining from 1,654 |
| --- | ---: |
| Advertised level file missing | 34 |
| Light index bounds | 16 |
| Animated replacement textures | 12 |
| Segment functions | 6 |
| Lightning effects | 5 |
| OOF decoding, fluids, non-hexahedral geometry and fatal idle robot fire | 16 |
| Wall types, segment ownership and groups, robot definitions, unsupported format and reactor triggers | 15 |
| Custom base definitions and object placement | 4 |
| Resolved filename, object trigger action, trigger targets, robot contents, particle effects, segment flags and timeout | 7 |
| Total | 115 |

## Validation and fork review

The ordinary and developer Windows CMake targets built successfully. The
`ham-prepare` descriptor control passed, including the packed POG alias
installation, independent drawing and collision phases, validation before
restore, saved phase restore and resource retirement. This is a controlled
native test. Scoped code quality passed. Raw receipts and original assets
remain on the build machine.

The starting fork head was `23ef7d022` on `cmake`. Both previous ending PRs
had landed before this run and there were no open fork PRs. The new
ready-for-review layers are the reproduced-baseline plan at `ecd7d669c` and
the runtime at `992378282`. Neither was merged.
[Plan PR 718](https://github.com/larsonm-personal/dxx-redux/pull/718/checks) /
[Runtime PR 719](https://github.com/larsonm-personal/dxx-redux/pull/719/checks) /
[Stack 720](https://github.com/larsonm-personal/dxx-redux/stack/720)

On the plan head, 28 checks passed and 13 skipped. The trusted controller
admitted that head before the branches were registered as a stack. Its
selected XL owner ran and passed. Both the fixed and expanded readiness
samples passed, 8/8 and 64/64. The extended plan executed all eleven
selected owners and the two-player LAN job executed its one selected owner.
The XL-source, scripts and Rust admission logs record that those profiles
had no matching changes on the standalone plan commit.
[Trusted XL run](https://github.com/larsonm-personal/dxx-redux/actions/runs/44943587684) /
[Extended run](https://github.com/larsonm-personal/dxx-redux/actions/runs/44943571383) /
[LAN run](https://github.com/larsonm-personal/dxx-redux/actions/runs/44943571336)

The runtime head finished with 36 passed checks, seven conditional skips
and zero failures. Linux builds, ASan, public headed and headless replay,
save integrity, the Linux and Windows XL contracts, and developer source
builds passed. The extended suite ran all eleven selected owners across
Linux, Android and JVM. The two-player LAN owner ran and passed. The trusted
Linux XL owner ran and passed, with 8/8 fixed and 64/64 expanded readiness
cases. The skips were for Rust path admission, the branch-specific Release
compile, unselected Pumo staging and nightly Release owners, scheduled
ledger publication, and the two optional retained-evidence workflows.
[Engine run](https://github.com/larsonm-personal/dxx-redux/actions/runs/44944290089) /
[Public replay run](https://github.com/larsonm-personal/dxx-redux/actions/runs/44944290180) /
[XL contracts](https://github.com/larsonm-personal/dxx-redux/actions/runs/44944290095) /
[Extended run](https://github.com/larsonm-personal/dxx-redux/actions/runs/44944290217) /
[LAN run](https://github.com/larsonm-personal/dxx-redux/actions/runs/44944290271) /
[Trusted XL run](https://github.com/larsonm-personal/dxx-redux/actions/runs/44944311418)

[Next tranche](xl_pumo_next_tranche_20261121.md) /
[Paste-ready kickoff](xl_pumo_next_kickoff_20261121.txt)
