# Thief Bot Multiplayer Movement Investigation

## Goal

Identify why the D2 thief robot visibly snaps or warps between positions in coop while other robots move more naturally, without introducing simulation desync.

## Plan

- [in progress] Trace thief AI movement, multiplayer ownership, send scheduling, and remote position application
- [pending] Compare the thief's movement and packet cadence with ordinary robots and the guidebot
- [pending] Confirm the root cause with focused diagnostics or an automated repro where practical
- [pending] Implement the smallest safe fix if the cause is sufficiently established
- [pending] Run scoped formatting, build, and relevant tests, then record results here

## Constraints

- D1 has no thief robot, so source changes are expected to be D2-only unless shared infrastructure is involved
- Preserve authoritative robot simulation and eventual convergence; visual smoothing must not create divergent gameplay state
- Preserve unrelated worktree changes
