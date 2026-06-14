# D1 Trine 2 Texture Precedence Investigation

- [x] Trace D1 texture conversion for animated overlays, including frame `#0` handling.
- [x] Compare D1 and D2 eclip metadata for `misc066` and the destroyed texture orientation.
- [x] Identify why many D1 textures still resolve through D2 texture/effect metadata.
- [x] Implement the smallest safe correction.
- [x] Run focused validation.

## Notes

- User observed Trine 2 in D2 cycling through `misc066#0`, while D1-style frames appear to be `#1` through `#6`.
- User also observed the blown texture mirrored left to right and many other incorrect textures, suggesting D2 texture/eclip metadata is still taking precedence over D1 metadata.
- D1 eclip 24 uses 9 `misc066` frames and destroys to D1 texture 350, while D2 eclip 24 uses 7 frames and different D2 artwork/destination.
- The old D1 replacement path only replaced textures considered unique to D1. That intentionally left many same-name but visually different textures using D2 bitmap data.
- The fix now replaces all D1 texture-mapped bitmaps while emulating D1 and reloads D1 effect frame bitmaps into the D2 frame slots used by the matching effect.
- Local D1 PIG size estimate for mapped textures plus effect frames is about 2 MB; the D1 replacement buffer was increased to 5 MB.
- Scoped code quality passed. Android native build `:app:externalNativeBuildDebug` passed.

## Follow-Up

- User playtest found the broad all-mapped D1 replacement path makes most textures worse, with many green/palette-wrong surfaces.
- The useful part appears to be animation consistency, not blanket replacement of same-name base textures.
- Follow-up root cause is that D1 levels were still falling back to D2's `groupa.256` palette. Broad D1 bitmap replacement must be paired with D1's `palette.256`.
- D1's `palette.256` lives in `descent.hog`; D1 levels now select it when visible through PhysFS, and the D2 palette loader skips the nonexistent inferred `palette.pig`.
- Re-ran scoped code quality and Android native build after the D1 palette change.
- User playtest now reports wall/level textures look good, but robot and powerup textures or palettes look wrong.
- Likely cause: wall/effect bitmaps are replaced with D1 art, while powerups and robots still use D2 object/vclip/polymodel bitmap tables under the D1 palette.
- Current pass applies D1 vclip metadata and frame bitmaps while D1 emulation is active. This should fix powerups and other sprite/vclip art first; robot polymodel/object bitmap replacement remains a larger follow-up if robot art is still wrong.
- User saw no visible change from the vclip pass.
- Local registered D1 `descent.pig` stores `Num_vclips` as 0 even though the fixed 70-entry vclip table follows and D1 reads all 70 entries. Treat 0 as `D1_VCLIP_MAXNUM` for D1 compiled properties.
