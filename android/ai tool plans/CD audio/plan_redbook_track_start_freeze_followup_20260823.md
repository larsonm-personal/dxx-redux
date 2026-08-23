# Redbook track start freeze follow-up

## Goal

Remove the remaining roughly half-second gameplay freeze reported when an
Android Redbook track changes while playing Obsidian from a Macplay image.

## Plan

- [x] Add phase timing around every synchronous track-switch operation and
  reproduce repeated transitions with maintained automation
- [x] Identify the blocking ownership boundary using device diagnostics and
  code inspection
- [x] Implement a race-safe nonblocking transition without changing desktop
  playback behavior
- [x] Extend integration coverage for rapid live track replacement and timing
- [x] Run scoped quality, Android builds and tests, Windows parity builds, and
  record the results

## Constraints

- Keep shared Android code as the implementation owner for D1 and D2
- Preserve playback ordering, callbacks, pause, lifecycle, and error behavior
- Do not edit the protected outstanding bug list
- Preserve unrelated working-tree changes

## Findings

- Track replacement synchronously joined the prior producer on the game thread
- A single SAF-backed seek or read could therefore extend the join to the
  reported half-second even though cancellation was checked between sectors
- The producer now persists until Redbook shutdown and consumes a latest-only
  request slot, discarding output from superseded generations
- Diagnostics distinguish game-thread request time, producer apply delay, first
  buffer delay, and discarded stale render chunks

## Verification

- Scoped code quality passed for the five changed plan, native, introspection,
  and automation files
- Android debug APK built successfully for arm64-v8a, armeabi-v7a, and x86_64
- `test_saf_redbook.ps1` passed all 69 game-side steps after installing the
  rebuilt APK, including four immediate live track replacements
- The final emulator run reported zero producer stop wait, a 4 ms latest-request
  apply delay, and five safely discarded stale chunks during the replacement
  stress
- Windows D1, D2, and headless parity builds completed successfully
