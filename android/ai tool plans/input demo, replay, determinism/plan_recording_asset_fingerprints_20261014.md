# Recording asset fingerprints

## Scope and implementation
- The shared recorder captures HOG/PIG/HAM SHA256 metadata automatically, including manual Android entry points and native test recordings
- The actual load/mount binds each source digest and owner; PIG fallback names and HOG-contained HAM/PIG members use the source that supplied the decoder handle
- Additive optional `asset_fingerprints` header field: `status`, `mode`, `files`, and an `error` only for unknown snapshots
- Each file has a loader-owned role, diagnostic basename, byte size and SHA256; HOG members additionally identify their `container` by an existing HOG role
- Modes are native_d1, native_d2, and d1_in_d2. Roles are d1_hog, d1_pig, d2_hog, d2_pig, d2_ham and mission_hog
- No inner schema version, edition guesses from filenames, launcher settings, migration or backfill
- Missing, unreadable, changed, shadowed or untracked source owners fail closed to unknown. Unsupported additional mission HAM definitions also make the snapshot unknown
- Sources are revalidated at recording start and final flush. Loader changes leave a sticky unknown through A -> B -> A transitions; identical-byte reloads stay valid. No per-frame hashing
- A complete snapshot is checked against actual loaded sources before playback inputs. Hash equality ignores diagnostic names/container location. Intentional native_d1 -> d1_in_d2 playback checks every recorded source; additional optional D2 dependencies are explicitly reported as unrecorded
- This verifies listed archive bytes, not all loose modifications, saved-state contents or engine behavior

## Validation
- Both native game, headless replay and upstream compatibility binaries built with CMake Debug -g0
- Real manual-start captures passed for native D1, native D2 and imported D1
- Controls cover unchanged hashes, correct D2 fallback PIG, exact source ownership, nonzero loader cursor preservation, renamed identical PIG, changed/missing/shadowed source, duplicate-role rejection, A -> B -> A during active recording, custom HOGs, HOG-contained HAM/PIG, and custom mission teardown
- Independent Python hashlib confirmed all 17 archive/member hashes in six generated base/custom recording headers
- All three source-transition recordings retained unknown status after the original bytes were restored
- Existing upstream compatibility CTests passed: D1 1.36 s; D2 15.71 s
- Input-demo parser, serialization, recorder and replay CTests are run separately in both game builds; final logs are in the local validation output
- Direct clang-format was scoped to changed C/C++ blocks because PowerShell execution is unavailable under the current task constraint
- No Android device or Windows runtime claim; Android shared-library source inclusion is covered by the same D1X/D2X_MAIN_SOURCES lists
- Generated recordings, copied public data and validation output remain uncommitted under temp/asset-fingerprints

## Follow-up ownership observation
The custom D2 test packages byte-identical d2demo.ham in assets.hog. Its recording proves d2_ham.container=mission_hog. After load_mission_by_name("d2demo") and StartNewGame(1), mission ownership is removed but the HAM loader has not refreshed the recorded origin. Capture correctly reports unknown because the original owner is unmounted; it does not substitute a newly visible loose file. This is evidence of retained source residency, not yet evidence of differing stale gameplay tables, since the test intentionally uses identical HAM bytes. Investigate separately through d2/main/mission.c load_mission/load_mission_ham and d2/main/piggy.c read_hamfile; no reload repair is included here.
