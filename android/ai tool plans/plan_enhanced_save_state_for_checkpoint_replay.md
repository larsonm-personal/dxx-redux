# Enhanced Save State For Checkpoint Replay Plan

## Goal

Decide whether input-demo checkpoint replay should move transient gameplay state from `.dximdemo` checkpoint metadata into the regular D1/D2 savegame format, then enumerate the state currently missing from saves and plan a save-format enhancement that improves both normal saves and input-demo checkpoints.

## Status

- [x] Create this plan file as the first work artifact.
- [x] Audit current D1 and D2 save/restore coverage.
- [x] Audit live gameplay globals that influence deterministic simulation after restore.
- [x] Decide whether save-format enhancement is preferable to checkpoint-only metadata.
- [x] Enumerate missing state by priority and risk.
- [x] Draft implementation and validation phases.
- [x] Implement phase 1 deterministic save-state helpers.
- [x] Implement D2 save-version bump and field persistence.
- [x] Implement D1 save-version bump and matching field persistence.
- [x] Implement phase 3 object and transient-state fidelity persistence.
- [x] Migrate input-demo checkpoint metadata to the enhanced save fields and drop pre-release legacy timer metadata compatibility.
- [x] Run host build and smoke regression validation.
- [ ] Validate a freshly recorded checkpoint-backed replay against the new save bytes.

## Working Notes

- A real D2 checkpoint-backed input demo reached different reactor-hit sequences when only spreadfire phase was changed, so at least one transient weapon-pattern state value is relevant to checkpoint replay.
- The intent is to keep the save file as the source of truth where practical, so input-demo checkpoint payloads do not need parallel metadata for ordinary game state.
- Current save files already preserve the durable world state: player inventory and stats, current selected weapons, objects, walls, doors, triggers, fuel centers, control-center state, AI state, automap, and D2 extras such as exploding walls, cloaking walls, light subtraction, markers, afterburner charge, and omega charge.
- The route is good. Adding ordinary simulation state to DGSS is cleaner than growing `.dximdemo` checkpoint metadata because it improves normal saves, keeps checkpoint replay using the same restore path as users, and keeps game-specific details in C.
- Phase 1 helper entry points now exist in D1 and D2 for d-tick cadence, object allocator plus homing scheduler state, and weapon sequence state. D2 also has a guided-missile rebinder that can reconstruct player guided pointers by scanning restored weapon objects.
- Validation after the helper pass: `run-windows-build.ps1 -Target d1`, `run-windows-build.ps1 -Target d2`, and `android/tests/test_input_demo_runtime_smoke.ps1` for both D1 and D2 all passed.
- Phase 2 is now in place in D1 and D2: the save versions were bumped, the deterministic runtime block is written and restored, both host builds passed again, scoped `android/run-code-quality.ps1 -Fix` passed for the touched files, and the existing D1 and D2 runtime smoke tests still passed.
- The known failing D2 checkpoint demo is not a valid phase 2 verdict because its embedded checkpoint blob was recorded before the save-version bump. A fresh checkpoint-backed `.dximdemo` recorded with the new build is still needed to exercise the new runtime block end to end.
- Phase 4 is now in place in D1 and D2: checkpoint recordings only carry DGSS bytes plus schema metadata, the shared parser rejects legacy timer-delta keys, and checkpoint replay now relies entirely on DGSS restore state instead of an `inferno.c` timer fallback.
- Validation after the strict phase 4 cutover: `test_input_demo_fixture`, `test_input_demo_recorder`, and `test_input_demo_replay` passed from `buildd1`, incremental `buildd1` and `buildd2` builds passed, and scoped `android/run-code-quality.ps1 -Fix -Paths ...` passed for the touched files.
- Phase 3 is now in place in D1 and D2: DGSS version 25/11 adds additive fidelity data for weapon `creation_framecount` plus full `hitobj_list[]`, active `morph_objects[]`, wall stuck-object state, and control-center transient timers. D2 also persists `Last_afterburner_time[]`.
- The save path no longer mutates live `creation_framecount` or force-finishes morph objects before writing, so checkpoint capture now preserves those transient systems instead of rewriting live state to fit old save behavior.
- Validation after the phase 3 pass: `run-windows-build.ps1 -Target d1` passed, D2 needed the known host-script fallback but `dxx-redux-d2` built successfully from the existing `buildd2` tree, and scoped `android/run-code-quality.ps1 -Fix -Paths ...` passed for the touched files.
- A follow-up AI fidelity pass is now in place in D1 and D2: DGSS version 26/12 adds pending AI awareness events plus live believed-player and last-fired-at positions so freshly recorded checkpoint saves preserve mid-fight robot context instead of dropping it.
- The old failing D2 replay demo from `android/temp_game_logs/d2_descent2_level1_20260427_111111.dximdemo` still does not pass because its embedded checkpoint bytes were recorded before the AI fidelity extension. Host replays of that old version still miss reactor destruction even after adding a narrow old-save clamp for impossible future `time_player_sound_attacked` values.
- Pre-release save-version handling is now collapsed again: D2 writes and reads the thumbnail, runtime, transient, and AI fidelity additions under version 23, and D1 does the same for versions 8 through 12 under version 8.

## Missing Or Incomplete State

### Critical For Checkpoint Replay

- Object allocator state: `num_objects` and the private `free_obj_list[]` stack in `object.c` are rebuilt in sorted order by `special_reset_objects()` after restore. Live recording continues with the original allocation stack, while replay continues with a reconstructed stack. Future object numbers can diverge even if the object array itself was saved.
- Tick scheduler state: `d_tick_count`, `d_tick_step`, and the static `timer` accumulator inside `calc_d_tick()` are not saved. `d_tick_count` drives AI cadence, reactor visibility checks, wall stuck-object cleanup, and other modulo scheduling.
- Homing scheduler state: `homerFrameCount`, `currentHomerFrameTime`, and `doHomerFrame` in `object.c` are not saved. Active weapon objects also lose `ctype.laser_info.creation_framecount` because `object_rw` does not contain it.
- RNG state: `d_rand_state` is not saved. Input demos currently restore RNG per frame, but normal saves and checkpoint starts should carry the engine RNG state. `d_rand_call_count` is diagnostic only and does not need to affect simulation.
- Weapon fire timers: `Next_laser_fire_time`, `Next_missile_fire_time`, `Last_laser_fired_time`, `Next_flare_fire_time`, and `Auto_fire_fusion_cannon_time` are not saved. Restore currently resets most of them to `GameTime64`; checkpoint metadata was added only because DGSS did not carry them.
- Fusion state: `Fusion_charge` and the static `Fusion_next_sound_time` inside `FireLaser()` are not saved. This affects fusion shot multiplier and overcharge damage timing.
- Primary fire pattern state: D1 `Spreadfire_toggle`; D2 `Spreadfire_toggle` and `Helix_orientation`. D2 currently keeps these as function statics inside `do_laser_firing_player()`, so saving them requires accessors or moving them to file-scope globals.
- Secondary fire pattern state: D1/D2 `Missile_gun`, D1/D2 `Proximity_dropped`, and D2 `Smartmines_dropped` are not saved. These affect alternating missile guns and multiplayer mine-drop reimbursement cadence.

### High Value Edge Cases

- D2 guided missile tracking: active guided missile objects are saved, but `Guided_missile[]` and `Guided_missile_sig[]` are not restored. Rebuild by scanning saved weapon objects or save object number/signature pairs.
- D2 omega timing: `Omega_charge` is saved, but `Last_omega_fire_time` is not. This affects omega recharge delay and fire cadence after restore.
- Persistent weapon collision memory: `laser_info.hitobj_list[]` is not saved; only `last_hitobj` is used to seed one entry on restore. Persistent weapons can damage objects differently after a mid-projectile save.
- Save-time mutations: `state_object_to_object_rw()` writes `obj->ctype.laser_info.creation_framecount = 0`, which mutates live state. The save path also finishes all morph objects before writing. Checkpoint capture should become non-mutating.
- Morph state: active `morph_objects[]` is not serialized; saves force morphs to completed objects. This is acceptable old save behavior, but it is not an exact checkpoint.
- Wall stuck-object state: `Num_stuck_objects` and `Stuck_objects[]` are not saved. This affects flares/weapons stuck in doors and cleanup when doors open.
- Control-center transient timers: D2 `Last_time_cc_vis_check` and static `controlcen_death_silence`, plus D1 static `controlcen_death_silence`, are not saved. The durable control-center state is saved, but these frame timers are not.

### Lower Priority Save Fidelity

- D2 weapon afterburner blob timing: `Last_afterburner_time[]` is not saved. Mostly visual, but spawned blobs consume object slots and can perturb object allocation.
- D1 palette flash state: D2 saves `Flash_effect`, `Time_flash_last_played`, and palette adds, but D1 only saves the screenshot palette and not the live flash fade state.
- D2 flickering light runtime state: `Num_flickering_lights` and `Flickering_lights[]` come from the level file and are not part of DGSS. `Light_subtracted[]` is saved, so this is mainly timer and enable-state fidelity.
- Recent-player visual bookkeeping such as shield delta/time/certainty is omitted from `player_rw`. This should stay low priority unless the HUD state itself becomes part of a regression signal.

## Implementation Plan

### Phase 1: Add Small State Accessors

- Add D1/D2 helpers for deterministic object allocator state: save and restore `num_objects` and `free_obj_list[]` without exposing the raw static array outside `object.c`.
- Add D1/D2 helpers for `calc_d_tick()` so the static accumulator can be saved and restored with `d_tick_count`.
- Add D1/D2 helpers for homing scheduler state in `object.c`.
- Add D1/D2 helpers for primary weapon sequence state, including D2's function-static spreadfire and helix values.
- Add D2 guided missile rebind helper that scans restored objects and repairs `Guided_missile[]` and signatures.

### Phase 2: Bump Save Versions And Persist Core Fields

- D2: bump `STATE_VERSION` from 23 to 24, leaving `STATE_COMPATIBLE_VERSION` at 20.
- D1: bump `STATE_VERSION` from 9 to 10, leaving `STATE_COMPATIBLE_VERSION` at 6.
- Append a version-gated deterministic-state block after existing non-coop fields and before Android coop trailer metadata.
- Store time-based fields as deltas from `GameTime64`, matching the current DGSS zero-based time convention.
- Read missing fields only when the new version is present; keep current defaults for old saves.

### Phase 3: Extend Object Serialization Carefully

- Add version-gated persistence for `laser_info.creation_framecount` and, if practical, `hitobj_list[]` for active `CT_WEAPON` objects.
- Remove or correct the live mutation of `obj->ctype.laser_info.creation_framecount` during save.
- Decide whether morph state is included now or left as a documented phase 2 save-fidelity improvement. If included, serialize `morph_objects[]` by object number and signature, not raw pointers.

Phase 3 status:
- Done for D1 and D2. The additive fidelity block now covers weapon hit history and framecount, morph runtime state, wall stuck objects, and control-center transient timers. D2 also covers afterburner blob timing.

### Phase 4: Remove Checkpoint Metadata Duplication

- Stop writing timer deltas into `.dximdemo` checkpoint metadata once enhanced DGSS is required for new input demos.
- Ignore backwards compatibility because this is pre-release.
- Keep `.dximdemo` metadata focused on demo schema, checkpoint bytes, and validation expectations, not duplicated gameplay state.

Phase 4 status:
- Done. Legacy checkpoint timing metadata has been removed from the shared schema and replay APIs, and old demos that still carry those fields are now rejected.

### Phase 5: Validate

- Add or extend a high-level integration test that saves a checkpoint, immediately restores it, and compares deterministic fields exposed by introspection or a small C test hook.
- Re-run the known failing D2 input demo in realtime and accelerated modes through `android/tests/run_input_demo_replay.ps1`.
- Run `run-windows-build.ps1 -Target d1` and `run-windows-build.ps1 -Target d2`.
- Run `android\run-code-quality.ps1 -Fix` after implementation and wait for it to exit fully.

