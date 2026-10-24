# Matchmaking and protocol security review

## Scope

- Matchmaking WebSocket authentication, lobby, chat, signaling, and persistence paths
- UDP STUN-compatible discovery and relay allocation/data paths
- Client handling of server and peer-controlled messages and UDP payloads
- Trust placed in Google Play Games Services (GPGS), server-issued identity, and lobby membership
- Resource-abuse controls appropriate for a small VPS with inexpensive bandwidth

## Threat model

- Unauthenticated Internet clients can open connections and send arbitrary bytes
- Authenticated but malicious or modified clients can create accounts and join games
- A malicious lobby peer can send crafted protocol messages and gameplay packets
- Attackers may try to reflect/amplify traffic toward unrelated third parties or use the relay as a general-purpose tunnel
- Attackers may deliberately consume CPU, memory, sockets, database capacity, logs, or bandwidth

## Review chunks

1. Inventory public listeners, protocol state machines, limits, authentication boundaries, and deployment assumptions
2. Audit server DoS and non-gaming-abuse resistance, including WebSocket, database, STUN, relay, logging, and shutdown behavior
3. Audit relay/STUN reflection, destination authorization, allocation ownership, spoofing, amplification, and traffic/accounting controls
4. Audit client trust boundaries and parser/resource safety for matchmaking, peer signaling, relayed UDP, level transfer, and engine packet ingress
5. Evaluate high-level architecture and GPGS dependency, then produce prioritized findings and a staged hardening plan

## Deliverables

- Evidence-backed findings with file/line references, severity, exploit sketch, and recommended mitigation
- Explicit distinction between confirmed issues, defense-in-depth gaps, and items requiring deployment/runtime validation
- A concise architecture recommendation that preserves ordinary Descent traffic and level transfer while preventing general-purpose relay use

## Working constraints

- Do not modify implementation while the branch is under active work by another worker
- Keep review artifacts under `android/ai tool plans/security/`
- Re-check the worktree before writing and avoid overwriting concurrent edits
