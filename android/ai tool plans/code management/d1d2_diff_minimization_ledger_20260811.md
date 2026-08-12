# D1/D2 Diff-Minimization Ledger, 2026-08-11

## Round snapshot

- Round ID: `DMR1`
- Branch: `cmake`
- Survey head: `0498798fc927581626c3f5978e219c68e64990c0`
- Target ref at survey: `upstream/main`
- Target tip: `6e76c5d8dafd02dc32a1c6312ce9440dc95b1aca`
- Merge base: `fb555eec75e1ed12c8348805ab335afb4c721b06`
- Process: `d1d2_diff_minimization_worker_process.md`
- Round plan: `plan_d1d2_diff_minimization_chunked_round_20260811.md`
- Worker policy: one fresh `gpt-5.6-sol` worker at medium reasoning effort per implementation chunk, no concurrent product writers

The survey head and target tip identify the starting repository state. Implementations use the live worktree and record per-chunk before and after metrics. If either ref or D1/D2 live content moves outside campaign work, append a new survey generation instead of replacing this snapshot

## Starting worktree ownership boundary

These changes existed before the campaign and are not owned by diff-minimization workers:

- `android/app/src/main/java/com/dxxredux/app/AdvancedSettingsPage.kt`
- `android/app/src/main/java/com/dxxredux/app/ImportLocationManager.kt`
- `android/app/src/main/java/com/dxxredux/app/SetupSections.kt`
- `android/app/src/test/java/com/dxxredux/app/ImportLocationMigrateTest.kt`
- `android/app/src/test/java/com/dxxredux/app/TouchEditorZoneEdgeTest.kt`
- `android/game_scripts/test_obsidian_level1_objective_markers.json5`
- `android/ai tool plans/code management/plan_next_30_local_correctness_fixes_20260811.md`
- `android/ai tool plans/gameplay/obsidian_level2_post_blue_grate_skip_20260811.md`
- `android/ai tool plans/metadata/obsidian_sng_track_list_view_20260811.md`
- `android/ai tool plans/ui/level-metadata-restore-flash.md`

Campaign-owned planning files added by the root orchestrator are this ledger, the process, and the round plan. Workers must preserve every other pre-existing path unless a later ledger row explicitly records user authorization and overlap handling

The following unrelated paths appeared or changed concurrently during the survey and are also outside campaign ownership:

- `android/app/src/main/cpp/extract/game_file_extensions.c`
- `android/app/src/main/cpp/shared/route_planner.cpp`
- `android/app/src/main/java/com/dxxredux/app/AssetManifest.kt`
- `android/app/src/main/java/com/dxxredux/app/GameFileFormats.kt`
- `android/app/src/main/java/com/dxxredux/app/KnownVersions.kt`
- `android/app/src/main/java/com/dxxredux/app/MissionZipMusic.kt`
- `android/app/src/main/java/com/dxxredux/app/ModManager.kt`
- `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt`
- `android/app/src/main/java/com/dxxredux/app/SetupGameFiles.kt`
- `android/app/src/test/java/com/dxxredux/app/AndroidGameFileExtensionsTest.kt`
- `android/app/src/test/java/com/dxxredux/app/AssetManifestTest.kt`
- `android/app/src/test/java/com/dxxredux/app/GameFileFormatsTest.kt`
- `android/app/src/test/java/com/dxxredux/app/MissionZipMusicDisplayTest.kt`
- `android/app/src/test/java/com/dxxredux/app/MissionZipMusicExtractedPreviewTest.kt`
- `android/app/src/test/java/com/dxxredux/app/MissionZipMusicTest.kt`
- `android/app/src/test/java/com/dxxredux/app/ModManagerMissionZipTest.kt`
- `android/app/src/test/java/com/dxxredux/app/SetupLaunchReadinessTest.kt`
- `android/tests/test_route_snapshot.cpp`

## Starting metrics

### Branch-attribution view

Comparison: merge base `fb555eec75e1ed12c8348805ab335afb4c721b06` to survey head `0498798fc927581626c3f5978e219c68e64990c0`

- All D1/D2 paths: 355 files, `+52570/-4359`
- Inherited modified paths: 318 files, `+36324/-4359`
- Branch-added sink paths: 37 files, `+16246/-0`

### Integration-pressure view

Comparison: `upstream/main` at `6e76c5d8dafd02dc32a1c6312ce9440dc95b1aca` to the starting live worktree

- All D1/D2 paths: 362 files, `+52729/-5055`
- Inherited modified paths: 325 files, `+36483/-5055`
- Branch-added sink paths: 37 files, `+16246/-0`

The seven-path and deletion-count differences between the views include upstream movement after the branch split. Use the branch-attribution view to decide whether the branch caused a change and the integration-pressure view to measure current merge cost

## Survey generation 2: external HEAD advance during Chunk 002

- New HEAD: `34ed94767d2a2dbca3e07dd1ba672be467cfb3f1`
- Target tip remains: `upstream/main` at `6e76c5d8dafd02dc32a1c6312ce9440dc95b1aca`
- Merge base remains: `fb555eec75e1ed12c8348805ab335afb4c721b06`
- The external commit advanced HEAD while the Chunk 002 worker was validating. It contains both accepted diff-minimization chunks plus unrelated pre-existing and concurrent work. The worker did not create the commit, and this ledger does not attribute the unrelated paths to either chunk
- Stable HEAD branch-attribution view: 318 inherited modified paths at `+36164/-4360`, plus 37 branch-added sinks at `+16246/-0`
- Stable HEAD integration-pressure view: 325 inherited modified paths at `+36323/-5056`, plus 37 branch-added sinks at `+16246/-0`
- Net stable-HEAD movement from generation 1: 160 fewer inherited additions and one more inherited deletion. The two accepted chunks account for 175 isolated inherited additions removed; their paired native-test registrations and concurrent changes to `d1/arch/sdl/gr.c` and `d2/libmve/mveplay.c` explain why the whole-generation metric is not the sum of chunk reductions
- D1/D2 paths changed between generation heads: the paired OGL sources, paired render sources, paired maths CMake test registrations, `d1/arch/sdl/gr.c`, and `d2/libmve/mveplay.c`
- Immediately after the head advance, concurrent work added one live line to each of `d1/main/state.c` and `d2/main/state.c`. The live-worktree integration-pressure view therefore became `+36325/-5056` for inherited paths while branch-added sinks remained `+16246/-0`. These state paths and their adjacent Android coop/save/test changes are outside campaign ownership
- Chunk 003 anchors are unaffected and remain eligible after root acceptance, but the single-writer rule prohibits dispatch while another product writer is actively changing the shared worktree

## Survey evidence

- Current top-100 integration-pressure report: `temp/d1d2_diff_summary.txt`
- Full current numstat: `temp/d1d2_diff_numstat.txt`
- Sorted current numstat: `temp/d1d2_diff_sorted.txt`
- Independent Sol-medium survey: `temp/diff_minimization_20260811/worker_survey.md`
- Root paired-addition scan found the largest exact or near-exact D1/D2 branch additions in OGL, newmenu, state, net UDP, kconfig, joystick, config, HUD/gauges, event, and smaller persistence and menu tails. Exact textual overlap is candidate evidence only; it does not override the ownership and coupling rules

## Queue states

- `TODO`: eligible after prerequisites
- `ACTIVE`: owned by exactly one worker
- `DONE`: implemented and validated with exact metrics
- `REJECTED`: inspected and shown not to improve ownership or clarity
- `DEFERRED`: valid candidate with a named prerequisite or live overlap
- `BLOCKED`: cannot proceed without missing authority or unavailable evidence

## Chunk queue

| ID | State | Rank | Scope | Expected inherited reduction | Risk | Prerequisite | Result |
|---|---|---:|---|---:|---|---|---|
| `DMR1-CHUNK-001` | `DONE` | 1 | Paired OGL lookup and profiling helpers | 84-90 | Low-medium | Typed callback and API-size gate | 95 inherited additions removed; focused contracts and configured builds green |
| `DMR1-CHUNK-002` | `DONE` | 2 | Paired main-view FOV policy | 70-82 | Low-medium | Chunk 001 accepted; no frame-orchestration movement | 80 inherited additions removed; focused policy and configured builds green |
| `DMR1-CHUNK-003` | `TODO` | 3 | Paired debug texture-overlay drawing | 60-64 | Low | Chunk 002 accepted | Pending |
| `DMR1-CHUNK-004` | `TODO` | 4 | Paired virtual-gamepad registration | 100-125 | Medium | Batch 1 green; adapter no more than about 40-45 lines | Pending |
| `DMR1-CHUNK-005` | `TODO` | 5 | Paired scene-object profiler scan | 52-56 | Low-medium | Batch 1 green; no second scan or allocation | Pending |
| `DMR1-CHUNK-006` | `TODO` | 6 | Remaining D2 input-demo helper residue | 50-58 | Medium | Earlier chunks green; exact replay ordering retained | Pending |
| `DMR1-CHUNK-007` | `DEFERRED` | 7 | D2 direct-restore slot parser | 28-31 | Low | Coherent state-adjacent reason; below standalone threshold | Threshold deferral |
| `DMR1-CHUNK-008` | `DEFERRED` | 8 | Paired last-player retention predicates | 24-28 | Low | Coherent adjacent config work | Threshold deferral |
| `DMR1-CHUNK-009` | `DEFERRED` | 9 | Paired network resync request mechanism | 35-45 | High | Separate correctness reason and deterministic host-loss coverage | Risk/payoff deferral |
| `DMR1-AUDIT-001` | `TODO` | 10 | Final residual path accounting and rerank | 0 | Low | Last accepted implementation chunk | Pending |

## Per-chunk instructions

Every worker must follow `d1d2_diff_minimization_worker_process.md`, use the live worktree, and preserve the ownership boundary above. Line numbers are survey anchors, not permission to rewrite adjacent code. A worker may reject its chunk after proving that the boundary or payoff gate fails. It must not substitute another chunk.

### DMR1-CHUNK-001: paired OGL lookup and profiling helpers

- Inherited sources: `d1/arch/ogl/ogl.c:3003-3049` and `d2/arch/ogl/ogl.c:3057-3103` at the survey head.
- Move only `android_profile_texture_lookup_note_ktx2` and `ogl_read_texture_with_extensions` into their natural existing owners: `android/app/src/main/cpp/shared/ogl_texture_android.c`, `ogl_texture_android.h`, `android_profile.c`, and `android_profile.h`.
- A typed game-prefixed PNG reader callback is allowed only if its total declaration and adapter surface is materially smaller than the two removed bodies.
- Do not move or reshape the interleaved KTX2/ETC2 upload transaction, the completed DXA mask callback, renderer state, or unrelated OGL code.
- Preserve KTX2 then PNG/JPG/TGA lookup order, candidate naming, hit-slot and extension metrics, timing buckets, success and failure returns, and Android guards exactly.
- Allowed validation support: a narrowly named new test under `android/tests/`, or focused additions to `android/tests/test_android_renderer_contracts.py`, only if it does not overlap unrelated work.
- Required evidence: isolated before/after numstat for both inherited files, focused lookup or source-contract coverage, D1 and D2 Windows build, configured Android ABI builds, scoped quality for touched files, and `git diff --check`. Record unavailable expensive integration cases rather than claiming them.

### DMR1-CHUNK-002: paired main-view FOV policy

- Inherited sources: `d1/main/render.c:100-150` and `d2/main/render.c:111-161` at the survey head.
- Create `android/app/src/main/cpp/shared/android_render_fov.c` and `.h` for the identical state, clamping, FOV-to-zoom mapping, and set/get/lock/effective accessors. Pass base zoom as data.
- Update only the relevant Android target source lists in `android/app/src/main/cpp/CMakeLists.txt` if required.
- Keep `Android_visual_only_render_pass`, render-list setup, zoom override, D2 `Window_rendered_data`, `window_num`, endlevel behavior, and two-pass frame ordering local.
- No callback table is allowed. Preserve FOV 0, 100, 110, 120, invalid clamp, persisted preference, and endlevel behavior.
- Allowed validation support: a new focused C test under `android/tests/` and only the minimum existing runner registration needed for that test.
- Required evidence: isolated metrics, focused policy tests, D1/D2 render smoke where available, both Windows builds, configured Android ABIs, scoped quality, and `git diff --check`.

### DMR1-CHUNK-003: paired debug texture-overlay drawing

- Inherited sources: the Android blocks in `d1/main/gamerend.c:523-555` and `d2/main/gamerend.c:974-1006` at the survey head.
- Add one narrow draw entry point to `android/app/src/main/cpp/shared/android_texture_debug.c` and `.h`, leaving one compact call at each inherited render site.
- Do not move general game rendering, merged-wall capture policy, overlay state production, or introspection.
- Preserve draw ordering, text and coordinates, base and hires labels, mode gates, and font RGB override restoration exactly.
- Allowed validation support: focused additions to `android/tests/test_android_renderer_contracts.py` or one narrowly named new test.
- Required evidence: isolated metrics, overlay/source-contract coverage, D1/D2 render smoke where available, both Windows builds, configured Android ABIs, scoped quality, and `git diff --check`.

### DMR1-CHUNK-004: paired virtual-gamepad registration

- Inherited sources: `d1/arch/sdl/joy.c:297-373` and `d2/arch/sdl/joy.c:294-370` at the survey head.
- First prototype, without product edits, a compact shared descriptor or initializer for the eight base axes, ten buttons, twelve axis buttons, three combiner axes, and four D-pad buttons.
- Allowed product paths after the gate passes: the two inherited `joy.c` files, a narrowly named new `.c/.h` owner under `android/app/src/main/cpp/shared/`, and the minimum Android target source-list changes.
- Reject the chunk if the combined game-local adapters exceed about 40-45 lines, expose the whole private `Joystick` or `SDL_Joysticks` layout, or obscure the Kotlin index contract.
- Preserve every index, range, deadzone, source, combiner relationship, D-pad mapping, and desktop SDL behavior.
- Required evidence: isolated metrics, descriptor fixtures, D-pad/axis/combiner/touch automation where available, both Windows builds, configured Android ABIs, scoped quality, and `git diff --check`.

### DMR1-CHUNK-005: paired scene-object profiler scan

- Inherited sources: `d1/main/game.c:172-201` and `d2/main/game.c:180-209` at the survey head.
- Put the scan in `android/app/src/main/cpp/shared/android_profile.c/.h` or one narrowly named engine-aware profiling source, compiled with the correct game headers, with one call from each inherited source.
- Pass a span and minimal ownership context if direct engine compilation is not clean. Do not add callbacks, allocation, a second traversal, or changed object ordering.
- Move no simulation, object lifecycle, or frame policy. Counter meaning and per-frame timing must remain exact.
- Required evidence: isolated metrics, focused active/projectile/reactor/remote-robot counter coverage, frame-time smoke where available, both Windows builds, configured Android ABIs, scoped quality, and `git diff --check`.

### DMR1-CHUNK-006: remaining D2 input-demo helper residue

- Inherited sources: `d2/main/fvi.c:815-858` and `d2/main/collide.c:113-128` at the survey head.
- Move only the boundary-probe gate/logger and homing-player-bump environment gate into branch-added `d2/main/input_demo_hooks.c/.h`.
- Do not combine this work with general sink cleanup, demo-format change, or simulation behavior change.
- Preserve every enable gate, event order, RNG-neutral behavior, message field, environment predicate, and call-site observation.
- Required evidence: isolated metrics, known boundary and homing-bump replays with exact final-state and RNG comparison, D2 Windows build, configured Android ABIs, scoped quality, and `git diff --check`.

### DMR1-CHUNK-007: D2 direct-restore slot parser

- Inherited source: `d2/main/state.c:2662-2693` and its call near survey line 2750.
- Natural owner: `android/app/src/main/cpp/shared/state_android_shared.c/.h`.
- This remains deferred unless a coherent state-adjacent chunk or correctness reason lifts it above the stopping threshold. It must never justify broad save serialization edits.
- Preserve null and malformed handling, case behavior, accepted `.sg0` through `.mg9` suffixes, extra-suffix rejection, logging categories, and direct-restore selection.
- Required evidence if activated: table-driven parser coverage, D2 save/restore integration, D2 Windows and Android builds, scoped quality, and `git diff --check`.

### DMR1-CHUNK-008: paired last-player retention predicates

- Inherited sources: `d1/main/config.c:100-116` and `d2/main/config.c:117-133` at the survey head.
- Move only `android_saved_last_player` and `android_should_keep_saved_last_player` to an existing natural config or `android/app/src/main/cpp/shared/net/auto_net.c/.h` owner if adjacent work makes the extraction worthwhile.
- Keep D1/D2 first-run defaults and music defaults local. Do not create a new subsystem for two small predicates.
- Required evidence if activated: empty, coop-autosave, transient auto-net, and ordinary pilot-name cases plus desktop config read/write and Android first-launch coverage.

### DMR1-CHUNK-009: paired network resync request mechanism

- Inherited anchors: D1 `d1/main/net_udp.c:84-85,3480-3511,5480,6484-6512` and D2 `d2/main/net_udp.c:92-93,3526-3557,5597,6635-6663` at the survey head.
- Deferred by default. Activate only for a separate correctness reason after deterministic host-loss and reconnect coverage exists and a concrete API proves smaller than the local mechanism.
- Do not move packet layout, host election, transport policy, sync reset policy, disconnect behavior, or timers merely to improve numstat.
- Required evidence if activated: two-emulator host loss and rejoin for both games, throttle and elected-master addressing checks, sync abort, both Windows builds, configured Android ABIs, scoped quality, and `git diff --check`.

### DMR1-AUDIT-001: final residual accounting

- Use a fresh read-only Sol-medium worker after the last accepted implementation chunk.
- Regenerate the branch-attribution and integration-pressure inventories and account for every inherited modified D1/D2 path and every branch-added D1/D2 sink by completed chunk, retained-policy family, stale/superseded decision, or below-threshold batch.
- Write a generation-stamped report under `temp/diff_minimization_20260811/` and append exact final metrics and residual decisions to this ledger.
- This audit may propose a new ledger generation, but it may not edit product code or silently reactivate deferred chunks.

## Coverage classifications

The initial full-diff survey assigns the live surface as follows. `DMR1-AUDIT-001` will repeat the mechanical path-by-path accounting after implementation changes.

- Active clean seams: chunks 001 through 006 above.
- Threshold seams: chunks 007 and 008, remaining playlist hooks, Android fatal call sites, and similar roughly 20-40-line combined residues.
- High-coupling retained policy: D2 escort and route behavior, broad D1/D2 state serialization, network packets and host policy, private newmenu geometry, frame orchestration, joystick private layout when the adapter gate fails, and interleaved texture upload transactions.
- Branch-added sinks: D1/D2 `input_demo_hooks.c`, D2 D1-in-D2 implementation and save translation sources, DXA metadata patching, and D1 custom support. These are destinations or feature owners, not inherited-conflict rankings.
- Completed or superseded historical candidates: broad input-demo extraction, playsave bridge, coop restore remapping, DXA mask callback repair, shader log helpers, playlist ownership, Android fatal handling, newmenu callback cleanup, and earlier broad OGL, coop, songs, HMP, EGL, font, effects, game-control, host-migration, classic-demo, and direct-command campaigns.
- Final residual batch: all other modified inherited paths whose branch-owned seam is below the stopping threshold or whose changes are substantive engine/game behavior rather than misplaced Android ownership.

## Chunk completion record template

### DMR1-CHUNK-000 completion

- Status:
- Worker: `gpt-5.6-sol`, medium reasoning
- Boundary decision:
- Inherited paths:
- Destination and support paths:
- Before metrics:
- After metrics:
- Isolated inherited reduction:
- Behavior and ownership preserved:
- Validation:
- Limitations:
- Out-of-scope worktree check:
- Residual and rerank:

### DMR1-CHUNK-001 completion

- Status: `DONE`
- Worker: `gpt-5.6-sol`, medium reasoning
- Boundary decision: Gate passed before product edits. Each inherited file contained the same 46 lines of helper implementation, 92 lines total. Moving the KTX2 metric update to `android_profile` and the extension lookup to `ogl_texture_android` required two typed public declarations totaling 6 formatted lines and no game-local adapter or callback because the existing shared OGL owner already uses the common `png_data` and `read_png` ABI. This is materially smaller than either inherited implementation and their combined duplicate surface
- Inherited paths: `d1/arch/ogl/ogl.c`, `d2/arch/ogl/ogl.c`
- Destination and support paths: `android/app/src/main/cpp/shared/ogl_texture_android.c`, `android/app/src/main/cpp/shared/ogl_texture_android.h`, `android/app/src/main/cpp/shared/android_profile.c`, `android/app/src/main/cpp/shared/android_profile.h`, and focused additions to `android/tests/test_android_renderer_contracts.py`
- Before metrics: integration pressure against `upstream/main`: D1 `+1924/-77`, D2 `+2017/-76`; isolated duplicated helper bodies: D1 46 lines, D2 46 lines, 92 total
- After metrics: integration pressure against `upstream/main`: D1 `+1877/-77`, D2 `+1969/-76`. Isolated worktree delta in the inherited files is D1 `+2/-49`, D2 `+3/-51`. Shared product owners gained 71 lines total and the focused contract test gained 28 lines
- Isolated inherited reduction: 95 additions, comprising 47 in D1 and 48 in D2. The one-line difference is an extra separator blank in the D2 source. The 92 implementation lines moved are the same 46-line helper pair in each game
- Behavior and ownership preserved: KTX2 lookup calls and their set, prefix, and base order remain at the original transaction sites. PNG-family lookup still tries `.png`, `.jpg`, then `.tga`; preserves candidate construction, one attempt count per slot lookup, slot and extension timing accumulation, hit fields, early success `1`, terminal failure `0`, and the existing outer timing/cache buckets. The shared monotonic clock helpers are behavior-identical to the inherited helpers: `clock_gettime(CLOCK_MONOTONIC, ...)` and the same seconds-times-1000000 plus nanoseconds-divided-by-1000 arithmetic. Android guards remain exact and desktop code is unchanged
- Validation: `python -m unittest android.tests.test_android_renderer_contracts` passed 6 tests after final formatting; the focused contract checks single ownership, paired call sites, extension order, metric ordering, and clock equivalence. Scoped `android/run-code-quality.ps1 -Fix` passed for all four shared C/header paths; the test path was included and had no applicable formatter stage. `run-windows-build.ps1 -Target both` completed for D1 and D2 with Ninja reporting no remaining work. With JDK 21, `android/gradlew.bat :app:externalNativeBuildDebug --console=plain` passed D1/D2 native configuration and build for `arm64-v8a`, `armeabi-v7a`, and `x86_64`. Final `git diff --check` passed, with only warnings about pre-existing CRLF paths
- Limitations: No emulator texture-pack smoke was run. There is no narrow runtime fixture for the asset-dependent KTX2-to-PNG fallback; source-contract coverage plus linking both games across all configured Android ABIs exercised the moved boundary without entering the explicitly excluded upload transaction
- Out-of-scope worktree check: Initial and final status were captured. Allowed paths were clean at claim time. All pre-existing dirty paths, including every unrelated modified test listed in the ownership boundary, were preserved untouched. Concurrent changes also appeared in `android_autoselect.cpp`, `android_resume_pilot.c`, `android_save_set.c`, `coop/coop_save.c`, `digi_tsf_music.c`, `rbaudio_bin.c`, `state_android_shared.c`, `MusicPickerPage.kt`, `SetupDialogs.kt`, `SetupFileImport.kt`, `test_android_save_set.c`, `test_autoselect_order_validation.py`, and `d2/libmve/mveplay.c`; this worker did not edit, format, revert, stage, or delete them. The final chunk diff is limited to the two inherited OGL sources, four named shared owners, the allowed renderer contract test, and this ledger section
- Residual and rerank: DMR1-CHUNK-002 is now the next eligible item. Its prerequisite is satisfied, and no frame-orchestration code moved in this chunk
- Root acceptance: accepted after review of the complete scoped diff, independent confirmation of D1 `+1877/-77` and D2 `+1969/-76` against `upstream/main`, a clean scoped `git diff --check`, and an independent rerun of the six-test renderer contract suite. No out-of-scope changes were attributed to the chunk

### DMR1-CHUNK-002 completion

- Status: `DONE`
- Worker: `gpt-5.6-sol`, medium reasoning
- Boundary decision: Gate passed before product edits. The paired sources contain identical 51-line Android blocks at the survey anchors. The movable seam is the two FOV state variables plus clamping, base-zoom mapping, and four existing engine-facing accessors, while the visual-only flag, zoom override, active override selection, render-list storage, endlevel gate, and two-pass orchestration remain local. The shared API needs the four existing accessor declarations plus one typed `int32_t` mapping declaration that accepts base zoom as data, with no callback or game-local adapter. This five-function boundary is materially smaller than the duplicated policy bodies and does not expose render internals
- Before metrics: integration pressure against `upstream/main`: D1 `+279/-28`, D2 `+431/-28`; the allowed paths had no pre-existing worktree changes at claim time
- Inherited paths: `d1/main/render.c`, `d2/main/render.c`
- Destination and support paths: `android/app/src/main/cpp/shared/android_render_fov.c`, `android/app/src/main/cpp/shared/android_render_fov.h`, the two relevant entries in `android/app/src/main/cpp/CMakeLists.txt`, focused `android/tests/test_android_render_fov.c`, and its minimum registrations in `d1/maths/CMakeLists.txt` and `d2/maths/CMakeLists.txt`. Existing `d1/main/render.h` and `d2/main/render.h` ABI declarations remained unchanged
- After metrics: integration pressure against `upstream/main`: D1 `+239/-28`, D2 `+391/-28`. The isolated inherited-file delta is D1 `+4/-44` and D2 `+4/-44`. The new shared implementation is 45 lines, its header is 12 lines, the focused test is 53 lines, Android target wiring gained 2 lines, and each native test runner gained 6 lines
- Isolated inherited reduction: 80 additions, 40 from each inherited renderer. Both renderers replace the duplicated policy body with one guarded include and pass `Render_zoom` to the shared mapping call
- Behavior and ownership preserved: FOV 0 keeps the caller-provided base zoom; 100, 110, and 120 still map to 43940, 52658, and 63858; every other value still clamps to 0. Locking still normalizes any nonzero input, suppresses only the effective FOV, and preserves the stored preference for unlock. `Android_visual_only_render_pass`, `Android_render_zoom_override`, active override selection, render-list capture and restoration, D2 `Window_rendered_data` and `window_num`, the endlevel gate, and two-pass ordering remain local and unchanged. No callback table or render-private structure was introduced
- Validation: the registered `test_android_render_fov` CTest passed independently in both D1 and D2 builds, covering 0/100/110/120 mappings, negative and unsupported clamping, nonzero lock normalization, persisted preference, unlock, and base zoom passthrough. Both Windows game executable targets and both FOV test targets compiled and linked successfully; isolated target rebuilds reported no remaining work. The full `run-windows-build.ps1` invocation for each game stopped only on the pre-existing dirty `android_save_set.c` test target because `PATH_MAX` is undefined, after the requested game and FOV targets had linked. With JDK 21, `:app:externalNativeBuildDebug` passed D1/D2 configuration and native builds for `arm64-v8a`, `armeabi-v7a`, and `x86_64`. One scoped `android/run-code-quality.ps1 -Fix` pass covered all touched paths and passed clang-format, UTF-8 BOM checks, Android CMake formatting, and cmake-lint; inherited D1/D2 files were excluded by the wrapper as designed. Scoped commit and final worktree `git diff --check` both passed
- Limitations: No focused FOV render-smoke or automation fixture existed under `android/game_scripts`, `android/tests`, or `android/helpers`, so no emulator render smoke was run. The host policy fixture and D1/D2 Windows links cover the extracted policy and ABI, while all configured Android ABI links cover its renderer integration. The umbrella Windows build remains red only because of the unrelated concurrent `android_save_set.c` compile failure described above
- Out-of-scope worktree check: Initial and final status and the complete scoped diff were audited. This worker did not edit, format, revert, stage, delete, or otherwise change any out-of-scope dirty path. During validation, external orchestration advanced `HEAD` from `0498798fc927581626c3f5978e219c68e64990c0` to `34ed94767d2a2dbca3e07dd1ba672be467cfb3f1` and committed the chunk together with pre-existing and concurrent work; the chunk paths are present and clean at the new head. The final live dirty paths are `plan_next_30_local_correctness_fixes_20260811.md`, `LanDiscoveryTab.kt`, `android/outstanding_bugs.md`, `lan_game_result_first_20260811.md`, and `plan_coop_save_death_spew_lifetime_20260811.md`; none was touched by this worker. No out-of-scope dirty path changed because of this chunk
- Residual and rerank: DMR1-CHUNK-003 is the next eligible item. Its prerequisite is satisfied, and this chunk moved no frame orchestration
- Root acceptance: accepted after review of the scoped implementation and source-list/test wiring, independent confirmation of D1 `+239/-28` and D2 `+391/-28` against `upstream/main`, and a clean scoped `git diff --check`. The focused D1/D2 CTest, game-target links, configured Android ABI links, and the unrelated umbrella Windows failure are recorded precisely. The concurrent HEAD advance and subsequent D1/D2 state edits are isolated in survey generation 2 above
