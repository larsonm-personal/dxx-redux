# Co-op route precompute stall

Date: 2026-08-15
Status: complete

## Symptoms

- A joining phone remained on a black screen during a co-op join
- Advanced settings remained at `discovering levels` without progress or logs
- Another phone stopped near 2 percent after active-mission priority switches

## Plan

- [x] Trace discovery, scheduling, cancellation, and game-launch ownership
- [x] Reproduce the no-total and stalled-analysis states from control flow and the supplied log
- [x] Prevent background metadata shutdown from blocking game launch indefinitely
- [x] Make discovery and stalled work observable in persistent monitor state and setup introspection
- [x] Add regression coverage for discovery, progress, and launch handoff monitor state
- [x] Run focused unit tests, Android APK/native build, and emulator launch verification

## Verification

- Focused route metadata monitor and progress tests pass
- Debug APK and all configured Android native ABIs build successfully
- Emulator discovery reported 35 levels and advanced persistently
- Emulator game handoff completed in 5 ms while precompute was active
- Full unit suite: 828 of 829 pass; the unrelated RAR import test cannot initialize the host sevenzipjbinding native library and fails identically in isolation

## Constraints

- Co-op joining must not wait for metadata discovery or route analysis
- Background analysis must resume safely after returning to the launcher
- Progress and logs must remain useful across restarts
