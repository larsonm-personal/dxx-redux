# Obsidian 9 original follower investigation

## Implementation acceptance

- Diagnose the first reversal with facing/velocity/goal dot products and actual
  collision state before selecting a steering change
- Apply a generic companion correction in the shared live/simulation follower,
  without mission IDs, changed radii, forced doors, or waypoint teleportation
- Preserve ordinary forward steering; explicitly test adverse velocity after
  contact, where facing and motion disagree
- Exercise Obsidian 9 at 1.0, 1.2, 1.4, and 1.6 speed with repeat determinism
- Compare all 88 core levels with the pre-change baseline at 1.0 and 1.6
- Distinguish clearing this doorway from any later independent level failures

1. Compare simulation velocity handling with the existing legacy path follower
2. Test original movement speed with unchanged objectives and collision radius
3. Trace the first off-route segment and isolate speed/steering differences
4. Keep only evidence-backed changes and validate all four core missions

## Initial controlled comparison (before implementation)

The sole behavior change tested was removing speed_up_actor's post-follower
8/5 velocity multiplier. Objectives, full collision radius, steering, path
acceptance, flares, and route planner were unchanged. This is a comparison of
unboosted current follower behavior, not a claim to reproduce the 1996 binary

- Boosted baseline: no keys, 4266 frames, no-progress timeout
- Unboosted: blue key at 15.38 seconds; later stops at the physical frontier for
  fly-through trigger 0 (navigation segment 2, semantic segment 16), 2538 frames
- Segment-transition diagnostics locate the departure:
  boosted frames 567/595/608/611/629/641 visit 216/214/213/212/218/211 while the
  current path index remains 32 (waypoint segment 220)
- Unboosted frames 750/777/787 visit 216/220/666 as intended
- The log establishes departure backwards from the door approach, not merely
  an overshoot into the side pocket. It does not yet identify the exact
  collision/velocity update that starts the reversal
- The initial checked-in upstream code (8a5daf0c) uses the same frame-scaled
  steering blend and companion turn-time rule. The deterministic integer helper
  is an equivalent formulation, not evidence of a replacement steering model

## Core acceptance check

All 88 core levels were tested unboosted, without updating regression files.
Five previously ok levels become timeout: First Strike 25 and 26,
Counterstrike 23, Obsidian 5 and 14. Castaway 1 loses physical completion too
(route_mismatch -> timeout). Therefore a blanket slowdown was rejected

Scratch evidence:
- android/temp/obs9_original_speed
- android/temp/obs9_normal_speed_core
- android/temp/obs9_normal_trace
- android/temp/obs9_fast_trace

Those initial experimental movement changes and temporary tracing were removed
before beginning the implementation below

## Follow-up experiment plan

1. Trace frames 567-595 at the physics/follower boundary: actual velocity,
   facing, selected waypoint, collision side, and door animation state
2. Determine whether the reversal is a collision response, steering blend,
   or premature waypoint acceptance. Do not infer the specific cause solely
   from the segment trace
3. Evaluate acceleration consistently with the original steering/turning
   assumptions instead of multiplying only the final velocity. Preserve full
   collision radius and the same movement behavior in headed/headless runs
4. Test the same first doorway traversal and the full core suite before keeping
   a behavior change. Keep the separate later trigger-0 frontier failure visible

## Implemented result

The steering trace isolated an opposed-velocity problem: at raw game time
635699, velocity z is -7613649; at 636791 it is +7594860. Facing still points
toward the waypoint (dot 64580), while actual motion points almost exactly
away (dot -64304). The legacy facing-based opposite-direction check therefore
does not fire, and normalizing/blending sustains nearly full-speed retreat

The retained correction is limited to active companion route following:

1. Remember the previous steering command, actor signature, waypoint, and
   simulation time. Only recognize a reversal with the same waypoint and a
   recent command (within a quarter-second); other robots do not alter this state
2. Require both motion reversal and the facing/motion disagreement, using the
   legacy 15/16 opposite-vector threshold. Sweep toward the waypoint at the
   larger player/companion radius and require an engine-openable door obstruction
3. Brake while the full-radius approach remains blocked, then let the existing
   follower resume toward the same waypoint. The pending contact expires after
   two simulated seconds and is invalidated by actor/goal/time changes
4. Do not modify wall state, keys, radius, position, route objectives, or global
   frame timing. Door/flaring/physics behavior remains authoritative

Immediate velocity redirection regressed other levels. Path cursor rewinding,
rebuilding paths, and broadening portal recovery were investigated and rejected.
None of those extra navigation mechanisms is retained. Existing path generation
and portal recovery are unchanged

## Test interface and coverage

- Native headed and headless runners accept -route-confirm-speed-percent 100..200
- PowerShell helper exposes -TestSpeedPercent (default remains 160). Nondefault
  speed is prohibited with WriteRegression or the Android Headed mode
- Nondefault raw results record test_speed_percent; canonical JSON gains no
  speed-field churn. The seed, fixed physics rate, and collision sizes stay fixed
- android/tests/test_obsidian_level9_motion_tolerance.ps1 builds by default,
  runs 100/120/140/160 percent twice each, checks exact repeat equality and blue
  key acquisition within 40 seconds, and keeps later level failures visible
- Full core comparisons at 100 and 160 percent cover 88 levels each. Relative
  to their respective pre-change baselines, no previously ok levels are lost.
  First Strike 26 improves from timeout to ok at 100 percent
- Final core evidence: android/temp/obs9_minimal_brake100 and 160, repeated in
  android/temp/obs9_verified100 and 160. Only the 160 percent run writes corpus
  files. Obsidian 9 reaches blue but still times out at trigger 0's later frontier
- All 176 cross-run raw-result comparisons are byte-identical (88 per speed).
  The focused four-speed test also passed with identical pairs. Windows D2
  headed/headless builds, 47 native CTests, and scoped code-quality checks pass

## Remaining scope

This fixes the observed pre-key door reversal, not every navigation failure or
Obsidian 9's later frontier. Synthetic spawn-position perturbations and a live
Android device playthrough are not yet covered. Future tolerance work should
keep both speed baselines and distinguish physical progress from full completion
