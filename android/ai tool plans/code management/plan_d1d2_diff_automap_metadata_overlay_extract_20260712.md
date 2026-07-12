# D1/D2 automap metadata overlay extraction plan

## Goal

Move the mirrored secret-area and numbered-objective label implementation out
of both inherited `main/automap.c` files, while leaving each game's private
automap structure, edge list, and D2 marker rendering local.

## Baseline

- `d1/main/automap.c`: `+319/-20` against `upstream/main`.
- `d2/main/automap.c`: `+505/-23` against `upstream/main`.
- The target implementation is 146 physical lines in each game. Apart from
  the pre-existing D2-style function name, parameter name, and whitespace in
  the 16-line text primitive, the secret/objective and segment-policy bodies
  are behaviorally identical.
- Six label-color definitions and three or four local prototypes per game are
  also owned exclusively by the target implementation.

## Boundary

- Add `shared/automap_metadata_overlay.c/.h` unconditionally to both main
  targets.
- Expose one draw entry point that returns the four candidate/projected counts
  through integer pointers. This avoids exposing the private `automap` type.
- Move the segment visibility predicates directly; they already depend only on
  public game globals and `Automap_visited`.
- Keep the reveal-edge colors, private active-map pointer, edge culling,
  introspection copy, and all D2 marker behavior in the engine files.

## Work plan

- [x] Record exact live blocks, call sites, definitions, and baseline metrics.
- [x] Add the shared header and implementation using the existing game headers.
- [x] Replace both pairs of draw calls with one four-counter shared call.
- [x] Remove the two 146-line local bodies, label-only colors, and duplicate
  prototypes.
- [x] Wire the source into both unconditional main source lists.
- [x] Run scoped static checks and compare final inherited-file metrics.
- [x] Build and link D1 and D2 for all Android ABIs.
- [x] Run the dual-game objective-overlay automap test and D2 secret-reveal
  automap test; add D1 secret-label coverage only if the existing mission data
  produces drawable labels.
- [x] Record the result in the campaign catalog and refresh the live queue.

## Guardrails

- Preserve label numbering: route-start steps do not consume a number, while
  invalid label positions do consume one.
- Preserve key colors and blue/red/gold key-index mapping exactly.
- Preserve the side-center fallback to `secret->label_pos` and integer fixed
  point averaging.
- Preserve behind/projection rejection and centered text coordinates exactly.
- Reset all four telemetry counts on every draw entry, including disabled
  overlays and missing metadata.
- Preserve segment bounds checks, found/reveal policy, and segment-limit order.
- Do not move or rename D2 marker arrays, marker drawing, or private automap
  state.

## Expected payoff

The exact result depends on final call formatting, but the modeled inherited
reduction is about 300 additions: 292 implementation lines plus label-only
definitions and prototypes, less one include, one CMake source entry, and a
compact call per game.

## Result so far

- D1 moved from `+319/-20` to `+161/-21`, a reduction of 158 additions.
- D2 moved from `+505/-23` to `+348/-23`, a reduction of 157 additions.
- One unconditional CMake source entry was added per game, so the exact net
  inherited-file reduction is 313 additions.
- The two inherited source diffs contain 324 fewer physical added lines; the
  shared source and header add 185 lines, for a net repository source reduction
  of 129 lines after the eight replacement lines and two CMake entries are
  included.
- D1 and D2 compiled and linked for arm64-v8a, armeabi-v7a, and x86_64. The
  shared source emitted no warnings.
- `git diff --check` passes with only the repository's line-ending notices.
- The 53-step launch/automap script passed in both games, including objective
  candidate/projected counts before and after toggling. The 29-step D2 secret
  reveal script also passed with nonzero drawable labels, segments, and visible
  secret edges.
