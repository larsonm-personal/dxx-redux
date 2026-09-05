# Castaway level 2 reactor access investigation

- [x] Reproduce current post-red-key failure and compare recent results
- [x] Identify the blocking wall and its actual trigger dependencies
- [x] Separate recent physical-navigation changes from planner or simulation-state errors
- [x] Document cause and proposed repair, scoped to the four development missions

## Confirmed failure

- Current run completes all three keys and objectives through fly-through trigger 19 at frame 5311 (89 seconds)
- The prepared route correctly requires switch 24 next, then fly-through 16, switches 21 and 20, reactor and exit
- Trigger 19 closes wall 207 (switch 24's surface) and the return wall 166 among others
- The simulation records trigger 19 and immediately calls prepare_next_goal in the same frame
- The selector rejects step 15 (switch 24) as INVALID_TARGET: wall 207 still has type 4 (WALL_OPEN), although its shootable texture is valid, trigger flags are zero, and 16 compiled firing candidates exist
- wall_start_decloak sets a closing state immediately but does not change WALL_OPEN to WALL_CLOAKED until wall_frame_process advances the transition
- guidebot_step_usable rejects WALL_OPEN; the selector's restorer fallback does not rescue this in-flight transition
- The failed selection clears the live route; the sim treats the missing goal as terminal instead of waiting for the pending wall transition
- Diagnostic log: android/temp/castaway2_wall_diagnostic/logs/castaway_redux.json_0_2_rupture.rl2_run_1.log

## History and scope

- All 11 checked-in revisions of Castaway 2 simulation data examined already timed out after switch 10, before the latest Obsidian 7 repair
- The latest path repair allows four additional objectives and exposes this later same-frame transition failure; it did not remove the prepared switch sequence
- This does not prove older manually played builds had no other regression or that fixing this handoff will complete the rest of the level
- No simulation JSON was regenerated into the working tree during this investigation; existing user changes are preserved
- Temporary native diagnostics were removed after reproducing the rejection

## Proposed implementation

1. Represent a required switch surface that is actively being restored as pending, rather than an invalid/unreachable target, using engine wall transition state
2. Preserve the intended next objective and allow normal engine frames to finish the relevant transition before selecting/activating it; do not globally delay every objective or open walls artificially
3. Handle this pending result in both live GuideBot guidance and the sim, with bounded no-progress handling for a transition that never completes
4. Preserve failure details instead of collapsing selector errors into an empty no-goal diagnostic
5. Add a regression for an open switch surface restored by a one-shot close-wall trigger and a full Castaway 2 integration run
6. Verify Castaway Redux, Obsidian, Counterstrike, and First Strike D1-in-D2 together, comparing objective progress as well as final statuses

## Implementation progress

- [x] Add engine-backed restoration pending handling to the shared selector
- [x] Retry pending objective selection in the simulation with a bounded wait
- [x] Run focused integration and the four core campaigns, comparing progress
- [x] Build both engines, run tests and scoped quality, refresh affected result

## Verified implementation

- The engine scan view exposes whether a wall is actively decloaking (restoring); native D1 returns false, while D1-in-D2 uses D2 wall behavior
- The shared compiled selector returns PENDING for the required switch while its surface restores, using the existing live-route pending/retry machinery
- The sim suspends navigation and door firing while selection is pending, advances ordinary engine frames, and retries; a transition that never settles times out after 5 seconds
- No fixed delay was added to normal objectives, and no wall collision or trigger activation was bypassed
- Castaway 2 completes all 20 objectives in 6237 frames, exit at 104 rounded seconds
- New test_castaway_level2_restored_switch_route.ps1 passed as written with identical full results across two runs; new selector test checks pending-to-valid transition and existing removed-surface rejection remains covered
- Four-core-mission check covered 88 levels: only Castaway 2 changed status or completed-objective sequence, from failed with 14 objectives to ok with 20
- Obsidian 7 repeatability test still passes at 7845 frames
- Windows D1 and D2 builds, all 45 D2 CTest tests, and scoped code quality passed
- Refreshed only Castaway 2 and its mission summary in the simulation corpus, preserving other pre-existing working-tree edits
- General enrichment of terminal selector error reporting remains a separate diagnostic improvement, not needed for this targeted repair
