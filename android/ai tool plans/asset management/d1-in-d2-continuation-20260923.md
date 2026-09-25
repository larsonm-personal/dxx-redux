# D1-in-D2 continuation from the September 23 state

Latest research handoff: [September 24 continuation](d1-in-d2-continuation-20260924.md).
Read it first for completed AI-encoding/host-test results, pending-run status
and the next delivery sequence. The historical detail below is retained

Date: 2026-09-23

Status: implementation in progress; rendered Spreadfire and dynamic checkpoint comparisons pass, strict qualification still open

## Refreshed handoff from the current working tree

Research revision: HEAD `361167d9` plus existing uncommitted changes. This section
supersedes the older stopping-point descriptions and immediate priority order
below. The consolidation plan's F1-F5 acceptance gates remain authoritative

This refresh inspected source, Git history and retained logs/reports. It did not
run new builds, games or tests. Existing music, guidebot, cockpit and replay work
is in flight in the shared tree; coordinate build/device ownership before the
next implementation slice. Do not treat all working-tree changes as one patch

September 24 implementation update: the completed-door state discrepancy is
fixed in the semantics owner, with a failing-before/passing-after actual door
fixture, loaded native/imported comparisons and ordinary D2 controls. All 53/61
host tests and the three-ABI Android build/package pass. AI schema validation
now has 29 passing tests and requires engine-specific fields and boss storage
bounds. Both paired runs below still need final comparison results; a fresh
door-fixed run is in `temp/d1-door-ai-parity`. See the ledger's September 24
entry for exact evidence and the new frozen APK

The subsequent observer slice now uses world schema 3: an explicit slot-zero
AI default plus lossless exceptions, including nonzero inactive saved clocks.
Both full host suites and 30 comparator tests pass; new capture/Android
verification is pending. Earlier schema-2 runs retain their archived checker.
Read the ledger's lossless-encoding entry before comparing traces across these
versions; do not infer a gameplay difference from the encoding alone

The extended Android sound retry encountered a device-system failure
(`DeadSystemException`) before engine startup. Let its existing harness finish
cleanup before retrying with `temp/d1-door-ai-app.apk`. The latest successful
Gradle artifact is in `build/outputs/apk/debug`; the old intermediates APK is
stale. The runner now detects an observed game process exiting early, but the
already-running retry still uses its earlier script. Device sound validation
and the synchronized level-transition failure remain open

### Confirmed stopping point

- The September 7 weapon plan describes the old overlay implementation. Current
  `d1_in_d2_primary_projectile` selects projectile 20 while retaining accounting
  record 12, and independent D1 generations publish the original artwork
- `temp/d1-weapon-art-final.log` reports ten native/imported Spreadfire firing,
  source and rendered-frame matches in both imported configurations. Android
  rendering and in-flight checkpoint rendering remain unverified
- The checkpoint adapter now preserves weapon creation/hit history, morphs,
  stuck flares, effect overrides, reactor timing, automap exploration and secret
  progress. The ledger records real-save comparisons and transactional rejection
  checks; this is substantial coverage, not a completed save-format audit
- The long level-7 recapture removes the exploding-wall size/position difference
  across 16,873 frames. The newer short level-7 report has matching native/imported
  SIM and FX events including object context. Raw layout/diagnostic differences
  and incomplete coverage still prevent strict qualification
- Full object/world restore and terminal records plus 16 world-state groups are
  now implemented. The focused native/D2 observer integration logs both pass:
  `temp/d1-world-observer-native-final.log` and
  `temp/d1-world-observer-imported-final.log`. The expanded paired run in
  `temp/d1-world-state-parity` now has a repeatable native reference. Its imported
  terminal cursor differed. The replay owner now lets imported D1 finish its
  exit-trigger frame like native D1. The fresh `temp/d1-exit-phase-parity`
  captures have matching complete results apart from engine/mission labels, and
  terminal cursors, endlevel/suspension flags and homing distance now match.
  Remaining raw representation differences still prevent strict qualification.
  Required world-field types/shapes were checked by 27 comparator tests and
  three retained real traces under world schema 1. The latest working tree
  advances to schema 2, adding saved AI storage as a seventeenth group: inactive
  AI locals, the complete path pool and allocator, cloak/awareness storage,
  path runtime and boss state. Both host builds and the focused real-engine
  observer fixtures pass; 28 comparator tests pass. The expanded paired run in
  `temp/d1-ai-world-parity` has reached native-repeat but has no final report at
  this inspection. Android compilation of this addition was deferred by the
  active-replay cleanup guard, not rejected by a compiler. Transition caches,
  private runtime state and complete semantic mappings remain unfinished
- Retained Android logs confirm D1-only and D2-to-D1 single-player interaction and
  transition passes. They do not clear the newly reported synchronized transition
  failure or verify audible effects, HUD scaling or dynamic checkpoints on-device
- Optional Guide-Bot source attachment is connected and publication/switching
  checks pass. Full actor behavior, restore and failure lifecycle still needs F3
- The effects-rate investigation now has a tested production correction. D2's
  mixer honors each published sample's rate, and both engines share exact-rate
  conversion that also fixes SDL 1.x's 48 kHz duration error. All 53/61 host tests
  and Android packaging pass. Device retries exposed an automation dispatcher
  that did not recognize the owned D1 briefing. That dispatcher now sends Escape
  through the correct window, with 84 host briefing-frame matches, 61 D2 tests
  and Android packaging verified. Use the frozen
  `temp/d1-briefing-dispatch-app.apk` for the device sound check; consult the
  ledger for its hash and evidence. A check is already in progress in
  `temp/d1-sound-briefing-fixed-android.log`, with artifacts under
  `temp/d1-launch-runtime-20260923-233917`. No final result was present at this
  inspection. This APK predates the new AI observer

### Resume the in-flight work first

Before launching another capture or device scenario, inspect the two runs above
and preserve their final results. Do not infer success from reaching a menu or
from an incomplete log. The device runner must report its matching automation
run ID and pass the actual playback assertions. Keep emulator tests serial and
wait for replay captures to finish before retrying the guarded Android build

The latest observer evidence is retained in
`temp/d1-ai-world-build-fixed.log`,
`temp/d1-ai-world-{native,imported}-fixed.log`, and
`temp/d1-ai-world-schema-tests-final.log`. The initial full host suites each
failed the observer integration test; those two tests passed after the fix.
This is a failed-test rerun, not a new complete suite pass. Preserve that
distinction when summarizing validation

### Next implementation sequence

1. **Close current user-visible failures.** Prioritize the level-1-to-2 loss
   of overlay/control and device verification of the effects-rate repair. Capture
   the synchronized transition on host and client, checking game/window state, pause/sync flags,
   input dispatch and overlay attachment before/after each phase. Use single
   player and ordinary D2 as controls. For audio, validate the existing per-sample
   rate/conversion correction through actual playback; host evidence already
   identifies and fixes the 11025/22050 Hz mismatch. Compare native/imported
   audible duration and pitch with music off, then on. Verify the existing
   key-icon fix and Spreadfire on Android, including
   live shots across restore. Coordinate MIDI lag/source-menu issues with the
   concurrent music work. Exit: a reproducible scenario and regression check for
   each repaired issue, with unresolved configurations explicitly recorded

2. **Finish the current F1 observation slice.** Read the pending expanded report
   and preserve its earliest restore/frame/terminal differences. Complete
   validation of the new AI/path/boss observations, add transition-cache and
   remaining private-runtime observations, audit remaining saved globals, and
   finish object-subtype, engine-specific AI-field and semantic cardinality
   validation. Measure the expanded observer's capture cost on the short case
   before scheduling the corpus; preserve state coverage if optimizing it. Add
   explicit, identity-checked mappings for native/D2 storage differences while retaining
   raw evidence. The manifest now archives/hashes nonignored untracked sources
   as well as `git diff HEAD`; use the updated runner for future captures and
   retain the limitations of older source manifests. Exit: complete declared
   state coverage with missing or malformed evidence rejected, plus host and
   Android validation of the observer

3. **Close measured F2 differences, then run the whole corpus.** Use the unchanged
   saved checkpoints, two native runs and one imported run on frozen binaries.
   Diagnose the earliest genuine difference in its owning subsystem; repeat the
   focused case before the dynamically discovered eight-demo corpus. Preserve
   the completed-door discrepancy as a recapture check: walls 86/87 in short
   level 7 first differed at frame 2011. The new owner preserves native OPENING
   after retirement; actual animation/contact tests pass. Confirm the raw
   replay difference disappears and retain the separate network lifecycle
   obligation. No global normalization from 1 to 4 is appropriate.
   Preserve separate recording/native, native repeatability and native/imported verdicts.
   Extend real-save cases for remaining native AI storage/globals and actual
   save/reload/rewind. Exit: exact supported simulation parity, with historical
   recording disagreement explained independently

4. **Complete F3 optional behavior.** Keep the working attachment path. Exercise
   missing/invalid/valid D2 sources, cold companion spawn, morph/draw/sounds/flares,
   doors, save/restore and retirement across switches. Prove baseline invariance
   with optional assets present but the companion absent, and simulation
   invariance under camera toggles. Exit: a complete optional lifecycle without
   changing unenhanced D1 behavior

5. **Finish F4/F5 consolidation and qualification.** Inventory callers before
   removing old overlay/capture/backup paths. Cover stock/custom resource changes,
   normal/secret travel, death, co-op joins/transitions, errors and ordinary D2
   controls. Qualify registered-PC content on Windows and Android first, including
   arm64 runtime evidence; list untested editions/platforms explicitly. Native
   D1 remains the reference until a separate retirement decision

Implementation owners and reusable runners are listed below. Record new evidence
in the implementation ledger, keeping test scope and frozen build identity
explicit. Leave `android/outstanding_bugs.md` untouched per its file instruction

## Earlier research baseline and detailed work packages

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

Implementation follow-up: `test_d1_weapon_art.ps1` now compares ten actual firing/render frames in native D1, imported D1 alone and imported D1 with D2 installed. Source pixels, palettes, state and rendered RGB all match, including custom artwork, reload/retirement and firing in D2 before returning to D1. The reported old sprite defect was not reproduced in this matrix. The checkpoint adapter now restores weapon creation frames/full hit history, active morphs, stuck flares, reactor death-silence timing, effect animation overrides, automap exploration and discovered-secret progress. Secret identities are translated only after validating the native level-content identity. The dynamic fixture covers ordinary D2 save/reload, resumed morph completion, door cleanup and transactional rejection of malformed records. Android packaging and the isolated D1-only play/transition scenario pass. See the implementation ledger for exact evidence and limits. These results do not close Android rendered Spreadfire checks, complete checkpoint fidelity or F1-F5

The pinned long level-7 strict capture now completes. Native repeatability passes, and native/imported terminal results plus SIM/FX values match. The exploding-wall position/size difference at frame 12,822 has been fixed and a fresh imported capture confirms it across all 16,873 frames. The remaining named-object differences concern 13 engine-layout fields; raw AI hashes and RNG context still fail strict comparison. Complete the missing world snapshots and explicit mappings; do not infer a strict pass from the matching terminal result

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
- The weapon/morph/stuck/effect/timer omissions are now repaired and exercised against actual native saves. Finish the remaining checkpoint schema audit, including inactive native AI storage and remaining saved globals, before claiming full restore coverage
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

Exit: each claimed edition/platform/feature has explicit acceptance evidence; transitional owners are removed or documented as incomplete. Record sprite verification, baseline fidelity and optional-feature support separately in the ledger. Leave `android/outstanding_bugs.md` untouched per its file instruction

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

Reuse `android/helpers/test_d1_gameplay_rules.ps1`, `test_d1_ai_frames.ps1`, `test_d1_ai_checkpoints.ps1`, `test_d1_campaign.ps1`, `test_d1_render_candidates.ps1`, `test_d1_briefings.ps1`, `test_d1_weapon_art.ps1` and `test_d1_in_d2_bootstrap.ps1` for relevant host comparisons; supply their required data arguments. Use `android/helpers/test_d1_in_d2_android.ps1` (including `-SoundCheck`) and the Trine 2 automation scenario for serial Android checks. The Android helper consumes the standalone JSONC scenario; there is no `android/tests/test_d1_in_d2_standalone.ps1` runner

For implementation slices, run relevant CMake/CTest targets and scoped mixed-language quality checks. Broaden validation when a change affects another domain, then perform the full declared matrix at qualification. Append new results to the implementation ledger with their exact scope

## Research checklist

- [x] Read repo instructions and identify authoritative versus historical plans
- [x] Reconcile the September 21 stopping point with September 23 source/history
- [x] Inspect retained eight-demo pass and incomplete paired-capture report
- [x] Verify current Spreadfire selection, native bitmap publication and Guide-Bot attachment
- [x] Write continuation order, ownership, acceptance gates and existing runners
- [x] Perform the host rendered Spreadfire comparison with custom art and D1/D2 switching
- [ ] Complete Android and live-shot restore rendering coverage
- [ ] Complete fresh strict captures and F1/F2 evidence
- [ ] Finish optional lifecycle and F4/F5 qualification
