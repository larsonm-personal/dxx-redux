# UDP security compatibility repairs

## Objective

Restore legitimate LAN and launcher multiplayer behavior broken by the direct
UDP hardening while retaining the new bounds, framing, sender-authentication,
and anti-amplification checks.

## Compatibility invariants

- Legacy automatic color values in sequence and game-info records remain valid.
- Reliable duplicates are acknowledged and discarded before state-dependent
  command authorization is evaluated.
- Structurally valid robot and pickup retries may become obsolete before they
  arrive; handlers safely drop stale state without blocking the reliable queue.
- Every peer may publish its powerup-cap contribution.
- The host may replay player marker and flag state during late-join catch-up.
- RetroProtocol normal gameplay remains direct peer-to-peer for three or more
  players; reliable and non-Retro gameplay remain host-relayed.
- A host-authorized proxy envelope preserves the claimed peer identity for P2P
  heartbeat packets, while gameplay batches remain authenticated through the
  host relay.
- Proxy transport keepalives do not enter the native game protocol parser.
- Malformed sizes, unknown opcodes, spoofed identities, unauthorized relays,
  and invalid indices remain rejected.

## Work order

1. Repair D1/D2 sequence parsing and normalize full game-info colors after
   decoding.
2. Move reliable duplicate/ACK handling ahead of state-dependent semantic
   validation in both engines.
3. Match multiplayer command authority rules to the actual D1/D2 producers.
4. Preserve peer identity for authorized proxied P2P ping/pong traffic.
5. Consume the proxy's one-byte NAT keepalive at the transport boundary.
6. Extend focused D1/D2 validation tests for the restored producer paths.
7. Reconcile the independent network-compatibility audit and repair confirmed
   compatibility failures without weakening packet framing or sender identity.
8. Run targeted tests, both engine builds, scoped code-quality checks, and the
   two-emulator public-shareware LAN suite;
   inspect the final diff for unrelated changes.

## Verification ledger

- [x] Sequence packets accept legacy automatic color sentinels.
- [x] Full game-info accepts and safely normalizes automatic colors.
- [x] Reliable duplicates receive ACKs before semantic validation; obsolete
  pickup and robot commands retain structural/auth checks and safely drop in
  their handlers.
- [x] Peer powerup-cap updates pass authenticated validation.
- [x] D2 host marker/flag catch-up passes; unrelated peers cannot impersonate.
- [x] Three-player RetroProtocol normal traffic accepts the claimed direct peer
  route while reliable/non-Retro traffic still requires the host route.
- [x] Authorized proxied P2P ping/pong and normal gameplay use the claimed peer
  route; reliable gameplay uses the host relay route.
- [x] Direct and relay keepalives are consumed before engine delivery.
- [x] Unknown/truncated/spoofed packets remain rejected.
- [x] Focused D1 and D2 `test_udp_security_validation` tests pass.
- [x] Full Windows D1/D2 builds and Android `assembleDebug` pass.
- [x] Proxy keepalive integration test passes.
- [x] Public-shareware two-emulator LAN sessions pass for D1/D1, D2/D2 with
  reversed roles, and D2/D1; both peers reach level 1 with two connections.

## Audit follow-up ledger

These are test/diagnostic gaps identified by the independent audit, not
confirmed reasons to weaken authentication in this repair:

- Add a three-player runtime campaign covering direct RetroProtocol normal
  gameplay, host-relayed reliable traffic, and proxy heartbeat identity.
- Add delayed/duplicated reliable-batch campaigns across world changes, rapid
  slot reuse, and host migration before changing the current world/slot fences.
- Exercise host-authorized direct-route reattempt after endpoint rebinding;
  retain exact source-route matching unless runtime evidence proves the current
  reattempt path cannot recover.
- Run the public-shareware LAN suite from a fresh pilot in continuous
  integration and ensure the relevant workflows run on pushes, so a local pass
  cannot be bypassed by workflow trigger coverage.
