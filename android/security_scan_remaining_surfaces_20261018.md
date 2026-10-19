# Security audit of remaining hostile-data surfaces

Date: 2026-10-18

Audited revision: `3bab162341dd85dab3bb7978a68d5ab17b99ff4d`

Mode: read-only review; no product fixes were made

## Executive summary

This review extends the earlier bad-level/mod-data and direct-UDP audits across
saves, player files, demos, checkpoint transfer, matchmaking and relay code,
media decoders, Android entry points, importers, JNI, diagnostics, build
configuration, and dependencies.

The highest-priority new result is that several legacy binary readers still
perform writes using values taken directly from hostile files:

1. D1 and D2 save restore trust core object, wall, door, trigger, robot-center,
   and fuel-center counts before writing fixed arrays. The Android cooperative
   preflight validates only the header and cooperative trailer, so it does not
   protect the legacy body. The same body is reachable from a received
   cooperative save/checkpoint.
2. D1 and D2 classic-demo playback uses an unbounded signed-byte string length,
   trusts recorded player counts and player indices, and has event paths that
   index world arrays without consistently validating the recorded values.
3. D1 and D2 player-file parsing trusts `NHighestLevels` before reading into the
   fixed `HighestLevels` array.
4. The shared legacy `PHYSFSX_readString` and `PHYSFSX_gets` helpers have no
   destination capacity. Current player and netgame-settings readers call them
   with small fixed buffers.

Those are memory-corruption defects, not merely graceful-error gaps. They
should be fixed before broad protocol hardening. The repair should validate the
entire candidate object in a non-mutating preflight or temporary model, then
commit only after validation. It must preserve all valid legacy formats and
must not silently clamp malformed values into a different valid object.

The most consequential previously identified open risks remain cooperative
packet authority and non-transactional restore, fail-open or unauthenticated
matchmaking/LAN control paths, unbounded server and extraction work, private
endpoint TLS authentication, relay admission/capacity, and the MVE startup
audio queue race. Current pinned Rust TLS dependencies also include versions
covered by 2026 RustSec advisories.

## Scope and limitations

The review treated these inputs as hostile: local and transferred saves,
checkpoints, classic and input demos, player/config files, imported disc and
archive content, media, deep links and intents, WebSocket JSON, relay and LAN
traffic, mission transfers, JNI arguments, stored diagnostics, and release
configuration.

The review combined:

- the complete line-by-line branch adversarial ledger as the baseline;
- current-head approximately 500-line block inventory through the last block
  of every selected module below;
- current-head searches for count, length, index, allocation, file, network,
  authentication, logging, and publication sinks;
- call-chain tracing for newly identified high-risk paths; and
- local pinned-version comparison with authoritative RustSec advisories.

This was static analysis. No fuzz campaign, sanitizer build, live hostile
network exercise, Android device test, or penetration test was run. `cargo
audit` is not installed in this workspace, so the dependency result is based on
the lockfile and advisory records rather than a locally generated audit report.
The selected modules cover the trust-boundary implementations; this is not a
claim that every generated file, UI-only composable, test, or vendored library
was reread independently. Findings from the older branch ledger are called out
as such so that new current-head discoveries are distinguishable.

## Trust-boundary inventory

| Boundary | Admission | Sensitive sink | Main failure classes |
|---|---|---|---|
| Save, rewind, checkpoint | filesystem, launcher resume, cooperative transfer | fixed engine arrays and global world state | memory corruption, partial restore, cross-generation state |
| Player and netgame settings | user/profile storage and imported config | fixed arrays and stack buffers | memory corruption, persistent startup failure |
| Classic demo | user-selected `.dem` | engine world, player and media tables | memory corruption and invalid world mutation |
| Input demo | JSONL plus embedded checkpoint | allocation, restore, simulation controls | resource exhaustion and invalid control domains |
| Matchmaking | remote WebSocket JSON | session/lobby authority, database, relay | authorization bypass, replay, resource exhaustion |
| LAN lobby/proxy | unauthenticated local UDP/JSON | membership, launch, proxy routes | spoofing and bounded-capacity exhaustion |
| Mission transfer | peer HTTP | app storage and mission publication | storage exhaustion, races, stale publication |
| Disc/archive import | SAF file descriptors and archives | extraction filesystem and decoders | traversal, decompression/CPU/storage exhaustion |
| Movie/MIDI/audio | imported assets and game data | native decoders and audio callback | memory safety, concurrency, CPU/memory exhaustion |
| Android IPC | exported activities, deep links, dynamic receivers | launches, migration, file grants | spoofing, duplicate engine start, unintended disclosure |
| JNI | Java strings, arrays, FDs, callbacks | native ownership and global state | exception misuse, bounds, lifetime races |
| Diagnostics/build | logs, exports, signing commands, privacy text | credentials and user/network metadata | secret disclosure and release-policy drift |

## Coverage ledger

Ranges are inclusive. Every listed range was included through the listed final
block. Very small policy/configuration files are one block. The prior branch
ledger supplied full-line baseline review; this audit checked current-head
deltas and re-traced the hostile-input sinks relevant to this report.

### Saves, player files, and demos

| File | Lines | Reviewed blocks |
|---|---:|---|
| `d1/main/state.c` | 3157 | 1-500; 501-1000; 1001-1500; 1501-2000; 2001-2500; 2501-3000; 3001-3157 |
| `d2/main/state.c` | 4313 | 1-500; 501-1000; 1001-1500; 1501-2000; 2001-2500; 2501-3000; 3001-3500; 3501-4000; 4001-4313 |
| `d1/main/playsave.c` | 2115 | 1-500; 501-1000; 1001-1500; 1501-2000; 2001-2115 |
| `d2/main/playsave.c` | 1880 | 1-500; 501-1000; 1001-1500; 1501-1880 |
| `d1/include/physfsx.h` | 374 | 1-374 |
| `d2/include/physfsx.h` | 369 | 1-369 |
| `d1/main/newdemo.c` | 3630 | 1-500; 501-1000; 1001-1500; 1501-2000; 2001-2500; 2501-3000; 3001-3500; 3501-3630 |
| `d2/main/newdemo.c` | 4846 | 1-500; 501-1000; 1001-1500; 1501-2000; 2001-2500; 2501-3000; 3001-3500; 3501-4000; 4001-4500; 4501-4846 |
| `android/app/src/main/cpp/shared/coop/coop_save.c` | 2484 | 1-500; 501-1000; 1001-1500; 1501-2000; 2001-2484 |
| `android/app/src/main/cpp/shared/multi_save_transfer.c` | 1563 | 1-500; 501-1000; 1001-1500; 1501-1563 |
| `android/app/src/main/cpp/shared/state_android_shared.c` | 1424 | 1-500; 501-1000; 1001-1424 |
| `android/app/src/main/cpp/shared/rewind_file.h` | 409 | 1-409 |
| `android/app/src/main/cpp/shared/input_demo_hooks_shared.c` | 2895 | 1-500; 501-1000; 1001-1500; 1501-2000; 2001-2500; 2501-2895 |
| `android/app/src/main/cpp/shared/input_demo_fixture.cpp` | 1973 | 1-500; 501-1000; 1001-1500; 1501-1973 |
| `android/app/src/main/cpp/shared/input_demo_recorder.cpp` | 1165 | 1-500; 501-1000; 1001-1165 |
| `android/app/src/main/cpp/shared/input_demo_replay.cpp` | 947 | 1-500; 501-947 |
| `android/app/src/main/cpp/shared/input_demo_newdemo_shared.c` | 762 | 1-500; 501-762 |

### Matchmaking, relay, transfer, and server

| File | Lines | Reviewed blocks |
|---|---:|---|
| `android/app/src/main/java/com/dxxredux/app/multiplayer/MatchmakingService.kt` | 1563 | 1-500; 501-1000; 1001-1500; 1501-1563 |
| `android/app/src/main/java/com/dxxredux/app/multiplayer/MissionTransferService.kt` | 737 | 1-500; 501-737 |
| `android/app/src/main/java/com/dxxredux/app/multiplayer/NetworkProtocol.kt` | 679 | 1-500; 501-679 |
| `android/app/src/main/java/com/dxxredux/app/multiplayer/LocalhostProxy.kt` | 625 | 1-500; 501-625 |
| `android/app/src/main/java/com/dxxredux/app/multiplayer/LanDiscoveryTab.kt` | 1337 | 1-500; 501-1000; 1001-1337 |
| `android/app/src/main/java/com/dxxredux/app/multiplayer/MultiplayerScreen.kt` | 1551 | 1-500; 501-1000; 1001-1500; 1501-1551 |
| `android/app/src/main/java/com/dxxredux/app/multiplayer/UpnpClient.kt` | 341 | 1-341 |
| `server/src/ws_handler.rs` | 3191 | 1-500; 501-1000; 1001-1500; 1501-2000; 2001-2500; 2501-3000; 3001-3191 |
| `server/src/config.rs` | 396 | 1-396 |
| `server/src/db.rs` | 532 | 1-500; 501-532 |
| `server/src/relay.rs` | 174 | 1-174 |
| `server/src/protocol.rs` | 483 | 1-483 |
| `server/src/http_api.rs` | 313 | 1-313 |
| `server/src/rate_limit.rs` | 143 | 1-143 |

### Media and native decoding

| File | Lines | Reviewed blocks |
|---|---:|---|
| `d2/main/movie.c` | 953 | 1-500; 501-953 |
| `d2/libmve/decoder8.c` | 872 | 1-500; 501-872 |
| `d2/libmve/decoder16.c` | 717 | 1-500; 501-717 |
| `d2/libmve/mvelib.c` | 516 | 1-500; 501-516 |
| `d2/libmve/mveplay.c` | 1030 | 1-500; 501-1000; 1001-1030 |
| `android/app/src/main/cpp/shared/midi_preview.c` | 908 | 1-500; 501-908 |
| `android/app/src/main/cpp/shared/midi_metadata.c` | 787 | 1-500; 501-787 |
| `android/app/src/main/cpp/shared/digi_tsf_music.c` | 1454 | 1-500; 501-1000; 1001-1454 |

### Android entry points, storage, import, and JNI

| File | Lines | Reviewed blocks |
|---|---:|---|
| `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt` | 6389 | 1-500; 501-1000; 1001-1500; 1501-2000; 2001-2500; 2501-3000; 3001-3500; 3501-4000; 4001-4500; 4501-5000; 5001-5500; 5501-6000; 6001-6389 |
| `android/app/src/main/java/com/dxxredux/app/MainActivity.kt` | 5218 | 1-500; 501-1000; 1001-1500; 1501-2000; 2001-2500; 2501-3000; 3001-3500; 3501-4000; 4001-4500; 4501-5000; 5001-5218 |
| `android/app/src/main/java/com/dxxredux/app/SetupDiscImport.kt` | 1233 | 1-500; 501-1000; 1001-1233 |
| `android/app/src/main/java/com/dxxredux/app/SetupFileImport.kt` | 832 | 1-500; 501-832 |
| `android/app/src/main/java/com/dxxredux/app/ConfigImportExport.kt` | 846 | 1-500; 501-846 |
| `android/app/src/main/cpp/jni_main.c` | 1910 | 1-500; 501-1000; 1001-1500; 1501-1910 |
| `android/app/src/main/cpp/jni_saf.c` | 112 | 1-112 |
| `android/app/src/main/cpp/jni_level_metadata.cpp` | 1720 | 1-500; 501-1000; 1001-1500; 1501-1720 |
| `android/app/src/main/cpp/shared/jni_string.c` | 89 | 1-89 |
| `android/app/src/main/cpp/shared/android_jni_overlay.c` | 89 | 1-89 |
| `android/app/src/main/AndroidManifest.xml` | 183 | 1-183 |
| `android/app/src/main/res/xml/network_security_config.xml` | 10 | 1-10 |
| `android/app/src/main/res/xml/file_paths.xml` | 10 | 1-10 |

### Archive and installer backends

| File | Lines | Reviewed blocks |
|---|---:|---|
| `android/app/src/main/cpp/extract/stuffit_extract.c` | 515 | 1-500; 501-515 |
| `android/app/src/main/cpp/extract/sti2_extract.c` | 2582 | 1-500; 501-1000; 1001-1500; 1501-2000; 2001-2500; 2501-2582 |
| `android/app/src/main/cpp/extract/sow_extract.c` | 1270 | 1-500; 501-1000; 1001-1270 |
| `android/app/src/main/cpp/extract/hfs_reader.c` | 1408 | 1-500; 501-1000; 1001-1408 |
| `android/app/src/main/cpp/extract/inno_reader.c` | 3753 | 1-500; 501-1000; 1001-1500; 1501-2000; 2001-2500; 2501-3000; 3001-3500; 3501-3753 |
| `android/app/src/main/cpp/extract/pkg_reader.c` | 1021 | 1-500; 501-1000; 1001-1021 |
| `android/app/src/main/cpp/extract/iso9660_reader.c` | 994 | 1-500; 501-994 |
| `android/app/src/main/cpp/extract/cue_parser.c` | 250 | 1-250 |
| `android/app/src/main/cpp/extract/extract_cd.c` | 502 | 1-500; 501-502 |
| `android/app/src/main/cpp/extract/extract_gog.c` | 207 | 1-207 |
| `android/app/src/main/java/com/dxxredux/app/MissionZip.kt` | 712 | 1-500; 501-712 |
| `android/app/src/main/java/com/dxxredux/app/MissionZipExtractionStore.kt` | 771 | 1-500; 501-771 |

### Diagnostics, policy, build, and dependency lock

| File | Lines | Reviewed blocks |
|---|---:|---|
| `android/app/src/main/java/com/dxxredux/app/DebugLog.kt` | 431 | 1-431 |
| `android/app/build.gradle` | 591 | 1-500; 501-591 |
| `android/privacy_policy.md` | 39 | 1-39 |
| `server/Cargo.toml` | 37 | 1-37 |
| `server/Cargo.lock` | 2893 | 1-500; 501-1000; 1001-1500; 1501-2000; 2001-2500; 2501-2893 |

## Confirmed vulnerabilities and proposed fixes

### RV-001 - Critical - Full save-body counts can overrun engine arrays

Confidence: high

Related prior findings: BR-0195, BR-0206, BR-0350, BR-0353, BR-0354,
BR-0355, BR-0360

Evidence:

- `state_android_preflight_coop_restore` in
  `state_android_shared.c:831-859` checks the `DGSS` header/version and
  cooperative metadata, not the legacy body between them.
- D1 `state.c:2583-2596` and D2 `state.c:3571-3582` derive
  `Highest_object_index` from a signed file count and write `Objects[i]`
  without first proving `0 <= count <= MAX_OBJECTS`.
- Subsequent counts reach fixed `Walls`, `ActiveDoors`, `Triggers`,
  `RobotCenters`, and `Station` arrays. D2 additionally has exploding-wall and
  cloaking-wall arrays. Their raw `*_read_n_swap` helpers accept the supplied
  count rather than destination capacity.
- D2 indexes `Robot_info[obj->id]` during restoration after the untrusted object
  body has been read.
- `rewind_file_read` limits a memory read to remaining source bytes but knows
  nothing about destination capacity and does not latch a short-read failure.
- `multi_save_transfer.c` verifies transfer geometry and FNV integrity, then
  calls `state_restore_coop_from_memory`. Integrity proves that the bytes were
  transferred intact; it does not make their internal counts safe.

Impact: a crafted local save or accepted cooperative save/checkpoint can write
beyond fixed native arrays, corrupt memory, or crash. Combined with BR-0195's
missing host-only authority on save-transfer messages, an admitted peer can
potentially drive the remote path.

Proposed repair:

1. Define a checked save cursor with exact remaining-byte arithmetic, typed
   reads, a latched error, and `read_array(destination, capacity, count)`.
2. Implement a complete non-mutating structural preflight for every legacy and
   Android extension section. Validate count domains, record sizes,
   discriminants, object IDs, and references before indexing anything.
3. Prefer decode into a temporary save model or snapshot and commit only after
   semantic validation. At minimum, run a full preflight before
   `StartNewLevelSub` or any global mutation.
4. Reject malformed values; do not clamp them. Preserve every value in the
   valid writer domain, including supported Saturn/Rebirth variants.
5. Authenticate BEGIN/CHUNK/APPLY as host-only and bind them to the current
   session and transfer generation before allocating or applying.

Verification: paired D1/D2 tests for each count at `-1`, `0`, maximum,
maximum+1, and `INT_MAX`; truncation after every field; invalid object types,
IDs, segments, and references; local resume, launcher resume, cooperative
restore, rewind, and checkpoint paths; ASan/UBSan fuzzing; and a valid historic
save corpus with byte-for-byte state expectations.

### RV-002 - Critical - Classic demos trust string lengths and indices

Confidence: high

Related prior finding: BR-0389 fixed only the classic-demo wall-record count

Evidence:

- D1 `newdemo.c:395-401` and D2 `newdemo.c:534-540` read a signed byte `len`
  and pass it to `newdemo_read(str, len, 1)` without a destination capacity.
  Callers include 60-byte HUD messages, `CALLSIGN_LEN+1` callsigns, team names,
  and mission buffers. Lengths above the destination overflow it; negative
  lengths cross a signed/unsigned I/O boundary.
- Start and trailer parsing assign a recorded byte to `N_players` and loop over
  `Players[i]` without first proving `N_players <= MAX_PLAYERS` (for example D2
  `newdemo.c:3440-3453`).
- Multiplayer events use recorded signed-byte `pnum` directly in
  `Players[pnum]` across cloak, death, kill, connect, reconnect, disconnect,
  and score cases (D2 `newdemo.c:2790-2990`, with a corresponding D1 block).
- Some world events validate recorded geometry only for a derived lookup but
  then call the mutator with the original values. D2 wall-hit handling computes
  a guarded `wall_num` at `2330-2331` but calls
  `wall_hit_process(&Segments[segnum], side, ...)` at `2340-2341` regardless.
- Short-position and object decoding uses recorded segments and object IDs in
  `Segments` and `Robot_info` lookups before a uniform semantic admission pass.

Impact: opening a crafted classic demo can cause stack/global buffer overflow,
out-of-bounds array access, invalid table lookup, or world corruption.

Proposed repair: replace `nd_read_string` with
`nd_read_string(dst, capacity)` using an unsigned encoded length, requiring
`1 <= len <= capacity`, a terminating NUL within the encoded field, and an exact
read. Add typed validators for player, segment, side, wall, object, texture,
sound, robot, weapon, and level domains before every event mutates state.
Preflight the demo header/trailer and validate each event atomically before
dispatch. Reuse one checked decoder in D1 and D2 where practical without
changing the on-disk format.

Verification: mutate every length and index field through signed and unsigned
boundaries, truncate every event byte, fuzz event sequences under ASan/UBSan,
and replay the shipped/public legacy demo corpus to identical observable state.

### RV-003 - Critical - Player-file mission count can overflow `HighestLevels`

Confidence: high

Evidence: D1 `playsave.c:1171` reads a signed `NHighestLevels`, then
`playsave.c:1248` uses it as the `PHYSFS_read` element count into a fixed array.
D2 `playsave.c:996-1002` has only a release-disabled `Assert(count <=
MAX_MISSIONS)` and does not reject negative values. Negative values can convert
to very large unsigned counts; positive values above the capacity overwrite
the fixed array.

Proposed repair: require `0 <= count <= MAX_MISSIONS`, prove the remaining byte
span with overflow-safe arithmetic, parse into a temporary `PlayerCfg`, and
publish only after the complete file passes validation. Keep legacy
version/size heuristics after, not instead of, the hard bound.

Verification: both games at negative, zero, maximum, maximum+1, and integer
extremes; truncation at every entry; endian variants; historic `.plr` corpus;
ASan/UBSan.

### RV-004 - Critical - Legacy string and line helpers are destination-unaware

Confidence: high

Evidence: D1/D2 `include/physfsx.h:90-113` implement
`PHYSFSX_readString(file, char *)` and `PHYSFSX_gets(file, char *)` without a
capacity. The string helper reads until NUL/EOF and then calls `strlen`, so an
unterminated input can both overwrite the destination and make `strlen` scan
out of bounds. D2 player parsing calls it with a 10-byte Guide-Bot name and a
128-byte local buffer. D1/D2 netgame-settings parsing uses `PHYSFSX_gets` with a
50-byte stack buffer.

Proposed repair: introduce bounded helpers returning a typed
complete/truncated/I/O-error result, always NUL-terminate on failure, and reject
overlong semantic fields instead of silently truncating them. Migrate all
callers and remove or make the unbounded variants impossible to call.

Verification: empty, exact-capacity, capacity+1, no-NUL, no-newline, embedded
NUL, very large, and EOF-at-every-byte fixtures for every caller.

### RV-005 - High - Cooperative control and save-transfer packets lack uniform authority

Confidence: high

Prior finding: BR-0195

The multiplayer switch does not apply a single host-only policy to all
authoritative cooperative messages. Save-transfer BEGIN/CHUNK/APPLY and READY
handling are especially dangerous when combined with RV-001; other inventory,
difficulty, warp, repair/damage, and restore messages have similar authority or
index gaps.

Proposed repair: create one declarative packet-policy table keyed by message
type, allowed sender role, exact length, game phase, and generation. Validate it
before any handler-specific read or relay. Pass authenticated sender identity
to handlers rather than allowing payloads to self-identify. Preserve protocol
functionality by testing every valid host/client flow and mixed-version refusal
before changing acceptance.

### RV-006 - High - Save/rewind restore is not transactional across peers

Confidence: high

Prior finding: BR-0206

Restore mutates native globals after starting a level and before all later
reads and semantic checks succeed. In cooperative restore, the host and peers
can apply at different times, and a pre-apply READY packet is treated too much
like successful application. A late parse, allocation, timeout, or disconnect
can leave peers on different world generations.

Proposed repair: use a two-phase protocol: authenticate and receive, fully
validate and stage on every required peer, collect staged-success, commit at a
common generation boundary, then acknowledge commit. Failure before commit
leaves the prior state byte-identical. Gate ordinary gameplay packets during
the transition.

### RV-007 - High - Extraction work lacks one cumulative budget

Confidence: high

Prior finding: BR-0018

Individual StuffIt/STI2, SOW, HFS, Inno, PKG, ISO, and mission/archive paths
have gained many local checks, but no single attempt-owned budget consistently
limits nested entries, expanded bytes, sparse output, metadata, decode work,
temporary storage, and recursion across all backends. An archive can remain
locally valid while exhausting aggregate CPU, memory, storage, or file count.

Proposed repair: thread one extraction-budget object through every backend and
nested mission-music/archive materialization path. Charge before allocation,
decode, write, recursion, and publication; use checked arithmetic; cancel
promptly; and roll back only attempt-owned output.

### RV-008 - High - Matchmaking authentication and deployment can fail open

Confidence: high

Prior findings: BR-0106, BR-0107, BR-0108, BR-0143

The server review identified operation without configured Google
authentication, replacement of broken configuration with public defaults,
plaintext service under incomplete TLS configuration, and replayable keypair
authentication without a fresh challenge.

Proposed repair: make production mode fail closed at startup unless identity,
TLS certificate/key, database, public origin, and relay policy are complete.
Use a server nonce, expiry, audience, and one-time replay cache for signed
authentication. Keep explicitly named development modes separate and visibly
unsafe, with production credentials unavailable to them.

### RV-009 - High - Server admission and work are insufficiently bounded

Confidence: high

Prior findings: BR-0115, BR-0116, BR-0117, BR-0125, BR-0126, BR-0133,
BR-0139, BR-0140

Open gaps include WebSocket message/client-field size, ICE-candidate count and
size, an unenforced rate limiter, expensive external identity verification and
status responses under concurrency, relay capacity/lifetime, blocking SQLite
work on async workers, and attacker-driven durable database growth.

Proposed repair: define budgets at connection, identity, room, IP, account, and
global levels; reject oversized frames before JSON allocation; bound every
collection and string; enforce rate limits before expensive work; use bounded
blocking pools and queues; cap durable cardinality/retention; and expose
backpressure metrics.

### RV-010 - High - Lobby/session authorization and revocation have gaps

Confidence: high

Prior findings: BR-0121, BR-0123, BR-0127, BR-0137, BR-0154, BR-0156

Friend-state invariants, ban/session revocation, connection-generation
ownership, database-error handling, kick authority, and chat authority have
open defects. These are authorization issues even if the resulting action is
"only" disruption or impersonation.

Proposed repair: centralize action authorization around the current
authenticated connection generation and room membership. Revoke all live
sessions on ban, fail closed on authorization-store errors, and test stale
connections and reconnect races for every action.

### RV-011 - High - Relay credentials, admission, and capacity need hardening

Confidence: high

Prior findings: BR-0132, BR-0133; new logging observation

Relay tokens are logged by the Android client at
`MatchmakingService.kt:1445` and by server relay/allocation paths. The current
32-bit token functions as a session selector for up to the relay lifetime.
Source-address checks reduce but do not eliminate risk, especially for shared
NAT/source-IP conditions and exported logs. Exact slot/authentication and
capacity/lifetime controls are also open.

Proposed repair: never log raw bearer material; use a keyed short fingerprint
for correlation. Replace the token with at least 128 bits of randomness and
bind it to session, slot, direction, and generation. Authenticate relay
allocation and packets before state growth, cap sessions per identity/IP and
globally, and expire/erase credentials deterministically.

### RV-012 - High - LAN lobby control is unauthenticated

Confidence: high

Prior findings: BR-0342, BR-0488

LAN JSON announcements and control/member identities are forgeable, and
discovery admission can consume resources. A hostile device on the LAN can
impersonate lobbies or members, interfere with ready/launch behavior, or flood
the discovery collection.

Proposed repair: sign a bounded invitation/lobby transcript with an ephemeral
host key, bind joins and subsequent messages to that lobby generation and
source, add nonces, and cap/expire discovery records per source and globally.
Retain an explicit compatibility path only if the UI clearly labels it
unauthenticated.

### RV-013 - High - Dynamic proxy routes are attacker-expandable and teardown races

Confidence: high

Prior findings: BR-0497, BR-0498

The first packet from a new endpoint can consume one of a small number of
dynamic routes, sockets, and coroutines. Concurrent route addition, diagnostic
iteration, and shutdown can invalidate collections or publish work after stop,
leaving ports/coroutines owned by no live generation.

Proposed repair: authenticate or explicitly approve an endpoint before route
allocation; serialize ownership in one actor/state machine; cap pending and
live routes separately; and make shutdown reject publication and join every
child before completion.

### RV-014 - High - Private matchmaking TLS accepts any certificate

Confidence: high

Prior finding: BR-0499

For addresses classified as private, the client uses a trust-all
`X509TrustManager` and hostname verifier. The UI can persist arbitrary server
URLs and sends a fresh Play Games authorization code after connection. DNS is
resolved once for policy and again for connection, introducing a time-of-check
to time-of-use gap.

Proposed repair: confine trust-all behavior to a debug-only test build that
cannot obtain production credentials. For user-configured private servers, use
a local CA, explicit certificate/public-key pin, or clearly confirmed TOFU
fingerprint with change warnings. Resolve/connect under one generation-owned
policy without a second unbound DNS decision.

### RV-015 - High - MVE startup audio trimming races the mixer callback

Confidence: high

Prior finding: BR-0364

Android startup trimming traverses, advances, and frees the same MVE ring-buffer
head used by the SDL_mixer postmix callback without excluding that callback.
An overlap can read freed memory, double-free, corrupt queue state, or crash.

Proposed repair: hold the SDL audio lock across queue measurement and discard,
or give one serialized owner exclusive responsibility for head advancement and
buffer reclamation. Gate/unregister the callback during inactive and teardown
states. Stress with forced callback barriers under ASan/TSan.

### RV-016 - High - UPnP response bodies are unbounded

Confidence: high

Prior finding: BR-0457

UPnP discovery/control uses whole-body `readText` for attacker-controlled HTTP
and SOAP responses. A local device can trickle or stream an unbounded response,
holding a coroutine and growing memory.

Proposed repair: enforce connect/header/body/idle/total deadlines and maximum
header/body sizes while streaming; reject excess immediately; cap redirects
and XML depth/text; bind all work to the active network generation.

### RV-017 - High - Shipped TLS dependency versions have known advisories

Confidence: high for version match; reachability varies by code path

`server/Cargo.toml` pins `rustls = 0.23.37`; `Cargo.lock` contains
`rustls-webpki 0.103.9` and `aws-lc-sys 0.38.0`. Relevant advisories include:

- rustls 0.23.13 through 0.23.44 accepting a TLS 1.3 handshake message at the
  wrong encryption level, patched in 0.23.45:
  <https://rustsec.org/advisories/RUSTSEC-2026-0285.html>
- rustls-webpki CRL parsing panic, patched in 0.103.13:
  <https://rustsec.org/advisories/RUSTSEC-2026-0104.html>
- rustls-webpki name-constraint issues, patched in 0.103.12:
  <https://rustsec.org/advisories/RUSTSEC-2026-0098.html> and
  <https://rustsec.org/advisories/RUSTSEC-2026-0099.html>
- rustls-webpki CRL distribution-point issue, patched in 0.103.10:
  <https://rustsec.org/advisories/RUSTSEC-2026-0049.html>
- `aws-lc-sys` X.509 name-constraint bypass, patched in 0.39.0:
  <https://rustsec.org/advisories/RUSTSEC-2026-0044.html>

Proposed repair: update the compatible rustls/tokio-rustls/reqwest/webpki/
aws-lc dependency graph together to at least rustls 0.23.45,
rustls-webpki 0.103.13, and aws-lc-sys 0.39.0, or newer compatible patched exact
versions. Run the server integration suite, then add a locked `cargo audit` or
OSV policy gate to normal validation with documented advisory exceptions.

## Confirmed lower-severity vulnerabilities and privacy/release defects

| ID | Severity | Evidence / prior finding | Proposed fix and verification |
|---|---|---|---|
| RV-018 | Medium | Provider display names can influence filesystem paths in direct audio, GOG/SOW/CUE staging, and direct game-file flows (BR-0418) | Separate display name from generated storage name; normalize to an allowlisted basename, reject separators/control/reserved names, and enforce canonical containment after creation. Test malicious SAF names across every importer. |
| RV-019 | Medium | Main-source network security config permits cleartext emulator/localhost endpoints in a release resource (BR-0343) | Move permissive policy and endpoints to debug/androidTest manifests/resources; add a release APK manifest/network-policy assertion. |
| RV-020 | Medium | Active debug log size is unbounded and server log retention is not uniformly bounded (BR-0446, BR-0131) | Rotate by bytes and age, cap total generations, handle disk-full without recursive logging, and test sustained hostile traffic. |
| RV-021 | Medium | Backupable preferences retain unused peer callsigns/stable IDs and the privacy policy does not describe actual network/social/relay/log handling (BR-0450, BR-0642) | Stop persisting unused identities, exclude sensitive state from backup, add retention/deletion rules, and update policy/data-safety declarations from a generated data-flow inventory. |
| RV-022 | Medium | Bundletool signing passwords are placed on the process command line (BR-0549) | Use protected temporary password files or environment/file-descriptor support; verify process listings and captured build logs contain no sentinel secret. |
| RV-023 | Medium | Touch join approval does not bind the displayed requester/team strongly enough to the accepted generation (BR-0266) | Carry one immutable request ID and authenticated peer identity from display through acceptance; reject replacement/stale requests and test tap/race interleavings. |
| RV-024 | Medium | Release/profile behavior can diverge from debug-only receiver and endpoint assumptions | Add a release-variant security contract test covering exported components, dynamic receiver flags, cleartext policy, debuggability, backup rules, and test credentials. |

## Likely defects requiring focused confirmation

These have a credible hostile-input or resource path but should receive a
small reproducer before implementation details are finalized.

| Area | Risk | Proposed confirmation/fix |
|---|---|---|
| Mission transfer concurrency | The service has size, hash, chunk, expiry, same-address, and one-use-token checks, but simultaneous/resumed requests may race publication or cleanup | Barrier tests around token consume, sidecar read, file move, cancellation, and service restart; serialize one transfer generation and atomically publish data plus sidecar. |
| Android provider-tree scan (BR-0462) | Deep/wide/cyclic providers can cause unbounded traversal and work without a `CancellationSignal` | Synthetic cyclic and million-entry providers; bound depth, nodes, bytes, and time and propagate cancellation into every provider query. |
| Launcher preparation transactions | Storage faults during sequential marker/config/mod writes can leave a mixed launch generation | Fault inject each write/flush/rename and publish one complete generation pointer only after all sidecars are durable. |
| JNI overlay/callback lifecycle (BR-0044) | Repeated callback installation and exception/lifetime edges can retain references or call stale owners | Stress attach/detach, Activity recreation, process teardown, and injected Java exceptions; use one generation-owned global reference and checked exception boundary. |
| Media decode budgets | Current MVE/MIDI parsing has many local bounds, and HMP event reads were fixed, but aggregate frame/audio/event work still needs a uniform ceiling | Fuzz headers/opcodes and add per-file decoded-pixel, audio-byte, event, loop, and wall-clock budgets. Preserve valid movies and MIDI/HMP corpus output. |

## Defense-in-depth backlog

1. Add libFuzzer/AFL++ entry points for save preflight, player files, classic
   demos, input demos, MVE chunks, HMP/MIDI, CUE, and each archive backend.
2. Run native malformed-input corpora with ASan, UBSan, and allocator hardening;
   run MVE/proxy ownership stress with TSan where supported.
3. Introduce a common checked binary cursor rather than adding isolated bounds
   checks around raw `PHYSFS_read` calls.
4. Give every remote/local operation an attempt generation, cancellation token,
   cumulative work budget, and atomic publication point.
5. Redact secrets at the log API rather than relying on every caller. Add a
   test that exports diagnostics containing sentinel credentials/tokens and
   fails if any sentinel survives.
6. Generate a release security manifest report in CI: components, intent
   filters, receiver flags, providers/paths, cleartext policy, backup policy,
   signing mode, test endpoints, and debug features.
7. Add locked dependency scanning for Rust, Gradle/JVM, NDK/native vendored
   code, and build tools, with a documented update and exception policy.

## Existing defenses worth preserving

- The manifest exports only the launcher and the intentional LAN invitation
  deep-link activity; game, preview, services, and `FileProvider` components are
  not exported.
- The LAN invitation activity parses and validates the `descent:` invitation
  before forwarding a validated address with a fresh identifier.
- Dynamic receivers use explicit policy: internal receivers are not exported
  in release, and externally driven automation/debug receivers are debug-only.
- `file_paths.xml` exposes narrow cache subdirectories rather than app-wide or
  filesystem-wide roots.
- Mission transfer has a hard size, chunk geometry, digest, expiry, one-use
  token, and same-address checks. These should remain while race behavior is
  strengthened.
- Cooperative save transfer already limits total bytes, validates chunk count,
  length and offset, has timeouts/checksums, and checks generation/world visit.
  Sender authority and semantic save validation should be added without
  weakening those controls.
- Newer Android save-footer parsers validate allocator, effect, morph, stuck,
  AI, weapon, and object-link state, and publish object topology only after
  validation. The missing work is the older core body, not removal of these
  checks.
- Previously fixed parser defects include HMP event bounds, classic-demo wall
  record bounds, D1-in-D2 translated references, strict JNI UTF-8/exception
  handling, input-demo weapon/control domains and checkpoint allocation, and
  many backend-specific archive checks.

## Compatibility-preserving implementation order

1. Freeze real compatibility corpora first: D1/D2 saves and pilots across
   versions/endian variants, classic demos, cooperative transfers, input demos,
   movies/HMP, archives, and current LAN/online sessions. Record semantic
   outcomes, not just successful opens.
2. Fix RV-003 and RV-004 with bounded readers and temporary player/config
   publication. These are narrow, high-value changes.
3. Fix RV-002 with one capacity-aware classic-demo decoder and uniform typed
   index validation in both games. Run the full demo corpus and sanitizers.
4. Fix RV-001 by building a complete save-body preflight/staging layer. Do not
   attempt piecemeal clamping. Verify local restore before enabling it for
   cooperative transfer.
5. Fix RV-005 and RV-006 together: authenticated packet policy plus staged,
   all-peers-or-none restore. This is where functional-regression risk is
   highest, so gate it with real host/client compatibility tests, packet
   loss/reorder injection, and explicit old-version refusal behavior.
6. Close release trust failures: RV-008, RV-014, RV-017, RV-019, and RV-022.
7. Apply cumulative budgets to server, relay/proxy, UPnP, provider scans,
   mission transfer, extraction, and media. Favor early rejection and explicit
   user-visible errors over silent truncation.
8. Fix MVE ownership under forced concurrency, then complete logging/privacy
   cleanup and release security-contract automation.

At every stage, the acceptance rule is: valid existing content and valid
network flows retain their behavior; malformed content is rejected before
state mutation; failure leaves the previous generation usable; and limits
produce typed, diagnosable errors rather than hangs or silent partial state.
