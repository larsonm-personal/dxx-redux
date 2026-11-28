# November 27 mission ownership and gameplay continuation

1. Verify the published bundle, predecessor replay reports, admission and original-assets checks. Join the pinned and local cases by their owned bytes and ordinal.
2. Inspect each publisher-listed Dan Wentz release for the advertised difFusion member. Record package and member fingerprints. Track Darkhorse editions separately.
3. Qualify the source room lighting and controls for Range Test 1 and Textures Test across both rendering paths. Follow each remaining publisher and historical gate independently.
4. Continue the dated report's surface, idle AI, Cavern and Android experiments. Repeat both frozen inventories serially on the final production pair. Publish verified checkpoints on their immediate parent and record exact CI results.

## difFusion ownership checkpoint

The publisher lists seven ZIPs on the Dan Wentz page. Reading every ZIP member and all eleven embedded level members found no dffusion.rl2. The Darkhorse page links the standalone difFusion release. Its descriptor matches the frozen Dan Wentz descriptor. Its level differs from the playable Darkhorse HOG in levelpack.7z. Both are independent controls. [Dan Wentz catalog](https://www.descent2.de/dwentz), [Darkhorse catalog](https://www.descent2.de/darkhorse)

Fresh flights used the November 27 production executables, one worker, Hotshot 2, seed 1, idle inputs and 120 fixed updates at 60 Hz. Each test staged the selected owner's files in an isolated mission directory.

| Ordinal 1 owner                | Headless entry | Headless live at 120 | Windowed entry | Windowed live at 120 |
| ------------------------------ | -------------- | -------------------- | -------------- | -------------------- |
| Frozen Dan Wentz               | No             | No                   | No             | No                   |
| Frozen Darkhorse               | Yes            | Yes                  | Yes            | Yes                  |
| Publisher standalone Darkhorse | Yes            | Yes                  | Yes            | Yes                  |

The frozen Dan Wentz ordinal still fails before gameplay with the advertised level missing. The baseline retains all 1,111 pinned and 543 local ordinals. The November 27 ZIP's complete October index contains 974 separate publisher ordinals. [Package, level and flight receipt hashes](../XL_compat/evidence/diffusion-owner-search-20261127.json)

## Format 30 room lighting

Range Test 1 has 11 rooms with a 70-percent white ambient floor. Textures Test has 5,215 rooms at 60-percent white. The surface renderer decodes each room's three channels using the publisher's ten-bit square-root encoding and squared normalized value. It applies the per-channel maximum to the face's ambient input before vertex shading. The format 30 admission change covers player starts and hostages. Rooms requiring model or effect lighting retain their separate gate.

A private instrumented build exercised the authentic levels, both zero-intensity controls and an eleven-room calibration using the three colors observed in Subspace. Each entered and survived 120 idle updates in headless and windowed play. The unmodified and forced old texture-merge paths both changed the following numbers of framebuffer pixels relative to their zero-intensity control. Repeat captures matched exactly. No changed channel became darker.

| Input                                 | Update 1 | Update 30 | Update 60 |
| ------------------------------------- | -------: | --------: | --------: |
| Range Test 1                          |   151445 |    148747 |    155922 |
| Textures Test                         |        0 |         0 |    156752 |
| Range Test 1 with the Subspace colors |   147060 |    144307 |    151402 |

The Range Test 1 observer recorded 120 shader draws and 5,040 fixed-function draws across the flight. The old texture-merge control recorded all 5,160 draws in the fixed-function path. Textures Test recorded 6,838 shader and 27,802 fixed-function draws, or 34,640 fixed-function draws in the control. These pixel counts compare the Redux framebuffer to the corresponding Redux control. The publisher-game measurements use a different camera and viewport. [Flight and framebuffer evidence](../XL_compat/evidence/room-ambient-surfaces-20261127.json)
