# GQR-0042 Reconnect Route-Proof Plan

## Goal

Require a route-bound challenge proof before every known reconnect identity can
change player ownership, address, token, packet time, connected/readiness, or
sync state. Preserve the accepted request counter through host migration and
keep the paired D1/D2 hooks minimal.

## Scope

- Shared Android reconnect authentication and pending-challenge state
- Paired D1/D2 UDP request dispatch and authenticated-player replication hooks
- Focused native tests for direct, proxy, NAT rebound, replay, and migration
- Windows paired builds and Android ABI builds where available

The adjacent transcript-versioning and pre-verification resource-budget items
remain owned by GQR-0043 and GQR-0044.

## Work

- [x] Recover GQF-0055 evidence and identify every pre-proof mutation path
- [x] Define a shared staged-request result that commits the replay counter only
      after route proof succeeds
- [x] Preserve the accepted counter in full-info and sync replication
- [x] Route starting, waiting, and playing known identities through one proof
      completion path in paired D1/D2 code
- [x] Add direct, proxy, NAT rebound, stale/duplicate request, proof replay,
      lost challenge, and migrated-counter regression coverage
- [x] Run focused tests, scoped quality checks, paired Windows builds, and
      Android ABI builds as feasible
- [x] Record final changed-path, original-file diff, test, and limitation evidence

## Acceptance

- No known reconnect request mutates player state before a matching live route
  proof
- A rejected, stale, lost, wrong-route, or replayed proof leaves all player
  state and the accepted replay counter unchanged
- A valid proof commits its request counter exactly once before the caller
  performs its network-state transition
- Host-migration replication retains the last accepted counter
- D1 and D2 hooks remain behaviorally paired

## Completion evidence

- The shared route-proof gate binds request counter, normalized IP/port route,
  direct/proxy mode, challenge, expiry, and network-state context
- Signature validation no longer advances the accepted counter; successful
  challenge verification rechecks and commits it exactly once
- Full-info/sync identity records now carry the accepted 64-bit counter
- The paired `net_udp.c` changes are symmetric at 54 additions and 28 deletions
  per title against the item start, with the policy and serialization logic kept
  in branch-added shared files
- Registered D1 and D2 `test_net_udp_reconnect_auth` CTests passed, including
  starting/direct, waiting/NAT-rebound, playing/proxy, wrong route/mode/context,
  expiry, lost challenge, stale request, proof replay, and migration round trip
- `run-windows-build.ps1 -Target both` passed after the final source changes
- `:app:externalNativeBuildDebug` passed arm64-v8a, armeabi-v7a, and x86_64
  before an unrelated concurrent edit left `shared/secretarea.c` with an
  unterminated Android conditional; all six current reconnect objects remain
  successfully built and up to date in each ABI graph, while the repeat aggregate
  build stops on that unrelated source
- Scoped code quality and `git diff --check` passed for the owned files
