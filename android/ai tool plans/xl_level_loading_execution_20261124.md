# XL level loading report — 2026-11-24

## Full Windows census

| Catalog      | Entered gameplay before | Entered gameplay after | Survived 120 frames before | Survived 120 frames after |
| ------------ | ----------------------: | ---------------------: | -------------------------: | ------------------------: |
| Pinned       |             1,072/1,111 |            1,074/1,111 |                1,070/1,111 |               1,072/1,111 |
| Local        |                 528/543 |                536/543 |                    525/543 |                   533/543 |
| **Combined** |         **1,600/1,654** |        **1,610/1,654** |            **1,595/1,654** |           **1,605/1,654** |

The fixed population contains 1,654 advertised entries, including incomplete packages and copies of the same level in different distributions. Each run used one worker, Hotshot (2), seed 1, 120 frames at 60 Hz and the normal idle inputs. An entry reached gameplay when the runtime receipt identified the requested mission, level number and loaded level member. A pass additionally required all 120 frames in that level without death or transition. The earlier 1,595 passes plus four deaths and Zugzwang's loaded receipt establish the 1,600-entry baseline. Zugzwang recorded 809 loaded segments and no completed ticks, including on a separate 45-second check.

The two final runs used the same executable and the original catalog case order. Pinned used the ordinary census cache. Local used the same census runner with one archive per shard. The comparison checked every case ID, independent of its failure group, and verified the load identity for each entry credited with gameplay.

Runtime commit `c40b98fcafbc52ce1dbdea8fd9945955f609d9b6`. Executable SHA-256 `74c439fa194a242bc6a400044321de00909be62afb0f335102d1d9af05364969`.

| Catalog | Inventory SHA-256                                                  | Aggregate receipt SHA-256                                          |
| ------- | ------------------------------------------------------------------ | ------------------------------------------------------------------ |
| Pinned  | `bf4356a7a5fbe64832a40d7065ea02d8b81276ebd8bbba6874012f7851fa0cf8` | `3c48369f29bbddb053a7aec4f1c342a1efc6645da0d7e639ea16a350b46699d4` |
| Local   | `e7d480aa401522d37f6ec75da01f6cec7c314c901728f1b291131e52a62818df` | `64c1c94aee4d9f932051ce77c263b4ec1f62186892f474f9247a6becbc9d6d57` |

### Case-by-case changes

**Pinned gains and regressions, both milestones.**

- Gain `levelpack.7z/missions/spotlight/crownx.mn2:1`
- Gain `levelpack.7z/missions/spotlight/rtarena.mn2:1`
- Zero regressions.

**Local gains and regressions, both milestones.**

- Gain `levels/elitehunt.zip/elitehnt.mn2:1`
- Gain `levels/kalisto.zip/kalisto/kalisto.mn2:1`
- Gain `levels_multiplayer/code-yena-xl.7z/cdeyena2.mn2:1`
- Gain `levels_multiplayer/coordination-xl.7z/crdntion.mn2:1`
- Gain `levels_multiplayer/nocturnl.zip/d1nctrlnl.msn:1`
- Gain `levels_multiplayer/rtarena.zip/RTARENA.MN2:1`
- Gain `levels_multiplayer/Temple Of Helix.zip/missions/tohelix.mn2:1`
- Gain `prophxl131beta.zip/missions/sapphire wolf sp/PROPHXL.MN2:1`
- Zero regressions.

## Geometry and shared loader fixes

The first repeat used the exact local Kalisto release, descriptor `kalisto/kalisto.mn2`, level 1 and its advertised level bytes. The level SHA-256 was `d08f99744dfef0df7e6250f423d2889809a6fd343dda4727af68047bc33e6e05`. It has source layout 27/40, 12,095 vertices and 4,518 segments. Rooms 3145 and 3146 use six referenced vertices and deleted slots. Pinned D2X-XL `2576a98b5de310e3ac7c9777df2677d1627b8bae` loaded the same bytes with explicit sides. The windowed reference diagnostic recorded Hotshot, seed 1, idle inputs and 120 completed 60 Hz ticks for Kalisto, Temple of Helix, Code Yena XL and Coordination XL. The final Redux windowed test also rendered all 120 frames for those four levels and Crownpoint X on the same settings. Desktop display settings matched before and after.

| Exact entry                                                     | First Redux blocker                     | Next blocker or passing control                                                                                                                                                                                                                             |
| --------------------------------------------------------------- | --------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `levels/kalisto.zip/kalisto/kalisto.mn2:1`                      | Deleted vertex slots and explicit sides | Passed after the authored-side change                                                                                                                                                                                                                       |
| `levels_multiplayer/Temple Of Helix.zip/missions/tohelix.mn2:1` | Explicit sides                          | Room 271 exposed owner/group `1/-1`. Preserving game-40 room data admitted the level                                                                                                                                                                        |
| `levels_multiplayer/code-yena-xl.7z/cdeyena2.mn2:1`             | Explicit sides                          | Passed after the authored-side change                                                                                                                                                                                                                       |
| `levels_multiplayer/coordination-xl.7z/crdntion.mn2:1`          | Explicit sides                          | Passed after the authored-side change                                                                                                                                                                                                                       |
| `levelpack.7z/missions/spotlight/rtarena.mn2:1`                 | Legacy fluid behavior                   | Passed with default water physics                                                                                                                                                                                                                           |
| `levels_multiplayer/rtarena.zip/RTARENA.MN2:1`                  | Legacy fluid behavior                   | Passed with default water physics                                                                                                                                                                                                                           |
| `prophxl131beta.zip/missions/sapphire wolf sp/PROPHXL.MN2:1`    | Legacy fluid behavior                   | Passed with default water physics                                                                                                                                                                                                                           |
| `levels/elitehunt.zip/elitehnt.mn2:1`                           | Legacy fluid behavior                   | Exposed legacy equipment function 15, wall flags 280 and the equipment assignment guard. All cleared in the room metadata layer                                                                                                                             |
| `levelpack.7z/missions/spotlight/crownx.mn2:1`                  | Unsupported 11/31 layout                | The legacy byte wall references, double color tables, colored wall type and reserved ambient flags decode and enter gameplay. The pinned original repeated the pass with the fixed protocol after the diagnostic profile was restored for its startup retry |
| `levels_multiplayer/pmanicd2.zip/pmanicd2.mn2:1`                | Ordinary geometry control               | Passed throughout                                                                                                                                                                                                                                           |

The native inventory and modern layout contracts and the original Water Arena fixture passed on Windows. The Water Arena fixture exercised default water physics. The 11/31 inventory contract covers byte wall references, double colors, a colored wall in the converted level and rejection of the unsupported 11/32 pair.

## Audit of 34 apparent missing files

Compared the original archive and descriptor paths, including case, sibling and nested archives and the named members of the owning HOGs. The fresh original D2X-XL lookup trials repeated the previous results for these 34 advertised entries. The original rejected missions covering 31 entries and admitted three missions whose requested level member had zero available bytes.

| Distribution and descriptor                     | Entries | Exact advertised content and audit result                                                                                                                                                                                         |
| ----------------------------------------------- | ------: | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `levelpack.7z/missions/danwentz/dffusion.mn2`   |       1 | `dffusion.rl2` absent in the selected descriptor's ownership scope. The separate `missions/darkhorse` descriptor has its own HOG                                                                                                  |
| `levelpack.7z/missions/diedel/wfc.mn2`          |       1 | `wfc.rl2` absent. The nearby `wfc20` mission has a different advertised file                                                                                                                                                      |
| `levelpack.7z/missions/sidewinder/picasso.mn2`  |       1 | `picasso.rl2` absent. The HOG contains a differently named level                                                                                                                                                                  |
| `levelpack.7z/missions/single/ffyl.mn2`         |      27 | All 27 members absent. Ordinals 1–17 name `pdl8.rl2` through `pdl24.rl2`, 18 names `aquaphnx.rl2`, 19–20 name `pdl25-1.rl2` and `pdl25-2.rl2`, and 21–27 name `pdl26.rl2` through `pdl32.rl2`                                     |
| `levelpack.7z/missions/single/reflux.mn2`       |       1 | `reflux.rl2` absent                                                                                                                                                                                                               |
| `levels_multiplayer/dmentia2.zip/DMENTIA2.MN2`  |       1 | Ordinal 2 declares `dmentia5.rl2`, which is absent                                                                                                                                                                                |
| `levels_multiplayer/nocturnl.zip/d1nctrlnl.msn` |       1 | Fixed. The single D1 member `d1nctrnl.rdl` exists as PLVL version 1 in the same-directory `d1nctrnl.hog`. The descriptor stem has an extra `l`. The normal D2 `NOCTURNL.MN2` and standalone D1 CAVERN11 remained passing controls |
| `levels_multiplayer/picasso.zip/Picasso.mn2`    |       1 | `picasso.rl2` absent. The HOG contains a differently named level                                                                                                                                                                  |

## Android launcher

The isolated x86-64 diagnostic app imported the unchanged `levels_multiplayer/code-yena-xl.7z` through the normal launcher and entered Code Yena XL, `cdeyena2.mn2` level 1, on Hotshot. It reported a live ship after more than two seconds, with 15 of 15 checks passed. Diagnostic receipt ID `d1e81975b2864b6f8edfb4a2edc6bc7f`. The imported archive SHA-256 `be0bf549082e025e7d1c2df3323a438740d8c7189d495306f30b82a2234e3b33` matched the local catalog. The emulator's display size, density and stay-awake setting matched before and after. Game files, profiles, saves, screenshots and raw logs stayed local.

Android Kalisto still fails during launcher preflight on the embedded `kalisto.hxm`. The authentic HXM has one Descent 2 replacement robot (ID 46) and replacement model records. This is a separate Android resource-normalization issue after the Windows geometry fix. Compare the retained archive path and the mounted companion path using that exact embedded file.

## Remaining entries and next run

| Remaining first blocker          |       Pinned |      Local |     Combined |
| -------------------------------- | -----------: | ---------: | -----------: |
| Advertised level file missing    |           31 |          2 |           33 |
| Custom base definitions          |            1 |          1 |            2 |
| Robot definitions                |            2 |          0 |            2 |
| Lightning effects                |            1 |          1 |            2 |
| Waypoint controllers             |            1 |          1 |            2 |
| Unsupported level format         |            1 |          1 |            2 |
| Robot generator capacity         |            0 |          1 |            1 |
| Non-hexahedral geometry          |            0 |          0 |            0 |
| Fluid behavior                   |            0 |          0 |            0 |
| Robot fire killed the idle pilot |            2 |          2 |            4 |
| Timeout after entering gameplay  |            0 |          1 |            1 |
| **Still short of 120 frames**    | **39/1,111** | **10/543** | **49/1,654** |

Of the 49 entries short of 120 frames, 44 stopped before gameplay and five reached gameplay. The 33 missing entries include all seven remaining incomplete descriptor paths.

### Remaining case IDs with owned level data

- **Custom base definitions** — `levelpack.7z/missions/darkflamewolf/acat-xl.mn2:1`, `levels_multiplayer/acat-xl.7z/missions/acat-xl.mn2:1`
- **Robot definitions** — `levelpack.7z/missions/diedel/forest.mn2:1`, `levelpack.7z/missions/single/sphere.mn2:1`
- **Lightning effects** — `levelpack.7z/missions/diedel/steel-xl.mn2:1`, `levels_multiplayer/firsteel.7z/steel-xl.mn2:1`
- **Waypoint controllers** — `levelpack.7z/missions/diedel/speed.mn2:1`, `levels_multiplayer/speed.7z/missions/diedel/d3/speed.mn2:1`
- **Unsupported level format** — `levelpack.7z/missions/single/pmlev1.mn2:1`, `levels_multiplayer/fv24b.zip/fv24b.mn2:1`
- **Robot generator capacity** — `levels/amalthea.zip/amalthea/Amalthea.mn2:1`
- **Robot fire killed the idle pilot** — `levelpack.7z/missions/darkflamewolf/wolfpak3.mn2:6`, `levelpack.7z/missions/diedel/pyrotech.mn2:1`, `levels_multiplayer/pyrotech.7z/missions/diedel/pyrotech.mn2:1`, `levels_multiplayer/pyrotech.7z/pyrotech.mn2:1`
- **Timeout after entering gameplay** — `levels_multiplayer/zugzwang.7z/zugzwang.mn2:1`

The first unresolved entry in pinned catalog order is difFusion, `levelpack.7z/missions/danwentz/dffusion.mn2`, level 1. The owned `dffusion.rl2` is missing. The next loader task selected from the runnable original content is Pumo Mines Lev.1 (Prelim.), `levelpack.7z/missions/single/pmlev1.mn2`, level 1, member `pmlev1p.rl2` (17/35), SHA-256 `dd43019eb3f90318d7196d4ff676927585ffb2a23949ce824979776503a5316c`.

1. Start with Pumo Mines level 1 and the pinned D2X-XL reader. A private trial decoded old skybox and equipment segment functions 14/15, reserved segment flags, camera and effects records, and a 59-byte game-35 event owned by camera object 92 (type 16, target segment 766, side 4) followed by the 1,400-byte binding table. Verify serialized boundaries and source behavior before enabling the pair. Confirm each new blocker through the normal runner.
2. Inspect the remaining custom-definition and effect failures as shared groups. The two ACAT distributions contain authored `descent2.ham` overrides. Forest advertises custom robots without an owned HXM in its packaged mission; compare the original's scoped asset fallback. Sphere has the legacy custom HXM `sp-01.HXM`; compare normalization with the Android Kalisto rejection. In both Steel entries, the first inconsistent record stores lightning fields, while D2X-XL normalizes its ID to the particle renderer after reading. Trace the original union semantics before implementing conversion.
3. Follow both Speed entries together. Each has 20 authored waypoint controllers and also contains lightning records. For Amalthea, qualify 24 robot centers against the native capacity of 20. Check the independent authored negative flicker delay at segment 6626, side 5. The side has no portal or wall, and its overlay 295 is emissive in the owned brightness map. Observe the original visibility and timer behavior before changing the negative-delay gate.
4. Keep local `fv24b.zip`, `fv24b.mn2` level 1, on its separate 10/31 reader branch. The trial still truncated near offset 85,067 after applying early wall and color widths. Treat the four idle deaths and Zugzwang as post-load cases. For Zugzwang, timebox a probe of the first incomplete tick with the same inputs. Keep all confirmed missing files in the frozen population and track any corrected external distribution independently.
5. Use one local gameplay worker and the same protocol, plus windowed desktop and Android launcher checks on representative new entries. Rerun both complete inventories after the last production runtime change. Publish tested immediate-parent layers, check each predecessor's public replay admission before opening a dependent PR, and verify actual CI execution.

## Public review and CI

| Pull request                                                                               | Verified predecessor admission                                                                                   | Hosted results on the published runtime head                                                                                                                                                                                                                                             |
| ------------------------------------------------------------------------------------------ | ---------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [#768 · authored side geometry](https://github.com/larsonm-personal/dxx-redux/pull/768)    | [#767 replay run](https://github.com/larsonm-personal/dxx-redux/actions/runs/45542342253)                        | [Native tests](https://github.com/larsonm-personal/dxx-redux/actions/runs/45554456609) · [XL contracts](https://github.com/larsonm-personal/dxx-redux/actions/runs/45554456620) · [Branch-head replay execution](https://github.com/larsonm-personal/dxx-redux/actions/runs/45558419897) |
| [#769 · legacy room metadata](https://github.com/larsonm-personal/dxx-redux/pull/769)      | [#768 replay admission](https://github.com/larsonm-personal/dxx-redux/actions/runs/45554456626/job/139003274820) | [Native tests](https://github.com/larsonm-personal/dxx-redux/actions/runs/45555227049) · [XL contracts](https://github.com/larsonm-personal/dxx-redux/actions/runs/45555227112) · [Branch-head replay execution](https://github.com/larsonm-personal/dxx-redux/actions/runs/45558420274) |
| [#770 · scoped D1 archive lookup](https://github.com/larsonm-personal/dxx-redux/pull/770)  | [#769 replay admission](https://github.com/larsonm-personal/dxx-redux/actions/runs/45555226923/job/139005598950) | [Native tests](https://github.com/larsonm-personal/dxx-redux/actions/runs/45555747814) · [XL contracts](https://github.com/larsonm-personal/dxx-redux/actions/runs/45555747625) · [Branch-head replay execution](https://github.com/larsonm-personal/dxx-redux/actions/runs/45558419995) |
| [#771 · early colored-wall layout](https://github.com/larsonm-personal/dxx-redux/pull/771) | [#770 replay admission](https://github.com/larsonm-personal/dxx-redux/actions/runs/45555747680/job/139007729622) | [Native tests](https://github.com/larsonm-personal/dxx-redux/actions/runs/45556694054) · [XL contracts](https://github.com/larsonm-personal/dxx-redux/actions/runs/45556694085) · [PR-head replay](https://github.com/larsonm-personal/dxx-redux/actions/runs/45556694076)               |

The first geometry head `ac5ec9e20a4477125a6ba783539beae0ca9eefe6` passed the 61 Ubuntu asset-free contracts and then failed to link the boss fixture due to two new geometry symbols missing from its test-only stubs. The follow-up head `e833f3b9777322ac6c354678f47abcadfedde5cd` supplied those stubs. Its Ubuntu run then passed all 61 contracts and the linked boss test. Its Windows lane ran 56 contracts. Kept the failing and passing logs locally. [Initial failing job](https://github.com/larsonm-personal/dxx-redux/actions/runs/45552991458/job/138998372908) · [Corrected Ubuntu job](https://github.com/larsonm-personal/dxx-redux/actions/runs/45554456620/job/139003149560)

The PR-triggered public fixture builds for #768–#770 passed, and the late replay admission then skipped the final native and headed jobs because those PRs had acquired children. The branch-head public workflow dispatches used the exact published commits and reran both replay jobs. The extended sample also reported that stack-tip gate on #768. The separate branch-specific XL Release compile and optional LAN timing export were outside those workflows' admission conditions. [#768 late admission](https://github.com/larsonm-personal/dxx-redux/actions/runs/45554456626/job/139015111618) · [#769 late admission](https://github.com/larsonm-personal/dxx-redux/actions/runs/45555226923/job/139014928539) · [#770 late admission](https://github.com/larsonm-personal/dxx-redux/actions/runs/45555747680/job/139017106107)

[Live stack #766](https://github.com/larsonm-personal/dxx-redux/stack/766) · [Frozen baseline and preceding failure register](../XL_compat/evidence/gameplay-census-alien-frontier-20261124.json) · [Paste-ready next-run prompt](xl_level_loading_next_kickoff_20261124.md)
