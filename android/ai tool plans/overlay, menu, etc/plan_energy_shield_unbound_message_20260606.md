[x] Trace the D2 energy-to-shield converter control path and Android unbound-actions activation
[x] Find the existing in-game message for missing converter ownership if one exists
[x] Patch the narrowest shared path so unbound-action activation shows the same HUD feedback
[x] Run scoped formatting/build validation

# Energy Shield Unbound Action Message Plan

## Goal

When the Android unbound-actions menu invokes `Energy->Shield` without the player owning the converter, show an in-game HUD message instead of failing silently.

## Notes

- This is D2-only; D1 has no energy-to-shield converter action.
- Prefer reusing the existing game-side message if present.
- Keep the action path consistent with normal bound controls where possible, so touch/controller menu activation does not need separate gameplay rules.
