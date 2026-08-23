# Redbook track start freeze follow-up

## Goal

Remove the remaining roughly half-second gameplay freeze reported when an
Android Redbook track changes while playing Obsidian from a Macplay image.

## Plan

- [ ] Add phase timing around every synchronous track-switch operation and
  reproduce repeated transitions with maintained automation
- [ ] Identify the blocking ownership boundary using device diagnostics and
  code inspection
- [ ] Implement a race-safe nonblocking transition without changing desktop
  playback behavior
- [ ] Extend integration coverage for rapid live track replacement and timing
- [ ] Run scoped quality, Android builds and tests, Windows parity builds, and
  record the results

## Constraints

- Keep shared Android code as the implementation owner for D1 and D2
- Preserve playback ordering, callbacks, pause, lifecycle, and error behavior
- Do not edit the protected outstanding bug list
- Preserve unrelated working-tree changes
