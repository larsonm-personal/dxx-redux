# Obsidian level 3 blastable wall simulation fix

## Phase 1: Diagnose the interaction

- [x] Trace route-confirmation wall classification, flare aiming, and close-range recovery
- [x] Reproduce or inspect Obsidian level 3 at the affected blastable wall
- [x] Determine whether the failure is aiming, projectile collision, damage, or wall eligibility

## Phase 2: Implement the narrow fix

- [x] Preserve real flare interaction as the primary mechanism
- [x] Assign only blastable-wall simulation flares player ownership and decisive damage
- [x] Avoid bypassing keyed, trigger-only, locked, or ordinary solid walls

## Phase 3: Verify

- [x] Add a deterministic Obsidian level 3 high-level regression test
- [x] Run scoped code-quality checks
- [x] Build Windows D2 and Android debug, then run all D2 CTests
- [x] Record results and remaining limitations

## Results

- Wall 104 received correctly aimed flares, but robot ownership prevented D2's
  blastable-wall damage path from accepting the shot
- Route-confirmation blastable-wall flares now retain real projectile flight and
  collision while using player ownership and decisive simulation-only damage
- If the projectile callback still does not occur, close recovery requires the
  same wall to have been targeted by a real flare, its travel deadline to have
  elapsed, actor adjacency, current visibility, and `WALL_BLASTABLE` type
- Obsidian level 3 advances through wall 104 and completes trigger 6 at frame
  461 in two byte-identical runs
- A follow-up route-navigation fix now carries the level through trigger 7 and
  the next blastable wall; the remaining stall is on the approach to trigger 8
- Counterstrike levels 1, 2, 6, 12, and 17 remain `ok` with byte-for-byte
  matching frame counts against checked-in results
- Windows D2 build, Android debug build, all 45 D2 CTests, focused integration
  test, and scoped code-quality checks passed
