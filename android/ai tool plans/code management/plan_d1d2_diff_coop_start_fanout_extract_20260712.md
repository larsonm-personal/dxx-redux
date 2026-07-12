# Coop start fanout extraction plan

## Goal

Move the identical missing-coop-start fanout search out of both upstream-
original `gameseq.c` files into one shared engine source.

## Rules

- Preserve offset order, scale order, quick-distance test, segment lookup, and
  exact source-position fallback.
- Keep assignment counters, logging, and player-object mutation in gameseq.
- Build both games on desktop and Android, then run the too-few-start mission
  scenario when its fixture is available.

## Result

- Each `gameseq.c` lost 64 additions; after CMake wiring, the inherited-file
  reduction is 126 additions.
- All three Android ABI targets compile and link the shared fanout search.
