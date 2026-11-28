# Level loading follow-up, November 27, 2026

## Final production census

Revision `560e6c1c3faba38b5dd137aa69cdd4052566e88d` ran each advertised ordinal with one gameplay worker, Hotshot 2, seed 1, idle inputs and 120 fixed updates at 60 Hz. An entry required the requested owner and ordinal. Survival required 120 completed updates with a live pilot.

| Inventory                | Ordinals | Headless entered | Headless alive at 120 | Windowed entered | Windowed alive at 120 |
| ------------------------ | -------: | ---------------: | --------------------: | ---------------: | --------------------: |
| pinned                   |    1,111 |            1,079 |                 1,077 |            1,079 |                 1,077 |
| local                    |      543 |              541 |                   539 |              541 |                   539 |
| October publisher        |      974 |              928 |                   926 |              928 |                   926 |
| October distinct subset  |       71 |               57 |                    57 |               57 |                    57 |
| October remaining subset |      903 |              871 |                   869 |              871 |                   869 |

The frozen case comparison with PR #800 recorded 0 changed measurement fields. The October comparison recorded changes in 3 ordinals. The two October subsets partition the 974 publisher ordinals. The last publisher receipt was written at 2026-11-28T09:29:09+00:00.

| Changed publisher ordinal        | PR #800 first blocker  | Final first blocker   | Headless entered | Windowed entered |
| -------------------------------- | ---------------------- | --------------------- | ---------------: | ---------------: |
| missions/single/lor-xl20.mn2 (1) | missing owned OOF skin | segment_ambient_light |                0 |                0 |
| missions/test/rngtest1.mn2 (1)   | segment_ambient_light  | survived              |                1 |                1 |
| missions/test/textures.mn2 (1)   | segment_ambient_light  | survived              |                1 |                1 |

The frozen FFYL descriptor advertises 27 ordinals. The selected owner lacked the requested member for 27 headless runs and 27 windowed runs. The October catalog contained the same descriptor and 27 member names. All 27 also stopped at the missing-member check in both modes. Archived FFYL editions retain their own results and hashes.

### First stopping reason by inventory

Each cell gives the headless and windowed counts, in that order. Pilot deaths occurred after entry. All other rows stopped before entry.

| First stopping reason            |  Pinned | Local | October publisher |
| -------------------------------- | ------: | ----: | ----------------: |
| advertised member missing        | 31 / 31 | 2 / 2 |           32 / 32 |
| declared custom textures missing |   0 / 0 | 0 / 0 |             1 / 1 |
| format_30_gameplay               |   0 / 0 | 0 / 0 |             5 / 5 |
| pilot death during idle flight   |   2 / 2 | 2 / 2 |             2 / 2 |
| replacement textures missing     |   1 / 1 | 0 / 0 |             0 / 0 |
| segment_ambient_light            |   0 / 0 | 0 / 0 |             5 / 5 |
| unsupported format 28            |   0 / 0 | 0 / 0 |             1 / 1 |
| unsupported format 31            |   0 / 0 | 0 / 0 |             2 / 2 |

## Mission ownership

I checked the seven ZIPs linked by the Dan Wentz catalog, including every HOG and all eleven embedded level members. None supplied dffusion.rl2. The frozen levelpack.7z descriptor missions/danwentz/dffusion.mn2 still fails before gameplay on ordinal 1. Both Darkhorse releases have their own playable member. Their level SHA-256 values differ. [Dan Wentz catalog](https://www.descent2.de/dwentz) | [Darkhorse catalog](https://www.descent2.de/darkhorse) | [Ownership evidence, PR 801](https://github.com/larsonm-personal/dxx-redux/pull/801)

| Owner or cohort                          | Headless entry | Headless alive at 120 | Windowed entry | Windowed alive at 120 |
| ---------------------------------------- | -------------: | --------------------: | -------------: | --------------------: |
| Frozen Dan Wentz                         |            0/1 |                   0/1 |            0/1 |                   0/1 |
| Frozen Darkhorse                         |            1/1 |                   1/1 |            1/1 |                   1/1 |
| Standalone Darkhorse                     |            1/1 |                   1/1 |            1/1 |                   1/1 |
| Candidate Range Test 1 and Textures Test |            2/2 |                   2/2 |            2/2 |                   2/2 |

The frozen baseline uses runtime 3ab7fdeeb4399b02b30b6ff8d391bd0556d7e17c. The room-lighting candidate is 7e341d6de85958ae9137bf5d314e5cfd919b3ba9. The complete headless repeat on the room-lighting revision matched all 1,654 earlier outcomes, member hashes, update counts and first blockers. I also checked the archived receipts from the previous production runtime for every historical case. In 2010, 33 of 41 entered and 32 survived on each backend. In 2015, 48 of 48 entered and 47 survived on each backend. These historical denominators are independent. The two newly entered publisher ordinals belong to the separate 974-case October inventory. The five production desktop inputs comprise the two publisher assets, two zero-intensity controls and one colored calibration. Each survived 120 updates in both backends. Ordinary Android selected the expected archives, showed rngtest1 and (untitled) as the respective internal names and passed all 25 and 17 steps. Display settings matched before and after.

## Renderer and historical experiments

- In Midgard, passive framebuffer readback matched every authored RGB pixel in the reference captures on all three clocks. During the deferred draw, depth testing left all 7,384, 6,139 and 5,346 previously identified pixels unchanged. Disabling the depth test changed 714, 1,029 and 683 pixels in those sets. Depth values were unchanged across the draw. An earlier observer that altered the framebuffer binding failed the control comparison.
- For With Flying Colors 2.0, cycling all 32 lights in the 2010 repacked and team-neutral control and authentic 2015 edition produced identical corner changes across 3,632 delta records. Removing each source twice applied its effect once, and restoration recovered every corner. The two editions differed in 231 room/source static-light values across 29 light sources. Independent calculations from the retail texture emissions and the room centers of each edition matched all 231 results on both runtime paths. The XL path uses exact distance, per-contribution clamping and downward fixed-point multiplication. The classic path uses approximate distance and truncates the signed product toward zero. Fourteen of sixteen starts and two powerups occupy team rooms in the authentic 2010 edition. Those fourteen rooms have 76 outward portal sides. The original 2010 edition is still blocked.
- For LOR XL 2.0, the host decoder compared 225,280 bytes of normalized RGBA plus dimensions, frame counts, flags, per-frame masks and average color. All 19 PNGs matched owner-derived TGA controls. All nineteen PNGs also had the same RGBA as the Styx TGAs in an image reader. At runtime the eight TGAs with descriptor 8 matched. The eleven with descriptor 40 decoded differently because the CTGA path with zero y-start reverses scanlines regardless of the top-origin descriptor bit. The authentic sample has 4,930 rooms, 44 wall triggers, four object triggers and 51 effects, including 34 sound effects. The format 30 surface-only qualification covers a different set of requirements.
- All eight room-293 pilots in the first series of Cavern trials entered the boss chamber. The longest flight in that series lasted 649 updates. Four more pilots started inside. The best inside flight removed 424.69 of 2,625 shield units. No pilot completed the uninterrupted boss-and-exit sequence. The underlying test member restores the missing generator and stays separate from the author's distribution.

## Publisher reference and ordinary gameplay

The original 2010 With Flying Colors and the separately repacked control each returned from 120 simulation updates in the publisher's game. The temporary pilot profile selected vertex lighting. Both runs read the expected HOG, entered the advertised member and retained 900 rooms and 32 light indices. Neither trial visited the lightmap bake. Earlier trials with lightmaps enabled spent their 1,200-second observation windows before the first simulation return. The original and repacked 2010 editions still require separate qualification for Redux.

The first ordinary weapon contact and shield loss occurred on the same update in Wolfpak and all three Pyrotechnica owners. Aligning projectile movement by one update still leaves a position difference. The radial-hit measurements below retain the first contact and shield-loss receipts. The frozen census used production executables.

The restored-generator Cavern control crossed the genuine exit trigger on update 19 in the windowed game and entered the 769-room Nexus. The ordinary headless transition and the one-use briefing skip entered the same Nexus member. Its SHA-256 was bbddf88cb29585810a4ec253adc4cfb11eb40d9cb42404369638834cb8f5337b. Each pilot survived another 120 updates in Nexus. These pilots began on the isolated exit approach. The separate boss trials appear below.

The Redux overlay captures reproduced the archived authored and keyed framebuffers. Reprojecting the submitted geometry and sampling the actual level-zero textures at the affected pixels gave alpha 255 in the authored edition and alpha zero in the keyed control, for every attributable pixel. The observations cover 35 pixel samples for Fire and Steel slot 1505, 1,462 for Pyrotechnica slot 1507, and 2,117 for Pyrotechnica slot 1555 across three clocks. Both GL filters selected the nearest texel. These measurements use the selected 64-by-64 textures.

On Android the imported Lunar Outpost Reloaded edition reached the check for format 30 ambient lighting on surfaces and objects. Android also reported the unsupported trigger or producer requirements. The desktop headless and windowed tests reached the same requirements. None entered ordinal 1.

The publisher's linked fragment shader uses the sampled overlay alpha to blend the decal with the base. The submitted faces were opaque, the base texels were opaque, and the overlay texels at the corresponding level-zero coordinates were transparent. The publisher used linear magnification and trilinear minification. The three first-frame readbacks reproduced every original RGB pixel. Fire and Steel also reproduced its two later frames. Pyrotechnica's later full frames varied across original runs, including two archived runs without the new observer. The texture bytes still agreed in repeat readbacks. The seven mip levels and their bilinear footprints are recorded separately. These results identify the source texture and blending behavior. They leave the later Pyrotechnica screen-pixel comparison open.

## Verification procedure

1. Check the current stack tip and the November 27 evidence bundle. Trace the missing Dan Wentz ordinal to its publisher. Keep both Darkhorse owners separate.
2. Recheck the format 30 room lighting in both desktop renderers and on Android. Compare owned model textures and inspect the first remaining LOR requirement. Exercise the ordinary Cavern exit and one-use briefing skip.
3. Follow the original light switches, team rooms, deferred faces, overlay texels and ordinary projectile contacts. Preserve the controls and the clocks associated with each measurement.
4. Run both complete frozen inventories and the separate October publisher inventory with one gameplay worker. Record mission entry and survival independently for each backend and each ordinal.
5. Read the actual replay ZIPs and job logs, qualification suite and trusted original-asset receipts for the immediate predecessor before publishing each child. Keep the stack open for review.

## Additional verification

### Replay qualification

The native and headed artifacts each passed 34 replay executions across 7,654 compared frames on every predecessor from #800 through #804. On #804 the headed job also passed the required imported-D1 flight comparison across 360 frames, its simulation RNG and terminal result. Its overall parity qualification remains incomplete. The scoped report lists 4 unqualified scenarios and 3 excluded imported cases. Frame diagnostics, object states and world states remain unqualified streams. The evidence contains the original comparison status, checks and ZIP hashes.

The local inventory already includes the standalone Darkhorse release. Its archive and descriptor match the publisher ZIP byte for byte. The local case and the independently staged download both survived 120 updates on both final executables, opening `fb80847ff9b3d299eb82397040a080b89f6e13f6192a426e278476a88347b065`. The 543 local ordinals include that case once. The levelpack Darkhorse edition has different level bytes.

### First radial hit

The instrumented controls reproduced the earlier object traces. Each first shield loss equals the recorded maximum damage minus its distance-based reduction. The distances, blast centers and pilot centers are in the experiment receipts. Values below are fixed-point units.

| Owner       | Engine | Update | Blast distance | Maximum damage | Shield loss |
| ----------- | ------ | -----: | -------------: | -------------: | ----------: |
| wolf        | ref    |     63 |          9,649 |      1,179,648 |   1,175,306 |
| pyro-pinned | ref    |     55 |         11,854 |      1,310,720 |   1,304,793 |
| pyro-nested | ref    |     55 |         11,854 |      1,310,720 |   1,304,793 |
| pyro-root   | ref    |     55 |         11,854 |      1,310,720 |   1,304,793 |
| wolf        | redux  |     67 |        328,350 |      1,179,648 |   1,031,891 |
| pyro-pinned | redux  |     57 |        332,304 |      1,310,720 |   1,144,568 |
| pyro-nested | redux  |     57 |        332,304 |      1,310,720 |   1,144,568 |
| pyro-root   | redux  |     57 |        332,304 |      1,310,720 |   1,144,568 |

The original blast centers were 0.147 world units from the Wolfpak pilot and 0.181 units from the Pyrotechnica pilot. In Redux both were about 4.735 units away. The maximum damage and radius matched across engines for each weapon. The different blast origins and measured distances account for the first shield-loss amounts. The publisher player-hit handler passes the player to ExplodeSplashDamageWeapon. When the contact distance differs from the player radius, that function scales the vector from the player to the contact using the projectile size. The Redux player-hit handler scales it using the player size. From the recorded centers, the floored Euclidean distance reproduces every publisher scalar, and the Redux quick-distance calculation reproduces every Redux scalar. The observer did not modify collision handling.

### With Flying Colors team starts

After the 120th simulation return, a separate diagnostic called the publisher's team-spawn eligibility check on the retained solo-session start table. This measures the selection rule with the active starts in that session. Network match behavior remains open.

- wfc20-2010-original retained 16 start positions. The publisher's selector returned 0.
- wfc20-2010-packed retained 16 start positions. The publisher's selector returned 0.

### Cavern supply route

The level inspection placed a Mega Missile at slot 83 in room 235, four portal crossings from room 293 in the inspected graph. Eight additional pilots tested that pickup before the boss. The observed inventory showed a Mega Missile for 8 pilots. 2 entered the chamber. The ammo log recorded spending the missile for 0 pilots. The greatest observed boss shield loss was 9.00 of 2,625 units.

The complete frozen and publisher indexes retain each descriptor, ordinal, source identity, backend outcome, update count and receipt digest. The historical figures above refer to the earlier production runtime. [Frozen cases](../XL_compat/evidence/loading-evening-frozen-20261127.jsonl) · [Publisher cases](../XL_compat/evidence/loading-evening-publisher-20261127.jsonl) · [Counts and predecessor replay receipts](../XL_compat/evidence/level-loading-evening-census-20261127.json) · [Renderer and gameplay experiments](../XL_compat/evidence/level-loading-evening-experiments-20261127.json) · [Next kickoff](xl_next_loading_kickoff_after_evening_20261127.md)
