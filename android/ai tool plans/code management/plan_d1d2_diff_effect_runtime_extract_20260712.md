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
