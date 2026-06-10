# Energy Shield Hold Plan

Goal: Make the unbound-actions touch overlay `Energy->Shield` action behave like a held button while the user keeps pressing it, so Descent 2 can continue transferring energy above 100 into shields.

1. [done] Trace the unbound action menu touch path and how button bindings are injected into the input mixer.
2. [done] Patch the smallest Android touch overlay path so `BTN_ENERGY_SHIELD` sends a held press and matching release when invoked from unbound actions.
3. [done] Add or update focused tests around the held-action policy.
4. [done] Run scoped formatting and relevant Android unit tests.
