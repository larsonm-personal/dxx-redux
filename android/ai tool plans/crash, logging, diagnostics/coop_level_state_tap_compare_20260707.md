# Coop level state tap comparison

## Goal
- Re-evaluate the level 7 coop texture issue as a level/render state divergence, not an asset replacement issue
- Use the new coop log to identify which state changed before rendering
- Improve tap texture diagnostics so a single-player tap and coop tap can be compared directly

## Plan
- [completed] Inspect the latest coop log for level texture/state signatures and tap output
- [completed] Trace the existing signature and tap diagnostic code
- [completed] Add targeted tap fields for raw side state, wall/door/clip state, texture/effect names, and relevant signatures
- [completed] Validate with scoped code quality and native builds
- [completed] Tell the user exactly what SP-vs-coop tap pair to gather
