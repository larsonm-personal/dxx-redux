# D1 level 6, 9, and 15 demo replay follow-up

Goal: verify the new D1 input-demo recordings reproduce in plain D1 and D1-in-D2, then fix any deterministic replay failures exposed by those fixtures.

Plan:

1. [done] Refresh repo instructions, confirm the new demo files exist, and create this plan.
2. [done] Identify the existing replay commands and run the three demos first with the D1 engine. Level 6 and level 15 reproduce with RNG trace comparison; level 9 fails in plain D1.
3. [pending] Run the same demos through D1-in-D2 and capture the first desync diagnostics. Deferred until the plain-D1 level 9 replay is understood, since D1-in-D2 results would be stacked on a known plain-D1 failure.
4. [in progress] Fix any clear engine/save/compat nondeterminism found, keeping input-demo special casing out unless it is only diagnostics. Added shared state-trace fields for the first changed robot AI static entry, plus an alternate hash that removes that first changed entry. A danger-laser behavior change was tested and rejected because it broke the passing level 6 demo, so no engine behavior change is currently kept.
5. [in progress] Rerun the failing demo set and update this plan with the final results. Current lead: level 9 first diverges in robot AI static/animation state at frame 468 while AI-local hashes, player pose, weapon state, and robot object pose still match. Object 175 then physically diverges at frame 471. This points toward robot animation/current-subobject-angle state advancing differently, not pathfinding, RNG, or danger-laser geometry.
