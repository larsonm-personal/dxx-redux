# Matchmaking and client protocol security review

Date: 2026-10-21

Status: static review complete; implementation changes intentionally deferred while another worker owns the branch

## Executive summary

The custom UDP relay is substantially safer than a general TURN server. A relay allocation is fixed to two authenticated lobby identities, each direction has a separate AES-256-GCM key, endpoints must complete a challenge/response bind, packets have replay protection, and the destination is the other proven endpoint rather than an address supplied in each datagram. A relay client therefore cannot directly use the server to send arbitrary packets to an unrelated third party.

The largest confirmed gap is the separate ICE candidate path. The matchmaking server accepts client-asserted `host`, `srflx`, `upnp`, and `predicted` addresses without proving that public addresses belong to the authenticated connection. It then instructs other clients to send probes to them. During late join, a malicious joiner can repeatedly resubmit candidates at the WebSocket rate limit and cause the honest host to start many blind-probe jobs. This allows low-bandwidth third-party UDP pestering, private-network/loopback probing from another player's device, and client-side resource exhaustion.

The relay prevents third-party redirection but still transports arbitrary payloads between two cooperating clients. Production also accepts reusable P-256 installation identities, not only GPGS identities. Current relay budgets permit 1 MiB/s per identity, 4 MiB/s per room, and 8 MiB/s globally indefinitely while traffic remains active. The global ceiling is about 675 GiB/day or 20 TiB per 30-day month at saturation. These limits protect memory and bound work, but they do not yet make persistent non-gaming tunneling economically unattractive.

The legacy game UDP ingress has already received extensive crash and bounds hardening. The existing direct-UDP report correctly retains cryptographic peer authentication and complete typed decoding as residual work. The matchmaking relay authenticates the logical sender at the transport layer, but an admitted malicious peer is still intentionally able to supply arbitrary game payload bytes to the native parser.

## Threat model and security objectives

- Unauthenticated Internet traffic can reach HTTPS/WSS, relay UDP, and STUN UDP
- An authenticated user can run a modified client and can acquire multiple installation identities
- A lobby member can submit arbitrary control messages, ICE candidates, mission metadata, mission-transfer framing, and gameplay datagrams
- Two malicious users may cooperate to use the relay as a tunnel
- The desired policy permits ordinary global Descent play and mission/save synchronization but should reject or economically suppress unrelated traffic
- VPS resource goals include bounded CPU, memory, sockets, database growth, logs, and sustained egress

## Prioritized findings

| ID | Severity | Finding | Primary evidence |
| --- | --- | --- | --- |
| MM-01 | High | Unproven ICE candidates cause honest clients to probe attacker-selected Internet, loopback, or private-LAN endpoints | `server/src/ws_handler.rs:2766-2788`, `server/src/ws_handler.rs:2833-2864`, `server/src/ws_handler.rs:2914-2987`, `ConnectivityChecker.kt:64-101`, `LocalhostProxy.kt:374-400` |
| MM-02 | High | Late-join `STUN_RESULT` can be replayed up to the generic 30 messages/s limit, spawning repeated host probe jobs and third-party traffic | `server/src/ws_handler.rs:620-627`, `server/src/ws_handler.rs:2766-2993`, `MatchmakingService.kt:1817-1822` |
| MM-03 | Medium | Relay is a bounded arbitrary two-party tunnel; sustained quotas are much higher than game traffic and allocations have no maximum active lifetime or long-window byte quota | `server/src/resource_budget.rs:20-53`, `server/src/relay.rs:295-352`, `server/src/relay.rs:369-377` |
| MM-04 | Medium | P-256 installation authentication provides possession plus one-time PoW, not GPGS account scarcity; reusable pseudonymous identities can receive full lobby and relay privileges | `server/src/ws_handler.rs:724-776`, `server/src/ws_handler.rs:968-1048`, `server/src/pow.rs:77-101` |
| MM-05 | Medium | Unknown-token relay and non-allowlisted STUN floods remain unbudgeted receive-loop work and can starve valid UDP on the single socket loops | `server/src/relay.rs:468-487`, `server/src/stun.rs:41-72` |
| MM-06 | Medium | Connection/event tables and daily log files have no retention; authenticated candidate spam is logged at info level and connect/disconnect churn writes persistent rows | `server/src/main.rs:28-45`, `server/src/db.rs:129-164`, `server/src/db.rs:489-507`, `server/src/ws_handler.rs:2789`, `server/src/ws_handler.rs:1943-1947` |
| MM-07 | Medium | Authenticated commands are globally serialized, while several database-backed operations lack command-specific limits or pagination; an unbounded friend list is the clearest cumulative case | `server/src/ws_handler.rs:1129-1138`, `server/src/friends.rs:65-120`, `server/src/ws_handler.rs:3288-3290` |
| MM-08 | Medium | Mission-transfer client trusts an attacker-selected `chunk_size`; a persisted transfer can allocate `ByteArray(chunkSize)` on resume and terminate the process with OOM | `MissionTransferService.kt:372-381`, `MissionTransferService.kt:584-627` |
| MM-09 | Medium | A mission host's public TCP listener admits unauthenticated clients into all seven 15-second transfer slots before token/address validation | `MissionTransferService.kt:241-284` |
| MM-10 | Medium | GPGS verification work is globally bounded but the declared per-IP authentication-failure limiter is never called, allowing avoidable Google requests and log work up to the connection limit | `server/src/rate_limit.rs:49-50`, no production call to `record_auth_fail`, `server/src/ws_handler.rs:891-928` |
| MM-11 | Low | Admin routes are on the public API by default when no separate admin listener is configured; bearer auth protects mutations, but failed auth/log/body work has no admin-specific budget | `server/src/http_api.rs:32-47`, `server/src/http_api.rs:220-240`, `server/config.default.jsonc` |
| MM-12 | Defense in depth | Relay AES-GCM sequence state is local to a `RelayTransport` instance; reusing credentials after an in-process transport restart risks nonce reuse even though the server replay window rejects duplicate sequence numbers | `server/src/relay.rs:111-160`, `RelayTransport.kt:14-115` |
| MM-13 | Low / operational | Both supplied JSONC examples leave a trailing comma before `}`, but the custom comment stripper does not accept JSON trailing commas; copying either example makes production startup fail closed | `server/config.default.jsonc`, `server/config.lan.jsonc`, `server/src/config.rs:104-111` |

## Detailed analysis and recommended fixes

### MM-01 and MM-02: candidate validation and probe authorization

The server validates address syntax, port nonzero, and non-multicast/non-unspecified status. It does not establish ownership:

- A claimed `srflx`, `upnp`, or client-provided `predicted` IP need not equal the public IP observed on the authenticated WebSocket
- `host` candidates may contain loopback, link-local, public, or arbitrary RFC1918 addresses
- Client-provided `predicted` candidates suppress the server's own derivation
- Submitted candidates are copied into `PEER_CANDIDATES`, connectivity-check offers, and late-join `probe_addrs`
- A late joiner can resubmit `STUN_RESULT` repeatedly while its pending admission remains active; no attempt ID, one-shot gate, or candidate-specific rate limit prevents new probe jobs

Initial connectivity checking sends at most roughly 15 request probes during the three-second/200 ms loop, so its traffic amplification is small. The late-join path is worse: each accepted message causes the host to send one blind datagram to every supplied/derived address, and generic control-plane admission permits 30 messages per second. Multiple malicious joiners can therefore generate thousands of host-side sends and coroutine launches per second while also probing third parties.

Recommended protocol change:

1. Stop accepting client-provided `predicted` candidates. Derive them only from server-validated reflexive observations
2. For Internet matchmaking, do not distribute `host` candidates. LAN discovery already provides a separate local-network path. This avoids turning another player's device into an RFC1918/loopback scanner
3. Require public candidate IPs (`srflx` and `upnp`) to match the authenticated WebSocket's observed public IP. Server-generated `observed` candidates may use that IP plus a bounded client port
4. Prefer an authenticated STUN transaction tied to the WebSocket session: issue a short-lived credential/transaction over WSS, have the STUN listener record the observed source address for that session, and construct the reflexive candidate server-side
5. Add a candidate-attempt UUID and accept one bounded candidate set per player per attempt. Ignore byte-identical retries; require a server-issued retry transition for replacements
6. Add a dedicated low-rate candidate limiter and a hard cap after normalization, for example 8 total candidates, 2 per type, and 32 candidate pairs per peer after prioritization/deduplication
7. Make late-join probing idempotent and cancel/replace a single job rather than launching another job per message

### MM-03 and MM-04: relay policy and identity

The relay is not conventional TURN. Its fixed two-peer allocation and proven endpoint bindings are strong design choices and should be retained. It prevents a sender from selecting arbitrary packet destinations, prevents off-path endpoint replacement, authenticates the claimed slot, and re-encrypts separately to the destination.

It does not distinguish game bytes from arbitrary bytes. Content inspection would tightly couple the server to both legacy engines and can be evaded by tunneling data in syntactically valid game messages. Economic and lifecycle controls are a better primary defense:

- Use a token bucket with a game-sized sustained rate and a generous burst for join/sync. Start with telemetry rather than guessing; the existing 1 MiB/s identity rate can remain as a short burst, but the long-window average should be much lower
- Add per-identity and per-installation 10-minute/hour/day byte budgets, plus a server-wide monthly alert/circuit breaker
- Give every allocation an absolute lifetime (for example several hours), then reauthorize/rekey over the live WSS session
- Require both identities to remain in the same live lobby and revoke on any control-plane ownership change, as the code mostly does today
- Consider requiring GPGS verification for relay privilege while still allowing installation-authenticated users to browse and attempt direct play. If non-GPGS relay access is required, give it a smaller quota
- Record aggregate byte/packet counters per identity/allocation, not payload contents, and expose capacity/abuse metrics
- Keep a conservative global egress cap at the OS/provider layer so process compromise or an application regression cannot create an unlimited bill

GPGS is useful as a stable, ban-able identity and makes mass identity creation more expensive. It does not attest that the client is unmodified and must never be treated as authorization for game-state claims. Installation keys are useful continuity credentials, but proof-of-work is only a registration throttle. Neither identity system replaces per-command authorization, quotas, or abuse response.

### MM-05: volumetric UDP handling

The application budgets correctly bound response traffic and expensive authenticated crypto. They do not bound the cost of receiving and rejecting datagrams that fail before those budgets. An attacker can flood random relay tokens or non-allowlisted STUN traffic; each listener is a single receive loop, so valid traffic competes with the flood.

Use host/provider controls in front of the process: nftables/iptables UDP packet-rate limits, provider DDoS filtering where available, separate relay and STUN queues/ports, increased receive buffers only after measuring, and packet-drop counters. Application code should also count sampled rejection classes so an attack is visible without logging every datagram.

### MM-06 and MM-07: durable and control-plane resource growth

SQLite calls are synchronous and many execute while the global control transition mutex is held. The common 30 messages/s per-session limit allows high aggregate work across 500 connections. Specific risks include repeated friend-list materialization, friend/block writes, candidate fanout, status queries that count durable tables, and connection-event/log growth.

Recommended controls:

- Add a global authenticated-message work budget before the control mutex, plus command-specific costs and per-identity limits
- Cap friends/blocked rows per identity, paginate friend lists, and separately rate-limit every database-writing command
- Bound and validate all stored strings even when the 16 KiB WebSocket frame limit already bounds the containing message
- Add retention for connection events, player-count history, admin logs, and rotated text/JSON logs; checkpoint WAL and alert on database/filesystem size
- Cache aggregate public status counts instead of running synchronous `COUNT(*)` queries on every `/status` request
- Move blocking SQLite work off async runtime workers or use a dedicated database worker with a bounded queue

### MM-08 and MM-09: peer mission transfer

SHA-256 verifies that received bytes match the host's advertisement; it does not make a malicious host's archive trustworthy. The client should independently enforce:

- A small allowed range for `chunk_size` (and preferably one protocol constant)
- Exact `ceil(size/chunk_size)` chunk count
- Exactly 64 lowercase hexadecimal characters per chunk hash
- A cap on header array elements and all allocations derived from the header
- Archive extraction path, file-count, expanded-size, compression-ratio, nesting, and parser limits (many of these already exist in the import layer)

For the host listener, reject source IPs that have no active authorization before consuming a transfer slot, use a much shorter request-header deadline, rate-limit accepts per source, and cap pending unauthenticated sockets separately from active transfers. Preserve the one-use token and source-address binding.

Mission content remains a high-risk trust boundary because imported level data reaches legacy native parsers. GPGS identity and an attacker-chosen content hash do not establish publisher trust. A future content-signing or community moderation model would improve attribution, but strict native parsing and extraction sandboxing remain required.

### MM-10 and MM-11: authentication and deployment

Call `record_auth_fail` on failed authentication paths, and decide whether its five/hour policy is appropriate for shared carrier NATs before enforcing it as a hard ban. A short exponential backoff keyed by both source IP and claimed identity is less likely to punish a household or mobile carrier. Retain the global auth slots/deadline and bound Google response bodies as currently implemented.

Bind the admin listener to loopback or a private management interface by default and do not proxy `/api/v1/admin/*` through the public nginx virtual host. Add nginx `limit_conn`, request-rate/body-size/header-timeout controls and a small cache for public status endpoints. The bearer token remains necessary even on a private listener.

### MM-12: relay nonce lifecycle

The server creates fresh keys and an epoch for every allocation, which is good. Ensure a client never constructs a new `RelayTransport` with old credentials and resets its outbound sequence to one. Either make the transport object own the entire credential lifetime, persist the sequence across proxy restarts, or obtain a new relay allocation/key epoch whenever the transport is rebuilt.

### MM-13: configuration examples

The checked-in default and LAN JSONC files both use a final trailing comma. `strip_jsonc_comments` removes comments but then passes the result to strict `serde_json`, which rejects trailing commas. Remove the final commas or deliberately support trailing commas, and add both example files to the existing schema/loader test. Failing closed is appropriate, but a shipped example that always fails encourages ad hoc environment-only deployments and makes security settings easier to omit.

## Existing defenses worth preserving

- 16 KiB WebSocket message/frame cap and 20-second absolute pre-auth deadline
- Bounded socket count, transport admission, TLS handshake timeout, auth slots, and outbound queue byte permits
- Finite production connection/relay caps enforced at startup
- Fixed-pair relay allocations, separate directional keys, AEAD associated headers, replay windows, proven endpoint rebinding, and revocation on player/room exit
- Per-pair, per-identity, per-room, and global relay packet/byte budgets
- STUN response allowlist, exact request framing, small fixed response, and per-source/global response budget
- Server-issued connectivity check IDs and validation of reports against current lobby/attempt candidates
- Direct-UDP crash/bounds/authority hardening documented in `android/security_scan_direct_udp_20261016.md`
- Mission archive source-size limit, final SHA-256 verification, one-use transfer grants, source-IP binding, and extraction-layer limits

## Suggested implementation order

1. Close candidate redirection and make late-join probing idempotent (MM-01/MM-02)
2. Bound mission-transfer framing and unauthenticated transfer sockets (MM-08/MM-09)
3. Add long-window relay quotas, absolute allocation lifetime, accounting, and relay privilege policy (MM-03/MM-04)
4. Add durable-data/log retention and global/command-specific control-plane budgets (MM-06/MM-07/MM-10)
5. Harden deployment firewall/nginx/admin topology and add abuse observability (MM-05/MM-11)
6. Make relay credential/sequence lifetime explicit (MM-12)

## Validation plan

- Candidate tests: reject third-party public IP, loopback, link-local, multicast, unspecified, client-predicted, and private host candidates on the Internet service
- Late-join test: repeated/stale `STUN_RESULT` yields at most one probe job and one bounded address set for the current attempt
- Relay soak: valid gameplay/sync bursts succeed while sustained two-client arbitrary traffic hits 10-minute/hour budgets and allocation expiry
- UDP flood test: random tokens/non-allowlisted STUN do not starve a valid session under the configured host firewall limits
- Database/log soak: connection churn and every write command remain within fixed disk growth and queue latency
- Mission transfer tests: zero, negative, huge, and inconsistent chunk sizes/counts/hashes fail before allocation; seven slow unauthenticated sockets do not block an authorized transfer
- Malicious-peer integration: malformed and unauthorized native packets are rejected on both direct and relayed paths without process termination or fanout

## Scope not completed in this pass

- Current third-party dependency CVE/RustSec/Android dependency scan
- Dynamic fuzzing of WebSocket, relay, STUN, mission ZIP, or native packet parsers
- Production firewall, nginx include files, provider quotas, database size, or observed traffic measurements
- Cryptographic protocol review by an independent specialist

## Verification performed

- `cargo test --locked`: 61/61 library tests, 96/96 integration tests, and 13/13 NAT simulation tests passed
- Two of three process-readiness tests passed. `process_readiness_and_restart_with_synthetic_database` fails on this Windows host because its child uses `env_clear()`, which removes Windows environment required to initialize Winsock; the child relay bind exits with Winsock error 10106. Starting the same binary without clearing the Windows environment stays running. This appears to be a Windows test-harness issue, not a protocol failure
