# Direct UDP multiplayer security scan plan, 2026-10-16

## Objective

Audit the multiplayer attack surface reachable when hosting or joining a normal D1/D2 UDP game over LAN or a manually entered Internet IP. Identify malformed or unauthorized traffic that can corrupt memory, crash either peer, mutate game state, exhaust resources, disclose sensitive data, or bypass protocol state.

Matchmaking WebSocket services, the matchmaking backend, relay-server implementation, account authentication, and lobby-service business logic are out of scope. A packet type remains in scope if the normal game UDP socket will parse it, even when its intended sender is a currently unshipped service.

## Method

1. Inventory listeners, packet identifiers, dispatchers, size gates, sender/token gates, and protocol states
2. Trace each top-level packet from receive through all reads and state mutation in D1 and D2
3. Trace encapsulated MDATA gameplay commands through `multi.c`, `multibot.c`, and shared co-op handlers
4. Review reconnect, join/sync, observer, proxy/direct-connect, save-transfer, and host-migration helpers
5. Review both `net_udp.c` files completely in approximately 500-line blocks, recording every block in the coverage ledger
6. Review directly reached supporting modules in approximately 500-line blocks where their size warrants it
7. Rank findings by remote reachability, memory-safety impact, authorization impact, and denial-of-service cost
8. Propose concrete fixes and focused malformed-packet tests; do not change behavior during the scan unless separately requested

## Verification requirements

- Every line of D1 and D2 `net_udp.c` is represented in the block ledger
- Every accepted top-level UPID packet type is mapped to size, sender, token, state, and parser checks
- Every MDATA command is mapped to its declared length and parser-side validation
- D1/D2 divergences are called out explicitly
- Existing defenses are recorded so fixes do not regress them
- The report distinguishes confirmed defects from defense-in-depth recommendations

## Progress

- [x] Scope and threat model fixed to LAN/direct-IP UDP sessions
- [x] Top-level ingress and packet-contract inventory
- [x] D1 `net_udp.c` block review
- [x] D2 `net_udp.c` block review
- [x] D1/D2 `multi.c` and `multibot.c` payload review
- [x] Shared Android network helper review
- [x] Finding validation and D1/D2 parity check
- [x] Final prioritized fix ledger and recommended implementation order

## Deliverable

See `android/security_scan_direct_udp_20261016.md`.
