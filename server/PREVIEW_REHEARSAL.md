# Isolated online preview rehearsal

Online release remains disabled. Protocol 3 requires matching server and client
artifacts. Listener readiness is necessary, not a gameplay or authentication
qualification result. Do not run the existing installation/update scripts for a
local rehearsal: they modify service and proxy configuration.

## Local gates

Use the exact Rust version in `rust_version.txt`, committed Cargo.lock, and the
repository-pinned JDK/Gradle tools. Run from the repository root:

```sh
timeout 120 cargo test --locked --manifest-path server/Cargo.toml
cargo clippy --locked --manifest-path server/Cargo.toml --all-targets -- -D warnings
# With the pinned Gradle distribution and JDK available:
gradle -p android -PhostJvmOnly=true :proxy-tests:test
```

`process_readiness` launches the real binary with an empty inherited environment,
synthetic database and loopback listeners. It verifies relay/STUN bind failures
terminate the process, readiness requires all configured listeners, process stop
removes HTTP availability, and restart opens the same synthetic database with no
rooms or relay leases. Every child is killed and reaped on success or failure.
The WS integration suite separately proves real STUN and authenticated UDP paths.
The JVM suite uses the actual proxy; it does not replace Android device testing.

`/api/v1/health` is process liveness. Use `/api/v1/ready` for listener readiness:
503 until all required listeners bind, 200 while all remain alive. Any unexpected
required task exit shuts down the process with status 1. Readiness includes the
protocol version and aggregate socket, room and lease counts, never keys or
addresses. Verify those counts return to baseline after each lifecycle scenario.

## Owned TLS and artifact rehearsal

With Python 3.11+ and OpenSSL available, use supplied locally built artifacts:

```sh
python3 server/rehearse_preview.py --binary /absolute/path/candidate
python3 server/rehearse_preview.py --binary /absolute/path/candidate --previous-binary /absolute/path/previous
```

The runner binds only loopback listeners, clears inherited server configuration,
uses a disposable synthetic database and generates a one-day localhost test
certificate in a private temporary directory. Normal trust must reject that
certificate; explicit test trust must complete a real WSS upgrade. Readiness and
all aggregate resources must return to baseline before each owned process stops.
All children are reaped and temporary state removed on failure as well as success.

Without a distinct previous binary this reports restart only. With one, it tests
previous/candidate/previous against the same synthetic database and requires the
same protocol version. Receipts contain artifact hashes and phase outcomes. This
has been exercised with distinct e079f44 and 7e0bd045 debug server artifacts
on Python 3.12.14/OpenSSL 3.5.7, with every phase passing. It
qualifies local TLS/process rollback only: no proxy topology, real database
migration, matched Android rollback or gameplay claim follows from it.

## Branch Android gameplay campaign

The existing extended-suite workflow accepts the bounded `online-preview`
campaign on a reviewed topic branch:

```sh
gh workflow run nightly-tooling-sweep.yml --ref online-restoration -f campaign=online-preview
```

It selects only `test_mp_public_shareware`, builds the matching locked server
with the repository-pinned Rust version, and stages hash-verified public D1
shareware onto three verified disposable emulators. The runner owns loopback WS,
HTTP, STUN and authenticated relay listeners with a synthetic in-memory database.
It uses the production installation challenge/proof path with development
authentication disabled. The preceding host test runs the actual Android service
against the real local server with a deterministic test key. This is not a GPGS
provider or public TLS deployment qualification. Occupied test ports fail rather than killing unrelated
processes, and this public variant makes no firewall changes.

Pass requires actual native D1 engine selection and advancing native gameplay
counters on every peer link. The campaign adds a third client, removes the middle
slot, proves surviving traffic beyond the relay timeout, and rejoins the departed
slot with fresh routes. Host closure and final disconnect must retire rooms,
relay leases, control sockets and client jobs/sockets. Logs alone and absent
introspection cannot pass. The next matrix also exercises host/nonhost background/resume, short and long
UDP-only loss, delay/duplication/reordering, loopback address/port rebinding, WS-only loss
and cancellation, and server restart followed by explicit fresh-game recovery.
These new device cases require terminal receipts before being treated as qualified;
internet NAT behavior remains a separate gate.
Retain the exact head SHA, Actions run ID and receipts before expanding the matrix.

`loopback_fault_proxy.py` changes only owned loopback forwarding. Its atomic
control file independently selects WS or UDP faults; per-client upstream UDP
sockets preserve distinct endpoint identities. Impairment introduces deterministic
loss, duplication, delay and reordering; rebinding changes upstream source ports; the address-change case uses only the
owned 127.0.0.2 loopback address.
It never logs payloads or changes firewall/network configuration. Four owned-socket
negative/positive controls in `test_loopback_fault_proxy.py` qualify the injector
before the device owner runs. The public owner has a 1800-second budget and runs
cases sequentially using one prepared APK/emulator set. Background dwell follows
the LAN test's 110-second launcher/resume route without hidden-process broadcasts.
After cancellation, resource assertions run without another disconnect so a
resurrected retry cannot be hidden by cleanup. Long-loss/server-restart recovery
requires a newly launched game with advancing native peer traffic, not only WS
reconnection. No automatic restoration of the interrupted game is claimed.

## Private staging prerequisites

Record the owned hostname/provider, deployment owner, approved service account,
network/TLS termination topology, secret provisioning mechanism, client flavor
and identity provider, and previous known-good artifact. Resolve these facts
before producing destination-specific commands. No destination is assumed here.
Do not copy production databases or credentials into rehearsal.

Build once from the reviewed SHA using `cargo build --locked --release` and the
matching Android build. Record SHA-256 hashes, tool versions, protocol version and
configuration schema; retain the previous matching artifact pair. Validate the
exact nginx identity-header configuration in an isolated namespace or owned
staging instance. Only explicitly configured proxy peers may set X-Real-IP.
Prove the real GPGS or approved direct-install producer, not a development bypass.

Use a synthetic database and two/three owned clients. Cover all pairs at least
once, then apply pairwise fault scenarios rather than every Cartesian combination:

| Scenario                                  | Roles                   | Required observation                                                           |
| ----------------------------------------- | ----------------------- | ------------------------------------------------------------------------------ |
| connect, cancel, retry                    | host and joiner         | valid game payload or bounded visible failure; counts return to baseline       |
| duplicate login, old close, stale packets | host and nonhost        | replacement owns state; old work cannot change it                              |
| room leave, kick, switch, end             | three clients           | stable slots; only affected leases revoked; no orphan room                     |
| relay/server restart                      | two clients, then three | bounded failure/recovery; new keys; old traffic rejected                       |
| loss, duplication, reorder, delay         | one impaired pair       | valid payload recovers or visible terminal state; replay does not reach engine |
| NAT/address change, background/resume     | host and nonhost        | route proof, fresh ownership, no leaked jobs/sockets/mappings                  |
| late join, stale session, version skew    | three clients           | explicit compatible admission or clear rejection; no partial launch            |
| roster capacity and shared NAT            | eight clients           | valid roster plus bounded reconnect waves; excess admission rejected           |

Capture aggregate counters, reason codes, timing and test IDs. Never capture
credential-bearing control bodies, user chat, invite codes or relay keys. Local
NAT models do not qualify carrier NAT, IPv6, mobile background limits, certificate
renewal or real provider outages; reserve those for the owned staging matrix.

## Rollout and rollback gates

Before any opt-in preview: exact-head CI and independent review pass; Android APK
and two/three-device tests pass; each accepted identity producer works; staging
matrix passes; all counts return to baseline; the operator has rehearsed rollback.
The release browser stays disabled until those gates are explicitly accepted.

Rollback is a matched artifact/configuration operation. Disable new admission,
notify/drain the preview within the agreed bound, stop the service, preserve a
private database snapshot, restore the previous reviewed binary/configuration and
matching client artifact, then repeat readiness and a synthetic two-client game.
Do not serve protocol 2 clients from protocol 3 or roll back only one endpoint.
Restore a database snapshot only if schema compatibility requires it, and retain
the failed-state snapshot for diagnosis. No rollback, deployment, firewall change,
secret creation or live probe is executed by this document or the local tests.

## Installation identity contract

The `p256_challenge` method uses a fresh connection-bound nonce and P-256 ECDSA
with SHA-256. The domain-separated transcript includes protocol version, nonce,
public key, callsign, platform, audience and proof-of-work solution. Challenges
are single-use and expire with the absolute authentication deadline. Replacing an
authentication attempt invalidates its outstanding challenge. Registration costs
bounded client proof-of-work; known keys still require a fresh signature.

Set `INSTALLATION_AUTH_AUDIENCE` to the exact client-visible `scheme://host:port`,
including the port (for example the approved HTTPS hostname with `:443`). Empty
configuration disables this method. The client derives the audience from its
actual connection URL, never from the challenge; the server checks its configured
audience. The debug loopback campaign explicitly uses `http://10.0.2.2:9000`.
Production Android connections require WSS with normal certificate validation.

Installation keys are scoped to that origin and retained in private, non-backed-up
app storage. App-data deletion loses the identity; this is not a cross-device
account or an account-recovery mechanism. Installation identities remain unverified
and do not satisfy GPGS-only room policy. The old timestamp-only keypair method
remains restricted to explicit loopback development mode. No live account or
credential is created by the fixture tests.
