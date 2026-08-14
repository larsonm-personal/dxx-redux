# GQR-0044 reconnect verification admission plan

## Goal

Close `GQF-0057` by bounding unauthenticated reconnect signature verification before JNI and Java cryptography while preserving the route proof and versioned transcript behavior from GQR-0042 and GQR-0043

## Scope

- [x] Inspect both reconnect verification call paths, shared policy ownership, lifecycle resets, and focused tests
- [x] Add a fixed-memory per-source and global verification admission policy in branch-added shared code
- [x] Apply admission before request and proof verification with minimal symmetric D1/D2 plumbing
- [x] Add load, fairness, expiry, source-port rotation, and rejected-before-crypto tests
- [x] Run scoped formatting, focused reconnect tests, and feasible D1/D2 Windows and Android native builds
- [x] Record exact limits, validation results, and any blockers

## Constraints

- Keep policy and state in `android/app/src/main/cpp/shared/net`
- Key per-source limits by network address rather than ephemeral source port
- Keep all storage fixed-size and reset it with reconnect authentication lifecycle changes
- Do not modify the canonical quality ledger or the root campaign plan
- Preserve unrelated concurrent work

## Completion record

Implemented one shared fixed-memory verification gate with 32 source records, four attempts per IPv4 source per one-second window, and sixteen attempts globally per one-second window. The source key deliberately excludes the UDP port. Request and challenge-proof paths consume the same budget immediately before their JNI verification callback. Cheap transcript, identity, route, replay-counter, and packet checks remain ahead of admission. Authentication reset, host generation rotation, and received generation changes clear the gate

Focused coverage proves source load rejection, source-port rotation resistance, another source's remaining admission after a noisy source reaches its cap, global rejection under address rotation, window expiry, lifecycle reset, and that rejected attempts do not invoke the verification callback

Validation:

- Scoped `run-code-quality.ps1 -Fix`: passed
- Direct MSVC C11 `/W4 /WX` build and focused executable: passed
- D1 and D2 `test_net_udp_reconnect_auth` CTests: passed
- `run-windows-build.ps1 -Target both`: passed
- Android `:app:externalNativeBuildDebug`: passed for arm64-v8a, armeabi-v7a, and x86_64
- `git diff --check` on the touched product and test files: passed

No blockers remain
