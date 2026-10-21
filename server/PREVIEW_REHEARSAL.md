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

## Branch Android gameplay campaign

The existing extended-suite workflow accepts the bounded `online-preview`
campaign on a reviewed topic branch:

```sh
gh workflow run nightly-tooling-sweep.yml --ref online-restoration -f campaign=online-preview
```

It selects only `test_mp_public_shareware`, builds the matching locked server
with the repository-pinned Rust version, and stages hash-verified public D1
shareware onto two verified disposable emulators. The runner owns loopback WS,
HTTP, STUN and authenticated relay listeners with a synthetic in-memory database.
It uses explicit debug identities; this is not a production GPGS or direct-install
identity qualification. Occupied test ports fail rather than killing unrelated
processes, and this public variant makes no firewall changes.

Pass requires actual native D1 engine selection, live network gameplay with both
connected callsigns on each client, a bounded gameplay soak, then zero rooms,
relay leases and control sockets after explicit disconnect. Logs alone and absent
introspection cannot pass. This initial two-client campaign does not establish
three-client late join, mobile background recovery or internet NAT behavior.
Retain the exact head SHA, Actions run ID and receipts before expanding the matrix.

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
