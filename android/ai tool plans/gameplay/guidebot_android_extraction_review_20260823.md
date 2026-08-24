# GuideBot Android extraction review

## Goal

Identify new GuideBot routing, policy, diagnostics, and test-support code that
can move out of `d2/` into shared modules under `android/`, while leaving thin
engine integration and classic GuideBot behavior in the upstream tree.

## Plan

- [done] Map GuideBot-related additions in `d2/main/escort.c` and
  `escort.h`, including their engine-global dependencies.
- [done] Classify candidates as pure policy, portable state machine,
  engine adapter, or classic D2 behavior.
- [done] Recommend extraction boundaries, APIs, order, and regression tests.
- [done] Record the review findings and provide concrete source references.

## Results

### Refined placement rule

The primary goal is minimizing changes to original D2 source files, not moving
the largest possible line count under `android/`. New files under `d2/main` do
not disturb the 1996 implementations and are preferable for D2-specific code
that needs engine types or globals. The Android shared folder should contain
only logic that is genuinely portable, already part of the shared route
planner, or likely to be reused by another game integration.

### Current shape

- Relative to upstream, `d2/main/escort.c` has 4,024 added and 271 removed
  lines, and `escort.h` has 146 added lines. This includes replay, thief,
  multiplayer, menu, and restoration work in addition to GuideBot routing, so
  the whole delta should not be moved as one unit.
- The Android route controller begins around `escort.c:196`. It currently owns
  route goals, unexplored targets, dirty/event state, audit scheduling,
  diagnostics, path-stall state, and more than 40 counters and flags.
- The shared route planner, certifier, decision projection, and decision
  adoption policy are already correctly located under
  `android/app/src/main/cpp/shared`.

### Extraction candidates

1. Immediate, low risk: keep the already isolated pure policy files
   `escort_owner_policy.c/.h`, `escort_exit_policy.h`, and
   `escort_goal_policy.h` as new files under `d2/main`. They already achieve
   the original-file-diff goal, depend only on scalar inputs, and have
   standalone tests. Moving them again would add cross-directory coupling
   without providing reuse because D1 has no GuideBot.
2. High value: add `guidebot_route_runtime.c/.h` as new files under `d2/main`.
   They should own a `guidebot_route_runtime` value containing the
   route goal, unexplored target, dirty state, event generations/masks,
   suppression flags, audit cursor/timestamps, replan reason, and efficiency
   counters. Pure operations should include reset, target-mode change, event
   recording/coalescing, event consumption, audit-domain selection, certificate
   event mapping, and adoption accounting.
3. High value: add `guidebot_route_goal.c/.h` under `d2/main` for projecting a
   `level_metadata_route_step` and plan summary into a normalized GuideBot goal.
   Move guidance-mode selection, goal-object selection, targetability checks,
   frontier-goal creation, instruction text, and route-step field copying out
   of `escort.c`. Pass segment/wall bounds and resolved wall location as inputs
   instead of reading D2 globals.
4. High value: replace the many scalar introspection getters in `escort.h` with
   one `guidebot_route_runtime_snapshot` copy API. `game_introspect.cpp` can
   serialize that shared struct directly. Retain a few command and notification
   entry points in `escort.h`; remove counter-by-counter accessors.
5. Medium value: extract secret-goal ranking into a new D2 file such as
   `guidebot_secret_goal.c/.h`. The deterministic comparison and scan of secret
   entrances are pure once the module receives BFS ranks. Keep HUD failure
   messages in `escort.c` or a thin command adapter.
6. Medium value, after the runtime extraction: add new D2 modules for
   `guidebot_diagnostics.c`, `guidebot_commands.c`, and
   `guidebot_multiplayer.c` only where the separation is cohesive. Diagnostics,
   metadata refresh orchestration, route notifications, path endpoint handling,
   respawn/warp commands, and owner packet handling may use D2 types directly.
   They do not need callback-heavy wrappers merely to live under `android/`.
7. Add a new `guidebot_extensions.h` for all added GuideBot APIs used by other
   D2 files. Original `escort.h` should return close to its upstream contents.
   Each notification call site then needs only this new include plus its
   existing one-line hook.

### Proposed D2 module boundary

- `guidebot_route_runtime.c/.h`: route state, target mode, event coalescing,
  audit bookkeeping, route adoption, counters, and the introspection snapshot.
- `guidebot_route_goal.c/.h`: metadata-step to D2 GuideBot-goal projection and
  instruction text.
- `guidebot_commands.c/.h`: find secret, find unexplored, resume default,
  respawn, and warp entry points. Split warp geometry later only if this file
  becomes unwieldy.
- `guidebot_multiplayer.c/.h`: ownership state machine integration, packet
  encode/decode, release, and disconnect transfer. Continue using the existing
  pure owner-policy module.
- `guidebot_diagnostics.c/.h`: path traces, navigation traces, parity capture,
  and test-only controls.
- `guidebot_extensions.h`: small umbrella header for original D2 call sites;
  internal structs remain in the narrower module headers.

The existing `guidebot_route_decision`, `guidebot_route_certifier`, route
planner, route snapshot, and cache code should remain under Android shared.
Those modules already operate on normalized views and are used by both host
metadata analysis and game integration.

### Code that should remain in D2

- Classic `do_escort_frame`, `escort_create_path_to_goal`, object searches,
  buddy messages, and menu behavior should remain in `d2/main/escort.c`.
- Path creation, polishing, and stop/recovery application should remain thin D2
  adapter operations because they directly mutate `object`, `ai_local`,
  `ai_static`, and `Point_segs`.
- Warp/respawn placement should remain D2 code because it is built around FVI,
  segment geometry, collision radii, and object relinking.
- Multiplayer packet encoding/sending and owner application should remain in
  D2 networking integration. Only validation and transition policy belongs in
  shared code.
- Input-demo checkpoint capture/restore should not be folded into the portable
  route runtime. It can move to the D2-specific Android adapter later if
  reducing the upstream diff is worth the additional integration file.

### Recommended sequence

1. Add `guidebot_extensions.h` and move added declarations out of original
   `escort.h` without changing behavior.
2. Extract the goal model/projector and switch `escort.c` to the module-owned
   goal through a small query API.
3. Extract event/coalescing/counter runtime state and expose one snapshot.
4. Move command, multiplayer, and diagnostics implementations into new D2
   files after route state is no longer held in file-local
   `Escort_route_*` globals.
5. Add a host integration test that runs notify, coalesce, replan, identical
   adoption, changed adoption, and certificate failure sequences through the
   runtime API before deleting the old controller code.
6. Reduce `escort.c` to high-level hooks for level initialization, per-frame
   route monitoring, goal selection, path completion, and classic-state
   invalidation. Keep notification hooks in wall/object/trigger files as
   explicit one-line calls rather than introducing a general event bus.

This review changed documentation only. No gameplay source or build output was
modified.
