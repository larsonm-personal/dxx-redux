# Initial LAN startup diagnosis

Verified October 20: origin/cmake is 66e5e27da9850a9f142ac5c13d9e65432d48f4e3.
PR #103 is OPEN, not merged, at 01401425aff549eefd8b5a9e9fd8a8458d01a9b8.
Investigation uses a separate worktree/branch based on that PR head, preserving it.
Existing measured engines are 2eb278f (before) and 04b6cf3 (after).

Plan:
1. Recover original startup diagnostics for successful runs 39375337680 and
   39380229137 before running new emulator work.
2. Build device-local critical-path timelines. Separate launcher preflight,
   Activity/library startup, native initialization, mission/player selection,
   level loading, network synchronization and briefing protocol creation.
3. Trace observed gaps into current source, distinguishing normal launcher
   behavior from test-only automation. Never sum overlapping peer stages.
4. Add focused logging/experiments only where existing evidence cannot identify
   the work or retry causing a delay. Use solo comparison if it isolates a gap.
5. Report quantified causes and remaining uncertainty; no speculative redesign,
   merge, history rewrite or push to cmake.
