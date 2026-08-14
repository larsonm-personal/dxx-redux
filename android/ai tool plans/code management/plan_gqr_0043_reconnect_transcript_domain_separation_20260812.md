# GQR-0043 Reconnect Transcript Domain Separation

## Goal

Close GQF-0056 by giving reconnect signatures an explicit protocol version, game title, message role, and per-process CSPRNG session nonce while preserving the route-proof behavior from GQR-0042 and minimizing inherited D1/D2 edits

## Plan

- [x] Inspect the completed GQR-0042 reconnect protocol, persistence format, packet layouts, and paired D1/D2 call sites
- [x] Define branch-added transcript and nonce policy with explicit version, title, role, game-generation, token, slot, counter, and challenge bindings
- [x] Plumb the policy through shared reconnect request and challenge handling with symmetric minimal D1/D2 hooks
- [x] Add focused cross-title, role, restart/reuse, downgrade, and migration tests
- [x] Run scoped formatting, reconnect tests, and feasible Windows and Android native builds
- [x] Record exact validation and diff-minimization metrics here

## Results

Implemented reconnect transcript version 2 in the branch-added shared auth owner. Every request and proof transcript now binds the canonical protocol domain, protocol version, D1/D2 game kind, request/challenge role, 128-bit CSPRNG game-generation nonce, legacy token, and the message-specific counter, key, payload, slot, or challenge fields. Hosts rotate the generation nonce through the existing Java `SecureRandom` JNI bridge for each game, full game info and sync replicate it, and clients reject requests outside their current title and generation. Sequence and migrated player records carry the version, title, generation, key, and accepted counter. The fixed Android-only wire size makes pre-v2 packets fail the exact-size/version gates instead of being interpreted as v2

The inherited footprint is limited to four paired Android-guarded seams in `d1/main/net_udp.c` and `d2/main/net_udp.c`: shared sequence serialization, the title-specific auth reset argument, one full-info/sync generation write/read, and one host-generation rotation call. Each paired header has one size-expression change. Protocol construction, validation, migration serialization, and policy remain in branch-added shared files

Focused coverage now checks cross-title separation, distinct request/challenge roles, changed generation after restart or reuse of the 32-bit token, explicit little-endian token bytes, migration preservation, sequence round trips, pre-v2 downgrade rejection, route proof replay, and challenge binding

Validation:

- MSVC `/W4 /WX` direct build of `test_net_udp_reconnect_auth.c` plus `net_udp_reconnect_auth.c`: passed
- Focused reconnect executable: passed with `net UDP reconnect auth tests passed`
- Scoped `android/run-code-quality.ps1 -Fix` on all changed branch-added shared and test files: passed
- `git diff --check` on all nine touched product/test paths: passed, with only Git's existing CRLF conversion advisory for the paired inherited headers
- Standalone `android/tests` CMake configure could not be used because that partial project requires an unavailable host PhysFS package
- MSVC AddressSanitizer linking was unavailable because the installed toolchain lacks `clang_rt.asan_static_runtime_thunk-x86_64.lib`
- Aggregate Windows/Android builds were not rerun because the shared worktree has a known unrelated `secretarea` unterminated-conditional build blocker owned by another item
