# Live difficulty runtime extraction plan

## Goal

Relocate the byte-identical live-difficulty runtime from both upstream-original
`gamecntl.c` files into one unconditional shared engine source.

## Boundary

- Move clamping, history, eligibility, persistence, replay staging, coop host
  authority and broadcast, and HUD notification together.
- Keep menu construction and direct-command iteration in each game.
- Compile the same source into both main targets without callbacks.

## Validation

- Confirm the two 108-line bodies are identical before movement.
- Build Windows D1/D2 and all Android ABIs.
- Run live-difficulty, input-demo replay, and coop host-authority coverage.

## Result

- The exact 108-line runtime moved out of each game; after two CMake entries,
  214 inherited additions were removed.
- All three Android ABI targets compile and link the shared runtime.
