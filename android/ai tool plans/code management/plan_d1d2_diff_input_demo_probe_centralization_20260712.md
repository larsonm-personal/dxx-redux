# Input-demo probe centralization plan

## Goal

Reduce branch-owned diagnostic implementation embedded in upstream-original D1
and D2 engine files by moving probe formatting and state into the existing
branch-added input-demo hook sources without changing simulation behavior,
diagnostic gates, message text, or call ordering.

## Roadmap

1. [ ] Tranche 1, active: move the stateless D2 collision formatters from
   `d2/main/collide.c` and the AI schedule formatter from `d2/main/ai.c` into
   `d2/main/input_demo_hooks.c` with declarations in `input_demo_hooks.h`.
   Implementation and static validation are complete; build and replay
   validation remain deferred while the shared build trees are in use.
2. [ ] Tranche 2: move paired D1/D2 FVI weapon-versus-robot far-miss probes into
   their game-specific input-demo hook sources while preserving divergent gates
   and output formats.
3. [ ] Tranche 3: move D2 controls and wiggle probes, retaining a controls-only
   threat window and changed-state gate.
4. [ ] Tranche 4: move D2 physics drag, motion-detail, and fate probes, retaining
   physics-only threat and changed-state contexts.
5. [ ] Tranche 5: move D2 render probe state and formatters using a read-only
   descriptor for private render-object lists.

Only tranche 1 is authorized in this pass.

## Tranche 1 steps

- [x] Record live source boundaries, exact bodies, callers, and upstream metrics.
- [x] Add unchanged public declarations to `d2/main/input_demo_hooks.h`.
- [x] Move the four collision formatter bodies and two AI schedule helper bodies
  into `d2/main/input_demo_hooks.c`.
- [x] Remove only the original formatter blocks and their branch-added trailing
  separator blanks.
- [x] Confirm all call sites remain in the same order and remove only includes
  made dead by the move.
- [x] Run exact-body, symbol, diff, whitespace, ASCII, and payoff checks.
- [ ] Run D2 build and deterministic replay validation after the campaign owner
  releases the shared build trees.
- [x] Record final metrics and validation limitations.

## Expected payoff

- Collision formatter region: 167 code lines plus one branch-added separator.
- AI schedule formatter region: 82 code lines plus one branch-added separator.
- Expected formatter reduction: 251 inherited-file additions.
- No CMake changes or new engine-file includes are required because both source
  files already include `input_demo_hooks.h`.

## Tranche 1 result

- The collision block moved as 168 exact lines, including its separator.  After
  removing only `static` linkage, the old and new normalized SHA-256 values are
  both `15545f6a278cba67e12abd435fa56838ef1594fdc080f993428d9551ef8ec175`.
- The AI schedule block moved as 83 exact lines, including its separator.  After
  removing only `static` linkage, the old and new normalized SHA-256 values are
  both `2aa9556dd0d50b77cc9becf02549219ed7cb448aa928c22ffd91e37619c0051c`.
- `collide.c` is otherwise identical to its pre-tranche version.  `ai.c` is
  otherwise identical except that its now-unused, branch-added
  `input_demo_debug_logging.h` include was removed.
- The original-file upstream addition counts changed from 631 to 463 in
  `collide.c` and from 682 to 598 in `ai.c`.  This removes 252 inherited-file
  additions: 251 formatter/separator lines and one dead include.
- The destination needed `collide.h` for `PERSISTENT_DEBRIS` and `textures.h`
  for `TmapInfo`.  Both are confined to the branch-added hook source.  There are
  no CMake changes and no new includes in either upstream-original source.
- Static validation found one declaration and one definition for each moved
  public function, with every existing engine call site unchanged.  Targeted
  `git diff --check` passed.  An independent read-only audit found no behavior,
  type-compatibility, or dependency discrepancy.
- Git reports its existing CRLF-to-LF normalization warning for `ai.c` and
  `input_demo_hooks.h`; the scoped diff itself is whitespace-clean.
- Build and deterministic replay checks were intentionally not run because the
  shared build trees are reserved by the concurrent font-validation pass.
