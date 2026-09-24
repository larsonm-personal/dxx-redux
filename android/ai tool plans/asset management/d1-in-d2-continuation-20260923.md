# D1-in-D2 continuation from the September 23 state

Date: 2026-09-23

Status: research and continuation plan complete; implementation and new runtime validation not started

Research baseline: HEAD `756378a3`, including the D1-in-D2 changes in `3ce7e963`. The working tree also contains ongoing music work and edits to `android/outstanding_bugs.md`; this research does not modify them

## Where we actually left off

The governing architecture remains the [consolidation plan](d1-in-d2-consolidation-plan.md), especially section 0 (F1-F5). The [implementation ledger](d1-in-d2-implementation-ledger.md) records the substantial September 21 implementation. Its last entry is no longer the latest result: [September 23 suite repairs](../testing/suite_failures_20260923.md) fixed the next replay failure and connected optional Guide-Bot loading

The June overlay design and September 7 weapon/semantics audits are historical inventories. Do not execute their unchecked implementation lists without checking current code. In particular, the current architecture publishes original D1 tables and bitmaps independently; it does not require translating every D1 weapon bitmap into a D2 bitmap slot

| Area | Current evidence | What remains |
| --- | --- | --- |
| Independent D1 resources | Base/custom generations publish original bitmap, weapon, robot, effect, model and sound definitions. Earlier ledger evidence includes isolated registered-PC Android startup and play through the first exit | Wider lifecycle, content-edition and platform coverage; invariance when optional D2 assets are present |
| Spreadfire and weapon behavior | `d1_in_d2_primary_projectile` selects 20 with native assets. Normal firing retains accounting record 12. `test_loaded_d1_weapon_firing` checks emitted IDs/damage, energy and cadence. Quad/Fusion initialization and parent-aware Smart children also have owned implementations | Targeted rendered Spreadfire comparison and remaining weapon/persistence coverage; the open sprite report is not yet disproved |
| Gameplay and presentation | Dedicated owners now implement AI, pathing, weapons, campaign/trigger operations, briefings, cockpit and many collision/physics rules, with native comparisons recorded in the ledger | Integrated exactness and uncovered interactions, not a wholesale restart of those domains |
| Long level-7 replay | September 21 fixed robot-egg radius and an extra RNG draw, but still failed. September 23 found the next difference at frame 4744: lava shove/rotation RNG ordering. Commit `3ce7e963` fixes it and extends collision-motion observations | Fresh strict paired capture after the fix |
| Ordinary D1-in-D2 regressions | Retained `temp/suite-fixes-20260923/replays-final.log` ends with `RESULT: PASS (8 regression demo(s))`, using original expectations | This runner's terminal accommodations do not establish full frame/world parity |
| Strict replay evidence | Existing paired runner stages D1-only resources and separates recording/native, native repeatability and native/imported. Latest long level-7 report contains complete matching native repeats | Imported capture timed out after 300 seconds; suite notes also report binaries changing during other work. Full semantic schema and restore/terminal snapshots remain incomplete |
| Optional Guide-Bot | `prepare_assets` now calls `d1_in_d2_prepare_available_guidebot` after custom preparation and before publication. It reads an available D2 package instead of depending on previous live tables. Native texture wrapping excludes extension slots | Full actor/resource/restore/error lifecycle and removal of legacy capture/append paths; the old claim that production never attaches the package is obsolete |

These are source checks and retained results, not new test runs. The latest ordinary replay pass and incomplete strict report were inspected directly. No new compilation, game launch, emulator run or screenshot comparison was performed for this plan

## Recommended delivery sequence

### 1. Close the reported Spreadfire presentation question first

This is a bounded user-visible issue and can be checked before expanding the strict replay observer

- Reproduce ordinary firing in the same registered First Strike level in native D1 and D1-in-D2, with matched renderer, resolution, palette and camera. Capture two volleys so both Spreadfire orientations are exercised
- Observe the actual emitted object, weapon record, bitmap name/index, decoded pixels, palette, transparency flags and displayed size through engine introspection. Compare a rendered in-flight frame, not just a no-render replay result
- Verify projectile 20 and accounting record 12 separately. Follow `Laser_render` through `draw_object_blob` to the live bitmap. Original-source publication should now make the September 7 D1-index-in-D2-table diagnosis obsolete on the native-generation path; prove that in the failing scenario before choosing a fix
- Repeat after a checkpoint containing live shots, a D1 -> D2 -> D1 switch, and a custom `sprdblob` replacement. Check D1 alone and D1 with optional D2 assets present, with the companion disabled
- Extend the existing loaded weapon fixture and a reusable Android scenario where needed. Keep native D2 projectile 12 and its art as a control. Fix only a reproduced asset, lifecycle or renderer defect in its owning layer

Exit: paired rendered evidence and exact source/runtime checks explain whether the reported sprite problem persists. If it is already fixed by generation publication, record that result instead of introducing a new bitmap remapper. Only then close the sprite-specific bug; the broader compatibility task stays open

### 2. Re-establish strict paired evidence on a stable build

Start with `d1_descent_level7_20260921_144212.dximdemo`, then dynamically discover and run the complete D1 corpus (currently eight recordings)

- Build native D1 and D2 once, pin both executable hashes, source revision/patch, assets and harness, and avoid overlapping rebuilds during capture. Current music changes mean old binaries cannot silently stand in for the current source tree
- Use `test_d1_replay_parity.ps1`, which already produces two native captures and one imported capture. Keep the embedded checkpoint; do not switch to `-D1InD2StartFromLevel`
- Resolve the observed capture budget issue with progress evidence. The ledger already records a complete pre-fix imported traced run taking 351.351 seconds, longer than the wrapper's 300-second default. Inspect progress and disk use, then select a justified per-capture budget (600 seconds is an initial candidate), or improve trace overhead. Do not classify timeout as gameplay divergence or suppress a stalled run
- Preserve the disk-space reserve and artifact retention. Use existing bounded capture/comparison tooling; do not expand all large traces into additional unbounded copies
- Confirm that native/imported collision motion and SIM ordering remain equal through frame 4744 and onward. Compare FX separately too. An ordinary terminal pass is useful evidence but cannot substitute for these checks
- Report all three relationships independently. Preserve native-versus-recording diagnostic failures and representation differences; do not rewrite old expectations to manufacture agreement

Exit: all selected captures complete on identical pinned builds, and the first genuine semantic/RNG difference is identified for every failing case. If none remains in the observed fields, report that bounded success while F1's missing observations remain open

### 3. Complete F1's state contract and close measured F2 gaps

`d1_replay_parity.py` currently declares four coverage gaps and deliberately returns incomplete/failure rather than qualification. Finish the evidence that its report says is missing

- Emit complete state immediately after checkpoint restore and before the first simulation advance, plus world/transition state before retirement. Extend the existing shared observer rather than building another engine model
- Cover player/inventory, active objects and allocator/segment links, weapon hit/creation state, AI/path/boss state, doors/triggers, reactor caches, effects/morphs/stuck objects, clocks and both RNG streams
- Define explicit typed identity/layout mappings: native reactor identity, D1 hide submode versus D2 storage, unused D2 capacity, mission/executable labels and diagnostic source context. Validate identities before mapping; retain raw records. Do not ignore populated fields or real ordering differences
- Resolve the observed terminal exit-marker differences at their observation/transition boundary. Strict mode must not copy actual values into expected values
- Review the still-present translator omissions: `d1_save_translate_skip_weapon_fidelity_state`, `d1_save_translate_skip_morph_state`, and discarded effect timing. Compare the native save writer/restore consumers and test actual restored behavior before assigning causality to any corpus mismatch
- Add focused integration cases for persistent shots with prior hits, a homing scheduling boundary, active morph, a flare stuck in a moving door, and a one-shot effect. Compare frame zero and first subsequent frames as well as final state
- Fix the earliest demonstrated engine difference in its owner, rerun that case, then the corpus. Keep orchestration tests for missing traces, identity mismatch, first-frame differences and terminal mismatch

Exit: the strict gate has complete declared coverage and fails on genuine mismatches, missing evidence and unsupported cases. Native repeatability, native/imported parity and reproduction of historical recordings remain separate verdicts

### 4. Finish the now-connected optional Guide-Bot lifecycle (F3)

Do not implement the already-landed production attachment again

- Exercise D1-only, valid D2 package present, incomplete/invalid optional package, and repeated mission switches. Confirm failed optional preparation leaves the native generation usable
- With no companion actor, compare baseline source definitions and simulation with/without D2 files. The new source-count texture fix is a specific regression to retain; audit other total-count assumptions around optional extension entries
- With the actor enabled, verify cold spawn, morph, drawing, movement/doors, goal/robot sounds and flares, then save/load and retirement/reload. Verify mapped resource identities rather than hardcoded D2 slots
- Compare native enemies in an interaction-free fixture. Do not demand whole-world equality after deliberately adding a companion that can collide or fight
- Exercise cameras separately, including homing candidate selection. Keep simulation independent of extra views

Exit: optional features work from cold launch and restore without changing the unenhanced D1 baseline. Preserve the working source-package preparation and retire legacy capture only after its remaining callers are accounted for

### 5. Consolidate and qualify supported scope (F4/F5)

- Remove old overlay/capture/backup paths after a caller inventory, preserving one prepare/publish/retire lifecycle. Review the new lava mode branches in `collide.c` for movement into the semantics owner while retaining their exact RNG phases
- Exercise real save/reload/rewind, normal and secret exits/death/return, compound triggers, custom PG1/DTX/HX1, failed preparation recovery, and D1 -> D2 -> D1 transitions. Keep ordinary D2 secret travel and resource behavior as controls
- Complete content/rules identity and applicable co-op join/restore/travel checks. Treat native-D1/D2 wire interoperability and classic `.dem` playback as separate support decisions; `.dximdemo` success does not imply either
- Repeat original-resource/render/audio checks and isolated Android D1-only startup/play/menu/shutdown. Investigate the separately noted console-headless startup dependency; the existing no-render full executable is not proof of that path
- Establish registered-PC Windows/Android support first, including Android arm64 where the recordings originated. List shareware/OEM/Mac and other platform gaps explicitly and exercise them before advertising support
- Keep native `d1/` as the executable reference. Deleting it requires a separate decision after qualification, not merely eight passing ordinary replay results

Exit: each claimed edition/platform/feature has explicit acceptance evidence; transitional owners are removed or documented as incomplete. Update the outstanding bug entries to distinguish sprite verification, baseline fidelity and optional-feature support

## Existing entry points to reuse

Run from the repository root after coordinating ownership of builds/devices. Preserve prior artifacts using the existing retention helpers

```powershell
.\run-windows-build.ps1 -Target both

# Ordinary regression guard, separate from strict fidelity qualification
pwsh android/tests/test_input_demo_regressions.ps1 -RecordedGame d1 -D1InD2

# Focused paired capture; use this budget only after checking progress/cost
pwsh android/tests/test_d1_replay_parity.ps1 `
    -DemoFileName d1_descent_level7_20260921_144212.dximdemo `
    -TimeoutSeconds 600

# Full paired corpus, discovered dynamically
pwsh android/tests/test_d1_replay_parity.ps1 -TimeoutSeconds 600
```

Reuse `android/helpers/test_d1_gameplay_rules.ps1`, `test_d1_ai_frames.ps1`, `test_d1_ai_checkpoints.ps1`, `test_d1_campaign.ps1`, `test_d1_render_candidates.ps1`, `test_d1_briefings.ps1` and `test_d1_in_d2_bootstrap.ps1` for relevant host comparisons; supply their required data arguments. Use `android/tests/test_d1_in_d2_standalone.ps1` and the Trine 2 automation scenario for serial Android checks

For implementation slices, run relevant CMake/CTest targets and scoped mixed-language quality checks. Broaden validation when a change affects another domain, then perform the full declared matrix at qualification. Append new results to the implementation ledger with their exact scope

## Research checklist

- [x] Read repo instructions and identify authoritative versus historical plans
- [x] Reconcile the September 21 stopping point with September 23 source/history
- [x] Inspect retained eight-demo pass and incomplete paired-capture report
- [x] Verify current Spreadfire selection, native bitmap publication and Guide-Bot attachment
- [x] Write continuation order, ownership, acceptance gates and existing runners
- [ ] Perform the rendered Spreadfire comparison
- [ ] Complete fresh strict captures and F1/F2 evidence
- [ ] Finish optional lifecycle and F4/F5 qualification
