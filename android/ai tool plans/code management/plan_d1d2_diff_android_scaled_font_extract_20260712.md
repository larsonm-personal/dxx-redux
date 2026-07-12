# D1/D2 Android scaled linear-font extraction plan

## Goal

Move the byte-identical Android scaled linear-font renderer out of both
inherited `2d/font.c` files into one shared translation unit without changing
glyph rasterization, text controls, spacing, or desktop call selection.

## Baseline

- `d1/2d/font.c`: `+214/-1` against `upstream/main`.
- `d2/2d/font.c`: `+214/-1` against `upstream/main`.
- The target `#ifdef ANDROID` block is exactly identical: lines 855-981,
  127 physical lines and 3,968 normalized characters in each game.
- Both call sites are already identical and can retain the current function
  names after including a shared declaration header.

## Boundary

- Add `shared/android_font_scale.c/.h` to both 2D static libraries.
- Keep `get_char_width` and `get_centered_x` in `font.c`; they already have
  external linkage and the shared implementation can use narrow declarations.
- Reproduce only the private arithmetic macros needed by this renderer:
  `BITS_TO_BYTES`, `INFONT`, and `FONTSCALE_X/Y`.
- Continue using the public canvas/font globals and bitmap operations directly;
  no callback layer or private engine structure is required.
- Leave the cross-platform generic linear renderer and OpenGL renderer local.

## Work plan

- [x] Confirm exact equality, block boundaries, call sites, and live metrics.
- [x] Add the shared implementation and declaration header.
- [x] Replace each 127-line block with one shared include.
- [x] Add the source to both 2D CMake targets.
- [x] Run scoped static checks and record exact inherited-file reduction.
- [x] Build and link both games on all Android ABIs.
- [x] Run readable-tiny text, menu scale, control/help readability, and a D1
  menu path so masked and unmasked scaled text both execute.
- [x] Update the catalog and live aggregate.

## Guardrails

- Preserve the 0.99/1.01 activation tolerance exactly.
- Preserve packed-font bit order, underline baseline rows `+2/+3`, foreground
  and background colors, and transparent masked background.
- Preserve proportional and fixed-width data offsets, kerning-derived spacing,
  centered-X calculation, newline height, color controls, line spacing, and
  underline control consumption.
- Preserve allocation/free ordering and the distinction between `gr_bitmapm`
  and `gr_bitmap`.
- Do not route the pre-existing generic linear fallback through the Android
  helper in this tranche.

## Expected payoff

Removing 127 lines and adding one include in each inherited `font.c`, plus one
CMake source entry per game, models an exact reduction of 250 inherited-file
additions. The shared implementation/header should remain substantially smaller
than the 254 duplicated lines.

## Result so far

- Each `font.c` moved from `+214/-1` to `+87/-1`, removing 127 inherited
  additions per game.
- One 2D CMake source entry was added per game. Including those entries, the
  exact inherited-file reduction is 252 additions.
- The shared implementation and header add 159 lines, for a net repository
  source reduction of 93 lines after replacement includes and CMake wiring.
- The shared source produced a distinct object for D1 and D2 on arm64-v8a,
  armeabi-v7a, and x86_64, and all six final game links passed.
- Static stale-body and diff checks pass. D1 exercised the scaled menu path in
  the 53-step launch/automap flow. D2 passed the 24-step readable-tiny help,
  50-step controls-readability, and 16-step menu-scale scripts.
