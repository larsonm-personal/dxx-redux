## Goal

- finish the remaining header/source cleanup survey for direct implementation includes and nearby poor-taste shared-source wiring
- if the survey is clean after this batch, continue into the next small d1/d2 diff-minimization extraction

## Plan

- [completed] inspect the remaining direct `.c` includes and the target wiring around them
- [completed] convert the remaining coop offenders to real shared translation units and remove the local `.c` include wrappers
- [completed] validate the Android coop slice with `android\gradlew.bat :app:externalNativeBuildDebug --console=plain`, then rerun the same build after scoped `android\run-code-quality.ps1 -Fix`
- [completed] rerun the direct-include survey and confirm there are no remaining `#include ... .c` matches in the repo code search

## Outcome

- `shared/coop/coop_save.c` no longer depends on per-target macro templating from `d1/main/coop_save.c` or `d2/main/coop_save.c`; the remaining d1/d2 behavior differences now live behind normal `DXX_BUILD_DESCENT_II` branches in the shared header/source pair
- `shared/coop/coop_warp.c` is now compiled directly instead of being text-included through local wrapper `.c` files
- the d2 coop-save wrapper header now matches the d1 wrapper header, shrinking the local d1/d2 diff in this slice