# Active ordinary-save runtime continuation

Baseline: cmake 1b26c43cf928f9798513c0652a833c28f50a9e98

## Scope

- Restore the derived D2 countdown seconds before the first physics step
- Preserve an active non-boss robot death sequence saved on its starting frame, using the existing boss serializer's zero-to-minus-one convention
- No save format, ordinary clock rebase, RNG counter reset, legacy gameplay cadence, or guided ownership changes

## Regression plan

Build test_upstream_compat and run each scenario from its own empty writable directory:

    test_upstream_compat --active-save-runtime /path/to/public/d2/assets countdown
    test_upstream_compat --active-save-runtime /path/to/public/d2/assets robot-death

These are explicitly synthetic checkpoints on a normally loaded public D2 level, not gameplay-authored mission routes or UI save-menu coverage. They call the shipping save writer/reader and countdown/death routines. Countdown compares first-step physics and SIM state after warm/zero cached-second restoration; robot coverage includes uninterrupted/resumed dying dispatch through actual explosion, exact SIM state, matching explosion step, per-step rotation within one fixed-point unit for the existing sentinel convention, and elapsed/inactive action controls. Both must reject the pre-fix source.

Keep released-guided save ownership separate: the save does not encode active guidance, so alive projectile scanning cannot distinguish active and released missiles. An explicit identity extension requires a separate format decision.

The public D1 moving-door continuation fixture preserves doors, walls and triggers for all 180 steps, but its player's position/velocity first differ at frame zero, shields at frame three and SIM state at frame 36. This is a separate untriaged physics/continuation discrepancy, not a door failure or complete trajectory pass.

## Focused validation

Both current-source baseline scenarios exit 1 at their targeted assertion. Both fixed scenarios exit 0, including warm/zero-cache countdown controls, identical first-step SIM state and rotation, and active/elapsed/inactive death offsets. The fixed and baseline binaries differ only in their state.c object.

The validation binaries compile current cmake state.c, cntrlcen.c and ai2.c, with the new fixture, reusing older engine object files for unchanged surrounding infrastructure. This establishes the targeted source behavior; it is not a complete current-tree build or Android qualification. The existing fixture translation unit emits pre-existing string-literal warnings; changed production C files compile without warnings. Fixture teardown retains the existing MEM allocation warning. No PowerShell, full aggregate suite, or physical audio check was run.
