# Next XL tranche: usable resources and time equipment

Prepared November 9, 2026, approximately 20:50 PST. Implementation window ends
**Tuesday November 10, 2026 at 12:00 PST (20:00 UTC)**. This is an implementation
handoff, not a claim that an overnight implementation has started.

## Recommendation

Finish the normal user path from importing genuine XL resources to using
slow-motion and bullet-time equipment in gameplay. In parallel when capacity is
available, close the shared authored-camera and lightning-owner gaps exposed by
Boiling Point and Dinter. Prefer a few complete features over another stack of
per-test or per-switch changes.

The main user-visible target is: import the resources, launch an unchanged
original mission, collect the equipment, activate it through normal controls,
hear and see it work, save, restart, and continue. Warp Shock level 6 is the
existing original-content target for time equipment. Boiling Point level 7 is a
second integration target requiring equipment, camera and lightning work.
Sphere level 2 is the first resource-completeness probe, with further gameplay
blockers to investigate after import. Full Dinter support is not promised in
this window: its 43 current admission findings include substantial particle work.

Companion [technical notes](xl_next_tranche_notes_20261109.md) contain exact
evidence, seams and commands. [Sol goal prompt](xl_next_tranche_goal_20261109.txt)
is ready to copy into a new implementing session.

## Starting point and boundaries

- Published stack tip: `268e711f008cbf5770b74ee365da4d821723fb57`, tree
  `d01c46fbe590d56cfb69ba63a1bbf7432e91a74f`,
  [PR 461](https://github.com/larsonm-personal/dxx-redux/pull/461).
  Checked during this planning session: open, mergeable, not merged.
- The stack already contains the test cleanup. Keep its shared Pumo scenario
  runner, shared receipts, bounded combat cases and independent source controls.
  Do not reconstruct the deleted 77-file chain.
- Inspect `AGENTS.md` and `.github/copilot-instructions.md`. Keep shared XL code
  under `android/XL_compat`; make small necessary D1/D2 integration hooks.
- Refresh remote state at implementation start. If cmake contains the published
  result, start there; otherwise use the published tip. If history was squashed,
  compare the integrated content rather than demanding literal ancestry.
- Create an isolated implementation worktree. Do not edit the old root branch,
  the validated review source, the repaired stack worktree, or existing stashes.
  Do not mix unrelated launcher/tooling PRs into this feature.
- This handoff does not authorize publishing, force-pushing, merging, or changing
  PR metadata. Earlier authorizations covered the repaired stack and PR 461 only.
  Local code, builds, diagnostics and reviewable patches are the work product.
  Follow repository rules for commits unless separately authorized.
- Genuine private assets stay private; do not embed them in public code or APKs.
  Use the user's original archive or pinned private asset overlays, through the
  existing owned-resource/import boundary. Preserve original mission bytes.

## Work packages and acceptance

### A. Complete the genuine-resource user path first

1. Use the existing ResourceModCatalog/ResourceModImport/ResourceModStore path
   to import and enable XL resources. Verify all catalogued sound dependencies
   and the camera addon are staged into the owned installation. Diagnose missing
   content versus incorrect mounting; a correct importer may need no code fix.
2. Incorporate the genuine time-pickup strips into that same resource pipeline:
   `textures/d2/slowmotion#0.png` and `textures/d2/bullettime#0.png`.
   They are vertical 16-frame strips. Reuse existing image/resource facilities,
   verify frame order and ownership, and retain bounded decoding/import rules.
   Private assets PR 4 is already merged; do not wait for a new asset worker.
3. Re-run original Sphere 2 and Warp Shock 6 with complete resources under a
   real renderer. Record the next actual blocker. A missing addon is not an
   unsupported engine feature, and satisfying one dependency is not a load pass.
4. Exercise the ordinary Android import/enable/launch flow once with the built
   candidate. Compare with desktop staging. Fix shared ownership/catalog logic
   where appropriate; do not create per-mission resource installers.

Acceptance: original resources are imported and enabled normally, retained by
the correct owner, visible to native runtime, and survive a launcher restart.
Desktop and Android agree on the required files. Missing/foreign assets still
produce clear failure. Report exact unresolved mission blockers. Dummy-audio
probes establish lookup only; demonstrate actual playback separately.

### B. Make slow-motion and bullet-time usable end to end

This is the primary runtime feature, not another arithmetic port. Much of the
math and many engine consumers already exist, but only tests call activation.

1. Trace the pinned reference's normal pickup, input and per-frame order. Wire
   one production frame adapter to `xl_runtime::slow_motion_step`; apply energy
   deltas through the existing player resource owner and deliver sink events
   once, synchronously. The returned event list is an audit, not a second queue.
2. Implement normal pickup IDs 48/49, equipment ownership, duplicate behavior,
   HUD/feedback and equipment-center spawning where original content requires
   it. Share pickup semantics between placed items and spawned equipment.
   Update admission only with the implemented behavior and resource dependency.
3. Add ordinary desktop bindings and accessible Android actions using the
   existing control/action infrastructure. Handle press consumption once.
   Activation cannot depend on a developer command or test-only player mutation.
4. Connect slowdown/speedup messages, genuine transition sounds and the actual
   audio behavior requested by the reference. Distinguish a mixer-capability
   fallback in the reference from silently dropping an implemented event.
5. Preserve the world/console-player clock distinction. Reuse the existing
   physics, AI, weapon, lifetime and Omega consumers; do not globally scale
   FrameTime or build a parallel slow-motion system.
6. Save equipment and active/transition clock state through the existing XL save
   block. Validate the complete state before applying it; define timestamp
   rebasing on cold restore. Cover pause, death/new game, level transitions and
   energy exhaustion according to source semantics. Leave stock paths inert.
7. Demonstrate a visible difference between ordinary slow-motion and bullet-time
   in actual gameplay, then cold-save/restore and continue. Use unchanged Warp
   Shock 6 if it is admitted. If another blocker remains, use an existing admitted
   mission for a separately labelled focused integration demonstration and keep
   the original Warp Shock failure visible. Such a demonstration does not qualify
   Warp Shock support.

Acceptance: ordinary users on desktop and Android can acquire and operate both
items; clocks, resource cost, rendering, sound and persistence agree with the
qualified reference profile. Classic/multiplayer paths remain unaffected.
Incomplete pieces remain explicitly unsupported; do not enable a partial pickup
just to make admission green. Preserve the existing Omega input-demo rejection
until its render-clock timeline is implemented. That replay feature is deferred
from this tranche and must be disclosed in the final report.

### C. Share authored camera and lightning behavior across missions

This is independent runtime work suitable for a second worker, or the next
serial package once A is stable. Take the bounded, source-supported parts first.

1. Separate camera binding from trigger firing. Boiling Point binds a camera to
   an effect object; Dinter binds cameras to moving robots. The reference creates
   object cameras for these general owners, not only CAMBOT. Trace attachment,
   rendering, movement, deletion/death and restore. Reuse `camera_bindings.h` and
   runtime camera ownership rather than adding mission-specific admission rules.
2. Audit lightning emitters versus zero-bolt target/anchor objects. The pinned
   reference retains those objects but skips creating emitters for `nBolts <= 0`.
   Support that distinction throughout admission, runtime ownership and save
   restoration. Do not delete anchors or fabricate visual bolts for them.
3. Resolve target lookup and orientation/length fallback from the reference.
   Target 0 or a missing named target does not automatically mean malformed
   content in the reference. Preserve valid shape/range checks and establish
   deterministic owner ordering where duplicate IDs exist.
4. Re-run the two original inventories after each semantic change; group
   remaining findings by missing behavior. Then run real rendered gameplay for
   any newly admitted content, including cameras and effects visible on screen.

Acceptance: supported camera owners show the correct live view and survive
their required lifecycle; supported lightning targets/emitters match reference
semantics. Original Boiling/Dinter finding counts may fall, but that alone is not
a gameplay pass. Dinter's nonstationary particles, bubbles and nondefault
lightning profiles remain a separate follow-on unless A/B/C are complete early.

## Schedule through noon

Times are America/Los_Angeles, PST. They are decision points, not permission to
wait idly or to declare incomplete features complete. Start from the current
clock; if delayed, preserve the final integration window and reduce stretch work.

| Window | Main path | Concurrent work, if authorized and available |
| --- | --- | --- |
| Nov 9, now-21:30 | Isolate current source, read notes, inspect input/save/audio seams; begin genuine import | Inventory camera/anchor owner semantics against pinned source |
| 21:30-23:00 | Finish A staging and obtain first rendered original-mission results | Define and implement bounded C camera/anchor changes |
| 23:00-03:00 | B normal pickup/control/frame/audio integration; build and play as soon as a complete path exists | Finish C lifecycle and rendered verification; report patch and remaining findings |
| Nov 10, 03:00-07:00 | B active persistence, Android controls/resource flow and actual gameplay | Integrate C; fix concrete cross-feature failures |
| 07:00-09:00 | Combined original Warp Shock/Boiling/Sphere attempts; fix demonstrated blockers | Only if main feature is complete: take one bounded remaining effects behavior |
| 09:00-10:00 | Freeze new scope, remove duplication, review changes and update touched feature docs | Finish any already-started bounded work or leave it isolated |
| 10:00-11:30 | Build final candidate, desktop and Android verification, unchanged readiness measurement, required suites | No new feature work; fix regressions in candidate |
| 11:30-12:00 | Final diff audit, artifact identity, reproducible handoff and known-limit report | No publication without separate authorization |

At 23:00, record A's actual status and B's remaining interfaces. At 03:00, a
normal pickup/control demonstration should exist; if not, concentrate on its
blocking dependency and stop C expansion. At 07:00, reassess whether the full
time-equipment slice can be qualified. Preserve incomplete code separately if
necessary, leave admission honest, and explain what remains. Do not spend the
last five hours expanding test machinery around an unfinished feature.

## Coordination and consolidation

One Sol coordinator can run this serially. If the implementing session has
explicit authorization for agents, use at most three implementation roles:

- Gameplay owner: B, frame/input integration, pickup/equipment and save state.
- Resource/platform owner: A, Kotlin catalog/import and Android control wiring,
  plus the first original-mission launch evidence. Coordinate shared input files
  with gameplay owner before editing them.
- Camera/effects owner: C and its narrowly scoped validation.

Each uses an isolated worktree and records base, changed files, ownership and
validation. Shared admission/save/CMake files have one integrator; contributors
send specific proposed edits instead of overwriting each other. Integrate a
complete feature at a time, using patches if commits are not authorized. Run
emulator tests serially and serialize heavy builds when memory/storage contends.
The coordinator owns the final combined-tree evidence, not just worker reports.

Prefer two or three feature-sized patches. Share resource acquisition, pickup
semantics, state ownership, test setup and result verification at existing seams.
Do not introduce a generic plugin framework or broad upstream D1/D2 deduplication.

## Validation without rebuilding the test bureaucracy

- Extend existing focused runtime/resource/reference tests for genuinely new
  behavior. Add a compact gameplay scenario for the complete time-equipment
  path, reusing the existing scenario runner and persistence verification where
  applicable. No per-key, per-switch, per-case Python/CMake wrapper chains.
- Keep a few meaningful boundaries: ordinary activation of both modes, duplicate
  pickup, insufficient/exhausted energy, transition/cold restore, and disabled
  stock/network paths. Use existing arithmetic oracle coverage rather than
  multiplying frame/seed combinations in the integration test.
- Preserve private asset provenance and shared receipt checks. Do not duplicate
  every engine state transition into a Python expectation table.
- During implementation, run affected checks. On the frozen candidate, run the
  required public/private selections, D1/D2 builds and affected Android tests.
  Existing validation from the old tree is baseline evidence only.
- Use native Linux storage for private-suite output; the previous identical
  suite timed out on WSL's Windows-mounted filesystem and passed on native
  storage under unchanged limits. Do not inflate timeouts as a workaround.
- Keep the frozen eight-case headless readiness measurement unchanged. Genuine
  extra-resource and rendered configurations are separate labelled evidence.
  Report load/tick, rendered behavior, assisted feature exercise and unassisted
  play separately. All four baseline passes have `xl_active=false`; 4/8 is not
  a claim that half the XL campaign corpus is playable.
- Run the scoped mixed-language formatter once the edits stabilize. Register
  any new top-level Android automation runner through the existing catalog.

## Noon deliverables

1. Reviewable integrated source changes and an exact base/tree identity. Include
   any isolated unfinished patch explicitly; do not silently fold it into the
   qualified candidate. No new micro-PR stack.
2. Windows D2 executable and Android APK built from that candidate, with paths,
   hashes, required private-resource setup and ordinary activation instructions.
   State any unavailable platform or failed build plainly.
3. A short feature table: imported resources, time pickups/controls/audio/save,
   camera owners, lightning anchors/emitters, and each original mission tested.
   Distinguish implemented, demonstrated, blocked and deferred.
4. Exact commands and bounded logs for the new gameplay demonstrations, final
   relevant checks and unchanged headless baseline. Include assistance used and
   whether audio was actually audible; don't label dummy audio as hearing proof.
5. Updated touched feature documentation and a concise next-blocker list. Avoid
   sweeping historical report rewrites. State the remaining Omega replay limit.

These are delivery requirements for the implementing run, not results already
achieved by this planning task. If a requirement is unfinished at the cutoff,
report it as unfinished and preserve a reproducible handoff rather than claiming
the full implementation goal complete.
