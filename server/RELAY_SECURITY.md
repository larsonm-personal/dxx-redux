# Relay security and bandwidth policy

The relay is a fixed, authenticated two-player path, not a public TURN service or a
general UDP proxy. A client cannot choose a third-party destination. Both players
must belong to the live game admission and prove their UDP bindings. Production
requires a verified Google account; installation keys and PoW are loopback development fixtures only.
Google sign-in does not prove that a client is unmodified or that opaque game bytes are gaming.

There is deliberately no game-packet content classifier. Colluding clients can
encode non-game content in valid-looking game messages. Admission, attributable
identities, finite resource budgets, revocation and operational limits are the
primary defenses. The relay terminates encryption; payloads are not logged or
persisted by the accounting system.

## Policy configuration

Set `relay_policy` in the server JSONC configuration, or set `RELAY_POLICY_JSON`
to a JSON object. The environment object replaces the file object; omitted fields
use defaults. Unknown nested fields, zero limits, out-of-range integers and rates
above the existing one-MiB/s identity ceiling are rejected. Limits use bytes, not bits.

| Field                         | Default       | Behavior                                                                |
| ----------------------------- | ------------- | ----------------------------------------------------------------------- |
| `enforce_identity_quotas`     | `false`       | Observe identity overages initially; enable after representative traces |
| `sustained_bytes_per_second`  | 262144        | Identity token-bucket refill, 256 KiB/s                                 |
| `burst_bytes`                 | 268435456     | Identity burst capacity, 256 MiB                                        |
| `identity_ten_minute_bytes`   | 536870912     | 512 MiB per ten-minute window                                           |
| `identity_hour_bytes`         | 2147483648    | 2 GiB per hour window                                                   |
| `identity_day_bytes`          | 8589934592    | 8 GiB per day window                                                    |
| `monthly_bytes`               | 1099511627776 | Always-enforced server-wide monthly cap, 1 TiB                          |
| `allocation_lifetime_seconds` | 7200          | Absolute lifetime of each key generation, two hours                     |

Identity windows are monotonic fixed windows anchored on use, not sliding windows
or calendar days. The token bucket limits boundary bursts. All of a player's
allocations, rooms and control connections share the same identity record. The
key is the server's stable player UUID associated with the verified Google subject,
not the player's address or a caller-selected installation string. Rotating an
installation cannot mint a new production quota. Players sharing NAT do not share
these identity quotas.

Records are bounded at 4096 and expire after a day without charged traffic. Full
tables reject new identity records rather than evicting somebody else's quota.
Identity windows and counters are process-local and reset on a process restart;
the global monthly reservation ledger does not reset. Preserving per-identity
history across process restarts is not claimed by these counters.

Start by observing normal D1/D2 gameplay, full rosters, late joins, sync and large
mission/level transfers before enabling identity enforcement. A model test checks
that a single-peer 256-MiB transfer below the existing short-window rate fits the
defaults; this is not a substitute for native gameplay/transfer qualification.
Transfers to multiple peers share one sender's allowance. Tune the policy to the
owned VPS allowance and legitimate measured workloads, not an assumed hosting price.

## Always-enforced global breaker

The relay retains its short authenticated input/work limits and additionally caps
estimated egress at 8 MiB per one-second window. Every emitted challenge, bound
acknowledgment and data packet is charged to its authenticated sender, including
framing and a conservative 256-byte UDP/IP/fragmentation allowance. Invalid AEAD,
replay, unknown tokens and requests that produce no response are not charged.
Counters describe traffic authorized for emission, not delivery acknowledgments;
failed socket sends remain conservatively charged.

SQLite durably commits credits before the UDP loop may spend them. At startup and
when credit runs low, the service prepays at most 64 MiB at a time, clamped to the
remaining configured monthly allowance. Replenishment runs off the UDP loop with
only one reservation outstanding. Failed persistence never creates credit. A
crash/restart forfeits unused credits; it cannot refund emitted traffic. Repeated
restarts can therefore exhaust the allowance conservatively. New allocations are
refused when there is insufficient prepaid credit for both players' binding replies.

One bounded SQLite row records the high-water UTC calendar month and committed
bytes. New months reset the reservation count; clock rollback cannot reopen an old
month. The UDP path also checks a monotonic deadline for the current month, so it
cannot keep spending old credits if maintenance stops. Keep system time correct.
Do not delete/replace the database or run instances against independent copies if
they are meant to share a cap. Production rejects temporary/in-memory database
paths and SQLite URI filenames; use a normal persistent filename on durable storage.

The breaker intentionally favors a bounded bandwidth bill over relay availability:
when exhausted, relay traffic stops. Direct paths and the control service remain
available. Identity observation mode does not prevent one account from spending
the global allowance; enabling measured identity limits narrows that exposure.
Provider/OS egress policing remains necessary to cover unusual tunnels/link-layer
billing, unrelated services, configuration mistakes or process compromise. This
application accounting is not a provider billing guarantee.

## Allocation lifetime and nonce ownership

Protocol 8 requires matching server/client artifacts. Each allocation captures its
room, game epoch and both authenticated control-connection owners. Keys expire on
the packet path after two hours by default, even if a peer keeps sending valid
keepalives. The lifetime is configurable from one hour to one day in production;
explicit loopback development fixtures may use ten seconds or more.

Near expiry the server makes one renewal attempt, up to 60 seconds before the
deadline. It stages a fresh routing token, epoch and directional keys while the old
route remains usable. Each peer proves the new UDP binding and acknowledges it over
its current WSS connection. Only after both proofs and acknowledgments does the
server commit. The old generation is then usable for at most three more seconds,
never beyond its original expiry. Missing acknowledgments abort staging after at
most 15 seconds without extending the old lease or launching repeated attempts.

Only one replacement is staged per logical pair, with at most 100 renewals and 200
physical transports for the supported 100 logical-pair capacity. Staging does not
consume another logical allocation slot. Crypto work reservations cover those 200
transports; identity, room, global input and economic budgets remain shared across
generations. Leave, kick, control replacement, game-epoch change and room teardown
revoke both generations. All metadata acknowledgments are scoped to current owners.

Clients keep the same proxy, local game port and real UDP socket during renewal.
They can receive the prepared generation before commit to tolerate opposite control
delivery order, but keep sending on the old generation until committed. Duplicate
offers/commits cannot recreate transports or extend staging/overlap deadlines.
Timed-out/aborted attempts cannot be restarted for the same old epoch. Retired keys
are receive-only during overlap and then discarded from the active decoder set.

Every client transport owns its nonce and replay state for the entire key lifetime.
A bounded process-wide non-evicting fingerprint set rejects reuse of either key in
either direction, even after proxy destruction. Fresh keys are mandatory on rebuild;
restarting the application establishes a fresh authenticated control ownership scope.
The guard permits at most 4096 transport key pairs per process and then fails closed.
Server-role JVM fixtures use an independent codec, not a production ownership bypass.

UDP return-path consent is shorter than the key lifetime. Same-address keepalives
request a fresh challenge/proof after 15 seconds, and DATA requires both endpoints'
last proofs to be less than 30 seconds old. Ordinary BOUND acknowledgments and DATA
do not refresh that consent. A same-route proof may arrive behind newer game DATA
within the replay window; moving to a different route retains the stricter ordering
check. This bounds traffic to a formerly owned/reassigned address independently of
the two-hour allocation lifetime, while retaining the existing bounded challenges.

## Protected metrics

`GET /api/v1/admin/relay-usage` requires the configured admin bearer token and is
available only on the router that carries admin routes. It returns aggregate
denial/forwarding counters, current policy, committed/remaining credit, a 75%-of-cap
alert, per-identity counters/windows and per-allocation sender counters. No payloads,
UDP addresses, routing tokens or AEAD keys are returned.

Credit maintenance also logs the 75% warning once per process/month. Five-minute
stats logs include aggregate egress, identity overages, monthly drops and persistence
failure counts; rejected datagrams do not generate individual accounting logs.

`identity_offset` and `allocation_offset` select deterministic pages of at most 100
records; the response supplies next offsets. Pages are point-in-time diagnostics,
not a transactional snapshot across requests. Allocation counters disappear on
revocation, while identity totals survive allocation/room changes within the process.

Private admin defaults, deployment policing, identity-threshold calibration and
native/provider qualification remain separate open verification gates.
