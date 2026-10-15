# Preserve ordinary-save wiggle phase

- Reproduce real public D1 moving-door first-step divergence, inspect identical pre-step physics and compare no-wiggle and phase-only controls: complete
- Preserve bounded effective phase in the save runtime tail while keeping ordinary game timers zero-based and replay/rewind absolute epochs unchanged: implemented
- Reserve native D1 version21 / Android D1 version22; final unpublished D2 45 / imported D1 46 includes both guided ownership and phase tail: implemented
- Validate real continuation, repeated save/load, no-wiggle controls, arithmetic and malformed records; rerun guided fixture against the combined format: passed native D1/D2
- Review scoped diff and limitations before publication: save core reviewed; required replay metadata and D1 checkpoint translation followups pending

## Causal evidence
Baseline GT930384 restores0 with identical initial player pose and physics. First x velocity -306338/-306361 and y80603/56415; shields diverge frame3. Disabling PF_WIGGLE or changing only its sine phase to the saved phase eliminates player pos/orient/physics/shields and SIM/FX state differences for180 frames. Full clock reassignment breaks FX immediately and is not the fix.

## Semantics
Store effective low16 phase, not just the current table index: low8 bits determine future carry. Restore offset=(saved_phase-low16(restoredGameTime)) mod65536. Saving includes an existing offset, so repeated zero-based loads work. Fresh-level resets clear the offset. Old native formats clear the offset. Replay/rewind already restore absolute epochs; computing the relative offset avoids double phase addition. D2 secret restores consume/validate but reset offset0, deliberately preserving their previous clock-following phase reset; secret-transition continuity is not changed.

The runtime word is validated0..65535 before applying, in the save's byte order. The legacy GameTime field remains0; changing its meaning would confuse old readers and replay checkpoint rebasing. Final D2 format45/46 appends phase after the guided ownership tail. The intermediate guided-only generation was never published.

## Scope
Native host fixtures exercise real public assets, ordinary writer/reader and GameProcessFrame. The replay/rewind test verifies absolute-epoch phase arithmetic; it does not replace an Android rewind UI run. No PowerShell, SDL interception, codec changes, aspect-zero workaround, or unrelated death/countdown edits.

## Local core validation and publication blockers
Native D1 door and D2 flight:180-frame state/RNG parity, second-generation30-frame parity, separate-process30-frame parity and no-wiggle controls pass. All9 combined guided cases pass. Added fresh co-op travel and fresh secret-game offset resets after lifecycle review. Android runtime path not executed. Production compile logs have no warnings; old fixture string-literal and teardown allocation diagnostics remain.

This intermediate commit is NOT publication-ready: new_level replay metadata must carry effective phase, and the D1-in-D2 checkpoint translator must accept native21 and apply phase plus the pending Fusion-active flag. The final extension retains versions21/22 and45/46; no intermediate format is published.
