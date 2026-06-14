# D1 Trine 2 Blown Texture Mismatch Investigation

- [x] Locate D1/D2 destroyed texture mapping path for D1 custom levels in D2.
- [x] Inspect Trine 2 level data around segment 178 side 2 and identify source texture ids.
- [x] Compare D1 and D2 runtime translation for the blown panel texture.
- [x] Implement the smallest safe correction if the cause is clear.
- [x] Add or run focused validation.

## Findings

- Trine 2 level 1 segment 178 side 2 uses D1 base texture 153 with D1 overlay texture 349.
- D1 overlay texture 349 is `misc066`; D1 eclip 24 destroys it to D1 tmap 350, `blown04`.
- D2 converts D1 tmap 349 to D2 tmap 372, also `misc066#0`, but D2 eclip 24 destroys to D2 tmap 371, `blown05`.
- The fix keeps the live texture mapping intact and temporarily applies D1 eclip destination tmaps while D2 is running a D1 compiled level.

## Validation

- Scoped code quality passed for `d2/main/gamemine.c` and this plan file.
- Android native build passed for `:app:externalNativeBuildDebug`.
- Windows host build was attempted. D1 built successfully; D2 configure stopped before compiling this change because local SDL_mixer headers/libraries were not found.
