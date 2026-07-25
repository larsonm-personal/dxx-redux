# D2 Level 10 Key Carrier Routing Investigation

## Goal

Determine why the new Guide-Bot route labels the blue and red key objectives as robot-carried in Descent 2 level 10, while the yellow key receives the ordinary key-finding label.

## Plan

1. [complete] Trace key-carrier discovery, objective construction, and label selection in the shared route planner
2. [complete] Inspect level 10 object, containment, and trigger metadata for all three keys
3. [complete] Compare the observed data against native route-planner tests and identify the root cause
4. [complete] Record findings and recommended fix or test coverage without changing gameplay behavior

## Findings

- The raw level contains three robot-carried keys using the same ordinary object containment fields:
  - red key: object 1, robot, segment 477
  - blue key: object 3, robot, segment 410
  - gold/yellow key: object 220, robot, segment 135
- Shared target discovery supports multiple contained-key objects and preserves the selected carrier identity. Label construction correctly emits `destroy_key_carrier` for any selected contained key whose owning object is a robot.
- Counterstrike level 10 is already the sole allowed partial base-campaign route. The semantic route reaches the blue carrier, then reports `gold key unreachable`.
- The yellow carrier's region is not intrinsically unreachable. The failure is caused by the simulated state after the blue objective: the planner places the player at the blue carrier's initial segment, inside an asymmetrically locked cage, and cannot route back out.
- After blue, the partial semantic plan has no targetable pending step. `escort_set_goal_object()` therefore falls through to the classic Guide-Bot goal selection. The classic path recognizes keys inside objects but only reports `Finding YELLOW KEY`; it has no carrier-specific instruction.
- After yellow is acquired, the failed prerequisite is gone and semantic planning can resume with the contained red carrier, which explains why red receives the new instruction.
- This is not caused by having multiple key carriers, a target-capacity issue, missing yellow metadata, or an unmodeled yellow-area trigger. It is a state-projection bug for a moving key carrier, followed by the silent semantic-to-classic fallback.

## Recommendation

Fix the semantic planner's post-objective state for moving contained-key targets. It must not assume that destroying a robot leaves the player at the robot's authored initial segment when that segment is beyond a non-reversible edge. A carrier-aware approach/continuation anchor outside the cage is safer than globally treating locked automatic doors as permanently open. A narrow legacy-label fix would hide the wording symptom but leave the route incomplete.

Add a focused Counterstrike level 10 regression that produces the complete sequence: blue carrier, yellow carrier, red carrier, triggers 18 and 25, reactor, and exit. Also add a small native topology test with an unlocked door on the approach side, a locked automatic reverse side, a moving key carrier beyond it, and a later objective outside.

## Detailed Reachability Study

1. [complete] Reconstruct the yellow carrier region and all boundary walls from the loaded level
2. [complete] Trace every trigger, control-center link, and special wall behavior that could open or bypass those boundaries
3. [complete] Compare shared-planner edge rules with classic player, robot, and Guide-Bot traversal behavior
4. [complete] Determine whether the failure is an authored exception, missing route model, bad wall interpretation, or dynamic carrier-movement issue
5. [complete] Record evidence, likely fix scope, and focused regression requirements

## Detailed Results

- The blue carrier starts in segment 410. Its only flyable connection is to segment 319 through paired door walls 184 and 185.
- From segment 319, the door is ordinary and unlocked. From segment 410, the reverse wall is flagged locked and automatic. There is no trigger attached to this door.
- The blue carrier uses `AIB_RUN_FROM`. Opening the door from the outside opens the connected wall too, allowing the carrier to flee. The player's real post-kill location is therefore not constrained to segment 410.
- `move_to_target()` nevertheless sets progression state to the target's authored segment after every objective. For the carrier, this means the planner fabricates a state in segment 410 after awarding the blue key.
- The edge model then correctly rejects the locked reverse side, so subsequent search cannot leave the blue cage and reports the gold key unreachable.
- A counterfactual route beginning in segment 410 with the blue key fails immediately in the same way. Beginning in segment 319 with the blue key succeeds and finds the full remaining route: yellow carrier in segment 135, red carrier in segment 477, triggers 18 and 25, reactor in segment 379, and exit in segment 393.
- The yellow component itself contains 96 currently flyable segments and its blue-key boundary is modeled normally. This rules out a yellow-carrier parser failure.

## Root Cause

The planner conflates a moving objective's initial location with the player's location after completing that objective. This works for stationary pickups, but not for an `AIB_RUN_FROM` carrier behind a one-way/asymmetric door. The incomplete semantic plan then causes classic Guide-Bot fallback, which knows that the yellow key is contained but has only the generic `Finding YELLOW KEY` message.

## Implementation

1. [complete] Define a carrier-only continuation anchor from the successfully planned approach path
2. [complete] Preserve that anchor after appending a robot-carrier key objective
3. [complete] Add a native asymmetric-door carrier regression
4. [complete] Run scoped formatting, native tests, and D1/D2 host builds
5. [complete] Regenerate Counterstrike metadata and confirm the complete level 10 route

## Implementation Results

- Route snapshots now preserve whether a robot uses the `AIB_RUN_FROM` behavior
- After appending a fleeing carrier objective, the planner scans its completed approach path for the first edge that is blocked in reverse and retains the segment immediately before that edge as the continuation anchor
- The carrier step still terminates at and labels the robot's authored position, so only subsequent planning uses the safe anchor
- A native three-segment regression proves that a fleeing blue-key carrier behind an asymmetrically locked door permits the later blue-key objective, while an otherwise identical non-fleeing carrier remains unreachable
- D1 and D2 route snapshot and level metadata scan tests pass
- Counterstrike level 10 regenerates with status `ok` and the sequence blue carrier, gold carrier, red carrier, trigger 18, trigger 25, reactor, and exit
- Full D1 and D2 host builds compile and link the affected game, headless metadata, and route test targets, but the aggregate build still fails on the existing `test_coop_player_session` include-path issue (`SDL_types.h` in D1 and `physfs.h` in D2)
