# XL catalog follow-up for November 26, 2026

All 1,654 advertised cases ran again on production runtime `f00211e38247bf1cf198df3b73e7d0e2eb3cd2cb`. The requested level entered gameplay in 1,620 cases. A live idle pilot completed 120 frames in 1,616. The four deaths and every missing dependency remain in the denominator.

| Frozen inventory | Advertised | Entered gameplay | Survived 120 frames | Missing level | Missing declared textures | Idle deaths |
| ---------------- | ---------: | ---------------: | ------------------: | ------------: | ------------------------: | ----------: |
| Pinned           |      1,111 |            1,079 |               1,077 |            31 |                         1 |           2 |
| Local            |        543 |              541 |                 539 |             2 |                         0 |           2 |
| Both             |      1,654 |            1,620 |               1,616 |            33 |                         1 |           4 |

The comparison joined every inventory ID to its package, descriptor, ordinal and individual receipt. Both milestones matched the previous November 26 run for every case. Against the initial census, the pinned inventory gained seven entries and seven survivals. The local inventory gained 13 entries and 14 survivals. Neither inventory regressed. Runs used one gameplay worker, Hotshot difficulty 2, seed 1, idle inputs and a fixed 60 Hz clock. Diagnostics and alternate releases contribute zero entries to these totals. [Case register](../XL_compat/evidence/gameplay-census-forest-20261126.json)

## First unresolved case and distribution ownership

**difFusion** remains first in frozen inventory order. Package `levelpack.7z`, mission `missions/danwentz/dffusion.mn2`, ordinal **1**, advertises `dffusion.rl2`. Reopening the archive confirmed that the selected descriptor has no owned copy. The publisher attributes another distribution to Darkhorse. The frozen Darkhorse case retains its own member and ID. [Publisher attribution](https://descent2.de/darkhorse)

All 27 frozen Fight for Your Life entries still lack an owned level. The separate `FFYL.rar` carries the same descriptor digest and supplies all 27 in its HOG. The same ownership check confirmed the remaining absent members for With Flying Colors, both Picasso cases, Reflux and Dementia 2. The earlier report gives the publisher checks, Picasso spelling diagnostic and per-package identities. [November 26 package audit](xl_level_loading_execution_20261126.md#missing-file-audit)

## Windowed desktop comparison

The retained November 26 table covered 762 pinned cases and all 543 local cases. The next unmeasured ID was `levelpack.7z/missions/single/lostlvls.mn2:15`. The follow-up flew all 349 remaining pinned cases on the same production windowed binary. Every one entered and survived 120 frames. The six receipts from an earlier interrupted batch were retained separately and added zero cases to the count.

| Inventory | Earlier receipts rechecked | Newly flown | Total measured | Entered | Survived 120 | Matching opened level SHA-256 values |
| --------- | -------------------------: | ----------: | -------------: | ------: | -----------: | -----------------------------------: |
| Pinned    |                        762 |         349 |          1,111 |   1,079 |        1,077 |                                1,079 |
| Local     |                        543 |           0 |            543 |     541 |          539 |                                  541 |
| Both      |                      1,305 |         349 |          1,654 |   1,620 |        1,616 |                                1,620 |

The full windowed table matched the headless milestone, completed-frame count and opened level identity for every gameplay entry. The 34 rejected cases had matching missing-member or missing-texture diagnostics. This leaves zero additional windowed loading failures to select. Desktop display settings matched before and after the runs. The worker used headless executable SHA-256 `76a9da84be2bb532a34f2ab69da37d925dbbb98d0b67ded5ed9bc632a6f369f5` and windowed SHA-256 `d3d8291a7c34fa2b85065a63382f6a9e15ca9bbae53ee1a08a563a695d72324d`. The complete windowed comparison table has digest `846f6cc772743972751cf342bc5ff9da7e4019ea12f66f9696427239460356dd`.

## The Forest on desktop and Android

The frozen Forest stopped at its missing declared texture bank. Its ownership scope contains no qualifying replacement texture set. A fresh reconstruction from the unmodified frozen pack reproduced the 633-room ZIP with SHA-256 `95e57d8a5a50f5487051fb549b9bfcffcb51f1672d3df3ca6f02c05a18d44cb5`. The recovered release and the two 1,442-room publisher editions each completed 120 windowed frames. The 16-player publisher edition uses level version 23 and game version 40. The other publisher edition uses versions 26 and 40. Subspace D3 completed the same windowed flight, and the Water Arena dependency test passed.

The ordinary Android launcher verified the recovered archive owner and internal name `Dimentional Intersect` in a 17-step run. It separately verified the 16-player archive owner and internal name `The Forest` in 15 steps. Pyrotechnica and Fire And Steel each passed 15 steps on their own imported archive. Both entered ordinal 1 with a live Hotshot pilot. The final Pyrotechnica introspection recorded an idle death. These launcher observations have their own timing and denominator. The Android runs used the diagnostic APK built from `1ad83c568a` (#787). The later runtime changes used by this census affect desktop and headless measurement. The installed APK digest was `8d5522eb3f9ddda27bba5bbc602ff528e610e6b8861cb2b076a92a4fc4c92994`. The read-only emulator retained its display size, density and stay-awake setting.

Three further launcher checks covered the 26/40 publisher Forest, pinned Midgard and prepared XL Pyrotechnica. The checks asserted the active archive after entering ordinal 1 on Hotshot.

| Selected mission            | Actual Android archive owner                | Internal level name     | Successful steps |
| --------------------------- | ------------------------------------------- | ----------------------- | ---------------: |
| The Forest, 26/40 publisher | `mod/forest-publisher-26-level-loading.zip` | `Dimensional Intersect` |               15 |
| 1999 (Midgard), frozen pack | `mod/levelpack-final-loading.7z`            | `1999`                  |               15 |
| Pyrotechnica (XL)           | `mod/pyro-observed-prepared-loading.7z`     | `Pyrotechnica`          |               16 |

The first two scenarios initially expected the menu title or member basename as the internal level name. Both reached gameplay with the correct archive and timed out at that title assertion. The retries used the original archive bytes and APK with corrected names. The 26/40 title spells `Dimensional` with an **s**. The 633-room member spells `Dimentional` with a **t**. Midgard used the entire frozen 102,161,128-byte pack. For prepared Pyrotechnica, the local archive's root and nested descriptor and HOG hashes both match their frozen pinned counterparts. The launcher disabled the earlier Midgard pack before selecting the prepared edition.

## Replacement textures

The three classic Redux cases installed the final authored directory row for each repeated slot. An independent POG decoder matched all 194 classic slot instances by source bytes, dimensions, flags, average color and offset. The read-only classic flag query agreed with the installation. The prepared XL Pyrotechnica case installed all 55 winning slots from its 72-row bank. It shares the bank bytes with classic Pyrotechnica.

The pinned D2X-XL comparison captured the effective dimensions, expanded pixels and two transparency bits from each installed bitmap override. The pixel column compares the full displayed extent of each slot with the decoded Redux replacement. The flag column compares only ordinary transparency and super-transparency. These measurements describe the bitmaps provided to the renderer.

| Mission and Redux texture path | Installed slots | Matching original pixels and dimensions | Matching original transparency bits | Windowed Redux            |
| ------------------------------ | --------------: | --------------------------------------: | ----------------------------------: | ------------------------- |
| 1999 (Midgard), classic        |              82 |                                      82 |                                  10 | Survived 120              |
| Fire And Steel, classic        |              57 |                                      50 |                                  14 | Survived 120              |
| Pyrotechnica, classic          |              55 |                                      55 |                                   7 | Entered, died on tick 105 |
| Pyrotechnica (XL), prepared    |              55 |                                      55 |                                  55 | Survived 120              |

The original recalculated transparency from palette keys in all 249 slot observations. Classic Redux retained the authored flags. In Fire And Steel, original slots 235–241 inherited the base bitmap dimensions. The base PIG names are `targ01b`, `targ02b` and `targ03b`. The original exposed 18 × 17, 62 × 12 and 50 × 23 pixels respectively. Each effective buffer matched the corresponding prefix of the decoded 64 × 64 replacement. The original loader copies the base bitmap properties after expanding its POG pixels. The relevant original function is `LoadReplacementBitmaps` in `gameio/loadtextures.cpp` at revision `2576a98b5d`.

On prepared XL Pyrotechnica, the installed flags agreed with the original in all 55 slots. The read-only Redux query returned authored flags that differed from the installation in 48 slots. Its concealed-liquid reader considers enabled illusion walls with water or volatile material. The 301-room level contains 62 walls, including 22 enabled illusion walls. None had either liquid material, so this case supplies no affected side. A different authentic level with such a side is needed to measure a navigation effect. [Prepared bitmap installer](../XL_compat/runtime/pog_install.h) · [Read-only flag query](../../d2/main/piggy.c) · [Concealed-liquid reader](../app/src/main/cpp/shared/secretarea.c)

## Four idle deaths on matched clocks

Wolfpak 3 level 6 and three Pyrotechnica level-1 identities were flown in both engines with observation-only builds. The clocks agreed at every shared update, including the fractional remainder at 60 Hz. The original and Redux first observed the following decisions for the robot that fired the first projectile.

| Case                                  | Robot type / slot | Matched updates | First visibility 2, original / Redux | First fire request, original / Redux | First projectile, original / Redux | Redux completed frames |
| ------------------------------------- | ----------------- | --------------: | ------------------------------------ | ------------------------------------ | ---------------------------------- | ---------------------: |
| Wolfpak 3, pinned                     | 48 / 69           |              82 | 23 / 24                              | 23 / 24                              | 24 / 29                            |                     82 |
| Pyrotechnica, pinned                  | 47 / 107          |              88 | 21 / 23                              | 21 / 23                              | 21 / 23                            |                    104 |
| Pyrotechnica, local nested descriptor | 47 / 107          |              88 | 21 / 23                              | 21 / 23                              | 21 / 23                            |                    104 |
| Pyrotechnica, local root descriptor   | 47 / 107          |              88 | 21 / 23                              | 21 / 23                              | 21 / 23                            |                    104 |

For Wolfpak, both primary cooldowns had reached −25,122 fixed-point units on tick 23. The original robot acquired visibility that tick. Redux acquired it on tick 24 and still had a ready primary cooldown. The first projectile reset the cooldown to 8,192 on tick 24 in the original and tick 29 in Redux. For Pyrotechnica, both cooldowns were −21,845 on tick 20. The original acquired visibility and fired on tick 21. Redux did both on tick 23. The secondary cooldowns agreed across the shared observations.

A second observer recorded the actual firing branch. Wolfpak uses an aim dot-product threshold of 57,344 (7/8 in 16.16 fixed point). The original measured 56,526 on tick 23 and 58,319 on tick 24, when the firing check passed. Redux measured 53,837 on tick 24 and 57,093 on tick 28. It first cleared the threshold at 57,587 on tick 29 and fired. Thus the ready gun waited for aim alignment after visibility had been acquired. In all three Pyrotechnica runs the first visible shot already cleared the threshold, at 62,682 in the original and 58,936 in Redux. The remaining timing difference there begins before the direct firing check. The extra instrumentation reproduced the same frame clocks, projectile and damage events, per-update RNG counts and state transitions.

On the first actor update, the original consumed two calling-thread random draws and Redux consumed zero simulation RNG draws. The counters measure different RNG interfaces. Redux's first ambient wake roll occurred on tick 3 in Wolfpak and tick 9 in Pyrotechnica. The original took no matching wake-roll branch during the compared updates. Its scheduler advances each frame. The Redux selector uses a 20 Hz counter, which was 8 on Wolfpak tick 24 and 7 on Pyrotechnica tick 23. The ordinary game kept its normal fire. The case register retains the four IDs and selected gate observations.

## Boss-room placement in the recovered Cavern

In the author's `dinter_multilevel-2.0.7z`, mission `missions/single/dinterx.mn2`, level 10, room 10 has lost the earlier generator. The recovered distribution is a separate case. In its boss room 649, all six early damage events activated the room 10 binding, selected type 54 or 60, and reached a ready gate. The boss itself, robot 76 in object slot 296, intersected all 30 attempted positions. A second assisted run delayed the first damage until tick 1,201. The center still overlapped the boss, but a fallback created robot 54 in slot 390 and raised the robot count from 140 to 141.

The original source at `2576a98b5de310e3ac7c9777df2677d1627b8bae` passes the boss-room center to the gate. Both engines try that point followed by up to four random points toward room vertices. Both reject a player, robot or reactor whose center lies strictly inside the summed radii. The original executable stopped before gameplay on this recovered member. Ordinary boss-room progression and exit completion remain open.

## Hosted qualification

The report extended the live stack on #790. All eight PR workflows passed on #791 at `a33a4917647396bbd75f964ff1451ba0f8c8f22c`. The native and headed artifacts each recorded 34 replay runs and 7,654 compared frames. Each passed three negative controls. The extended sample recorded 160 passing leaf cases. [Native replay](https://github.com/larsonm-personal/dxx-redux/actions/runs/45879104441/job/140127524170) · [Headed replay](https://github.com/larsonm-personal/dxx-redux/actions/runs/45879104441/job/140127524141) · [Extended suite](https://github.com/larsonm-personal/dxx-redux/actions/runs/45879104432)

The first public LAN attempt stopped during emulator network setup. The same-head retry verified shared Wi-Fi, then observed two players per peer in three sessions covering D1, D2 and D1 content in D2. [First attempt](https://github.com/larsonm-personal/dxx-redux/actions/runs/45879104284/job/140113862020) · [Verified retry](https://github.com/larsonm-personal/dxx-redux/actions/runs/45879104284/job/140122236297)

The independent trusted host completed all 113 required native checks, 16 format witnesses, the six modern source comparisons, both original lifecycles, camera checks and four navigation anchors on the same candidate. The receipt's source count of three tracks its historical subset. The 8/8 and 64/64 readiness jobs supplied separate load/tick observations. [Original-assets host](https://github.com/larsonm-personal/dxx-redux/actions/runs/45879132508/job/140114164617)

The [next kickoff](xl_level_loading_next_kickoff_20261126.md) resumes from the live stack and the first unresolved ordinal.
