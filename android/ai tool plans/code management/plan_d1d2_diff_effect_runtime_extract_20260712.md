# Deterministic effect runtime extraction plan

## Goal

Relocate the byte-identical effect timeline, bitmap application, and elapsed-
time reset kernel from both upstream-original `effects.c` files.

## Boundary and validation

- Keep effect tables, normal frame stepping, one-shot behavior, and file parsing
  in each game.
- Compile one shared source directly into both main targets.
- Preserve the exact elapsed-time boundary arithmetic and critical-clip rules.
- Build both desktop games and every Android ABI, then exercise replay/save
  restore paths that reconstruct animated effects.

## Result

- Both inherited files moved from 87 to 2 additions.  After CMake wiring, 168
  inherited additions were removed.
- All three Android ABI targets compile and link the shared effect runtime.
