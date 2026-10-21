# Online restoration implementation

Base: cmake 092f4c7d247eaeb26160933540f3bdbf2f318d4a

Keep the online release browser disabled until qualification completes. Use an
isolated local server and synthetic identities for development. The detailed
security working report is private and is not part of this branch.

## Work sequence

1. Establish real loopback control/data-plane tests and negative controls
2. Make connection replacement and room departure atomic and idempotent; enforce
   room authorization, explicit production authentication and trusted proxy identity
3. Bind relay admission and cleanup to sender, pair and game lifetime; verify
   legitimate native producer traffic and three-player global slot numbers
4. Qualify Android cancellation/reconnect and register bounded recovery coverage
5. Prepare isolated deployment/readiness/rollback rehearsal with synthetic state
6. Publish a reviewable opt-in rollout plan; actual staging/live work needs the
   owned destination and applicable approval, and is not implied by local tests

## Policies

- Duplicate login replaces the previous connection, fencing all old work
- Host departure closes its room consistently; online host migration is deferred
- Dev identity is explicitly local/test-only, never a production fallback
- Keep protocol changes explicit and coordinate client/server rollout
- No production probes, credentials, infrastructure changes or merge

## Evidence

Record exact commands, results and remaining gaps in each implementation receipt.
Use existing CI evidence from its owner instead of duplicating active runs.

### Connection and room lifecycle slice

Local Rust 1.94.1 build uses the committed Cargo.lock. Three new socket-level
negative controls failed before implementation; the complete integration target
then passed 69 tests after implementation, including replacement close ordering,
room isolation, legitimate friendship, host departure, repeated authentication and
close-before-welcome cleanup. Commands use `cargo test --locked --manifest-path
server/Cargo.toml --target-dir android/temp/online-server-target --test integration`.
This is local server coverage, not Android or internet deployment qualification.

Short control-plane transitions use one mutex, never across network awaits, so
replacement, authorization checks and mutation cannot interleave. Host departure
closes the room; a rejected join preserves the old room. Session retirement also
releases its STUN admission reference. Slow outbound clients are retired instead
of silently dropping state transitions. Existing native/game behavior is unchanged.

### Authentication and admission slice

Production GPGS now requires explicit configuration and rejects dev/unknown auth
methods. Local test dev authentication is restricted to loopback. Known server
keypair authentication remains available; release Android currently requires
configured Play Games for the online preview and reports unavailable authentication
explicitly on direct-install builds. This is a visible preview restriction, not
completion of Android keypair support. Release WS requires TLS and normal trust.

Pending sockets count toward the connection cap and have an absolute authentication
deadline. Explicit trusted proxy peers may supply exactly one valid X-Real-IP;
direct peers cannot override identity through headers. Frame/candidate sizes are
bounded. OAuth responses have time/size bounds and do not log credential payloads.

The full local `cargo test --locked` domain passes: module tests, 74 integration
cases and 13 NAT simulator cases. A local OAuth fixture checks the real form/bearer
exchange and verified-player response, including denial and oversize responses;
no external identity provider was called. Android source was scoped-formatted;
Android APK and device qualification are still pending. Host JVM proxy qualification
is being provisioned separately using the pinned Gradle/JDK versions.

### Protocol 3 relay slice

Protocol 3 requires coordinated server/client installation and rejects older and
future control protocol versions. Do not enable the release browser on this basis
alone. Each pair allocation supplies independent random AES-256-GCM uplink and
downlink keys for each endpoint over authenticated WSS. The relay decrypts and
reencrypts; this does not provide end-to-end secrecy from the service. Never log
control message bodies containing credentials. Local plaintext WS is test-only.

The UDP format is `DXR3`, token (u32 little endian), kind, sender slot, destination
slot, sequence (u64 little endian), ciphertext and a 16-byte GCM tag. The 19-byte
header is AAD. The nonce is four zero bytes followed by the sequence in big endian.
Sequences start at one, are bounded by signed 64-bit maximum, and cannot reset
with the same directional key. New allocations always generate fresh keys. Maximum
plaintext is 4096 bytes. A 64-packet replay window permits bounded reordering.

A source endpoint must complete an authenticated BIND/CHALLENGE/PROOF exchange
before forwarding. BOUND confirms admission. Reordered bind packets cannot replace
a newer endpoint. Challenge responses are no larger than requests. Per-pair work
is bounded; this is not yet a qualified internet-wide abuse policy. Stable global
slots support all 28 pairs in an eight-player roster. Room end, departure and kick
revoke the affected leases; inactive leases expire after 120 seconds. One-second
bind keepalives preserve active games. Duplicate game starts are rejected.

Local qualification includes real WS-issued relay keys, real dual STUN listeners,
4096-byte UDP exchange, duplicate rejection, all three pairs in a three-client
room, stable slots after a middle-player departure, and no rooms/leases/sessions
or STUN references after closure. Exhaustive pair unit tests cover slots 0..7,
address rebinding and fresh-allocation rejection. Rust and Kotlin share a fixed
AES-GCM fixture independently generated with Python cryptography. Host JVM tests
exercise the actual proxy in shared and separate socket modes and reject wrong
sources, wrong tokens and replay before engine delivery. These are synthetic game
payloads, not native valid-producer or Android lifecycle qualification.

Admission now allows 24 upgrades per minute per source IP: an eight-player roster
plus two complete reconnect waves, while retaining the concurrent cap and an
absolute authentication deadline that also wraps OAuth verification. The local
shared-NAT regression exercises the 24 valid connections and rejects the 25th.

Outstanding gates: native valid-producer traffic, full Android compilation and
device recovery, direct-install authenticated producer, deadline-specific timing
regression, production-shaped readiness/rollback rehearsal, independent protocol
review and exact-head CI. Owned staging/live destination and deployment approval
are still required before any external rollout.

Validation receipt: Rust 1.94.1 `cargo test --locked --manifest-path
server/Cargo.toml --target-dir android/temp/online-server-target` passed 41 module,
78 integration and 13 NAT cases under an external 120-second timeout. `cargo
clippy --locked --all-targets -- -D warnings` passed with the same manifest/target.
Pinned Gradle/JDK `-PhostJvmOnly=true :proxy-tests:test` passed all five active
proxy cases. Scoped mixed formatting and `test_validate_automation_catalog.ps1`
passed. `test_run_all_tests_catalog.ps1` is blocked on this host: its Linux child
supervisor exits 125 because `/proc/self/task/<pid>/children` is unavailable.
No other task's CI run was dispatched or changed.
