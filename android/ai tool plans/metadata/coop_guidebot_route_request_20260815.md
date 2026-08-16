# Co-op guidebot route request

Date: 2026-08-15
Status: completed

## Correction

- Co-op guidebot is a supported multiplayer feature
- The current Android level-load guard suppresses route metadata requests for every multiplayer mode
- Host and joining clients therefore cannot prepare the co-op guidebot route cache during level startup

## Plan

- [x] Trace co-op guidebot ownership, route consumption, and metadata readiness synchronization
- [x] Define the correct request policy for single-player, co-op host, co-op joiner, and competitive multiplayer
- [x] Restore route metadata requests for supported co-op roles in D2, including D1 missions running in D2
- [x] Add policy and high-level multiplayer regression coverage
- [x] Run native tests, Android tests, builds, and scoped quality checks

## Constraints

- Do not enable guidebot route work in competitive multiplayer
- Do not make joining block indefinitely on route generation
- Preserve deterministic co-op guidebot behavior across peers

## Verification

- D2 Windows build passed
- Escort owner policy native test passed
- Android debug APK build passed
- Scoped code quality passed
- Two-emulator co-op launch reached both game processes, but the existing LAN synchronization failure prevented level entry: the host remained at one player while proxy packets and keepalives continued
