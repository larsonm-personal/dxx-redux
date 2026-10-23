# Matchmaking security implementation

## Objective

Implement the findings and hardening plan in `matchmaking_protocol_security_review_20261021_findings.md`, one reviewable chunk per goal round. The implemented production authentication currently requires GPGS. The October 22 design review below revises the target to ordinary Google sign-in for matchmaking and optional PGS platform features. Retain installation/PoW fixtures only for explicitly enabled loopback development testing

## Completion ledger

Round entries below preserve the state and evidence at the time of each round. This ledger and the latest verification entry record current status; final release/device/provider/deployment gates are tracked separately and remain open

- [x] MM-04: GPGS-only production authentication, visible client failure, loopback-only installation tests
- [ ] AUTH-REV: replace matchmaking PGS authentication with verified Google account authentication; separate optional PGS initialization and feature access (reviewed below, not implemented)
- [x] MM-01: candidate ownership, authenticated STUN observations, server-issued candidate attempts
- [x] MM-02: immutable candidate submissions, bounded candidate/pair work, idempotent late-join jobs and bounded echo service
- [x] MM-08: validate all mission-transfer metadata before allocation or persistence, including resumed manifests
- [x] MM-09: bound and authorize mission-transfer socket admission before consuming transfer slots
- [x] MM-03: long-window relay quotas, absolute allocation lifetime/rekey, accounting, and aggregate circuit breaker
- [ ] MM-05: UDP admission/drop metrics, fair service under malformed traffic, and reviewed deployment rate-limit configuration
- [ ] MM-06: database/log retention, WAL maintenance, disk-growth metrics and limits
- [ ] MM-07: bounded global/command work, friend/block caps and pagination, stored-field bounds, cached public counts, bounded database worker
- [ ] MM-10: authentication-failure backoff with shared-NAT recovery
- [ ] MM-11: private admin default, admin budgets, public nginx admission/cache/header/body controls
- [x] MM-12: relay credential and sequence ownership across transport replacement
- [ ] MM-13: valid example JSONC configurations with loader coverage
- [ ] Final verification: malicious-peer direct/relay rejection, abuse/lifecycle integration tests, relevant builds/lint, final requirement-by-requirement audit

## Round 1: authentication policy

- Reject every production authentication method except `gpgs` before issuing any challenge, looking up an installation identity, or calling Google
- Permit retained non-GPGS fixtures only with `skip_gpgs_verify=true` and an observed loopback source
- In the Android client, permit automatic installation fallback only in debug builds to literal loopback/emulator host aliases
- Show a terminal Google Play Games authentication error when GPGS is unavailable for a production connection
- Exercise production rejection for new and pre-existing installation keys, forged follow-up proofs, explicit dev/keypair methods, and proxy-reported non-loopback sources
- Update owned local test configuration and rehearsal documentation to identify development bypasses accurately
- Run Rust authentication integration tests, host JVM service tests, scoped quality checks, and Android debug/release Kotlin compilation

## Verification

Round 1 completed:

- Server policy rejects production `p256_challenge`, `keypair`, and `dev` requests with or without configured Google credentials, including previously registered installation identities
- Follow-up proof messages cannot create a session without a permitted authentication attempt
- Development bypasses require an observed loopback source; a trusted proxy reporting a non-loopback source cannot enable them
- Client fallback requires both a debug build and a literal loopback/emulator host alias; unavailable production authentication closes the socket and shows a GPGS error
- Direct-install builds explain that their missing Play Games support prevents public online access while LAN play remains available
- Local gameplay/reconnect/outage fixtures explicitly opt into development authentication; Windows child fixtures preserve only SystemRoot in addition to their explicit server configuration, so Winsock can initialize
- Rust: 61 library tests and 98 integration tests passed with the locked dependencies in isolated target directory `server/target/security-auth`
- Host JVM suite: 29 passed, 3 skipped without optional fixture settings; the subsequent real-server run passed all 4 selected authentication, fallback, and outage tests
- Android: `:app:compileDebugKotlin` and `:app:compileReleaseKotlin` passed
- Scoped mixed-language formatting/lint passed
- Strict Clippy on installed Rust 1.96.1 reports existing `nonminimal_bool` at `server/src/lobby.rs:142` and `assertions_on_constants` at `server/tests/integration.rs:272`. With only those existing lint categories allowed, the library and integration target pass `-D warnings`; no lint suppressions were added to source
- Real Google provider sign-in was not exercised; local tests use synthetic identities and do not qualify production GPGS configuration

Final audit still requires the repository-pinned server Rust version (1.94.1), production-provider qualification, remaining finding-specific tests, and the deployment checks identified below. Round 1 used the installed Rust 1.96.1 toolchain

## Round 2: bounded and idempotent probing (MM-02)

- Accept a candidate submission once per lobby admission/game round, including an empty set, before any fanout, offer replacement, or relay allocation
- Restrict running-game submissions to a current, unexpired pending late join
- Bound candidate updates across room/round churn with a per-identity limiter
- Canonicalize/deduplicate and cap expanded candidates; cap and deduplicate target endpoints in each peer's offered checks
- Keep one client late-join probe job/lease per roster identity, restricted to offered endpoints, with fixed request/echo/time budgets; retire leases on roster removal or proxy shutdown
- Pace normal connectivity requests independently of received traffic and bound receive/echo work so a packet flood cannot accelerate probe sends
- Exercise repeated late-join submissions, empty reports, candidate churn, peer departure/slot reuse, endpoint echo restrictions, expiry, flood pacing, and legitimate connectivity recovery

Round 2 verification:

- 62 Rust library tests and 100 integration tests passed, followed by the additional expired-pending-admission integration test (163 total)
- The live WebSocket replay test submits 20 duplicate/replacement reports and verifies no new peer/probe messages, no offer-ID replacement, no admission deadline refresh, and no extra relay allocations
- Empty reports consume their admission's one submission; an explicit END_GAME reset allows fresh discovery for the next round
- Accepted candidate submissions have a per-identity 6/minute budget across socket/room/round changes
- Expanded candidates are deduplicated and capped at 8; offers use one best local choice per target endpoint and at most 32 per peer
- 32 host JVM tests passed, with 3 optional tests skipped in the default run
- Real local server/client authentication/reconnect and outage tests passed against the updated isolated server
- Live UDP proxy tests verify one blind-probe job despite 1000 repeated requests, endpoint restrictions, 32-echo quota, 3-second expiry, stale queued-job expiry, roster removal, and safe slot reuse
- The synthetic receive-flood test verifies 128-receive/32-echo/15-request ceilings and request spacing even when receives complete immediately
- Existing asymmetric-win, source-fencing, cancellation, relay, roster-survival, and keepalive regressions pass
- Android debug and release Kotlin compilation passed
- Scoped mixed-language formatting/lint passed; Clippy passes with only the two previously recorded baseline categories allowed

MM-01 remains open: the first bounded candidate submission can still name an unverified endpoint. Public candidate ownership, authenticated STUN observations, and server-issued attempt IDs (including stale reports across round changes) require the coordinated control/UDP protocol update in the next round

## Round 3: mission-transfer metadata (MM-08)

- Require valid transferable wrapper requirements before deriving any on-disk path
- Use the existing 1 MiB identity chunk size as the only wire/resume chunk size, with at most 256 hashes for the 256 MiB transfer limit
- Validate revision, exact integer fields, size, whole hash, chunk count and every lowercase SHA-256 before allocating derived storage or persisting metadata
- Bound both network headers and saved manifests before JSON parsing; apply the same validation on resume
- Use a fixed-size verification buffer rather than allocating from peer-selected chunk sizes
- Add real TCP receive tests for malicious framing and successful interrupted/resumed content, plus hostile persisted-manifest tests
- Run relevant JVM tests, debug/release Kotlin compilation, and scoped quality checks

MM-01 remains open and requires the coordinated control/UDP protocol update described above; round 3 addresses the independent peer-content allocation boundary first

Round 3 verification:

- The receive path validates the wrapper requirement before constructing a filesystem path or connecting; invalid hashes, oversized offers and non-transferable sources fail closed
- Network headers and persisted manifests are bounded to 32 KiB, with a two-level nesting limit checked before parsing and strict JSON syntax to prevent lenient quote/comment rules from bypassing the depth check
- Both paths require matching revision, size and whole-file hash, exact integer fields, the fixed 1 MiB chunk size, exact ceiling chunk count (at most 256), and 64-character lowercase hexadecimal chunk hashes
- Resume schema and verified-byte fields are validated before modulo, allocation or partial-file mutation
- Partial verification uses a fixed 64 KiB buffer; no peer-selected chunk size controls an allocation
- The real TCP tests reject 26 hostile metadata variants without persisting them or changing existing partial content; saved-manifest tests also cover schema/offset coercion, overflow and invalid alignment
- Boundary tests cover oversized network/disk metadata, 8000-deep arrays and lenient-JSON quote/comment bypasses, invalid requirement paths, direct resume validation, and successful admission of maximum-size metadata with exactly 256 hashes
- The live interrupted-transfer test resumes at the last verified chunk, completes an unaligned tail, compares the full resulting identity, and reverifies an already-complete transfer without requesting more payload
- `:app:testDebugUnitTest --tests '*Mission*Test'`: 216 tests across 30 classes passed, no failures/errors/skips, including archive extraction/import and mission admission regressions
- `:app:compileDebugKotlin` and `:app:compileReleaseKotlin` passed after final changes, with no new Kotlin warnings
- Scoped formatting/quality checks and `git diff --check` passed

Only the metadata/framing finding is closed here. Host TCP socket admission (MM-09), candidate ownership (MM-01), and the other ledger items remain open; final whole-file verification and import/distribution policy enforcement are preserved

## Round 4a: authenticated STUN foundations (MM-01)

- Add a bounded registry of server-issued candidate attempts, scoped to player, control connection, lobby admission and observed WebSocket IP
- Use a short-lived secret issued over the control channel to authorize at most two UDP observation slots, one per listener
- Return an unpredictable receipt only to the observed UDP source; require that receipt on the control channel before distributing an endpoint
- Make repeated requests idempotent, limit responses per slot, reject address changes and stale/cross-scope receipts, and expose explicit revocation operations
- Add a fixed-size authenticated STUN wire format on Rust and Kotlin, with strict framing/source/transaction checks and bounded client receive/retry work
- Verify live UDP proof issuance, reflection limits, fake/stale receipts, lifecycle/capacity bounds and matching wire vectors on both sides

This foundation round does not close MM-01. The follow-up must wire these APIs into server/client lobby discovery, bump the protocol, remove raw host/predicted/observed candidate injection, revoke attempts on every control/admission transition, and update the end-to-end candidate tests before production switches paths

Round 4a verification (component tests, not active production enforcement):

- Added `server/src/candidate_auth.rs`: at most 500 retained attempts, one per identity, scoped to non-nil server-owned player/connection/lobby/admission IDs and the observed control IP
- An attempt lives for 15 seconds and has two fixed listener observation slots, each limited to three responses; repeated issuance of the same scope neither rotates the credential nor refreshes expiry, response counters or consumed status
- Requests from a different IP cannot create an observation. Receipt redemption rejects forged/duplicate/cross-player/cross-connection/cross-admission/expired proofs and consumes even an empty report once
- Stale scope cleanup cannot revoke a replacement; scope and room revocation clear both registry indexes
- Internet attempts accept only conservative global-unicast IPv4 sources; explicit development permits loopback only, not RFC1918/link-local/documentation/multicast or IPv6 addresses
- Added the 40-byte request/52-byte response extension in `server/src/stun.rs` and `StunProofClient.kt`, with matching cross-language vectors and no caller-selected UDP response destination
- Receive-error logs on the new listener are capped at one/minute, including platforms that report oversized/ICMP datagrams as socket errors
- Client discovery has at most two servers, two sends per server, 16 receives per send and fixed 2-second retry deadlines; it restores the shared socket timeout and does not close that socket
- Live UDP tests cover receipt issuance/redemption, full-cone and symmetric observations, endpoint immutability before quota exhaustion, response ceilings, malformed/unknown/wrong-IP requests, revoked credentials and stale receipt reuse
- Client tests cover every response truncation, malformed lengths/fixed fields, wrong source/transaction, oversize, server-set bounds, cancellation and a receive flood that cannot advance the retry deadline
- Final Rust command: `cargo test --locked --manifest-path server/Cargo.toml --target-dir server/target/security-auth --lib --test candidate_proof --test integration --test nat_sim_tests`: 70 library + 6 proof integration + 101 existing integration + 13 NAT simulation tests passed (190 total)
- Final host-JVM run: 38 passed, 3 optional fixture tests skipped; all six new proof-client tests passed
- Android debug/release Kotlin compilation passed; no warnings were introduced by the new Kotlin files. Existing host-only UPnP URL deprecations and installation-test non-null-assertion warnings remain
- Scoped mixed-language formatting/quality checks and `git diff --check` passed. Strict Clippy still stops at the recorded unchanged `lobby.rs:142` baseline; allowing only the two previously recorded baseline categories passes the library and both integration targets without source suppressions

Follow-up integration constraints:

- Issue challenges only for server-authorized current waiting/late-join admissions, with the existing per-identity candidate budget. Client correlation IDs must not rotate or refresh server attempt state
- Use a fresh server-owned per-player discovery/admission ID on leave/rejoin, control replacement and END_GAME; the waiting game's nil game epoch cannot serve as that ID
- Match client challenge callbacks to both the current control socket and discovery request/generation, and redact challenge credentials/receipts in diagnostics
- Establish UPnP mappings before requesting the short-lived STUN challenge; accept a mapped endpoint only if the authenticated UDP observation actually proves it
- Prefer relay over guessing neighboring/unproven ports. Merely sharing the WebSocket's public IP does not prove another port belongs to this player
- Keep any local emulator loopback/10.0.2.2 routing accommodation strictly within explicit local development fixtures; production must never regain host/private endpoint declarations
- Replace the active listener and legacy client discovery path together, update real-server/client tests and prove stale reports after every lifecycle transition cannot trigger probes or replace routes

At the end of round 4a, no listeners, control messages or client discovery call sites had switched to these APIs. Round 4b below records the subsequent integration

## Round 4b: ownership-proof protocol integration (MM-01)

- Switch both server listeners and client discovery to the receipt protocol and protocol version 7
- Correlate challenges to the live lobby admission, control connection and client request generation
- Derive all distributed endpoints from redeemed observations, never raw host/upnp/predicted declarations or guessed ports
- Rotate admissions and revoke credentials on leave, kick, session replacement, room removal and END_GAME
- Preserve relay-only recovery for unavailable/expired discovery without authorizing UDP probes
- Replace obsolete claimed-address tests with real WebSocket/UDP proof producers, including lifecycle, spoofing, replay and genuine client discovery coverage
- Verify local emulator routing without weakening production endpoint ownership

Round 4b verification:

- Protocol version/minimum are now 7 on the server and version 7 on Android; v5/v6 are rejected before authentication. Matching client and server artifacts are required
- Added `GATHER_CANDIDATES` and a private `CANDIDATE_CHALLENGE` reply, correlated with the live lobby ID, server-owned per-player admission ID and client request ID
- `STUN_RESULT` now accepts only an attempt UUID and at most two redeemable receipts. Unknown legacy address/NAT fields are rejected, not used as a fallback
- Both active STUN listeners use the authenticated proof API. Removed the legacy responder, IP-only allowlist and raw host/reflexive/UPnP/predicted declaration path, including observed-IP/client-port injection and neighboring-port prediction
- Candidate quotas apply when the server first issues an admission's challenge; repeated requests cannot refresh or replace it, spend extra observation slots, or consume another candidate-admission budget
- The server constructs all advertised/offer/probe endpoints from receipt observations and derives NAT classification itself
- Leave, kick, control replacement, pending-admission timeout, room removal and END_GAME revoke credentials; END_GAME rotates each remaining player's admission ID
- Unusable source addresses can still obtain an attempt for an empty relay-only report, but cannot obtain UDP observations. Expired attempts can also select relay through an empty report; expired nonempty receipts remain rejected
- Android completes UPnP before requesting a challenge and never submits a mapping's claimed address. Challenge completion requires the current socket, network generation, lobby, admission and request ID
- Round changes cancel pending challenges and retire old shared sockets. Late-join probe messages must name the joiner's current admission before the proxy is invoked
- Added debug-only translation of a proven host `127.0.0.1` endpoint to `10.0.2.2` when the control connection explicitly uses that emulator alias. Release/public connections do not translate endpoints or accept new address declarations
- Rust test producers now obtain real WebSocket challenges and UDP receipts instead of fabricated candidate advertisements. Direct-route tests retain alternate-endpoint selection and validated-route survival; relay-capacity, concurrent late-join, repeated-report and teardown regressions pass
- Added real WebSocket/UDP tests for seven poisoned address declarations, forged receipt/credential confusion, idempotent challenge retries, and old control/UDP proofs across leave, kick, reconnect and END_GAME
- Client tests verify wrong-request/socket/generation/admission rejection, round cancellation/socket retirement, stale late-join probe suppression, a live valid probe send, debug/release translation boundaries and secret-free parsing diagnostics
- Rust full selected suite passed: 67 library + 102 WebSocket integration + 6 proof integration + 13 NAT simulation tests (188 total). Deleted tests covered removed prediction/allowlist APIs and were replaced by proof-boundary coverage
- Full JVM suite passed: 42 tests passed, 3 optional fixture tests skipped. Android debug/release Kotlin compilation passed; existing host-only warnings are unchanged
- Scoped mixed-language formatting/quality checks and `git diff --check` passed. Clippy passes with only the two previously recorded baseline categories allowed, without source suppressions
- No production listeners, external VPS configuration or running worker-owned server/device were restarted or replaced

Round 4b left the following work for round 4c:

- Exercise the actual service discovery job against the real Rust process and authenticated STUN listeners, not only protocol producers and callback/proxy component tests
- Qualify direct/relay gameplay on owned isolated emulators without interfering with the other worker's sessions
- Audit and bound reuse of stored candidate observations when issuing later initial/late-join blind probes; the receipt-redemption TTL alone does not bound the age of an already-stored candidate
- Keep established validated routes separate from authorization to issue new probes, and preserve relay recovery when stored direct observations are too old

## Round 4c: observation freshness and live discovery qualification (MM-01 code verified, device gate pending)

- Carry the original proof expiry into stored candidates; new probe offers must fit within the remaining proof window, without refreshing it on report receipt or retry
- Send late joiners through relay when the host's observations are too old, preserving already-established validated routes
- Add real-server tests for stale initial and late-join probe suppression and normal fresh-probe/direct behavior
- Run the actual service discovery job against an isolated Rust process, checking receipt submission, direct probing and socket handoff
- Inspect emulator/process ownership before device qualification; do not restart the existing worker-owned server or interfere with its devices

Round 4c verification:

- Receipt redemption carries the attempt's original expiry into the player's stored candidates. A delayed report cannot grant a new 15-second window
- Issuing a new probe requires a full three-second probe window to remain before that original expiry, on both local and remote observations
- Initial discovery with stale observations issues no connectivity checks; normal START_GAME still succeeds through relay
- Late join with stale host observations issues neither CONNECTIVITY_CHECK_GO nor LATE_JOIN_PROBE, admits the joiner through relay, and preserves existing relay allocations
- Expiring observations does not erase already-established validated routes; these cannot be mistaken for permission to issue new blind probes
- Added the actual `OnlineCandidateDiscoveryTest` service-job fixture against the real rebuilt Rust process and both credentialed UDP listeners, for direct and forced-relay modes
- The real fixture verifies server acceptance of the service's receipts, real direct UDP request/echo exchange, ready/report ordering, matching game-start assignments, and handoff of the exact original UDP socket/port to the localhost proxy
- The fixture uses an in-memory server, synthetic loopback development identities, fresh reserved ports and owned temporary data; every owned server process is stopped and reaped. It does not launch a native game process or qualify real Google credentials
- Rust passed: 68 library + 104 WebSocket integration + 6 proof integration + 13 NAT simulation tests (191 total), followed by an explicit isolated server binary build
- Full host JVM suite with the real-server fixtures enabled passed: 45 passed, 1 optional historical-comparison test skipped. Real installation authentication/reconnect and outage-ordering tests also pass against protocol 7
- Scoped mixed-language formatting/quality checks, Clippy with only the two recorded baseline categories allowed, and `git diff --check` passed. New Kotlin fixture warnings were fixed
- Native Android source did not change in this round; the matching debug/release Kotlin compilation from round 4b remains applicable

MM-01's code and host integration requirements are implemented and verified. Native-emulator release qualification remains an explicit final-verification gate, not an achieved result:

- Read-only ADB/process inventory confirmed existing `Nexus5X_Light_1`/`emulator-5554`, `Nexus5X_Light_2`/`emulator-5556`, and the original `server/target/debug/dxx-matchmaking.exe` process are active
- Only about 4 GiB RAM was free before further test work, so no extra emulator pair was started and no existing emulator, APK, app data or worker-owned server process was changed
- Final verification still includes owned-isolated-emulator direct/relay gameplay and malicious-peer native-packet checks, plus the existing pinned-toolchain/provider/deployment gates. This does not narrow the full implementation goal
- While that device gate remains pending, the next independent implementation chunk is mission-transfer socket admission (MM-09)

## Round 5: mission-transfer socket admission (MM-09)

- Reject sources without a current grant in the accept loop, before starting request work
- Keep bounded pending headers separate from the seven authenticated transfers, with a two-second absolute header deadline and a bounded per-source accept budget
- Bound grants, preserve idempotent retries without extending expiry, and validate token, source, offset and lifecycle before consuming a grant
- Use monotonic expiry, one active transfer per player, and a host watchdog for stalled writes and absolute transfer lifetime
- Catch malformed requests without uncaught coroutine failures, and close all owned sockets on stop/replacement
- Add live TCP host integration tests using the normal managed-archive validation path, covering shared-address players, malformed/slow requests, token binding/replay, resource ceilings and teardown
- Run mission JVM tests, debug/release Kotlin compilation, scoped quality checks and whitespace validation

Round 5 verification:

- The accept loop rejects any source without an unexpired grant before allocating a coroutine, request buffer or transfer slot
- Pending request sockets are capped at 14 globally and eight per source, separately from seven active transfers. The eighth pending slot permits a normal request alongside seven slow sockets sharing its address
- Request framing is limited to 4096 bytes and an absolute two-second monotonic deadline. Each blocking read uses the remaining deadline, so a byte drip cannot refresh it
- Strict bounded-depth JSON accepts only a string token and an exact integer offset, with no unknown fields. Invalid/misaligned/out-of-range offsets and wrong-source tokens do not consume the grant
- Grants are capped at seven, one per player, with a 30-second monotonic lifetime. Repeated authorization for the same player/address returns the existing token without extending expiry; player IDs are bounded rather than truncated
- One-use redemption, source binding, current host ownership and the seven-transfer/one-per-player limits are checked atomically under the session lock
- Accept budgets allow 32 admitted headers per source per ten-second window, with at most 64 source records. Only sources with a current grant can create records; address churn cannot grow the table or evict another source's budget. Exhaustion recovers when the window expires
- Source comparison uses numeric addresses without caller-selected hostname resolution, including IPv6 literals with bounded local interface scopes
- One host timer closes pending headers, active writes without progress for 15 seconds, and transfers older than 30 minutes. Sockets retain their tracked lease until blocking work returns; no per-connection watchdog coroutine or prematurely reusable permit is introduced
- Stop/replacement closes all owned pending/active sockets, cancels the host scope and clears grants/budgets. Stale request jobs cannot redeem against the replacement session
- Malformed framing/JSON and network exceptions are caught without uncaught coroutine failures or per-packet log amplification
- The sender transmits no more than the advertised archive size, including resumed transfers, and treats truncation as failure
- Added 13 live TCP host tests using the same managed import path, archive distribution checks, persisted identity and startup guards as Android. The File-based entry point removes only the Context dependency, not validation
- Tests cover seven simultaneous transfers from one address; seven slow headers alongside a real valid transfer; absolute byte-drip timeout; malformed/oversized/coerced requests; wrong-source tokens even when that source has its own grant; one-use replay with an already-pending socket; grant/pending/source-map ceilings and recovery; expiry; stop/replacement; aligned/completed resume; and the actual client receiving and revalidating a real host's archive
- The real scheduled stalled-writer test passed in 15.883 seconds and then completed a healthy transfer. Additional housekeeping tests exercise monotonic expiry/cleanup with an advanced timestamp; they are not claims that a 30-minute transfer was timed in real life
- Final command: `:app:testDebugUnitTest --tests '*Mission*Test' :app:compileDebugKotlin :app:compileReleaseKotlin` with JDK 21 passed: 229 tests, zero failures/errors/skips, both Kotlin variants successful, no new Kotlin warnings
- Scoped formatting/lint and `git diff --check` passed. No native source changed, and no external server, emulator, APK or worker-owned process was changed

The source budget is deliberately coarse: players sharing an address share its accept budget. A malicious co-NAT peer can temporarily deny new headers until the short pending deadline/budget window recovers; this bounds work rather than claiming immunity to transport-level floods. Normal seven-player shared-address transfers and retry headroom are covered. Native-device, provider and deployment gates remain open under final verification

The next independent implementation chunk is relay long-window quotas, allocation lifetime/rekey and aggregate accounting/breaker controls (MM-03)

## Round 6a: relay economic limits and accounting (MM-03, first chunk)

- Retain the existing authenticated short-window work limits and fixed two-peer routing
- Add bounded per-GPGS-identity sustained/burst and ten-minute/hour/day egress budgets that survive allocation, room and control-session churn within the process
- Make identity enforcement configurable and initially observe-only so real game/mission traces can establish an appropriate sustained allowance; always enforce the global monthly cap
- Pre-reserve bounded monthly egress credits durably in SQLite before forwarding, replenish outside the UDP loop, fail closed on persistence failure, and preserve the cap across restart and clock rollback
- Account for conservative UDP/IP overhead, all emitted handshake responses and data, without storing payloads or exposing keys
- Expose bearer-protected aggregate/per-identity/per-allocation accounting with bounded pagination
- Test boundary/recovery/churn/forgery behavior, persistent credit restart/concurrency, and actual WebSocket/UDP/admin integration; run relevant Rust tests, lint and an isolated binary build
- Keep MM-03 open until allocation lifetime/coordinated rekeying and final legitimate-gameplay/provider/deployment gates are verified

Round 6a verification:

- Added `server/src/relay_economy.rs` and `server/RELAY_SECURITY.md`; no dependencies or wire protocol changed
- Retained fixed-pair routing, source proof, AEAD, replay checks and existing short input/crypto budgets. Only authenticated responses eligible for emission reach economic accounting; forged ciphertext, replays, unknown tokens and requests without responses do not debit it
- All emitted DATA/CHALLENGE/BOUND responses are charged to the authenticated sender, including protocol framing and a conservative 256-byte UDP/IP/ordinary-fragmentation allowance. Failed sends remain conservatively charged; these counters do not claim peer delivery or exact provider billing
- Added a hard eight-MiB estimated-egress one-second window and always-enforced monthly cap, default one TiB. New allocations are refused when prepaid credit cannot cover both peers' initial binding replies
- Added configurable per-player token-bucket and ten-minute/hour/day windows, shared across all pairs/rooms/control connections. Defaults are 256 KiB/s refill, 256 MiB burst, 512 MiB/ten minutes, two GiB/hour and eight GiB/day
- Identity enforcement starts in observe-only mode, as the review recommended measuring legitimate traffic before imposing a guessed sustained rate. `relay_policy.enforce_identity_quotas=true` enforces the same checked budgets; both modes are tested. The global rate/monthly caps and finite identity-table capacity always enforce
- Production keys quotas by the stable server player UUID associated with GPGS, not a caller-selected installation string or IP. Installation/PoW identities remain loopback development fixtures, not an alternative production quota pool
- At most 4096 identity records are retained, with day-idle expiry and no quota-resetting eviction on churn. Pages contain at most 100 records. Identity counters/windows are deliberately process-local; their persistence across process restarts is not claimed
- Added one bounded SQLite high-water-month/reservation row, FULL synchronous WAL commits, immediate reservation transactions and at most one outstanding refill. Credits are committed in batches of at most 64 MiB, clamped to the remaining allowance, before the UDP loop may spend them
- Refill runs through `spawn_blocking`, never per datagram or under a held control/economic lock. Persistence failure creates no credit. Restart forfeits unused prepaid credits rather than refunding them; concurrent database handles cannot double-spend the cap
- UTC month rollover grants a new monthly allowance while a persisted future/high-water month rejects clock rollback. A monotonic month deadline prevents old prepaid credit from remaining usable indefinitely if maintenance stops
- Production rejects empty/in-memory database names and SQLite URI filenames, preserving a normal persistent filename requirement. Tests using real production authentication policy now use owned temporary on-disk databases rather than weakening that requirement
- `RELAY_POLICY_JSON` replaces the file's `relay_policy` object with defaults for omitted fields. Config subprocess tests cover defaults, file/env precedence, malformed JSON, negative/zero limits, misspelled nested fields and wrong boolean types
- Added bearer-protected `/api/v1/admin/relay-usage` on admin-capable routers, exposing aggregate counters, policy, prepaid/committed credit, quota windows, bounded per-identity pages and per-allocation sender counters. Tests verify no endpoint addresses, routing tokens, AEAD keys or payloads appear
- Credit maintenance logs the 75%-of-monthly-cap warning once per process/month, not per packet. Five-minute stats include aggregate egress, identity overages, monthly drops and persistence failures
- Live WebSocket/UDP/admin tests verify binding/data byte accounting, forged/replayed packet rejection without quota debit, independent same-address peer quotas, hard-limit drops, quota survival across END_GAME/new allocation, protected pagination and fresh-state restart at a depleted durable cap
- Unit/model tests verify fractional sustained refill, independent ten-minute/hour/day recovery, observation versus enforcement, identity-table saturation/recovery, month expiry/rollback, reservation errors/concurrency/restart and one-time warnings
- The two-full-roster/40-packets-per-path/shared-NAT relay test now enables economic enforcement: all 4480 data responses for 16 distinct identities pass without overage. A separate allowance model forwards 256 MiB of maximum-size single-peer payloads below the pre-existing short rate with identity enforcement on; it is not a native mission-transfer qualification
- Final selected Rust suite passed: 80 library + 108 WebSocket/UDP integration + six candidate-proof integration + 13 NAT simulation tests (207 total). An explicit isolated `dxx-matchmaking` binary build passed
- Clippy passed with only the two previously recorded baseline categories allowed; three new `let_and_return` warnings were fixed without source suppressions. Scoped mixed-language formatting/lint and `git diff --check` passed
- The first JVM invocation was up-to-date without rerunning against the new binary. Fixed `android/proxy-tests/build.gradle` to declare the actual server binary contents as a test input, then reran the real fixtures. Fresh reports from 08:38-08:39 UTC show 45 passed, one optional historical-comparison test skipped, zero failures/errors
- Actual service discovery/direct/relay socket handoff, installation reconnect, outage ordering, proxy forwarding, nonce/replay and relay transport fixtures pass against the rebuilt isolated Rust process. No Android runtime/native source changed in this round and no external VPS, normal worker-owned server or emulator was changed

MM-03 remains open. The next chunk is allocation absolute lifetime with coordinated WSS reauthorization/rekeying, including nonce/transport ownership integration with MM-12. Identity-threshold calibration, native gameplay/mission traces, pinned-toolchain/provider and deployment gates remain open. The global breaker bounds estimated relay egress but deliberately trades relay availability for the cap; observation mode alone does not stop one colluding account from spending that allowance

## Round 6b: bounded relay lifetime and coordinated rekey (MM-03/MM-12)

- Give every relay allocation an absolute packet-path expiry, captured room/game/control ownership, and one server-owned renewal attempt
- Keep return-path consent shorter than key lifetime: periodic same-route proofs and a hard 30-second consent lease for both DATA endpoints
- Stage fresh-token/fresh-key replacement routes while retaining old routes, require both proven UDP bindings plus current WSS acknowledgments, then commit with a short hard-bounded overlap
- Bound staging to one replacement per logical pair, retain logical allocation capacity during overlap, and keep economic quotas independent of key generations
- Reject stale/wrong-player/connection/room/game acknowledgments and revoke both generations on ownership change, timeout or teardown
- Update protocol version and client runtime together, preserving the same local game socket/proxy while staging and switching relay transports
- Make key/nonce ownership explicit across rotations and transport reconstruction; never accept replayed or cross-direction key reuse
- Add live server/client renewal, missing-ack/expiry, replay/forgery, shared-socket/owned-socket continuity and lifecycle tests; run Rust/JVM/Kotlin builds and scoped mixed-language quality checks
- Keep native/provider/deployment and threshold-calibration gates explicit; do not mark the full security goal complete on this chunk

Round 6b verification:

- Protocol version/minimum are now 8 on the server and Android. Matching artifacts are required; UDP framing remains DXR3. Preview/rehearsal and policy documentation identify the version/lifetime boundary
- Every production allocation captures room/game epoch and both current control-connection owners, with a monotonic hard expiry checked before crypto admission and again on the packet path. Keepalives cannot extend it
- Default allocation lifetime is two hours, configurable from one hour to one day in production. Explicit loopback development can use ten seconds or more. Lower/invalid lifetimes fail configuration validation
- The server initiates at most one actual renewal attempt per old epoch, up to 60 seconds before expiry. Temporary lack of prepaid credit does not consume the attempt before a replacement is created
- Replacement token, allocation epoch, directional keys, nonce and replay state are fresh. Routing tokens remain reserved while referenced by pending/retiring metadata, including orphan-cleanup windows
- Both peers must prove their new UDP bindings and acknowledge over their current authenticated WSS connections. A first acknowledgment may precede the other peer's binding; it is remembered without committing until both fresh bindings and acknowledgments exist
- Acknowledgments fence lobby, game epoch, old/new token, new epoch, membership and captured control ownership. Commit/abort metadata additionally includes the old epoch and peer identity/slot for client-side correlation
- Staging expires within 15 seconds without extending the original lease or repeatedly minting new replacements. Commit shortens the old generation to at most three more seconds, never beyond its original hard expiry
- Logical allocation capacity is preserved during overlap. At most 100 logical pairs, 100 renewal records and 200 physical transports are supported; crypto reservations were adjusted and tested for those 200 transports. Aggregate identity/room/economic budgets remain shared rather than resetting on rekey
- Leaving, kicking, control replacement, game-epoch changes and room teardown fence both generations. The live replacement test proves pending generations disappear when the same account obtains a new control connection
- Client rekeying preserves the existing PeerProxy, local game port, shared/owned real socket and counters. A prepared receiver accepts the new generation before commit, but outbound game DATA stays on the old transport until commit. Matched-generation challenges are answered with the matching transport/key/counter
- Duplicate offers do not construct another transport or refresh readiness deadlines. Duplicate commits do not reset counters or extend overlap. Aborted/timed-out old epochs cannot be restaged, and stale aborts cannot retire a committed replacement
- Service callbacks also fence current socket, network generation, proxy object, room/game epoch and roster identity. The asynchronous callback regression proves a successfully bound stale generation cannot submit READY on its replacement control scope
- Strengthened the bounded process key-fingerprint registry to reject reuse in either direction, including an old receive key becoming a new send key. It never evicts fingerprints and permits at most 4096 transport key pairs per process before failing closed
- Replaced server-role uses of the production client transport in JVM fixtures with independent `HostRelayCodec`, preserving real client nonce ownership without a test bypass. Cross-language AES-GCM/replay, reconstruction, cross-direction reuse, same-key directions and fresh-key replacement tests pass
- Source review confirmed online launch uses the existing matchmaking proxy; resume preferences persist server/room/player metadata, not relay keys. Process recreation cannot reconstruct an online relay from saved credential/sequence state; fresh authentication/allocation is required
- Final ownership audit found same-address BIND acknowledgments previously reused a one-time endpoint proof for the entire key lifetime. Added periodic consent: challenge/proof after 15 seconds and a hard 30-second proof age for both DATA endpoints. Ordinary DATA/BOUND acknowledgments cannot refresh consent
- Same-route consent proof may arrive behind newer game DATA within the replay window without changing route ownership. Different-route binding retains the stricter ordering requirement. Tests verify duplicate proof rejection, expired sender/destination refusal, recovery by fresh return-path proof and no activity refresh from an unproved expired-address challenge
- Live Rust tests cover staged traffic continuity, one-pair capacity at two physical generations, unproved/forged/outsider acknowledgments, fresh-key cutover, old-token retirement, missing-ack expiry without retries, teardown and control replacement. A clock-model test separately proves a keepalive at 7199 seconds cannot permit DATA at 7200 seconds or extend a shortened retirement
- Shared- and owned-socket JVM tests cover prepared-generation receive, idempotent staging/commit, same-source/local-port forwarding after cutover, retired-key rejection after overlap, timed-out staging, replayed offers, stale abort and continued replacement traffic
- Extended the actual MatchmakingService/Rust discovery fixture to use two actual localhost proxies and exchange bidirectional native-shaped UDP payloads for 53 seconds, through periodic consent refresh and at least two committed key renewals with 40-second development lifetimes. It verifies the same original client proxy/UDP socket and monotonically accumulated forwarding counters throughout
- Final Rust selected suite passed: 82 library + 111 real WebSocket/UDP integration + six proof integration + 13 NAT simulation tests (212 total), followed by Clippy and an explicit isolated binary build
- Clippy passes with only the two previously recorded baseline categories allowed. The new argument-count warning was fixed with a typed READY acknowledgment, without another lint allowance. Scoped mixed-language formatting/lint and `git diff --check` pass
- Android debug/release Kotlin compilation passed with no new runtime-source warnings. Final complete JVM suite against the rebuilt protocol-8 binary passed: 46 passed, one optional historical-comparison test skipped, zero failures/errors. Fresh reports are from 15:19-15:22 UTC; the extended real fixture took 64.342 seconds including discovery/setup
- No native C/C++ source, production VPS, worker-owned normal server, emulator, APK or app data was changed. The host fixture transports native-shaped bytes but does not run or qualify either native game parser

MM-03 and MM-12 code/host integration requirements are implemented and verified. This does not complete the full security goal: identity thresholds remain initially observe-only pending representative native gameplay/mission traces, and pinned-toolchain, provider and deployment gates remain open under final verification

Next independent chunk: MM-05 UDP overload admission/drop metrics and reviewed deployment policing. Include pre-crypto malformed/unknown/expired/consent rejection accounting, fairness at the two-generation physical ceiling, and review of non-unicast/bogon handshake-source rejection before reflection while preserving explicit loopback fixtures and a documented IPv6 policy. Prepare deployment controls locally; do not apply external VPS changes without operator target/authorization

## October 22 design review: Google matchmaking identity and optional PGS

Historical review status: this section originally recorded a source review and implementation plan only. The AUTH-REV rounds below now record the local Google-auth implementation and its verification. The GPGS-only target is superseded without reopening production installation/PoW authentication; no production deployment is claimed

### Decisions from the conversation

- Public matchmaking must require a Google-backed identity so a client cannot manufacture additional resource allowances by generating local identities
- Ordinary Sign in with Google is acceptable for this purpose. No evidence established stronger account-farming resistance from adding a PGS profile; do not treat that as a reason to require PGS
- Own the canonical multiplayer player UUID in our backend and map a verified Google account to it. No additional password-based account system is needed
- Interactive matchmaking authentication belongs to an explicit Connect or online-resume action. Startup, entering the browser, LAN play, single-player and automatic reconnection must not launch sign-in UI
- Retain PGS for its platform features, initially as an optional future achievements integration. Enabling PGS must be a separate deliberate action, not a side effect of connecting to matchmaking
- Keep fixed-pair relay authorization, endpoint consent, quotas, bans and global spending limits independent of whether PGS is enabled or achievements are earned

Google distinguishes the game's primary identity from its PGS platform identity in the [platform authentication guidance](https://developer.android.com/games/pgs/platform-authentication). The proposed separation follows that distinction; delayed PGS initialization is our opt-in product choice rather than a claim of full compliance with Google's startup-oriented Level Up flow

### Required code changes and findings

1. **Move interactive login ahead of the normal control connection**

   `MultiplayerScreen.kt:256,278`, `MatchmakingService.kt:448,558,959,1351` and `SetupActivity.kt:1855` contain the user, reconnect and automation entry points. Today `onOpen` calls `sendAuthenticate`, which requests a PGS server code inside a five-second timeout; `ws_handler.rs:595` starts a twenty-second absolute pre-auth deadline

   Add a focused `GoogleMatchmakingAuth` boundary using Credential Manager and explicit interactive/silent intent. A user choosing an account cannot share the five-second provider/network timeout. Complete interactive authorization before occupying an ordinary unauthenticated WSS slot; any pre-login challenge exchange must be separately bounded. Do not extend the WSS deadline to accommodate a human dialog. Preserve cancellation and generation checks across activity recreation, changed server/account, Disconnect, online resume and late provider completion

   Background reconnect must use a defined, noninteractive credential/session path or return to a visible sign-in-required state. Credential Manager auto-selection is conditional, not a guarantee that requesting credentials cannot show UI. The current AUTH_OK session UUID is not an implemented reconnect credential; do not assume it provides that facility. Define session lifetime, revocation, origin scope and account-change handling before introducing reconnect tickets

   [Credential Manager implementation guidance](https://developer.android.com/identity/sign-in/credential-manager-siwg-implementation) covers foreground activity context, Google ID tokens and nonce support. Pin actual compatible dependency versions during implementation

2. **Replace the server's PGS code exchange with Google ID-token verification**

   `server/src/identity.rs:25` currently exchanges an OAuth code and calls `games/v1/players/me`. A Google ID token is a different credential: introduce an explicit protocol method/field such as `google` and `google_id_token` in `server/src/protocol.rs:35` and `NetworkProtocol.kt:20`, rather than interpreting it as `play_games_token`. Update the production allowlist at `ws_handler.rs:716` and coordinate a protocol version change from current version 8

   Verify Google's signature with an allowlisted algorithm, configured audience, accepted issuer, expiry, bounded subject and the selected nonce/replay binding. Canonicalize accepted Google issuer spellings to one identity namespace. Use a maintained verifier; decoding a JWT is not verification. Follow [Google's server verification guidance](https://developers.google.com/identity/gsi/web/guides/verify-google-id-token)

   Preserve bounded authentication concurrency and parsing. Cache Google's signing keys using a shared HTTP client, bounded responses/key count, expiry and coalesced refresh. Unknown key IDs must not trigger unbounded HTTP requests or create an attacker-sized cache. Reject token-directed key URLs. Key-fetch failures and provider errors must fail closed without restoring PoW. Include this work in MM-10 failure backoff and MM-07 global-work budgeting

   **Custom-server trust needs a concrete design before sending tokens:** the UI accepts a caller-selected server URL while the app has a configured OAuth client ID. An ID token's audience identifies an OAuth client, not the URL selected in our UI. TLS alone does not prevent a malicious recipient from trying to reuse a bearer token. Establish a trusted origin-to-audience policy and server-validated, bounded nonce/session binding; test cross-server forwarding. Do not accept arbitrary server-supplied audiences or share a cached token across unrelated server origins. Nonce binding must address the intended recipient as well as simple replay

3. **Change the identity mapping and authorization semantics together**

   `server/src/db.rs:132,253` stores and looks up `gpgs_player_id`. Map verified Google subjects uniquely to the existing canonical player UUID, using a dedicated column or a small typed provider-binding table. Callsigns, email addresses, installation IDs and PGS IDs are not substitutes for the verified Google subject. Preserve one abuse budget per Google identity across accounts, sessions and installations; avoid user-accessible unlink/recreate paths that reset restrictions

   `ws_handler.rs:427,2367,3358` and `relay_lifecycle.rs:93` use `gpgs_verified` for session/room policy and renewal ownership. Replace this with explicit verified matchmaking-account semantics, set only after successful server verification. Update both ordinary and friend-join paths, relay renewal, diagnostics and user-facing text. Optional PGS state must never control these checks

   `config.rs`, `lib.rs:58` and test fixtures also use `skip_gpgs_verify` to select development limits, persistent-DB requirements and allocation lifetime. If renamed to a provider-neutral development setting, preserve every production invariant and observed-loopback restriction. Do not mechanically change just the authentication branch

   **Existing data:** the operator confirmed on 2026-10-22 that no accounts, bans or friendships need preservation. Treat existing state as disposable pre-release data and deploy with a fresh database, without migration or account-linking support. A stored PGS ID is not a Google subject; do not infer links from names/email. Local implementation work does not reset deployed data

4. **Isolate PGS and make it explicitly optional**

   `SetupActivity.kt:2915` initializes PGS at startup, and `AndroidManifest.xml:48` enables `PlayGamesInitProvider` before that. Both paths must be addressed. Disable automatic provider initialization in the merged manifest, remove unconditional startup initialization, and add `SUPPRESS_GAME_PROFILE_CREATION=true` as defense against unsolicited profile creation after initialization. Suppression alone is not an off switch for all PGS activity/UI

   The [SDK reference](https://developers.google.com/android/reference/com/google/android/gms/games/PlayGamesSdk) documents manual initialization when its provider is disabled. The [Android PGS guide](https://developer.android.com/games/pgs/android/android-signin#prevent-auto-triggered-profile-creation) documents profile-prompt suppression. These mechanisms require real-device lifecycle testing; delayed initialization is not proven by host tests

   Refactor `src/playServices/.../PlayGamesAuth.kt` into a platform-features boundary, with a corresponding unavailable implementation for builds without PGS. Its availability must depend on PGS configuration, not the matchmaking OAuth client ID. Remove matchmaking code requests from this boundary. Client-only achievements do not require adding PGS tokens or achievement telemetry to the relay server

   Groundwork should expose availability, persisted opt-in and authentication/feature readiness as distinct states. Default to disabled. A future explicit Enable Play Games action can initialize and, if needed, request platform sign-in. Only foreground feature use may initiate interactive recovery; failed background sync should wait. Do not expose a nonfunctional achievements button before there is a usable feature

   Disabling the app integration stops feature calls, invalidates pending callbacks and cancels sync; it must not disconnect matchmaking or claim to sign the user out of Google's platform account. There is no documented SDK shutdown operation in the inspected entry point: verify any residual initialized-SDK behavior through process restart instead of promising immediate removal of every platform callback

   Google sign-in and PGS may select different Google accounts. Do not assume matching identities or silently publish queued achievements to the currently selected profile. Keep PGS profile attribution separate. If achievements are implemented later, retain bounded local progress and explicit attribution for pre-opt-in progress; native events, achievement definitions, icons and full sync are a later feature chunk

5. **Separate build configuration and qualify supported distributions**

   `android/app/build.gradle:46,419,435,572`, `android/get_deps/tool_versions.conf`, `android/auth_config.template.jsonc`, both authentication source sets, and `server/server_config.template.jsonc` currently couple online access to Play Games. Add separately named matchmaking Google OAuth configuration and PGS app configuration, plus pinned Credential Manager/Google ID dependencies. Server ID-token verification needs an audience and signing keys, not the existing PGS OAuth code-exchange secret; remove that requirement from the new path

   Register actual Android application IDs/signing certificates for supported builds, including Play app signing versus local/debug signing where relevant. Google sign-in availability should be independent of PGS availability. Direct-install builds currently exclude the whole Play implementation and report LAN-only; enabling Google login for these builds is a separate dependency/distribution decision, not an automatic consequence of replacing the provider. Keep missing-provider behavior clear and fail closed for public access

   Update `android/README.md:182`, rehearsal instructions, runtime errors, protocol examples and auth-config loader tests. Keep historical security findings as historical evidence and use this ledger to identify the revised current target

### Proposed implementation chunks

- [x] A: settle token audience/origin, nonce, reconnect and existing-data policies; specify the coordinated protocol and abuse invariants
- [ ] B: server Google verification, unique account mapping, production/development policy and authorization rename, with signed-token and live control/relay integration tests
- [ ] C: Android user-initiated Google connection flow, account/cancellation lifecycle, configuration and reconnect behavior, tested against the new server
- [ ] D: optional PGS boundary, startup-provider removal, suppression and opt-in state, coordinated with C so public login remains usable
- [ ] E: supported-build verification, real Google-account/device qualification, documentation and deployment handoff

Implement B-D as one coordinated release even if reviewed in separate local chunks. The local branch now implements the Google-session protocol; deployment must coordinate protocol-9 clients, server, OAuth audience and reverse-proxy routes. B-E remain unchecked until their full verification scope is satisfied. Actual achievements are a follow-on feature, not a prerequisite to separating the authentication providers

### Acceptance tests

- Fresh launch, single-player, LAN, browser navigation and cancelled Connect produce no unsolicited Google or PGS prompt
- Explicit Connect and online resume succeed with a Google account that has no PGS profile; PGS unavailable/disabled does not block public matchmaking
- Cancellation, rotation, activity destruction, server/account switch and stale provider completion cannot authenticate the wrong connection; automatic reconnect never opens UI
- Production rejects unsigned/forged/expired/wrong-issuer/wrong-audience/wrong-nonce credentials and legacy installation/PoW/dev bypasses; cross-server token reuse fails
- Key rotation works without attacker-induced key-fetch storms; key/provider outages and authentication floods preserve bounded work and healthy-user recovery
- Reinstall/callsign changes and simultaneous attempts for the same Google identity produce the same canonical account and shared limits; bans and verified-only policies apply to ordinary join, friend join and renewal
- Existing-account transition, if needed, cannot claim an old account without proof or produce a second budget; otherwise test the selected fresh-schema release explicitly
- PGS opt-in, cancellation, feature failure, opt-out and profile changes leave matchmaking identity and budgets unchanged; inspect merged manifests for every supported build
- Host fixtures must exercise the real verifier with owned signing keys and negative cases. Existing `HostAndroidBoundaries.kt` forces PGS unavailable, so the current passing JVM suite does not test a real Google/PGS flow. Real provider/device tests remain necessary for prompt behavior and signing configuration

Review verification: inspected current Android startup/connect/reconnect, source sets/build configuration, Rust protocol/verification/database/authorization and existing fixture boundaries; checked the linked official documentation. This entry is documentation only, so no runtime test pass or provider qualification is claimed

## AUTH-REV implementation round 1: verifier foundation

Implementation is now authorized. Start with a separately tested Google ID-token verifier, then connect it to the coordinated protocol/database/client change. Keep the current production admission path closed to alternative identities until the replacement is wired and verified

- Add a pinned maintained JWT verifier with explicit RS256, Google issuer, configured audience, expiry, subject and nonce validation
- Use a fixed Google signing-key endpoint and one shared bounded key cache; coalesce refreshes and rate-limit unknown-key refreshes without caching attacker-selected key IDs
- Exercise real RSA signatures against an owned local HTTP key fixture, including malformed/forged credentials, cache rotation, expiry, outages, deadlines and concurrent unknown-key traffic
- Preserve existing production/provider behavior during this foundation chunk; it does not by itself complete the replacement
- Resolve account transition without inferring a Google subject from a PGS ID. Retain old records until an explicit authenticated linking or coordinated pre-release deployment decision, rather than silently dropping bans/data

Round 1 verification:

- Added `server/src/google_identity.rs` and `google_identity_tests.rs`, exporting the verifier from the library but not yet admitting Google tokens through the existing production protocol
- Pinned `jsonwebtoken` 11.1.0, reusing the AWS-LC backend already present through TLS. PEM support is enabled for test fixtures only; the lockfile records the resolved dependencies
- The verifier accepts only signed RS256 tokens for one configured audience and either documented Google issuer spelling, returning only the verified subject and expiry. It requires expiry, issued-at, subject and matching caller-owned nonce; the caller still has to consume and scope that nonce when the protocol is wired
- Bounded tokens to 8 KiB, key responses to 32 KiB and 16 keys, with bounded RSA components and key identifiers. Token-supplied key URLs/keys, unsupported critical headers and alternate algorithms are rejected before network work
- One shared HTTP client uses a fixed Google endpoint with redirects disabled. Cached keys have monotonic expiry respecting max-age/Age with a six-hour cap. Refreshes are serialized and at most once per minute, including failures/cancellation; unknown key IDs do not grow the cache. Valid cached keys do not wait for network refreshes
- Five owned HTTP/RSA fixture tests cover required/invalid claims, forged signatures, algorithm confusion, key-ID rotation/removal, 100 concurrent unknown key requests, stale-key outage/recovery, caller deadlines, cached-key availability during a slow refresh, malformed/oversized/duplicate key responses and cache-header expiry. These are verifier tests, not real Google account or full connection-flow qualification
- The initial negative test exposed library acceptance of multi-audience tokens; added a typed single-audience requirement and reran successfully
- Selected Rust suite passed: 87 library + 111 integration + six candidate-proof + 13 NAT tests (217 total). Clippy over library and tests passed with the two previously recorded baseline categories allowed; no new suppressions. Scoped quality checks and `git diff --check` passed
- Tested with installed Rust 1.96.1; the pinned 1.94.1 toolchain and real provider/device qualification remain open
- The isolated `dxx-matchmaking` binary build passed with the locked dependencies after the tests, confirming the production target builds without test-only PEM support

Next: finalize and implement origin-scoped login challenge/session ownership, wire the verified Google account mapping and authorization checks, then implement the Android foreground login and optional PGS lifecycle. Do not label the full AUTH-REV complete based on this foundation

## AUTH-REV implementation round 2: login ownership and server sessions

- Host login challenge/exchange endpoints beside `/ws`, so the client derives the HTTPS target from its selected WSS origin rather than trusting a URL returned by another server
- Configure the server's public HTTPS origin explicitly; bind Google's nonce to the origin and a random one-use challenge. The Android client must compute this binding from the selected origin, never blindly use a server-supplied nonce
- Keep pending challenges bounded and short-lived without keeping a WebSocket open while Google UI is active. Consume the challenge before provider verification, including failures
- Exchange verified Google tokens for opaque, bounded, expiring server credentials. Reconnect uses the server credential without invoking Google UI; cancellation/account changes discard/revoke it
- Keep credential issuance separate from quota identity. Resolve Google subjects to a unique server UUID, applying bans and existing room/relay checks to that UUID
- Add real HTTP/cryptographic ownership tests before wiring the Android flow. Existing deployed-account preservation was asked asynchronously; core protocol work can proceed independently

### Round 2 foundation verified

- Implemented bounded pending challenges (five-minute lifetime, 1024 global, 16 per source) and opaque hashed reconnect credentials (12-hour absolute lifetime, 2048 global, eight per verified Google subject). Failed exchanges consume challenges and lookup never extends session lifetime
- The nonce binds the random one-use challenge to a canonical HTTPS recipient origin, with an explicit port and a versioned hash rule to mirror in Android
- Added a separate unique Google-subject binding table and transactional player lookup/create. PGS IDs are not reinterpreted as Google subjects, and no existing accounts or deployment data were reset
- Owned RSA/HTTP fixtures prove concurrent exchange has one winner, wrong-origin tokens fail, consumed/expired challenges fail, credentials expire/revoke, account session caps apply, repeated verified logins share the canonical player and its ban, and identical PGS/name strings cannot claim that account
- Eight independent SQLite connections concurrently resolve one subject to one player; reopening the database preserves that mapping without orphan player rows
- Selected Rust suite passed: 92 library + 111 integration + six candidate-proof + 13 NAT tests (222 total). Scoped formatting/lint, Clippy with the two recorded baseline allowances, and `git diff --check` passed
- This is still foundation code, not the completed login flow. HTTP admission/endpoints, live WebSocket credential expiry/revocation, authorization renaming and protocol/client wiring remain next. No new public authentication route is enabled yet, and no real provider/device qualification is claimed

### Round 2 HTTP boundary verified

- Added POST `/api/v1/auth/challenge`, `/api/v1/auth/google` and `/api/v1/auth/logout` to the WebSocket listener. They require `google_auth_origin` / `GOOGLE_AUTH_ORIGIN` plus the configured Google client audience; an empty origin leaves them unavailable during the coordinated transition
- Requests share the existing global authentication work budget and eight concurrency permits with WebSocket authentication. A separate bounded source table allows 64 login requests per minute per IP, using the same exact trusted-proxy identity rules as WebSocket upgrades
- Admission precedes body reading and parsing. JSON bodies are limited to 12 KiB; one five-second deadline covers body reading and verification. Replies contain no token/error details and prohibit caching. Logout is idempotent and revokes only the presented bearer credential
- An owned HTTP fixture uses the real signed-token verifier and actual WebSocket router to exercise challenge/exchange/logout, replay refusal, cache headers, malformed and oversized requests, exhausted permits, per-source limits and untrusted forwarding headers. An incomplete-body TCP client receives HTTP 408 and releases its permit
- Selected Rust suite passed: 93 library + 111 integration + six candidate-proof + 13 NAT tests (223 total). Scoped formatting/lint, Clippy with the recorded baseline allowances, locked server binary build, and whitespace checks passed
- No production service was restarted or deployed. The routes are implemented, but the gameplay WebSocket still accepts the old PGS protocol. Next: Google-session wire method, canonical account/ban authorization and live expiry/revocation, followed by Android foreground sign-in and optional PGS

### Round 3 WebSocket identity switch

- Protocol 9 now requires `auth_method: google_session` and `session_credential` for production WebSocket admission. The Android version constant is coordinated, but the Android login implementation is still pending; this intermediate worktree is not a deployable matched release
- Google session lookup resolves the verified subject through the transactional canonical account mapping. Missing/revoked/expired credentials and banned accounts fail closed, including database ban-check errors
- Session/room/friend-join/relay-renewal authorization now uses `account_verified`, independent of optional PGS feature state. Legacy installation/PoW/dev paths remain observed-loopback-only; the production allowlist no longer admits PGS credentials
- Established Google WebSockets retain only a non-secret credential hash handle. A biased one-second timer checks expiry, revocation and bans, then uses normal disconnect cleanup. The idle deadline is tracked separately so timer ticks do not extend idle connections
- Extended the real HTTP/RSA fixture through a production-policy WebSocket: AUTH_OK gives a verified canonical account, logout closes the established session and removes it, and the revoked credential cannot reconnect
- Selected Rust suite passed: 93 library + 111 integration + six candidate-proof + 13 NAT tests (223 total), plus Clippy with the recorded baseline allowances and whitespace checks. The scoped formatter completed Rust checks but sandbox access to the installed Kotlin JAR failed; the exact pinned Kotlin linter then passed outside the sandbox on the changed constant file
- Next: Android foreground Credential Manager flow and origin binding, server credential lifecycle, separate optional PGS initialization/configuration, and the remaining expiry/ban/relay end-to-end acceptance cases. Do not deploy the intermediate protocol-9 server against the existing PGS-only Android flow

### Round 4 Android provider and exchange foundation

- Added a separate `GoogleMatchmakingAuth` provider using Credential Manager's explicit Sign in with Google option, a foreground activity context, and the origin-bound nonce. It does not initialize or invoke PGS
- Added independently named `matchmaking_google_client_id` / `MATCHMAKING_GOOGLE_CLIENT_ID` configuration. Pinned Credential Manager 1.5.0 and Google ID 1.1.1; the template documents Android signing registration and server audience matching
- Added `GoogleLoginClient` to derive HTTPS endpoints solely from the selected WSS origin, prohibit redirects, bound response bodies, cancel in-flight HTTP on coroutine cancellation and separate the human sign-in duration from five-second HTTP timeouts
- Cached server credentials have monotonic deadlines and exact origin scope and do not expose secrets through generated data-class diagnostics. Direct-install builds currently retain their provider-free boundary and explicitly report Google sign-in unavailable
- Host tests cover canonical host/port/IPv6 origins, rejected insecure/credential-bearing/path/query/fragment URLs, an independently calculated SHA-256 wire vector, nonce separation, session expiry and diagnostic redaction. The real HTTP helper and full Android/provider lifecycle still require integration qualification
- Android debug compilation and both focused host tests passed again after fixing the new response-body nullability warning. Scoped formatting/lint and whitespace checks passed; remaining Gradle deprecation notices are unchanged
- The build environment has no auth_config.jsonc, so real-account sign-in cannot be qualified here without configuration. No credentials were provisioned or private configuration changed
- Next: connect this provider/helper to explicit Connect/online-resume actions, define cancellation/account switching and noninteractive reconnect behavior, replace the old MatchmakingService PGS path, and disable/isolate PGS startup behind opt-in

### Round 5 Android connection flow and startup separation

- Explicit Connect/online-resume now obtains a Google server credential before opening a gameplay WebSocket. The service no longer calls PGS or requests its server auth code
- Automatic reconnect and `sendAuthenticate` use only the cached origin-scoped credential, with the protocol-9 `google_session` method and `session_credential` field. Expiry/rejection returns to a visible Connect-required state; local development installation fixtures remain loopback-only
- Disconnect/server switch cancels pending sign-in, invalidates its generation, drops local credentials and attempts bounded server logout. Stale completed sessions are revoked instead of attached to a replacement connection. Activity replacement/destruction cancels an in-progress interactive flow without disconnecting an already authenticated game
- Explicit sign-in clears prior provider selection and checks for a resumed activity before requesting credential UI. Credential transmission is checked against the actual socket URL, not mutable browser/UI text
- Removed SetupActivity's PGS initialization and disabled the PlayGamesInitProvider in the source manifest, with profile-creation suppression. Regenerated main and downstream debug merged manifests both show the provider disabled and suppression enabled. No remaining main-source PlayGamesAuth call sites were found
- Full JVM host suite passed against the owned protocol-9 server binary, including real local socket discovery/relay renewal/reconnect/outage tests; Android debug compilation passed. A subsequent focused run covers actual service cached-credential transmission and missing-provider rejection, plus the nonce/session tests
- Scoped formatter/lint and whitespace checks passed. Real Google UI, rotation/cancellation during provider UI, issued-token cancellation cleanup and signed Android-to-Rust HTTPS login still need qualification; the host provider boundary intentionally remains unavailable
- Next: finish the independently configured PGS feature boundary and persisted opt-in groundwork; complete account/cancellation/expiry integration coverage, supported release/distribution builds, operator configuration documentation and real-device qualification. No deployment was performed

### Round 6 optional Play Games feature boundary

- Replaced both PlayGamesAuth implementations with PlayGamesFeatures; removed the unused PGS server-auth-code API, GPGS_SERVER_CLIENT_ID build field and server_client_id template setting. No main-source startup or matchmaking call site invokes PGS
- PGS availability depends only on its app ID/build, while matchmaking uses its separate Google OAuth audience. The provider-free distribution exposes the same feature interface as unavailable
- Added separately represented availability, persisted default-off consent and authenticated profile readiness. Explicit enable initializes the SDK and may recover sign-in only while foreground; refresh never initializes or manually signs in. No unusable achievement UI was added
- Generation ownership rejects results after opt-out/replacement and prevents a delayed authentication check from launching a new sign-in after opt-out. Every refresh clears the old profile before resolving the current one; PGS profile IDs are not linked to matchmaking subjects
- Consent is excluded from cloud backup and device transfer. Disable stops our feature integration and invalidates pending results without touching matchmaking, but does not claim to unload the initialized SDK or sign out the Google platform account
- Updated Android setup documentation and the auth template to separate matchmaking audience/signing registration, same-origin HTTPS routes and optional PGS configuration
- Debug and release Play Kotlin compilation passed with no new source warnings. Both gate tests passed for default-off/restored-consent/unavailable states and stale-callback/profile replacement. Scoped lint passed after the formatter split a long condition; the release merged manifest confirms disabled automatic initialization and profile-prompt suppression
- Remaining: provider-free build qualification, real provider/device behavior, signed client/server HTTPS flow and cancellation/revocation coverage, remaining configuration/protocol cleanup and final acceptance audit. Actual achievement definitions/events/uploads remain a follow-on feature as planned

### Round 7 provider-free build and server cleanup

- Provider-free CI Android source set compiled successfully and generated its merged manifest with neither the PGS initialization provider nor PGS app ID. This qualifies that source boundary, not GitHub release signing or a physical device
- Removed the unreachable legacy server PGS code exchange and its old verifier/test module, plus GOOGLE_CLIENT_SECRET configuration. Production now has only the Google-session credential path; token-shaped legacy fixtures retain a second explicit loopback/dev guard and always remain unverified
- Updated server startup diagnostics, configuration examples, rehearsal instructions and relay policy documentation for Google subjects rather than PGS identity
- Added a real signed-credential WebSocket test proving an existing ban rejects login, a newly imposed ban closes a live connection, and absolute credential expiry closes the connection and prevents reconnect. The expiry fixture changes only the stored expiry under cfg(test), not the production lifetime
- Selected Rust suite passed: 93 library + 111 integration + six candidate-proof + 13 NAT tests (223 total). One removed legacy PGS test is replaced by the live Google ban/expiry test. Clippy with the recorded baseline allowances, scoped formatting/lint and whitespace checks passed
- Corrected the nginx template to route `/api/v1/auth/*` to port 9000 alongside `/ws`, with a 12 KiB body cap, disabled request buffering/cache and finite timeouts. Neither nginx nor Docker is available on this host, so executable nginx validation remains a deployment gate; no external configuration was applied
- Next priority: fail-closed handling of existing PGS-backed account data before production transition, cancellation-after-issuance and true Android-helper-to-Rust HTTPS coverage, then final provider/configuration/build audit. No existing database was reset or migrated

### AUTHREV implementation round 8: disposable pre-release data decision

- The operator confirmed that no existing account data needs to survive the Google sign-in transition, superseding the account-preservation question and round 7's proposed legacy-data gate
- Removed the speculative legacy-account startup guard, its database helper and its preservation test. No migration or account-linking machinery is required
- Updated release instructions for fresh Google-backed accounts. No deployed database was deleted, reset or migrated
- Validation passed: 93 library, 111 integration, six candidate-proof and 13 NAT tests (223 total), scoped formatting/lint and whitespace checks
- Remaining priorities are cancellation-after-issuance coverage, Android-helper-to-Rust HTTPS coverage and final provider/configuration/build qualification

### AUTHREV implementation round 9: cancelled login ownership

- Added bounded challenge receipts and possession-only POST `/api/v1/auth/cancel`. An exchange marks its challenge used before verification and checks that cancellation has not removed it before issuing a credential under the same state lock
- Cancellation removes pending/in-flight challenges or revokes the issued credential without requiring the client to have received its secret. Unknown/repeated cancellation allocates no state. Receipts expire five minutes after issuance and share the existing 1024-global/16-source challenge caps
- Android attempts challenge cancellation on provider/exchange failure, including coroutine cancellation, using a non-cancellable cleanup section with the existing five-second HTTP timeout. Network outages can still prevent cleanup; bounded session expiry remains the fallback
- Real signed-token server tests cover cancellation before/during verification, lost-response revocation, replay refusal, unaffected unrelated credentials and capacity recovery across 20 cancelled logins. The real HTTP/WebSocket fixture checks idempotent cancellation and live connection closure
- The Android helper now accepts an internal HTTP-client dependency for owned transport fixtures; production callers retain the standard TLS/redirect/timeout configuration. A JVM interceptor test proves provider cancellation and simulated exchange-response loss send challenge cleanup
- Validation: 224 selected Rust tests and seven targeted JVM tests passed; scoped formatting/lint passed. Actual Android-helper-to-Rust HTTPS, real Google provider/device behavior and final build/configuration qualification remain incomplete

### AUTHREV implementation round 10: Android helper to Rust over HTTPS

- Added an explicitly ignored Rust test-binary fixture with owned signing-key service and the actual production-policy authentication router. No fixture routes or configurable signing-key source were added to the production binary
- Added opt-in `GoogleLoginHttpsTest`, selected with `-PgoogleFixtureBinary`, plus a reproducible build/run command in `android/proxy-tests/README.md`. The test creates a temporary certificate, uses normal hostname validation with fixture-specific trust and forwards owned loopback HTTPS requests to the actual Rust routes
- Verified the real Android helper's nonce derivation, signed exchange, credential validity and logout across the language/HTTP boundary. A wrong nonce fails, and dropping an already-issued exchange response triggers challenge cleanup that actually revokes the Rust credential
- All eight targeted JVM authentication tests passed, including the HTTPS integration; scoped formatting/lint and Clippy with the existing baseline allowances passed
- This is JVM execution of the production helper, not an Android/Google provider test. Real device prompt/lifecycle behavior, OAuth configuration, supported-build qualification and deployment-proxy validation remain separate gates

### AUTHREV current acceptance audit

| Requirement                                 | Current evidence                                                                                                                                          | Remaining verification                                                                                       |
| ------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------ |
| Separate Google account from PGS profile    | Google-subject database mapping and signed-token ownership tests; independent PGS configuration/state                                                     | Real Google account without a PGS profile                                                                    |
| Explicit foreground login, silent reconnect | Sole provider sign-in call in explicit `MatchmakingService.connect`; cached-session reconnect tests; generation/activity guards                           | Device prompts, cancellation, rotation and activity recreation                                               |
| Bounded public authentication               | Signed-token negative/rotation/flood tests, finite budgets, production allowlist and signed-account verified-room/renewal tests                           | Real-provider and deployment qualification; UDP binding handshake remains covered by separate relay fixtures |
| Cancellation and lost responses             | Rust in-flight cancellation tests, JVM coroutine cleanup test and real HTTPS issued-response-loss revocation                                              | Provider UI cancellation and process/lifecycle behavior on device                                            |
| PGS default off and independent             | Isolated feature gate tests; no startup SDK call; Play debug/release merged manifests disable provider and suppress profile creation                      | Real SDK behavior after enable/disable/process restart                                                       |
| Build and configuration                     | Play and provider-free CI debug/release Kotlin compilation and manifest processing passed on 2026-10-22; actual auth-config loader regression task passed | Distribution signing/OAuth registration and actual provider qualification                                    |
| Existing data                               | Operator explicitly declared all pre-release account data disposable                                                                                      | Fresh database is a deployment instruction, not a local data-deletion task                                   |
| Deployment                                  | Local nginx routes/template and configuration instructions updated; extracted Ubuntu nginx configuration check passed                                     | Operator validation with actual certificates/ports and authorized deployment; no external changes made       |

The repository has no `auth_config.jsonc` in this environment and no selected dedicated Google-enabled test device. The operator was asked for an existing OAuth web client ID and a coordinated test device. Do not count host fixtures or unconfigured builds as real-provider qualification

Round 11 build evidence: `:app:compileDebugKotlin :app:compileReleaseKotlin :app:processDebugManifest :app:processReleaseManifest` completed successfully for both default Play and `-PciApk=true`. Parsed Play manifests have provider enabled=false and suppression=true; parsed CI manifests have package `com.dxxredux.app.ci`, no PGS provider and no PGS app-ID metadata. No APK was installed or signed for distribution. Source inspection confirms ordinary/friend joins and renewal consult `account_verified`; stale PGS verification comments were corrected. Full signed-provider admission-to-game coverage remains a distinct integration task

### AUTHREV implementation round 12: authentication configuration tests

- Extracted the actual Gradle authentication loader into `android/gradle/auth-config.gradle` and added `:app:testAuthenticationConfig`, included in the existing JVM test runner
- Independently validates optional matchmaking OAuth audience and PGS app ID before generating source/resources. Missing keys and template placeholders remain unconfigured; unknown/obsolete fields, wrong types, malformed IDs and oversized values fail with fixed messages that do not print configuration contents
- The task passed independent-default, template, single-provider, combined JSONC and 16 rejection controls. This does not create OAuth credentials or prove provider acceptance of a particular ID
- Scoped formatting/lint and whitespace checks passed. Both `test_validate_automation_catalog.ps1` and `test_run_all_tests_catalog.ps1` completed successfully after wiring the check into the existing runner
- Updated operator configuration documentation. Real OAuth/device qualification remains outstanding

### AUTHREV implementation round 13: signed-account authorization policies

- Added a real signed-token/production-policy WebSocket test with three distinct Google subjects. Authentication creates verified matchmaking sessions, ordinary JOIN_LOBBY succeeds for a verified-only room and JOIN_FRIEND_GAME succeeds through an existing accepted friendship without any PGS profile
- The test seeds only an already-established owned gameplay relay allocation, then exercises real renewal scheduling and receives matching renewal offers over both authenticated sockets. Removing either member's verified flag retires both key generations and the renewal record
- The seeded allocation is explicit: this test does not claim a new full UDP binding/gameplay qualification. Existing candidate-proof and relay lifecycle integration suites cover that separate transport path
- The focused test and full selected server regression set passed: 95 library + 111 integration + six candidate-proof + 13 NAT tests (225 total); the explicit JVM HTTPS fixture remains ignored by ordinary Rust runs. Scoped formatting/lint, Clippy with baseline allowances and whitespace checks passed

### AUTHREV implementation round 14: complete JVM regression and pinned Rust

- Ran the existing `android/tests/test_gradle_unit_tests.ps1` to completion with JDK 21. Its evidence validator reported 1221 executed/one skipped app tests, 21 executed metadata tests and 50 executed/five skipped proxy tests: 1292 executed total, no failures. The new auth-config task also passed
- Proxy skips were explicit opt-in fixture/history cases: owned Google HTTPS fixture, real-server installation/candidate/outage fixtures and historical proxy comparison. They are not counted as exercised by this broad run; the Google HTTPS fixture was separately run successfully in round 10
- Installed the CI-pinned Rust 1.94.1 alongside the existing toolchains, without changing the default. All 225 selected server tests passed under that exact version (95 library, 111 integration, six candidate-proof, 13 NAT)
- Extended verification to the three owned process-readiness rehearsals. The first run exposed a Windows fixture issue: clearing the entire environment also removed `SystemRoot`, and the child failed Winsock initialization. The fixture now preserves only that OS variable on Windows and captures startup output for useful failure diagnostics, while still excluding deployment configuration. All three rehearsals then passed
- No deployed services, databases or emulators were modified. Real OAuth/device qualification and external proxy validation remain outstanding

### AUTHREV implementation round 15: executable nginx configuration validation

- Found an existing Ubuntu WSL environment after Windows had no nginx/Docker executable. Queried its package metadata, downloaded exact Ubuntu package `nginx=1.24.0-2ubuntu7.18` and extracted it only into `temp/nginx-auth-validation/package`; no OS package/service installation was performed
- Generated a one-day owned test certificate and a disposable configuration wrapper from the actual repository nginx template. Only DOMAIN/certificate paths and listen endpoints were substituted; every route, upstream target, body cap, proxy header and timeout directive remained unchanged
- The initial unprivileged check accepted syntax but could not bind port 443. Switching the disposable copy to loopback-only 43443/43080 let `nginx -t` complete successfully. The repository template was not changed, nginx was not started as a service and no upstream or deployed server was contacted
- Reproduction artifacts remain under `temp/nginx-auth-validation/`, including `prepare-config.ps1`; the certificate expires after one day and is not a deployment certificate. This closes local executable syntax/configuration validation, not actual production certificate/port/reverse-proxy testing
- Rechecked that local `auth_config.jsonc` is absent. `keystore.properties` exists, so actual distribution source-set compilation can be checked next without exposing signing values; real OAuth/device qualification still needs operator input

### AUTHREV implementation round 16: actual direct-install variants and provider handoff

- Ran `:app:compileReleaseKotlin :app:processReleaseManifest :app:testAuthenticationConfig` with `-PgithubRelease=true -PisolatedGithubBuild=true`, then separately with `-PlegacyRelease=true -PisolatedGithubBuild=true`. Both completed successfully using JDK 21
- Parsed each resulting merged manifest: current direct install uses package `com.dxxredux.app.github`, minimum API 24; legacy uses `com.dxxredux.app.github.legacy`, minimum API 23. Both contain zero PGS init providers and zero PGS app-ID metadata entries
- These are source compilation, configuration and manifest checks, not signed APK packaging or device execution. No artifact was published or installed, and signing values were not displayed
- Added a concrete real-provider qualification checklist to `android/README.md` and corrected stale protocol-2/3 rollback wording in the rehearsal guide to the coordinated protocol-9 boundary. Scoped documentation formatting and whitespace checks passed
- Local source/build/host-fixture work now has evidence across Play, CI, current direct-install and legacy variants. The remaining authentication acceptance gap is operator-provided OAuth configuration and an owned Google-enabled device for actual provider/UI/lifecycle checks. The optional PGS boundary has no shipping feature UI; it must be device-qualified before an achievements feature is wired to it

## Deployment boundary

### AUTHREV implementation round 17: operator OAuth configuration and GitHub identity

- Operator supplied three OAuth exports in ignored `android/credentials/`, all in project `dxxrevival`, and authorized S21 device `R3CR40Q4XPK`; worker emulators remain out of scope
- Populate ignored `auth_config.jsonc` with the Web client audience only; retain unconfigured optional PGS
- Share Google identity implementation between Play and GitHub, keeping CI provider-free and GitHub PGS/update SDKs excluded
- Add tracked `android/CREDENTIALS.md` with local credential creation and preservation instructions
- Scoped formatting passed; GitHub release Kotlin compilation, merged manifest and authentication configuration tests passed. Seven focused Google login/matchmaking/PGS state tests passed. GitHub manifest includes Credential Manager and zero PGS initialization providers
- Signed GitHub `assembleRelease` passed (all three native architectures, release lint). APK signature verified against release SHA-1 `19:07:70:7C:4B:40:76:35:FA:B6:A6:A7:CF:A4:79:10:73:3B:6E:CD`; installed on S21 alongside existing Play app. Cold launch returned Status ok and a live process. Existing native engine warnings remain; no new native source was changed in this round
- Pending: real-provider S21 qualification; do not infer OAuth package/certificate registration correctness from exported client IDs alone. Asked operator for the HTTPS protocol-9 test server target; no deployed service/configuration was changed. Launch success is not evidence of completed Google login or prompt/lifecycle qualification

Prepare and validate deployment artifacts locally. Applying firewall/proxy/service changes or provisioning Google credentials on an external VPS requires the operator's deployment target and authorization; no production deployment is implied by this implementation goal
