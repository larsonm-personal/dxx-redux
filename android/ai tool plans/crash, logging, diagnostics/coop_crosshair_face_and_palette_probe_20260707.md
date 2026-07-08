# Coop crosshair face and palette probe

## Goal
- Make the tap probe identify a real mine face even when render-side tracking misses
- Add palette diagnostics for the tapped/visible face so palette-vs-texture mistakes are visible in one log

## Plan
- [completed] Inspect FVI ray helpers and palette globals
- [completed] Add a geometry ray fallback to the Android tap probe
- [completed] Add palette hashes and representative index-to-RGBA logging
- [completed] Validate with scoped quality checks and Android native build
