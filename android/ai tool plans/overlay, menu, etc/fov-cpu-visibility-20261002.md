# CPU-only baseline visibility for custom FOV

Status: implemented and validated

The previous main-view FOV override rendered a complete baseline view before the visual view. This implementation replaces the baseline draw with CPU projection, original portal traversal and object ordering, homing-list publication, automap discovery and demo records. Preserve base FOV, subviews and endlevel behavior. Keep shared logic in the existing render_gameplay_view service, with guarded D1/D2 integration

Audit: g3_start_frame_projection and render_setup_view already provide CPU-only setup. build_segment_list/build_object_lists are the existing CPU traversal services. do_render_object owns homing-list ordering and D2 demo-view exclusions. render_object emits object demo records, including an earlier morph-frame record in draw_morph_object. The CPU pass must retain those records and the visual pass must suppress duplicates. Lighting and model/sprite/texture preparation belong to the single visual pass

Validation: add opt-in comparison against the original baseline draw, checking ordered segments, homing candidates and automap discovery on the same frame. Verify one actual main scene pass with the existing MSAA pixel probe, across FOV values and depth modes, then rear/missile subview composition. Run serial D1/D2 integration on an available emulator, Android builds and both Windows builds. Preserve concurrent workspace changes and personal device data

The separately reported resolution/context GPU query error is outside this optimization

Implemented the shared CPU baseline collector and guarded D1/D2 hooks. It uses the original portal traversal and sorted object traversal, retains homing/automap/native-demo bookkeeping, and skips GPU frame setup, lighting and drawing. The visual pass draws the selected FOV once and suppresses duplicate demo records. The old baseline draw is available only through an explicitly enabled introspection comparison; it is off by default

Validation completed:

- Android debug APK built successfully for both games and all three ABIs; final build log: `android/temp/fov-cpu-final-build.txt`
- Both Windows builds and both `test_android_render_fov.exe` policy tests passed; host build log: `android/temp/fov-cpu-windows.txt`
- `test_fov_cpu_visibility.jsonc` passed all 104 steps for each game on emulator-5554. At 100, 110 and 120 degrees in both depth modes, the original and CPU paths matched ordered segment lists, ordered homing candidates and automap discovery. With comparison disabled, actual pixel probes passed with one scene pass and zero discarded baseline passes or scene errors. Evidence: `android/temp/graphics-safety-20261002-091055/results.txt`
- Extended MSAA integration passed for both games in RGB565 and RGBA8888: 96/96 steps per D1 run and 126/126 per D2 run, including 2x/4x MSAA, menu composition, wide-FOV main views and rear/missile subviews. Evidence: `android/temp/msaa-render-20261002-092611/` and `android/temp/fov-cpu-msaa-rerun.txt`
- Scoped formatting/lint completed successfully; logs: `android/temp/fov-cpu-quality.txt` and `android/temp/fov-cpu-final-quality.txt`

The first extended MSAA run failed before custom FOV because its slow diagnostic probe consumed the graphics confirmation deadline. Reordered the fixture to accept through D-pad/A before probing, and dispatch each acceptance as one controller sequence. The five-second product timeout is unchanged. The extended fixture now checks 100-degree single-pass rendering and 120-degree rear/missile viewport composition

No Retroid performance benchmark was run. Emulator visibility and pixel checks establish the tested behavior, not a measured frame-rate gain or a device-specific MSAA fix. Demo compatibility was verified in the follow-up below

Demo compatibility follow-up: verify both classic .dem and input .dximdemo formats. Audit frame/viewer/object/morph record placement and playback exclusions, record on Android through 100/110/120-degree CPU passes in both games, play each classic sidecar in its native Android engine and decode the D2 sidecar with the host reader, and replay each input recording on the host against its recorded final state. Add a reusable integration fixture and retain evidence under android/temp. No personal device data is involved

Demo compatibility follow-up completed:

- `run_fov_demo_tests.ps1` passed 81/81 steps for each game on emulator-5554. The fixture records movement and confirmed primary fire through FOV 100/110/120, verifies one scene pass while recording, installs the classic companion, plays it at all three FOV settings and waits for a normal return to the demo picker
- Both Android input recordings replayed on Windows with `-StrictComparison`: all final-result fields matched exactly. D1: 69 frames and 138 matching RNG events. D2: 68 frames and 1618 matching RNG events. A longer D1 firing run also matched all 189 frames' final state and 292 RNG events
- `verify_fov_demo_artifacts.py` checks the entire result, confirms recorded fire/movement, rejects truncated RNG traces and compares every RNG event field. It removes only this checkout's absolute prefix from source paths in memory; original files remain intact. The generic RNG comparer reported differences only in those Android-relative versus Windows-absolute source paths
- The D2 desktop classic reader decoded all 70 frames without truncation, with up to 62 objects per frame. Native D1 classic playback was checked on Android; the D2 dump command intentionally does not support native D1 .dem files
- Final evidence: `android/temp/fov-demo-20261002-100512/{d1,d2}/`, including Android logs, original three companion files, desktop replay results, RNG traces and verification.json. Combined Android log: `android/temp/fov-demo-complete.txt`

The audit found no changes to demo versions, event IDs or serialized layouts. The CPU pass calls the existing frame/viewer/object/morph writers under their original recording conditions, and the visual pass suppresses duplicates. Playback continues through the existing readers. No additional engine changes were needed

The reusable fixture selects the existing pilot on its second launch and waits for the actual demo-picker window, since Screen_mode can remain game after playback. Its end-of-playback allowance is 120 seconds so a slow recorded frame rate does not cause a false timeout. The wrapper supplies unique installed names and exports managed payloads, which remain stable after launch projection moves materialized demo files

To repeat the Android round trip and export companions, run `pwsh android/helpers/run_fov_demo_tests.ps1`. For each exported game directory, run the existing `android/tests/run_input_demo_replay.ps1` with the exported `fov_cpu_compat.dximdemo`, `-StrictComparison`, `-Mode accelerated`, `-RenderProfile default`, `-ResultCopyPath <directory>/replay-result.json` and `-RngLogPath <directory>/replay-rng.jsonl`. Use `-Runner windowed-no-present` for D1 or `-Runner headless-console` for D2. Then run `python android/helpers/verify_fov_demo_artifacts.py <directory>`. The generic `-TraceRng` / `-CompareRngTrace` switches are unnecessary here because the artifact verifier handles the source-path difference without relaxing RNG comparisons

These are targeted record/playback and cross-platform determinism checks, not an exhaustive run of every historical demo. Morphing-robot, multiplayer and long campaign demo scenarios were not exercised by this fixture
