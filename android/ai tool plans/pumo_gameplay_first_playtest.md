# Pumo gameplay-first playtest work list

## Priority and acceptance

User direction, November 1, 2026: prioritize the gameplay needed to start playing
and giving feedback; leave graphics-only work until the end. This supersedes the
older handoff's requirement to finish all camera/shield rendering qualification
before delivering a playtest build. It does not authorize changing original
mission content or silently dropping gameplay behavior.

The first deliverable is an installable Android build with an explicitly stated,
verified original-Pumo starting slice, ordinary controls and combat, working
save/load and restart, and a short known-limitations/feedback note. Deliver it
before completing the entire campaign. Keep expanding that same original mission
as later areas become ready. Do not call a level playable merely because the
inspector accepts it, or claim human traversal from controlled exit dispatch.

Use roughly one-hour coherent stacked PR increments in larsonm-personal/dxx-redux.
Continue child branches while ancestor CI runs, fix ancestors and propagate fixes.
Do not merge PRs. This plan does not start another timed development goal.

## Evidence baseline

The retained November 1 inspection parses all 14 original levels. Eleven have
no reported admission blockers; pmlev1 has 4, pmlev2 has 9 and pmlev3 has 18.
These counts include overlapping camera/object-trigger findings, not that many
independent missing systems. Zero reported blockers does not prove normal play.
The protected early-route owner passed using controlled exit dispatch, not a
human playthrough. Existing bubble and camera/shield primitive tests are reusable
evidence within their actual scope.

Sources:

- android/XL_compat/evidence/cameras.md
- android/XL_compat/evidence/bubbles.md
- temp/xl-overnight-20261101/final-original-pumo-blockers.json
- temp/xl-overnight-20261101/final_handoff.md

## Ordered work

### P0 - Get the existing starting slice into the user's hands

1. Audit the current opt-in APK's original mission import/start path and actual
   runtime admission. Identify the first ordinarily reachable unsupported boundary.
   Refresh branch/PR state first; do not lose unmerged PR 277 work.
   Output: a concrete supported starting route and exact next blocked transition.
2. Exercise original opening level/hub with existing automation and fixed probes:
   start with normal equipment; move, fire, take damage, collect pickups, operate
   relevant doors/switches, die/restart, save, quit and reload. Verify preserved
   mission/level/player state and no protected-data loss. Use original content.
   No autonomous 3D navigation is required; the user supplies normal play feedback.
3. Fix only blockers to that slice: crashes, broken controls, missing required
   assets, softlocks, essential progression and save/load failures. Reuse working
   rendering; do not add new shader or cosmetic fidelity work to this milestone.
4. Deliver a reproducible APK/build identifier, launch instructions, known supported
   boundary and a short feedback recipe: level, action, expected/actual result,
   save/checkpoint and logs where available. Keep unsupported areas truthfully
   gated without erasing their campaign entries. Do not wait for all camera work
   if the opening slice already meets these checks.

Exit: user can start ordinary play, save/resume and report issues in the verified
slice. A controlled test pass is a smoke check, not a claim of full human completion.
If step 1 reveals a gameplay blocker in the opening slice, fix that first rather
than assuming all eleven inspector-clean levels are ready.

### P1 - Unblock progression into the remaining original levels

5. Trace actual pmlev1/2/3 blockers to consumers and classify each by its authored
   role. Determine whether monitor images convey necessary information and whether
   lightning is decorative, damages anything, or participates in trigger/state
   logic. Record the source/content evidence; do not assume effects are cosmetic.
6. Integrate camera object gameplay: required addon/model ownership, placement,
   controller behavior (including behavior 135 where authored), collision, damage,
   death/persistence and relevant actor/weapon interactions. Prioritize behavior
   exercised by original Pumo and ordinary player equipment. Keep visible,
   intelligible camera bodies; defer shield/glow presentation.
7. Integrate object-trigger ownership, bindings and gameplay actions. Camera
   binding records are created during camera-manager setup, so do not reinterpret
   every object trigger as a later gameplay action. Verify any required door,
   objective, camera activation/deactivation and transition consequences.
8. Preserve these objects and their state through save/load, death/restart,
   level transitions and mission switches. Test real original-camera saves,
   not only a stock save after removing the camera.
9. Resolve required lightning gameplay/state behavior and remaining level-specific
   progression blockers. Retain an existing adequate visual path where possible.
   If an unsupported feature is proven presentation-only, explicitly defer its
   renderer in the development build; keep parsing, ownership and gameplay state.
   Change admission narrowly to distinguish implemented gameplay from deferred
   presentation. Do not blanket-disable effects or accept unknown gameplay.
10. Supply a functional monitor image now only when it is needed for navigation,
    puzzle information or another gameplay purpose. Reuse the existing capture
    foundations; defer exact shading, UV fidelity beyond legibility, and visual
    parity work. Decorative monitors can wait.

Exit per level: original content loads with required gameplay consumers active,
relevant interactions and save/resume pass, and the next transition behaves
correctly. Expand the user's playtest build at each completed level; do not hold
all three levels until the final one is ready. Ordering within steps 6-10 follows
the actual dependency audit, not a cosmetic feature checklist.

### P2 - Iterate on user feedback and campaign continuity

11. Triage feedback with this order: crashes/data loss, progression softlocks,
    controls/combat errors, save/load, performance that prevents play, then visual
    defects that hide essential information. Reproduce from the supplied save or
    fixed probe and add coverage to the existing relevant owner.
12. Verify hub returns, exits, secrets and campaign state as the user reaches them.
    Check checkpoint recovery, stock-mission isolation and Android suspend/resume.
    Keep a concise tested/untested matrix; do not delay initial delivery for a
    complete campaign traversal.
13. Publish updated fork PRs/builds with a brief changed-behavior and known-issues
    note. Run relevant local integration owners before publication and monitor
    current-head CI while continuing the stack. Do not call a CI-pending build
    fully qualified.

### P3 - Finish graphics-only work after playtesting starts

- Decorative camera monitors and exact monitor UV/material/lightmap parity
- Shield hit visuals, animated forcefields, pulse/glow and presentation profiles
- Remaining decorative lightning appearance and exact effect shader parity
- Optional 3D powerup presentation and other cosmetic enhancements
- Additional visual comparison matrices that do not resolve a gameplay defect

Reuse the completed graphics foundations; do not remove working features.
Visual work moves forward in priority only when evidence shows it blocks play
(for example an unreadable puzzle monitor, invisible hazard or unusable frame rate).

## Next branch and reporting rule

Start with P0's launch/runtime and save/resume audit, not the previously proposed
authored-monitor graphics integration. Each roughly one-hour PR should state
which playtest obstacle it removes or which delivery gate it proves. A task that
only improves appearance belongs in P3 unless its gameplay need is demonstrated.
Record actual elapsed time after P0, then update the estimate for the expanded
demo. The earlier 20-40 hour estimate covered a broader representative demo and
is not a reason to delay an already viable starting slice.

## Next overnight tranche: rendered GuideBot traversal pilot

User follow-up, November 1: try the existing rendered route-confirmation machinery
against original Pumo. The opening/PMC release is already delivered; this next
step supplements the earlier controlled-exit checks rather than restarting P0.

### Why and existing machinery

A rendered flight can expose navigation, clearance, door and objective-ordering
failures that fixed firing poses and direct exit dispatch cannot. Reuse:

- `android/helpers/regenerate_all_guidebot_simulations.ps1`: `-Mode Headed` for
  Android rendered confirmation, `-Mode Desktop` for native rendered confirmation,
  and `-Mode Headless` for non-rendered diagnostics
- `android/helpers/watch_guidebot_simulation.ps1`: interactive route browser
- `android/app/src/main/cpp/shared/route_confirmation.cpp` and `.h`: live planner,
  physics-driven companion actor, objective execution and terminal status
- `android/tests/test_guidebot_simulation_headed_headless_parity.ps1`: existing
  Counterstrike parity owner; evidence of the mechanism, not Pumo/XL qualification

The headed engine sets Viewer to the companion actor and follows the route in
rendered gameplay. It is not an ordinary player-input combat playthrough: the
player is immobilized/invulnerable, and some objectives use test-assisted damage
or pickup actions. Audit those assists and report them explicitly. A confirmed
route is supplementary traversal evidence, not proof of combat survival or full
player equivalence. The auto-level demo tests are a different feature.

### Bounded trial and acceptance

1. Spend the first coherent branch on feasibility and one original Pumo route,
   after urgent user feedback and existing CI failures. Time-box the initial
   investigation to roughly 1-2 hours; do not turn an unsupported planner into an
   unbounded overnight navigation rewrite
2. Audit mission metadata, target identity and runtime staging for the original
   fourteen-level `pmines` package, not the separate three-level demo. Confirm XL
   admission and required camera/Vertigo/effect resources survive staging. The
   generic Headed script currently calls reset_state, clear_mods and
   clear_save_files: use an owned disposable device/profile or a protected adapter
   with verified restoration. Do not aim it unchanged at user app data or assume
   the current Pumo protected runner automatically wraps this separate helper
3. Confirm what the planner understands about XL triggers, teleports, multiple
   authored exits and cross-level state. Begin at the authored hub start and
   choose an explicit intended exit/route so a different valid hub ending is not
   mistaken for the desired campaign path. Use a simple original level next if
   the hub's branching semantics prevent a meaningful first trial
4. Run a bounded rendered attempt with explicit device ownership, mission/level
   selection and output directory; keep Android tests serial. Retain the exact
   build/asset identity, objective sequence, last actor position/segment, route
   diagnostics, wall/trigger state, terminal status and screenshots or a short
   recording where supported. Use headless results as diagnostics, not a substitute
   for the requested rendered run. Do not overwrite canonical regressions merely
   to accept a changed result
5. Classify each outcome: confirmed route, partial coverage, timeout/stall, planner
   unsupported, staging/harness failure, or reproduced engine gameplay defect.
   A planner failure alone does not mean Pumo is unplayable. Reproduce suspected
   gameplay defects with normal player interactions before changing the engine
6. For XL objectives the planner cannot express, use targeted normal-activation
   probes and document the gap. Do not teleport past a blockage, grant a key,
   invoke an exit directly or use assisted objective completion as evidence that
   the missing ordinary progression works. Keep the harness's existing assists
   distinguishable from normal player checks
7. If the pilot is useful, expand across original levels/branches with per-level
   budgets and a tested/untested matrix. Pair important door/switch/exit results
   with ordinary-player activation and save/load checks. Fix evidenced defects;
   preserve original mission content and all independent gameplay admission gates

The next handoff must include the exact invocation or protected adapter, actor/
assist semantics, level/exit selection, coverage and retained failure evidence,
plus whether further GuideBot work is worthwhile. Carry this trial and its limits
into the fresh-context continuation prompt until resolved.
