# Guide-Bot pre-convergence save-load diagnosis

## Goal

Determine whether a save recorded before the current-state Guide-Bot cutover should converge after loading, or whether restarting the level is legitimately required because the saved world state itself is incomplete or inconsistent.

## Plan

- [x] Locate and inspect the supplied debug log for restore, world-state, objective, and route-decision evidence.
- [x] Correlate the published objective and certificate with the restored trigger, wall, and object snapshot.
- [x] Trace save serialization for the authoritative fields used by the current-state certifier.
- [x] Decide whether this is a planner defect, an invalid old world snapshot, or an unsupported save-format boundary.
- [x] Report whether restarting the level is required without adding compatibility behavior.
- [x] Make destructive shoot-switch completion depend on the source switch's current texture state.
- [x] Add focused coverage proving a destroyed switch stays complete after a linked door recloses.
- [x] Run scoped code quality, native tests, and the required host build.

## Constraints

- Diagnose only unless a current-version planner defect is proven and the user asks for implementation.
- Do not preserve obsolete Guide-Bot completion journals or add Android save migrations.

## Findings

- The restore completes at 14:12:26.284. Two milliseconds later the new build creates a fresh route to segment 172, the prepared activation segment for Counterstrike level 20 trigger 18, objective 2. The restored Guide-Bot path itself is not being reused.
- Normal restore explicitly clears transient Escort goals and paths, then republishes route guidance from the restored world before consumers can reuse pre-restore guidance.
- Trigger 18 is an `open_wall` action linked to walls 13, 3, and 1. The current-state certifier currently requires it while any linked wall is not passable. A linked door can therefore reclose and incorrectly resurrect the objective even though its one-shot source switch remains destroyed.
- The earlier Level 20 fix recorded trigger completion when the trigger fired. The current-state cutover removed that journal without replacing it with an equivalent durable-state rule, so it reintroduced the known regression rather than fixing malformed saved state.
- Shooting a destructible switch permanently replaces the source side's `tmap_num2`. The save format serializes and restores that texture, and the scan view already exposes `wall_is_shootable_trigger`, which becomes false after the texture is destroyed. That is authoritative current world state, not transition history or compatibility repair.
- The log does not prove that the old save is inconsistent. Restarting is not a dependable workaround because the same current build can reproduce the regression after a linked door recloses. The planner should treat a shoot-switch objective as complete when its source wall is no longer a shootable trigger, while retaining linked-effect checks for repeatable or non-destructive trigger activations.
- Implemented that source-switch rule as a constant-time check before linked-wall evaluation. A regression test covers both the destroyed shoot-switch case and the unchanged pass-through-trigger behavior.
- Scoped quality checks passed. The Guide-Bot certifier and level metadata scan tests passed, and complete D1 and D2 Windows host builds succeeded.
