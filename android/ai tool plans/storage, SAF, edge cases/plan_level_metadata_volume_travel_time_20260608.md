# Level metadata volume and travel time implementation

## Goal
- Add mine volume, D1 L1-normalized volume, and estimated completion travel time to level metadata.
- Keep secret-area scanning in its existing file and extend the separate metadata scanner.

## Plan
- [x] Extend shared metadata scan structs and adapter callbacks for geometry, objects, walls, keys, reactor, and exits.
- [x] Implement mine volume calculation and route/travel-time estimation in `level_metadata_scan.c`.
- [x] Serialize the new native metadata fields through JNI.
- [x] Parse and display the fields in the Android level metadata table.
- [x] Run scoped formatting/quality checks and a focused build or test where feasible.

## Notes
- Mine volume is computed as raw DXX world volume now.
- The normalized `x` display is anchored to D1 level 1 using the measured baseline below.
- Travel routing uses weighted segment-center Dijkstra with a greedy target order: nearest hostages, reactor, nearest exit. When a selected route hits a missing key door, it charges current position to the door-front segment, door to key, key back to door, then retries the original target.
- Trigger-opened non-key door routing is still treated as an unsupported blocker for this first travel-time pass.

## Baseline calibration follow-up
- [x] Add volume/travel fields to the headless metadata dump.
- [x] Run the D1 headless dump against a full-game `descent.hog`.
- [x] Fill `LEVEL_METADATA_D1_LEVEL1_VOLUME_BASELINE` from D1 level 1 raw volume.
- [x] Re-run formatting and focused build/test.

## Baseline result
- D1 level 1 `Lunar Outpost` measured raw volume: `4572902.81488615`.
- With that baseline, D1 level 1 normalized volume is `1.0x`.
- The base-game secret-area fixture was updated so headless metadata regressions include mine volume and travel fields.
