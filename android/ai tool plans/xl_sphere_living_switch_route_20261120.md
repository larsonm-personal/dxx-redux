# Sphere level-three switch route

Start from the merged fork at `ad7ccabd6`. The gameplay milestone is a connected flight from the six-life Android slot in room 1419 through a door unlocked by switch 4. Preserve the earned inventory, native enemy behavior and consequences of the flight.

1. Rebuild the ordinary headless target and measure the pinned frozen, authentic and expanded cohorts separately. Keep failed rows and input identities.
2. Import the device-written DGSS with SHA-256 `b40243892bbafdfaf6e684f7dcf164fee8af8e7bad6c6ff1006fc2fab0f2dfac` into the existing player test. Verify six lives, 79,250 points, room 1419 and the trigger and wall state from native inspection.
3. Resolve both target walls and their paired sides from the authored level. Fly a reachable route with ordinary controls and collisions, recording the actual passage, wall state, resources and casualties. If flight stops, retain the first failing transition and compare the relevant engine behavior with the pinned XL source before changing code.
4. Cold-load the earned continuation on Android and independently on Windows and Linux. Compare a device recording with clean desktop playback using frame, object, world and RNG traces. Record assists and discrepancies.
5. Publish coherent milestones to the fork as non-draft stacked PRs, run applicable CI, and record final results with the dated ledger, report, next plan and kickoff prompt.

## November 20 results

The ordinary Linux runner reproduced 4/8 with frozen inputs, 8/8 with authentic resources and 64/64 on the expanded sample. All passing cases ran 120 unassisted frames.

The first assisted flight stopped at room 1414. Both sides of its forward passage were open on import. Crossing the master switch on room 1415 side 2 closed that passage. The successful route instead takes room 1415 side 4, then flies through wall 43 from room 409 into 408. Both current-source Debug hosts independently resumed the original Android slot and finished with the same full emitted state, six lives, 81,250 points and an open door. The route uses the native test pilot's steering and primary-fire assists. Position assists were zero.

To reproduce, copy the hash-verified origin to the pilot user directory as `six.sav` and pass [the route](../XL_compat/tests/sphere_level3_living_switch_route.json) to the existing `-xl-player-test` owner with Sphere mission resources and retail D2 data. The input digest, assertions, flight sides, controls and checkpoint export are in the route.

The retained ordinary Android APK loaded the export, wrote its own slot and passed a cold load into room 408. Both Debug hosts independently loaded that device-written slot. The retained developer APK recorded two control bursts and a coast from the slot. Clean ordinary Windows and Linux playback each matched 182 frames, object state, 186 world records, 88,895 simulation RNG draws, ten FX draws and the terminal result. The recording starts after the gate crossing. Device evidence is from an x86_64 emulator. [Execution ledger and binary/input identities](../XL_compat/evidence/sphere-switch-43-20261120.json)

## Next gameplay work

Continue the six-life pilot from room 408. Its blue key is in room 1718. The checkpoint topology reports 101 passages to that room. Re-evaluate each gate as switches change the world. Earn the pickup and demonstrate a keyed door crossing on this same pilot. Record supplies, combat and any deaths. The gold and red key rooms currently have no route under the observed wall and key state. A secret transition or ending still requires its own native event.

### Replacement kickoff

Resume the connected Sphere campaign from the room 408 ordinary Android save identified in the execution ledger. Check the fork PR and its final CI first. Keep the six-life and two-life pilots separate. Use actual controls and native item pickup, preserve failed runs, and investigate engine discrepancies against the pinned XL reference. Repeat new gameplay on both Debug hosts, ordinary Android cold save/load and clean desktop playback. Publish locally tested non-draft immediate-parent PRs in the personal fork, monitor and repair CI while working, then deliver a dated report, ledger, next plan and replacement prompt. Keep merge decisions with the user.
