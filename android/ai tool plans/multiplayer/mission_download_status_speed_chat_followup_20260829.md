# Mission download status, speed, verification, and host chat follow-up

## Goal

Remove transient missing-mission status during an active LAN download, improve LAN transfer speed, add useful remaining-time and verification progress, and restore chat in the hosted LAN lobby.

## Plan

- [x] Trace the three-second join refresh and keep active transfer state authoritative
- [x] Replace the conservative transfer throttle with a higher LAN-friendly bound
- [x] Extend transfer status with elapsed-rate data and remaining-time formatting
- [x] Hash the final archive with determinate verification progress
- [x] Reuse the client chat panel in the hosted lobby layout
- [x] Add focused status/progress tests and run Android build and code quality

## Constraints

- Keep transfer and hashing work off the UI and lobby receive threads
- Preserve verified-byte resume behavior and whole-file SHA-256 enforcement
- Keep progress fields bounded and safe for LAN lobby packets
- Do not change D1 or D2 engine sources

## Validation

- Scoped code quality passed for the changed Kotlin, tests, and plan
- Focused mission refresh, identity, protocol, transfer, and LAN packet tests passed
- Android `:app:assembleDebug` passed for all configured ABIs

Status: complete
