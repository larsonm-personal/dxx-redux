# Plan: Add recording-side logging for next demo desync capture (2026-05-02)

## Goal
- Capture enough data during demo recording to prove ordering at the first awareness/path RNG fork

## Steps
- [x] Identify existing replay-only log gates and recording-state helpers
- [x] Enable awareness source logs for recording mode (not replay-only)
- [x] Add concise frame-level order markers around weapon wall impact and path generation for tracked window
- [x] Build host d2 target and verify no errors
- [x] Summarize what to collect from the next recorded demo

## Outcome
- Awareness entry/probe/post-gate/result logs now emit in both record and replay modes and include `mode=record|replay`
- Collision impact probe lines for player-wall, weapon-wall, player-robot, and weapon-robot now emit in both record and replay modes
- This enables direct record-side ordering checks between weapon wall awareness and path RNG consumers in the next demo capture
