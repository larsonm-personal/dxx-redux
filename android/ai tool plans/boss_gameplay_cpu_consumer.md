# Linked boss gameplay consumer check

Add an asset-free Linux CTest executable that links the D2 damage function and
the XL object-trigger, boss-spawn, robot-generator and health functions. Use
small authored actor, trigger and generator records. Verify fractional health
thresholds, attribution, operation during cooldown, consumed versus permanent
values, owner independence, and state snapshot/restore with a different clock.

Link the real robot usability and teleport actor checks. Fixture functions
provide bounded object storage, simple authored geometry and record-only effect
endpoints. They must not replace damage acceptance, trigger firing, generator
selection, cooldown, state validation or timer updates. Unexpected effects fail.

This check does not load a mission, render, exercise the normal save codec, or
qualify full-engine object allocation, placement and morph behavior. Keep the
existing Windows public contracts unchanged. Run the selective test target and
its CTest entry before publication; report broader engine checks separately.
