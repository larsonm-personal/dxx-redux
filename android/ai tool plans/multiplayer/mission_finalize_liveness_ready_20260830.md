# Mission finalization liveness and ready recovery

## Goal

Ensure a client remains present while downloading and finalizing a host mission, transitions out of verification reliably, and can use the Ready action once the imported mission matches.

## Plan

- [x] Trace transfer completion, import, mission resolution, status delivery, and Ready-button gating
- [x] Make transfer requests and status reports refresh host-side liveness during long local work
- [x] Expose explicit post-hash finalization progress and ensure failures leave verification state
- [x] Confirm local mission match enables Ready and is resent before the Ready packet
- [x] Add focused regressions for liveness and terminal transfer status
- [x] Run scoped quality, focused tests, and Android build

## Constraints

- Do not weaken host mission identity checks or transfer authorization
- Preserve automatic pruning for clients that have genuinely disconnected
- Keep import and hashing off lobby receive and UI threads

## Validation

- Scoped code quality passed for the changed Kotlin, tests, and plan
- Focused lobby status, liveness, mission protocol, transfer, and LAN packet tests passed
- Android `:app:assembleDebug` passed for all configured ABIs

Status: complete
