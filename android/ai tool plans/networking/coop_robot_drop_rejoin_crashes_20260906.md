# Coop robot-drop and rejoin crash investigation

## Evidence

User build 21930 / a6b40a4e, Castaway level 5

- 21:08:30: assertion in multi_do_create_robot_powerups: Net_create_loc must be 1..4
- 21:09:01 rejoin: SIGSEGV in subtract_light called by multi_do_light, segment 0x2d326561 (UUID bytes)
- Tombstone packet memory contains a 160-byte payload beginning with MULTI_COOP_POWERUP_SNAPSHOT_END and stale collection UUID data
- Both games' multi_send_data_direct validate the caller buffer but send global multibuf instead. Recovery uses a local buffer, exposing this existing transport bug
- Robot-drop wire packet has four object IDs, while sender serializes all created drops and receiver creates contains_count objects before asserting. Investigate custom level counts and allocation failure; preserve actual generated drops by batching

## Work

- [x] Correct direct transport buffer and validate lighting input before indexing
- [x] Batch robot contents into four-ID packets; validate received manifests and handle partial creation
- [x] Exercise actual engine/network automation, including a dirty duplication snapshot before rejoin (D1 and D2 passed)
- [x] Scoped formatting, paired builds, relevant tests, and results

## Implemented details

- D1/D2 multi_send_data_direct now sends its caller buffer, after validating command bounds before indexing the length table and rejecting out-of-range player slots
- D2 multi_do_light rejects an out-of-range segment before subtract_light or Segments access
- Robot drops serialize actual created objects in batches of at most four, grouped by type/id, using local packet storage
- Remote manifests validate sender slot, content count/type/id, segment, and remote object IDs before creating anything
- Partially successful creation still publishes/maps the created objects. Spawned robots also respect the network-created-object array capacity
- Introspection exposes received robot-drop object count and recovery row count for end-to-end assertions
- SpewRecovery now sends nine robot eggs through real UDP, seeds a nonempty duplicate-energy history, and requires ledger parity after each of two rejoins

## Exact rejoin packet reconstruction

Tombstone memory at 0x7a650e1ca8 contains the UDP header followed by 160 payload bytes. The payload starts at +10 with command 90 (snapshot end), consumes five bytes, then command 28 (robot fire) consumes 18 bytes. Offset 23 is ASCII 4 / command 52 (light); its segment at offset 24 is 0x2d326561 / ASCII ae2-. This exactly matches subtract_light's x0 argument. The stale UUID is from the duplication snapshot. A real recovery packet would begin with command 93 and stay one 160-byte message

## Evidence limits

The original robot crash stack proves its created count was outside 1..4. That tombstone does not contain the original manifest or exact count. Oversized custom robot drops are a concrete reproducible path in the old code; malformed/empty manifests are also now rejected. The user's exact quarry encounter has not yet been replayed

## Validation

- Scoped mixed-language formatter/lint passed
- Android D1/D2 build passed for all three ABIs, without new warnings from changed code
- D2 two-emulator SpewRecovery passed: nine remote robot-drop objects, partial teammate pickup, nonempty duplication snapshot, and complete recovery-ledger parity through two process restarts
- D1 two-emulator SpewRecovery also passed with nine remote eggs and full ledger parity across two rejoins
- Windows D1/D2 builds passed
- D2 CTest coop/reconnect/initial-sync/save-transfer selection: all 10 tests passed; D1 host build does not enable CTest, so D1 runtime coverage is the paired emulator test
- Final scoped git diff whitespace check passed
- Updated APK: android/app/build/outputs/apk/debug/app-debug.apk; installed on both test emulators


The earlier SpewRecovery test checked inventory and zero remaining spew, which can both be correct with an empty client recovery ledger. The new row-parity assertion prevents that false success and the seeded collection record makes stale global-buffer contents meaningful rather than mostly zeros
