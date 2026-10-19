# Retroid verification, 2026-10-19

Recorded physical-device results from PR #100 at 7c3b182e9 (Retroid Pocket 4 Pro,
API 33, Mali-G77), following baseline d8fab96c3:

- Mod sounds: D1 1 and D2 11 cases; renderer: D1 11 and D2 23 cases passed
- FOV/demo compatibility: 82 steps per game passed
- Slider navigation and all 9 chooser layouts passed
- Cache reuse: 6 steps passed, including the persisted-result hit
- Mod fuzz: 14 smoke cases; UUD2SP: 5 phases; UUD combinations: 15 phases passed
- Graphics recovery: all 18 faults passed in a separate direct run; the selected
  batch's default rotation skipped this owner
- Obsidian level 7: two cold-cache runs and two warm-cache probes passed with
  unchanged gameplay and assertions; the historical intermittent failure has no
  confirmed root cause and may recur

These were targeted follow-ups across multiple builds, not a repeat of the full
170-owner campaign or overnight fuzz. Uneasy4's documented route-planning timeout
remains; the cache test now uses Obsidian with the same assertion and deadline.
The later CPU-duty storage recovery fix still needs physical-device validation.
Original generated evidence is recoverable from commit 7c3b182e9.

Follow-up: optional CPU-duty writes now tolerate IOException without blocking
completion or marking a failed write as published. Added storage-retry and
robot-loader round-trip regressions; both isolated host checks fail with their
respective old behavior. Twelve focused D1/D2 classic-demo/HAM CTests passed.
Full host builds remain blocked by pre-existing newdemo.c pointer-type errors;
Android Gradle/device validation was unavailable in the follow-up environment.
