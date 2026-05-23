# on-device input replay and forcefield animation sync -- 2026-05-05

Goal:
- plan an on-device launcher path to play `.dximdemo` input replays directly from the advanced replay section
- review whether `force01#0-7` animated transparency timing can explain the frame-109 transparent-wall replay split, and propose a sync strategy if it can

Plan:
- [x] map the current advanced replay UI and staged/installed replay model
- [x] map the existing Android intent/native command-line handoff for `-inputdemo-replay`
- [x] design a launcher replay-play action that launches the selected `.dximdemo`, not the classic `.dem`
- [x] trace animated texture/frame timing for `force01#0-7` in record and replay
- [x] compare that path to the known frame-109 seg 254 side 4 transparent-wall evidence
- [x] propose a deterministic animation-sync approach and test coverage

Notes:
- start with planning and code review only; do not implement launcher changes until the plan is reviewed

Implementation progress:
- [x] add D2 effect helpers to derive loop phase and reapply texture/object mappings
- [x] add D2 runtime save/load support for effect loop time plus compact runtime overrides
- [x] mirror the same effect runtime save/load behavior into D1
- [x] validate incremental host builds for `dxx-redux-d2` and `dxx-redux-d1`
- [x] run scoped `android/run-code-quality.ps1 -Fix` on the touched engine files
- [x] study savegame `GameTime64` zeroing and confirm it is legacy compatibility from the 2014 `GameTime` to `GameTime64` transition, not a gameplay requirement
- [x] implement direct Play support for staged `.dximdemo` files in the launcher advanced page
- [x] validate launcher Kotlin compile and scoped code quality pass
- [ ] add a replay/save regression test that exercises a checkpointed animated wall or forcefield

GameTime64 study note:
- Saving the real `GameTime64` would be conceptually simpler for new-format saves, but it is not a drop-in change. The on-disk header slot is still the legacy 32-bit `fix`, object/player save structs still serialize many timer fields as 32-bit deltas relative to save-time, and checkpoint restore currently adds `input_demo_replay_checkpoint_start_gt()` because save files intentionally store a zero-based level time. Changing that now would require another coordinated save-format/read-path change beyond the focused effect-state work.

Launcher replay-play plan:
- Current UI: `AdvancedSettingsPage.kt` shows staged input demos from `InputDemoManager.listStagedDemos(filesDir)` in `RecordedInputDemosSection`. Each row has Save, Share, Add to Game, and Delete. `StagedInputDemo.file` is the `.dximdemo`; `classicDemoFile` is the optional `.dem` sidecar and must not be used for this feature.
- Current handoff: `SetupActivity.createGameLaunchIntent(game)` already builds a `MainActivity` intent, and `jni_main.c` already reads `input_demo_replay` and starts native `main()` with `-inputdemo-replay <path>`. Emulator logcat confirmed this reaches native replay startup.
- Add a per-demo Play action in the advanced replay section. Thread a callback from `SetupActivity` through `AdvancedSettingsPage` to `RecordedInputDemosSection`, e.g. `onPlayInputDemo: (StagedInputDemo) -> Unit`.
- In `SetupActivity`, add a helper that launches a selected input demo by game type. It should perform the same game-prep work as the normal launch path: write active set path, playlist, enabled mod paths, initial config, and music config, then start `MainActivity` with `game = demo.game` and `input_demo_replay = demo.file.absolutePath`.
- Do not install or delete the staged demo just to play it. Do not point at `.dem`. The replay result will naturally be written beside the selected `.dximdemo` by `actual_result_path_from_demo_path()`.
- Disable or reject Play with a clear launcher message if required data for that game is not ready. Keep this logic in launcher/Kotlin; native replay startup can stay unchanged.
- Test plan: add a launcher-level test or automation hook that stages a small `.dximdemo`, taps Play, and verifies the game intent carries `input_demo_replay`. Then run a by-hand/emulator replay using a known short replay. The current level-4 `183034` replay is still blocked on Android by the existing control-center checkpoint restore assertion, so it is not a clean UI acceptance test yet.

Forcefield animation sync analysis:
- The transparent texture in the failing shot path is `force01#0-7`, with base texture id 420 in the observed seg 254 side 4 path. `check_trans_wall()` samples `Textures[side->tmap_num]` when `tmap_num2 == 0`, so gameplay pass/fail depends on the current animated `Textures[420]` mapping, not just the side's stored `tmap_num`.
- `do_special_effects()` advances `Effects[]` using `FrameTime`, updates `ec->time_left` and `ec->frame_count`, and writes `Textures[ec->changing_wall_texture] = ec->vc.frames[ec->frame_count]`. `ECLIP_NUM_FORCE_FIELD` is 78, and this is the expected eclip for the forcefield animation.
- Replay frame timing after start is deterministic: `input_demo_prepare_replay_frame()` sets `FrameTime` from the recorded frame before `GameProcessFrame()` runs.
- The suspect gap is initial phase. The input-demo checkpoint uses `state_save_all_sub()` / `state_restore_all_sub()`. The reviewed state path restores side tmap values and runtime state, but does not serialize `Effects[].frame_count` or `Effects[].time_left`. Level setup calls `reset_special_effects()`, which reapplies the current default frame rather than the recorder's checkpoint phase.
- This is a good explanation for the frame-109 split: the same shot, same side, same UV, and same stock bitmap sampling can see a transparent texel in the recorder and an opaque texel in replay if `Textures[420]` points at a different `force01#N` frame.

Preferred sync strategy:
- Split effect state into two buckets.
- Bucket 1, derived loopers: effects that continuously loop from level start and have no runtime-only activation state should be reconstructed from elapsed level time. That includes the forcefield case. To support ordinary savegames, store an explicit effect-loop time anchor in the runtime state trailer because the legacy save path intentionally writes `GameTime64 = 0` to disk for compatibility. Input-demo checkpoints can still use their checkpoint `start_gt`, but the save/load path needs its own stored elapsed-time anchor.
- Bucket 2, saved overrides: effects whose state is changed by gameplay at runtime should be serialized explicitly in the save/runtime format. This includes active one-shots (`EF_ONE_SHOT`, `segnum/sidenum`, current `dest_bm_num`), stopped effects (`EF_STOPPED`), and any effect whose current `frame_count` or `time_left` differs from the deterministic derived loop state.
- Restore flow: after walls, doors, cloaking walls, and `GameTime64`-relative runtime state are restored, rebuild all looping effects from the saved effect-loop time anchor, then apply the compact override list, then reapply the resulting `Textures[]` and `ObjBitmaps[]` mappings. This preserves looping forcefields without writing hundreds of per-texture entries and still handles triggered animations correctly.
- Doors do not need new work here: partially open doors and cloaking walls already save through `ActiveDoors` and `CloakingWalls`. The missing piece is `Effects[]`, not door progress.
- Implement D2 first and mirror the same save/runtime semantics in D1 so both games keep the same behavior for savegames and checkpoint restores.

Validation plan:
- Add targeted temporary logging or state-trace fields around checkpoint capture, replay restore, and `check_trans_wall()` for seg 254 side 4: effect index 78, `frame_count`, `time_left`, `changing_wall_texture`, `Textures[420]`, sampled bitmap name/frame, UV, pixel, and pass result.
- Re-run the host replay for `android/temp_game_logs/d2_descent2_level4_20260505_183034.dximdemo`. Before the fix, confirm recorder/replay differ in forcefield frame at or before the frame-109 wall sample. After the fix, frame 109 should either match or the first divergence should move later.
- Add a high-level regression replay that starts from a checkpoint near an animated forcefield and fires through it. The assertion is that the same shot pass/fail result and state trace match on replay.
- Re-run `android/run-code-quality.ps1 --fix`, then the relevant host CMake build/test path. Android on-device replay verification should follow once the control-center checkpoint restore assertion is fixed or bypassed with a replay that does not hit it.
