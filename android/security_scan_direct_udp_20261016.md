# Direct UDP multiplayer security scan

Date: 2026-10-16

Status: remediation implemented and verified; protocol-level authentication follow-up remains

## Remediation status

The crash-oriented P0/P1 work is implemented in both games. Ingress now fails closed on unknown or malformed outer packets, binds claimed senders to known routes, validates complete command batches before host fanout, preflights object synchronization before mutation, and bounds observer, discovery, receive-loop, and rejection-log work.

| Findings                                                       | Status                                          | Evidence                                                                                                                                                                                                                   |
| -------------------------------------------------------------- | ----------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| UDP-01, UDP-02, UDP-04, UDP-05, UDP-09, UDP-11, UDP-12, UDP-14 | Fixed                                           | Exact/minimum outer framing, pre-index identity checks, bounded observer state, receive budgets, and sampled logs in both `net_udp.c` copies                                                                               |
| UDP-03                                                         | Fixed for memory safety and relay authorization | Proxy indexes, payload, nesting, origin, relay, destination, and allocation are checked in `net_udp_p2p_proxy_shared.c`                                                                                                    |
| UDP-06                                                         | Substantially mitigated                         | Complete batches are side-effect-free validated before forwarding; player/observer/host roles and high-risk player, object, robot, boss, wall, trigger, door, string, and drop fields are checked in both `multi.c` copies |
| UDP-07                                                         | Substantially mitigated                         | D1 gained structural preflight parity; both games validate object records and discriminants before the commit pass                                                                                                         |
| UDP-08                                                         | Fixed on the Android direct-IP path             | Full/lite game-info strings, enums, counts, locations, settings, session/generation/world fields, and sync start locations are checked before use                                                                          |
| UDP-10                                                         | Mitigated                                       | P2P and proxy updates require the packet source to match the claimed current route; authenticated NAT-rebind challenge reuse remains a protocol follow-up                                                                  |
| UDP-13                                                         | Mitigated                                       | Per-source and global discovery reply limits are active; a stateless cookie remains recommended for public Internet hosts                                                                                                  |

Verification completed on 2026-10-16:

- Scoped code quality passed
- Windows D1 and D2 builds passed
- D1 and D2 `test_udp_security_validation` passed
- Android native debug build passed for ARM64, ARMv7, and x86-64; a final incremental rerun covered the last validator changes

No shared `.c` source was inserted into either original D1/D2 CMake source list. Test registration is confined to `android/tests/CMakeLists.txt`.

## Executive summary

The original review found the ordinary UDP game socket unsafe to expose to untrusted Internet traffic. The main weakness was the boundary between `net_udp.c` and the legacy `MULTI_*` command handlers: fixed message lengths were checked, but claimed identities, indexes, strings, object records, and command authority were often trusted, and the host forwarded gameplay packets before semantic validation.

The implemented remediation closes the confirmed crash and out-of-bounds paths, rejects unknown or malformed outer packets, validates complete command batches before mutation or relay, preflights object synchronization, binds claimed senders to their current routes, and bounds discovery and receive-loop work. It also adds targeted malformed-packet coverage for both games and has passed the Windows and Android native builds.

Residual risk remains because source-address binding and the existing session token are not cryptographic peer authentication. Public Internet hosting should still add per-peer message authentication and replay protection, an authenticated NAT-rebind challenge, and a stateless discovery cookie. The legacy command set would also benefit from completing the typed decoder/schema work beyond the currently high-risk commands.

No matchmaking WebSocket, backend, account, or lobby-service logic was reviewed. Proxy/P2P and join-wait packet types remain included because the ordinary game UDP socket accepts them.

## Threat model

- An unauthenticated sender can transmit arbitrary UDP datagrams to a listening host or a client waiting for a direct-IP host
- A connected player or observer can send arbitrary packet types, lengths, field values, ordering, duplicates, and replays
- A malicious direct-IP host can send crafted discovery, sync, object, and gameplay data to a joining client
- UDP source addresses may be spoofed, and an address can change because of NAT rebinding; an address is a route hint, not proof of identity
- The attacker seeks native memory corruption, process termination, unauthorized state changes, desynchronization, resource exhaustion, or reflection

## Severity model

- Critical: a practical remote memory write/corruption path or comparable native-code compromise primitive
- High: a remotely reachable crash, broad unauthorized state mutation, or severe cheap denial of service
- Medium: constrained denial of service, route/session integrity loss, reflection, or a defense gap needing additional conditions
- Low: hardening with bounded impact

## Finding ledger

| ID     | Severity | Reachability                                        | Finding                                                                                                                                 | Primary locations                                                                                                                     | Fix area                               |
| ------ | -------- | --------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------- |
| UDP-01 | High     | Unauthenticated or malicious host                   | Several packet types reach fixed-offset token/parser reads without a minimum or exact length                                            | `d1/main/net_udp.c:1364-1460`, `d2/main/net_udp.c:1370-1466`                                                                          | Outer packet contract                  |
| UDP-02 | High     | Unauthenticated sender to host                      | `UPID_PONG` indexes `Netgame.players[data[1]]` before checking the player number                                                        | `d1/main/net_udp.c:9228-9240`, `d2/main/net_udp.c:9516-9528`                                                                          | Ping/pong validation                   |
| UDP-03 | Critical | Session participant; potentially token-aware sender | Proxy envelope trusts an unchecked destination index, accepts empty/nested payloads, and does not bind the claimed origin to the source | `android/app/src/main/cpp/shared/net/net_udp_p2p_proxy_shared.c:193-224`                                                              | Proxy parser and route authorization   |
| UDP-04 | Critical | Session participant or token-aware sender           | Reliable MDATA ACK accepts an unchecked sender index and is not bound to a source address or session token                              | `d1/main/net_udp.c:7815-7860`, `d2/main/net_udp.c:8059-8104`                                                                          | Reliable transport authentication      |
| UDP-05 | Critical | Admitted peer                                       | MDATA outer player number is used by address/reliability helpers without a complete range check                                         | `d1/main/net_udp.c:8257-8325`, `d2/main/net_udp.c:8516-8584`                                                                          | MDATA envelope validation              |
| UDP-06 | Critical | Admitted peer; host relays to all clients           | Legacy `MULTI_*` handlers commonly trust indexes and claimed identities; host forwarding happens before semantic validation             | `d1/main/net_udp.c:4571-4596`, `d2/main/net_udp.c:4731-4756`, `d1/main/multi.c:3347-3418`, `d2/main/multi.c:3559-3630`                | Command schema and authority policy    |
| UDP-07 | Critical | Malicious direct-IP host                            | Object sync commits unvalidated player/object/segment/type data; D1 also lacks D2's complete structural preflight                       | `d1/main/net_udp.c:3116-3280`, `d2/main/net_udp.c:3148-3352`                                                                          | Transactional object-sync decoder      |
| UDP-08 | High     | Malicious host; LAN discovery sender for lite data  | Game-info strings and several semantic fields are not validated before copy/use; sync locations index `Player_init`                     | `android/app/src/main/cpp/shared/net/net_udp_android.c:234-301`, `d1/main/net_udp.c:6322-6329`, `d2/main/net_udp.c:6521-6528`         | Game-info semantic validator           |
| UDP-09 | High     | Malicious host or joining client                    | Sequence callsigns need not terminate; `ADDPLAYER` can carry an unchecked player slot into writes                                       | `d1/main/net_udp.c:2142-2188,2390-2450`, `d2/main/net_udp.c:2149-2195,2398-2458`                                                      | Sequence decoder                       |
| UDP-10 | High     | Admitted peer or token-aware sender                 | P2P ping/pong can claim another player and replace that player's route without proof of ownership                                       | `android/app/src/main/cpp/shared/net/net_udp_p2p_proxy_shared.c:76-126`, `d1/main/net_udp.c:9138-9212`, `d2/main/net_udp.c:9426-9500` | Rebinding challenge and origin binding |
| UDP-11 | High     | Admitted peer; D2 host with observer delay          | D2 wraps the observer message index at the byte-buffer size instead of the metadata-array size, causing out-of-bounds metadata writes   | `d2/main/net_udp.c:8689-8731`                                                                                                         | Observer queue                         |
| UDP-12 | Critical | Malicious host or relayed peer command              | `MULTI_OBS_UPDATE` raises wire-derived capacity and copies past `Netgame.observers[MAX_OBSERVERS]`                                      | `d1/main/multi.c:5313-5333`, `d2/main/multi.c:7066-7086`                                                                              | Observer command validation            |
| UDP-13 | Medium   | Unauthenticated Internet sender                     | A small info request elicits a much larger response; the old response limiter is disabled                                               | `d1/main/net_udp.c:3780-3840`, `d2/main/net_udp.c:3810-3870`                                                                          | Discovery cookie/rate limiting         |
| UDP-14 | Medium   | Any sender able to flood the socket                 | Each frame drains every queued datagram without a packet, byte, or time budget; rejection logging adds cost                             | `d1/main/net_udp.c:7314-7388`, `d2/main/net_udp.c:7546-7622`                                                                          | Receive budgeting and log sampling     |

`Critical` here does not claim demonstrated code execution. It marks attacker-controlled native out-of-bounds writes or broad unsafe object construction that must be treated as potential memory corruption until sanitizer testing proves a narrower impact.

## Detailed findings

### UDP-01: outer length checks occur too late or do not exist

`pass_security_check` runs `valid_size`, then `valid_token`, sender, and state checks. That order is sound only if `valid_size` establishes every later read. It currently does not:

- `UPID_GAME_INFO_LITE_REQ`: its exact-size case is commented out, but `net_udp_check_game_info_request` reads fixed offsets through the request ID/version fields
- `UPID_ADDPLAYER`: accepted as variable-size, then `valid_token` reads bytes 1-4 and the sequence decoder reads the full `UPID_SEQUENCE_SIZE`
- `UPID_OBJECT_DATA`: only a maximum is enforced; `valid_token` immediately reads bytes 1-4, and the parser reads the object count at byte 5
- `UPID_PROXY`: no size case; token validation reads bytes 1-4 and the proxy handler reads bytes 5 and 6
- `UPID_OBSQUIT`: no size case before token validation
- `UPID_MDATA_ACK`: exact length is checked inside its handler rather than at the common boundary
- Unknown identifiers default to valid size rather than being rejected

Fix this with one exhaustive packet-shape table evaluated before token, sender, state, logging, or dispatch. Each entry should declare exact/min/max size, whether it carries a session or player token, valid receiver role, valid protocol states, and whether proxy wrapping is allowed. Unknown identifiers should fail closed. The table should cover inner `UPID_JOIN_DATA` payloads too.

### UDP-02: PONG player index is dereferenced before validation

Both games compare the incoming timestamp against `Netgame.players[data[1]]` before checking whether `data[1] < MAX_PLAYERS`. `UPID_PONG` has the expected 10-byte length, but it has no token and `valid_ip` does not bind it to a player. A 10-byte datagram sent to a host can therefore select an out-of-range player entry.

Validate the player number first, require an active non-local slot, require the datagram source to match that slot, and only then read the per-player ping state. A per-ping unpredictable cookie is preferable if spoofed source traffic is in scope.

### UDP-03: unsafe proxy envelope

`net_udp_process_proxy` reads the declared origin and destination before enforcing a minimum header. It validates `from_player`, but not `to_player`; the forwarding path indexes connection and player arrays using `to_player`. It only checks whether the source belongs to any player, not whether it belongs to the claimed `from_player`. A header-only packet recursively dispatches a zero-length payload, and nested proxy packets can recurse again.

Require at least `UPID_PROXY_HEADER_SIZE + 1`, validate both player indexes and connection state, bind source to the declared origin or an explicitly authorized relay, reject proxy-in-proxy, and pass a verified logical origin into the inner dispatcher. Prefer a non-recursive unwrap followed by the normal packet-contract check.

### UDP-04 and UDP-05: reliable envelope identity is unsafe

The ACK packet has no session token. Its claimed sender is used as the column in `player_ack[sender_pnum]` after only matching packet number/destination state. A forged ACK can suppress retransmission; an out-of-range claimed sender can write outside the ACK array when a queue entry matches.

The MDATA receiver reads `pnum = data[5]`; the `pnum < 0` branch is ineffective for an unsigned byte, and there is no complete `pnum < MAX_PLAYERS` check before `is_player_ip` or reliable receive-window indexing. Observer identity needs a separate explicit representation, not an exception that leaks into player-indexed helpers.

Add exact ACK framing and session/world identifiers. Resolve a verified sender from the source route before queue lookup, validate the claimed sender against it, and index only with that verified value. If per-peer session keys are introduced, MAC the ACK and MDATA header/body and add a replay counter.

### UDP-06: inner command validation and authority are incomplete

`multi_process_bigdata` correctly rejects unknown opcodes and messages whose declared fixed length exceeds the remaining packet. That prevents inner truncation but does not validate values or authorization. Examples found in both games include:

- `MULTI_FIRE`, `MULTI_POSITION`, cloak/invulnerability, sound, score, damage, repair, typing, rank, bounty, and several kill paths use an unchecked or incompletely checked player number
- `MULTI_MESSAGE` and observer chat use C string routines on a fixed-size wire field without first proving a terminator; the sender player index is also unchecked
- `MULTI_REAPPEAR`, reactor destruction/fire, powerup/object operations, and robot messages use object numbers before complete range/type/ownership checks
- D2 `MULTI_WALL_STATUS`, `MULTI_START_TRIGGER`, and `MULTI_MARKER` index wall, trigger, and marker arrays without bounds checks
- D2 `MULTI_ACTIVE_DOOR` accepts an unbounded `Num_open_doors` and reads the slot from global `multibuf[1]` instead of the handler's `buf[1]`
- hostage door uses `wallnum > Num_walls` instead of `>=`; boss teleport uses the same off-by-one shape for its segment-list index
- robot respawn trusts robot type, segment, remote player, and remote object identifiers, then maps even if creation returns `-1`

`Assert` maps to `assert`; it is not a network validation mechanism in release builds. More importantly, most legacy commands do not compare their claimed player to `authenticated_sender`. On clients, relayed MDATA currently resolves the UDP source as the host, so the original player identity is lost. The host forwards MDATA to observers/clients before semantic parsing, allowing one bad peer to fan a packet out.

Introduce a command schema indexed by `MULTI_*`: declared size, allowed sender role, claimed-player offset (if any), and a side-effect-free semantic validator. On the host, validate the complete MDATA batch before forwarding or executing it. Relay a verified logical origin. On clients, validate again before mutation. Keep local checks inside dangerous handlers as defense in depth.

### UDP-07: object synchronization is not transactional

D2 preflights record boundaries and exact packet consumption; D1 should receive the same structural check. Neither game validates all semantics before state changes. A clear record assigns `my_pnum` from the wire and calls `change_playernum_to`. Object records can select a remote object number used in `Objects[]`, set `Highest_object_index`, deserialize arbitrary object discriminants, and link an unvalidated segment. Later render/control code trusts object type, ID, control type, movement type, render type, model/vclip references, attachments, and ownership mappings.

Decode every record into temporary storage. Validate assigned player, owner, local/remote object indexes, segment, all discriminants, type-specific IDs, model/vclip references, attachment references, and count relationships. Reject the whole datagram without side effects on any failure. Commit only after the packet passes. Keep end-of-stream verification, but do not use it as a substitute for per-record safety.

### UDP-08 and UDP-09: strings and sync semantics

The Android full-info preflight is a useful defense: it requires exact serialized size and checks type, master slot, player counts, session/player token, generation, world visit, and D2 asset identity. It does not prove termination of all copied callsigns/game/mission strings or validate all enums and indexes. Lite info is accepted from discovery senders and likewise needs termination checks. During sync, `Netgame.locations[i]` directly indexes `Player_init`.

Sequence packets copy `CALLSIGN_LEN + 1` bytes without rejecting a missing terminator. `ADDPLAYER` obtains its target slot from the sequence payload and writes player/netgame/connection arrays with only assertions or assumptions from the sender.

Extend preflight to validate every fixed string and all player counts, locations, colors, game mode/status, difficulty, booleans, rate fields, and other enums. Parse into a temporary `netgame_info` and commit after validation. Require exact sequence size, canonical callsign termination/content, valid flags, and a runtime slot check before `net_udp_new_player` changes state.

### UDP-10: P2P address updates are self-asserted

P2P ping and pong packets have exact lengths and a session token, but the token is distributed in game-info and is a session discriminator, not a secret authenticator. A participant can claim another `from_player`; direct ping/pong then updates the claimed player's stored address from the datagram source. This can redirect traffic, disrupt liveness, and confuse later source checks.

Treat rebinding as a state machine. Send a fresh challenge to both the old/host-confirmed route and candidate route, update only after proof tied to the player's reconnect identity, and rate-limit attempts. A proxied packet must retain a verified logical origin instead of inheriting the relay address.

### UDP-11 and UDP-12: observer memory corruption

D1 increments `cur_obs_msg` and wraps it at `MAX_OBS_MESSAGES`. D2 instead compares against `MAX_OBS_MESSAGES * MAX_MESSAGE_SIZE`, even though the timestamp, offset, length, and ACK metadata arrays each have only `MAX_OBS_MESSAGES` entries. With observer delay enabled, normal queued traffic eventually writes past all four arrays.

`MULTI_OBS_UPDATE` accepts `buf[3]` as the observer count, raises `Netgame.max_numobservers` to it, and then copies that many eight-byte entries into the fixed `Netgame.observers[MAX_OBSERVERS]` array. The wire message contains only `MAX_OBSERVERS` entries. A malicious host can trigger this directly; because legacy command authority is incomplete, a peer-originated command may also be relayed as host traffic.

Change the D2 wrap bound to `MAX_OBS_MESSAGES` and add compile-time/central ring helpers so D1/D2 cannot drift. For observer updates, require host authority, `buf[3] <= MAX_OBSERVERS`, a valid event/slot pair, and explicit string termination. Never increase a local capacity field from a wire count.

### UDP-13 and UDP-14: amplification and scheduling denial of service

A 13-byte full-info request can elicit a response of roughly a kilobyte or more. The previous send limiter in `net_udp_send_game_info` is commented out. On networks where source spoofing is possible, this is a reflection/amplification service. Separately, `net_udp_listen` drains all pending datagrams on up to three sockets every frame. An attacker can keep the loop busy, and synchronous per-rejection logging increases CPU and storage work.

For Internet hosting, use a compact stateless cookie challenge before the full response. At minimum, install per-source and global token buckets for info replies, joins, reconnect attempts, and invalid packets. Give receive processing packet/byte/time budgets per frame, preserve fair service across sockets, and sample repeated rejection logs while retaining counters.

## Top-level packet contract ledger

Legend: `exact` means common ingress enforces exact length; `range` means a minimum/maximum is enforced; `late` means the handler checks after common gates; `missing` means no sufficient common check. "Token" is only a session discriminator unless backed by reconnect proof or a future MAC.

| Packet                                       | Size contract now                                                 | Sender/state contract now                     | Audit result                                                 |
| -------------------------------------------- | ----------------------------------------------------------------- | --------------------------------------------- | ------------------------------------------------------------ |
| `VERSION_DENY` (1)                           | exact 9                                                           | host-only receiver/IP                         | structurally acceptable                                      |
| `GAME_INFO_REQ` (2)                          | exact 13                                                          | host receiver; arbitrary source               | amplification/rate gap                                       |
| `GAME_INFO` (3)                              | Android exact preflight; common says variable                     | host IP; connect state                        | semantic/string gaps                                         |
| `GAME_INFO_LITE_REQ` (4)                     | missing (expected 11)                                             | host receiver; arbitrary source               | pre-dispatch OOB read                                        |
| `GAME_INFO_LITE` (5)                         | exact                                                             | discovery path accepts advertised host        | fixed-string validation gap                                  |
| `DUMP` (6)                                   | exact                                                             | host IP; player/session token                 | acceptable framing                                           |
| `ADDPLAYER` (7)                              | missing (expected sequence size)                                  | host IP and token                             | OOB plus unchecked slot/string                               |
| `REQUEST` (8)                                | exact sequence size                                               | host receiver; reconnect identity on Android  | callsign semantic gap                                        |
| `QUIT_JOINING` (9)                           | exact sequence size                                               | host receiver                                 | callsign semantic gap                                        |
| `SYNC` (10)                                  | Android exact full-info preflight                                 | host/join envelope                            | semantic/index gaps                                          |
| `OBJECT_DATA` (11)                           | maximum only; D2 record preflight                                 | host/join envelope and player token           | minimum and semantic gaps                                    |
| `PING` (12)                                  | exact 37                                                          | host IP                                       | acceptable framing                                           |
| `PONG` (13)                                  | exact 10                                                          | host receiver only                            | unauthenticated index-before-check                           |
| `ENDLEVEL_H` (14)                            | exact on Android                                                  | host IP, token, state/world checks            | preserve defenses                                            |
| `ENDLEVEL_C` (15)                            | exact on Android                                                  | host receiver, token, state/world checks      | verify claimed player/source in final schema                 |
| `PDATA` (16)                                 | protocol-dependent exact on Android                               | token plus handler source/world checks        | preserve defenses                                            |
| `MDATA_PNORM` (17)                           | bounded range on Android                                          | token; handler source check incomplete        | unsafe outer identity and inner commands                     |
| `MDATA_PNEEDACK` (18)                        | bounded range on Android                                          | token; handler source check incomplete        | same plus reliability state                                  |
| `MDATA_ACK` (19)                             | late exact 7                                                      | no token/source binding                       | unchecked index/write and forged ACK                         |
| `TRACKER_VERIFY` / `TRACKER_INCGAME` (21/22) | conditional build; not in common table                            | tracker socket only when enabled              | legacy tracker service excluded; fail closed on game sockets |
| `P2P_PING` (25)                              | exact                                                             | token                                         | claimed-origin/rebinding gap                                 |
| `P2P_PONG` (26)                              | exact                                                             | token                                         | claimed-origin/rebinding gap                                 |
| `PROXY` (27)                                 | missing                                                           | token; any player source                      | unchecked indexes/empty recursion/origin gap                 |
| `REATTEMPT_DIRECT` (28)                      | exact                                                             | host IP and token                             | validate embedded slot/address semantics                     |
| `OBSDATA` (29)                               | bounded like normal MDATA                                         | token; observer/player branching              | outer identity and inner command gaps                        |
| `OBSQUIT` (30)                               | missing (expected 9)                                              | token; observer route in handler              | pre-dispatch OOB read                                        |
| `RECONNECT_CHALLENGE` (31)                   | exact                                                             | host IP/token, cryptographic transcript       | good defense; preserve                                       |
| `RECONNECT_PROOF` (32)                       | exact                                                             | host receiver/token, route proof              | good defense; preserve/rate-limit                            |
| `JOIN_QUERY` (33)                            | exact 17                                                          | unproxied host receiver; session/query fields | good pre-admission envelope                                  |
| `JOIN_STATUS` (34)                           | exact 57                                                          | unproxied client receiver; host/session/query | good pre-admission envelope                                  |
| `JOIN_DATA` (35)                             | header 17 plus nonempty inner packet                              | active wait, host IP, session/query/visit     | good envelope; inner object/sync semantics still unsafe      |
| unknown ID                                   | accepted by common size/sender/token/state defaults, then ignored | none                                          | change to fail closed                                        |

## Encapsulated `MULTI_*` command ledger

Every opcode is length-framed by `message_length[]`: the parser rejects an unknown opcode or a command whose declared size does not fit in the MDATA body. The classification below records the stronger property found in this review:

- `L`: only the central length guarantee can be relied on; one or more semantic/authority properties remain unproven
- `P`: meaningful local validation exists, but it is incomplete or not consistently bound to authenticated origin
- `B`: newer path explicitly receives authenticated origin and/or uses a dedicated validator; retain and integrate into the central schema
- `N`: declared but unused/no state handler; still reject from the network unless explicitly allowed

This is deliberately conservative: `L` does not mean the handler has no checks, only that it cannot be considered safe at the network boundary.

### Commands present in both games

| Class | Commands and declared wire lengths (`D1/D2` when different)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| ----- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| L     | `POSITION(25)`, `REAPPEAR(4)`, `FIRE(20)`, `KILL(5)`, `MESSAGE(37)`, `OBS_MESSAGE(47)`, `QUIT(2)`, `PLAY_SOUND(4)`, `CONTROLCEN(4)`, `ROBOT_CLAIM(5)`, `CLOAK(2)`, `INVULN(2)`, `CREATE_EXPLOSION(2)`, `CONTROLCEN_FIRE(16)`, `CREATE_POWERUP(19 / 19+sizeof(vms_vector))`, `DECLOAK(2)`, `DEINVULN(2)`, `ROBOT_POSITION(5+sizeof(shortpos) / 6+sizeof(shortpos))`, `DOOR_OPEN(4 / 6)`, `ROBOT_EXPLODE(8 / 9)`, `ROBOT_RELEASE(5)`, `ROBOT_FIRE(18)`, `SCORE(6)`, `CREATE_ROBOT(6)`, `BOSS_ACTIONS(10)`, `CREATE_ROBOT_POWERUPS(27)`, `HOSTAGE_DOOR(7)`, `SAVE_GAME(26)`, `RESTORE_GAME(6)`, `HEARTBEAT(5)`, `KILLGOALS(11)`, `POWCAP_UPDATE(MAX_POWERUP_TYPES+1)`, `DO_BOUNTY(2)`, `TYPING_STATE(3)`, `GMODE_UPDATE(3)`, `KILL_HOST(7)`, `KILL_CLIENT(5)`, `RANK(3)`, `RESPAWN_ROBOT(60)`, `OBS_UPDATE(4+8*MAX_OBSERVERS)`, `DAMAGE(14)`, `REPAIR(11)`, `CREATE_EXPLOSION2(24)`, `DIFFICULTY(3)` |
| P     | `REMOVE_OBJECT(5)`, `PLAYER_EXPLODE(MULTI_PLAYER_DROP_LENGTH)`, `PLAYER_DROP(MULTI_PLAYER_DROP_LENGTH)`, `ENDLEVEL_START(3)`, `TRIGGER(3)`, `SHIP_STATUS(MULTI_SHIP_STATUS_LENGTH)`, `WARP_TO_PLAYER(17)`, `COOP_PEER_STATUS(8)`                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| B     | `COOP_RESTORE_INV(MULTI_COOP_RESTORE_LENGTH)`, `REWIND_REQUEST(3)`, `REWIND_RESULT(4)`, `REWIND_SAVE_BEGIN(MULTI_REWIND_SAVE_BEGIN_LEN)`, `REWIND_SAVE_CHUNK(MULTI_REWIND_SAVE_CHUNK_LEN)`, `REWIND_SAVE_APPLY(4)`, `REWIND_SAVE_READY(12)`, `COOP_RESTORE_STATUS(6)`, `COOP_POWERUP_COLLECTED(10)`, `COOP_POWERUP_SNAPSHOT_BEGIN(5)`, `COOP_POWERUP_SNAPSHOT_ENTRY(55)`, `COOP_POWERUP_SNAPSHOT_END(5)`, `REACTOR_PAUSE(7)`, `MATCEN_MODE(3+MATCEN_MODE_MAX_CENTERS)`, `COOP_RECOVERY(160)`, `COOP_BRIEFING(136)`, `COOP_TRAVEL(228)`, `COOP_ENDGAME(160)`                                                                                                                                                                                                                                                                                                                                       |
| N     | `END_SYNC(4)`, `BEGIN_SYNC(37 / 41)`, `MENU_CHOICE(2)` (no active receive mutation in current switch)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             |

`PLAYER_DROP_LENGTH` is 65/57 in D1 and 114/106 in D2 depending on Android feature build. `SHIP_STATUS_LENGTH` is 54/45 in D1 and 84/70 in D2. `COOP_RESTORE_LENGTH` is 102/86. The rewind chunk length is `8 + 432`.

### D2-only commands

| Class | Commands and declared wire lengths                                                                                                                                                                                                                                                                                                                                               |
| ----- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| L     | `MARKER(55)`, `DROP_WEAPON(12)`, `GUIDED(3+sizeof(shortpos))`, `STOLEN_ITEMS(18)`, `WALL_STATUS(6)`, `SEISMIC(9)`, `LIGHT(18)`, `START_TRIGGER(2)`, `FLAGS(6)`, `DROP_BLOB(2)`, `ACTIVE_DOOR(sizeof(active_door)+3)`, `SOUND_FUNCTION(4)`, `CAPTURE_BONUS(2)`, `GOT_FLAG(2)`, `DROP_FLAG(12)`, `FINISH_GAME(2)`, `ORB_BONUS(3)`, `GOT_ORB(2)`, `DROP_ORB(12)`, `PLAY_BY_PLAY(4)` |
| P     | `ESCORT_OWNER(9)`, `ESCORT_SPAWN(11+sizeof(shortpos))`                                                                                                                                                                                                                                                                                                                           |
| B     | `COOP_TRIGGER_MESSAGE(2+64)`, `GUIDEBOT_GOAL(GUIDEBOT_GOAL_PACKET_LEN)`                                                                                                                                                                                                                                                                                                          |
| N     | `ROBOT_CONTROLS(1)`, `MODEM_PING(1)`, `MODEM_PING_RETURN(1)`                                                                                                                                                                                                                                                                                                                     |

Before implementation, encode this inventory as compile-time data rather than maintaining prose and a switch independently. A test should iterate every enum value, assert one contract entry, and verify reject/accept behavior at `size-1`, `size`, and `size+1` where applicable.

## Approximately 500-line block coverage

### `d1/main/net_udp.c` (9,672 lines)

| Lines     | Primary content                       | Result                                          |
| --------- | ------------------------------------- | ----------------------------------------------- |
| 1-500     | declarations, globals, test hooks     | state arrays and observer queue identified      |
| 501-1000  | socket wrappers/open/receive          | datagram truncation and route handling reviewed |
| 1001-1500 | receive helpers and security gates    | UDP-01; token is not authentication             |
| 1501-2000 | connect/direct-join UI                | state/timeouts reviewed                         |
| 2001-2500 | sequence codec and new-player setup   | UDP-09                                          |
| 2501-3000 | welcome/join/object sender            | join state and envelope reviewed                |
| 3001-3500 | object receiver/rejoin/add-player     | UDP-07; D1 structural parity gap                |
| 3501-4000 | discovery/endlevel/info serialization | UDP-13                                          |
| 4001-4500 | full/lite parser                      | UDP-08                                          |
| 4501-5000 | dispatcher and endlevel paths         | pre-validation forwarding in UDP-06             |
| 5001-5500 | lobby and options                     | no new wire memory-safety finding               |
| 5501-6000 | lobby setup/presets                   | local configuration only                        |
| 6001-6500 | sync receiver/sender                  | UDP-08 location indexing                        |
| 6501-7000 | team/start/wait                       | state transitions reviewed                      |
| 7001-7500 | leave/listen/send/timeout             | UDP-14                                          |
| 7501-8000 | frame/reliable queues/ACK             | UDP-04                                          |
| 8001-8500 | MDATA and observer relay/buffer       | UDP-05/06; D1 ring wrap is correct              |
| 8501-9000 | PDATA                                 | existing exact/source/world checks noted        |
| 9001-9500 | P2P, ping/pong, refusal, extras       | UDP-02/10                                       |
| 9501-9672 | UI and observer quit                  | UDP-01 observer-quit size gap                   |

### `d2/main/net_udp.c` (10,018 lines)

| Lines       | Primary content                       | Result                                          |
| ----------- | ------------------------------------- | ----------------------------------------------- |
| 1-500       | declarations, globals, test hooks     | state arrays and observer queue identified      |
| 501-1000    | socket wrappers/open/receive          | datagram truncation and route handling reviewed |
| 1001-1500   | receive helpers and security gates    | UDP-01; token is not authentication             |
| 1501-2000   | connect/direct-join UI                | state/timeouts reviewed                         |
| 2001-2500   | sequence codec and new-player setup   | UDP-09                                          |
| 2501-3000   | welcome/join/object sender            | join state and envelope reviewed                |
| 3001-3500   | object receiver/rejoin/add-player     | UDP-07; structural preflight present            |
| 3501-4000   | discovery/endlevel/info serialization | UDP-13                                          |
| 4001-4500   | game-info parser                      | UDP-08                                          |
| 4501-5000   | dispatcher                            | pre-validation forwarding in UDP-06             |
| 5001-5500   | endlevel/lobby                        | state validation reviewed                       |
| 5501-6000   | lobby options/defaults                | local configuration only                        |
| 6001-6500   | presets/setup/sync receive            | UDP-08                                          |
| 6501-7000   | sync/send/team/start                  | location indexing and state transitions         |
| 7001-7500   | wait/join/leave                       | join-wait integration reviewed                  |
| 7501-8000   | flush/listen/frame/reliable           | UDP-14                                          |
| 8001-8500   | ACK/queues/send MDATA                 | UDP-04                                          |
| 8501-9000   | MDATA/observer/PDATA send             | UDP-05/06 and UDP-11                            |
| 9001-9500   | PDATA/P2P ping                        | existing PDATA checks; UDP-10                   |
| 9501-10000  | pong/refuse/extras/UI/observer send   | UDP-02                                          |
| 10001-10018 | observer quit                         | UDP-01 observer-quit size gap                   |

### Zoomed supporting modules

| Module/blocks                                                                  | Review result                                                                                                                                                            |
| ------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `d1/main/multi.c` 1-6352 in 500-line blocks                                    | fixed-length parser is sound; legacy player/object/string/authority family is UDP-06; observer update is UDP-12; newer co-op handlers generally use dedicated validation |
| `d2/main/multi.c` 1-8397 in 500-line blocks                                    | same family plus unchecked marker/wall/trigger/active-door handlers; object conversion trusts discriminants supplied by sync                                             |
| `d1/main/multibot.c` 1-1413 and `d2/main/multibot.c` 1-1649 in 500-line blocks | robot create/fire/respawn identifiers need schema validation; respawn maps failed object creation                                                                        |
| `net_udp_android.c` 1-963                                                      | reconnect challenge/proof is a strong boundary; full-info preflight needs semantic/string expansion                                                                      |
| `net_udp_android_autonet_shared.c` 1-236                                       | local automation/configuration, no new remote ingress defect                                                                                                             |
| `net_udp_join_wait.c` 1-285 and `net_udp_join_wait_transport.h` 1-226          | exact query/status and session/query/visit binding are good defenses                                                                                                     |
| `net_udp_p2p_proxy_shared.c` 1-302                                             | UDP-03 and UDP-10                                                                                                                                                        |
| `net_udp_reconnect_auth.c` 1-453 and `net_udp_reconnect_jni.c` 1-295           | bounded encodings and cryptographic reconnect proof; retain and reuse for route ownership                                                                                |
| initial-sync/endlevel/score-catchup helpers, each under 100 lines              | generation/world fencing is useful and should remain after centralization                                                                                                |
| `multi_save_transfer.c` 1-1563 in 500-line blocks                              | chunk/count/sender checks are substantially stronger than legacy commands; no new confirmed direct-UDP memory defect found                                               |

## Existing defenses worth preserving

- The socket layer supplies actual datagram length and reports possible truncation when the receive buffer fills
- Many fixed-size packet types already require exact lengths
- Android PDATA and endlevel packets have exact-size and source/world checks
- The join-data envelope binds host, session, query token, world visit, and allowed inner types
- Reconnect uses signed identity plus challenge/proof and route admission, not callsign/address alone
- `multi_process_bigdata` validates opcode and declared fixed length before dispatch
- D2 object sync preflights record boundaries and exact consumption
- World-generation stamps reject stale co-op gameplay during travel/recovery
- Shared co-op save/rewind/travel handlers generally have stronger sender, range, and phase checks than legacy handlers

## Prioritized fix ledger

| Order | Work item                                                                                                            | Findings closed/reduced           | Suggested location                                                               |
| ----- | -------------------------------------------------------------------------------------------------------------------- | --------------------------------- | -------------------------------------------------------------------------------- |
| P0-1  | Add shared table-driven UPID contract and fail-closed dispatch; enforce before all field reads                       | UDP-01, part of 03/04/09          | new shared helper under `android/app/src/main/cpp/shared/net/`, thin D1/D2 hooks |
| P0-2  | Fix PONG, proxy, ACK, MDATA player-index validation and bind source to verified logical origin                       | UDP-02-05                         | shared validators plus small D1/D2 call-site changes                             |
| P0-3  | Validate complete MDATA batches before forwarding; add command authority/schema table                                | UDP-06, UDP-12 and portions of 10 | shared validator with game-specific callbacks where constants differ             |
| P0-4  | Fix D2 observer ring wrap and bound/authenticate observer update                                                     | UDP-11/12                         | D2 one-line correction plus shared observer validator                            |
| P0-5  | Make object sync side-effect-free until complete structural and semantic validation; port D2 framing preflight to D1 | UDP-07                            | shared record validator, D1/D2 commit loops                                      |
| P1-1  | Validate all game-info/sync/sequence strings, enums, counts, locations, and slots before commit                      | UDP-08/09                         | extend `android_net_udp_game_info_preflight`; add sequence preflight             |
| P1-2  | Challenge route changes with reconnect identity; stop self-asserted P2P address replacement                          | UDP-10                            | reuse reconnect-auth primitives                                                  |
| P1-3  | Add discovery/join/reconnect token buckets, receive budgets, and sampled logging                                     | UDP-13/14                         | shared rate/budget helper                                                        |
| P2-1  | Derive per-peer session keys after join and MAC state-changing packets/ACKs with replay counters                     | strengthens 03-06/10              | protocol-versioned extension                                                     |
| P2-2  | Replace remaining raw wire-to-struct/object conversions with typed decoders                                          | defense in depth                  | incremental by packet/command family                                             |

Keep changes out of the original-source lists in `d1/main/CMakeLists.txt` and `d2/main/CMakeLists.txt`: shared implementation should be registered in the Android shared-source target/list, with only minimal hooks in D1/D2.

## Test strategy

1. Add a host-side datagram harness around the shared UPID validator and run every packet ID at lengths 0 through its minimum/exact boundary, plus `UPID_MAX_SIZE` and one byte over
2. Add regression vectors for `PONG pnum=255`, proxy `to/from=255`, empty/nested proxy, ACK sender 255, MDATA sender 255, sequence without NUL, and observer count 255
3. Add table-driven MDATA tests for every opcode: wrong claimed player, maximum player/object/segment/wall/trigger values, invalid enums, and unterminated string payloads
4. Run host and client parsers under ASan/UBSan using malformed batches and object-sync streams; verify rejection leaves state byte-for-byte unchanged
5. Add a malicious-client integration test proving the host does not relay a rejected command to a second client
6. Add a malicious-host join test for bad object indexes, player assignment, segment, object discriminants, and sync locations
7. Add observer-delay traffic exceeding `MAX_OBS_MESSAGES` and verify bounded ring behavior in both games
8. Add rate tests proving discovery amplification, invalid-packet logging, and receive work remain within configured budgets

## Residual risk after the P0 work

Strict bounds and authority checks will make crashes much harder, but a session token visible in game info cannot authenticate a hostile peer or off-path spoofer who learns it. Direct Internet hosting should ultimately use per-peer keys and authenticated state-changing datagrams. Until then, source binding plus host-side semantic validation is a substantial improvement, but it is not equivalent to cryptographic peer identity.
