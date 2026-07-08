# Coop door26 still wrong after palette invalidation

## Goal
- Explain why door26 still renders with wrong colors in coop after the previous palette invalidation
- Patch the actual timing/cache path that still leaves stale palette-baked GL textures alive

## Plan
- [done] Read project instructions and newest attached log
- [done] Compare palette load, texture invalidation, texture upload, and tap readback order
- [done] Patch the smallest remaining cache/palette timing issue
- [done] Validate with scoped quality checks and Android native build

## Notes
- Build 17330 did include the previous invalidation, but the log shows it fired while `last_palette=default.256`
- `door26#0` still had a pre-game GL handle at tap time, while the current palette was already WATER
- The fix moves the D2 Android GL invalidation to the final level palette load in `StartNewLevelSub`
