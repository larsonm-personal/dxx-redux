# Small and zero ordinary XL force damage

Pinned XL ApplyForceDamage has no Redux FORCE_DAMAGE_THRESHOLD floor. Bypass
that early return only for XL singleplayer. Preserve the existing classic,
D1-in-D2 and multiplayer cutoff, cheat override, protection, force calculation,
collision response and sound dispatch.

Extend existing native no-damage collision controls with equal physical player
and clutter masses. Keep the counterpart persistent to avoid unrelated clutter
response/immunity behavior. Player speed eight fixed-point units produces
16384 damage at difficulty one, currently below the Redux floor.

Cover raw force damage 0, 1, 16384, 21844, 21845 and 21846, both directions and
argument orders, trainee/normal difficulty, protected contacts, controlled
multiplayer dispatch and the existing stock-recovery classic stage. Observe
accepted zero dispatch through the real player-damage handler's shield mirror:
the object starts with a different shield sentinel than Players[].shields.
Do not add a test hook or infer zero dispatch from unchanged real shields alone.

Predict the existing collision sound-gate draw and randomized timestamp exactly;
protected contacts consume no such draw. Verify collision motion and the
persistent counterpart remain unchanged by the damage-threshold correction.

The matrix distinguishes an unconditional cutoff, global removal and an
inclusive-boundary variant. Actual production mutation executions remain
unperformed under the no-new-local-build constraint. Use existing exact-head
hosted native owners, and keep their pending status explicit. No new harness,
checkout, merge, complete force-parity or campaign-completion claim.
