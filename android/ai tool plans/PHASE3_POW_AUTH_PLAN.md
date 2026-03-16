# Phase 3: Proof-of-Work Keypair Authentication Plan

## Status: COMPLETE

All items implemented and tested. 48 tests passing (5 pow unit + 43 integration).

## Goal

Allow players without Google Play Games Services to authenticate using
a device-local Ed25519 keypair, gated by a proof-of-work challenge on
first registration to prevent bot/sybil attacks.

## Auth Flows

### GPGS auth (existing, unchanged)
```
Client -> AUTHENTICATE {play_games_token, callsign, ...}
Server -> AUTH_OK (or AuthFail)
```

### Keypair auth - known key (fast path)
```
Client -> AUTHENTICATE {auth_method: "keypair", public_key, auth_timestamp,
                         auth_signature, callsign, ...}
Server verifies signature + timestamp freshness
Server -> AUTH_OK
```

### Keypair auth - new key (requires PoW)
```
Client -> AUTHENTICATE {auth_method: "keypair", public_key, auth_timestamp,
                         auth_signature, callsign, ...}
Server doesn't recognize key
Server -> POW_CHALLENGE {challenge, difficulty}
Client computes SHA256(challenge || solution) with `difficulty` leading zero bits
Client -> POW_SOLUTION {challenge, solution}
Server verifies PoW + original signature
Server -> AUTH_OK
```

## Protocol Changes

### ClientMessage additions
- Modify `Authenticate`: add optional `auth_method`, `public_key`,
  `auth_timestamp`, `auth_signature` fields (all `#[serde(default)]`)
- New variant: `PowSolution { challenge: String, solution: String }`

### ServerMessage additions
- New variant: `PowChallenge { challenge: String, difficulty: u8 }`

## Implementation Files

### New: `pow.rs`
- `generate_challenge() -> String` -- 32 random hex bytes
- `verify_pow(challenge, solution, difficulty) -> bool` -- SHA256 leading zeros
- `verify_keypair_signature(pubkey_hex, message, signature_hex) -> bool`
  -- ed25519-dalek verification

### Modified: `db.rs`
- New table: `keypair_identities` (public_key_hash, player_id, created_at)
- New function: `find_player_by_keypair(pubkey_hash) -> Option<Uuid>`
- New function: `register_keypair_player(pubkey_hash, callsign) -> Uuid`

### Modified: `protocol.rs`
- Authenticate variant: add optional keypair fields
- New PowChallenge server message
- New PowSolution client message

### Modified: `ws_handler.rs`
- PlayerSession: add `auth_method: String` field ("gpgs", "keypair", "dev")
- AUTHENTICATE handler: branch on auth_method
- Pending PoW state: per-connection tracking of challenge + deferred auth info
- PowSolution handler: verify + complete registration + send AUTH_OK

### Modified: `config.rs`
- `pow_difficulty: u8` field (default 20, ~200ms on phone)

## Dependencies
- `ed25519-dalek = "=2.1.1"` (ed25519 signature verification)
- `sha2 = "=0.10.8"` (SHA-256 for PoW hashing)
- `rand = "=0.8.5"` (challenge generation)

## Difficulty Tuning
- 20 bits = ~1M SHA256 iterations = ~200ms on phone, ~20ms on fast CPU
- Server verification: 1 SHA256 call = <1us
- Configurable via POW_DIFFICULTY env var

## Integration Tests
- test_keypair_auth_new_key: full PoW challenge/response flow
- test_keypair_auth_known_key: returning keypair player, no PoW needed
- test_keypair_auth_bad_signature: rejected with invalid signature
- test_keypair_auth_bad_pow: rejected with wrong PoW solution
- test_keypair_not_gpgs_verified: keypair player can't join verified-only lobby
