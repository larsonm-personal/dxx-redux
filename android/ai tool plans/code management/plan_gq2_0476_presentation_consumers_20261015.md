# Chunk 0476 presentation and fixture identity consumers, 2026-10-15

Selected consumer trace following source/wrapper/native packages. Source gate closed and four later gates still open pending consolidated owner/disposition replay. No source edits, helper execution, device or payload operations.

## Preserve actual render readiness boundaries

android_graphics_safety336-394 requires a tagged main-view render, successful presentation, matching surface generation, same non-null EGL context and continued eligibility before considering presented gameplay. For first-run or live preview it marks candidate_ready only when rendered revision equals candidate revision, clearing apply deadline. First-run offer additionally excludes active confirmation/restoration, UI blocked/background, multiplayer, demo playback/replay/no-render, no level, dead player, queued edits, nonidle record and invalid/mismatched capability report. Preserve these real gates.

prepare_main_view706-870 records each queued live candidate before GL. It increments candidate revision, resets readiness and starts apply deadline without extending an existing one. Candidate application, confirmation debounce, first-run editing/settling and safe accepted/all-off completion are distinct states. It notifies UI before potentially stalled live GL calls. apply_snapshot failure calls renderer_failed; nontrial failure becomes storage failure. Actual armed non-first-run paths can set candidate_ready after application or when no apply/persist is needed; do not generalize first-run/live presented-revision proof to every challenge.

Both games' selected EVENT_WINDOW_DRAW ranges gate main render on shared safety plus no automap/idle screen, then tag actual game_render_frame completion. Shared EGL swap350-408 respects allow_present, reports failed/no-surface/recreated-surface failures, performs actual eglSwapBuffers and reports successful generation after releasing surface snapshot. Keep compact inherited render hooks and shared Android policy.

## Preview framebuffer and request path

Paired ogl.c readback ranges prepare framebuffer readback and sample only for pending merged-wall/introspection work, preserving driver workaround against unconditional menu glReadPixels. Dense framebuffer probe139-223 reads actual drawable RGBA, records GL error and counts pixels with brightest channel>=8, plus total RGB/extents.3230-3270 serializes those fields; hidden idle screens omit them. This proves a real GPU readback when executed, not scene identity or Android compositor presentation.

Introspection3316-3366 increments request generation and defers output until a new framebuffer sample completed. If a request arrives during sampling/swap, it publishes sampled generation and leaves newer request pending. Publication uses temp file, checked flush/close then rename; generation is advanced before publication, so failed write/rename does not itself promise a retry. Existing publication/freshness owner reconciliation applies, not a new implementation here.

Selected preview Activities validate owned requests before native launch, load the selected engine library, delete prior debug introspection/temp outputs and set native path. Debug broadcasts invoke native request generation; LevelPreview also requests presentation probe, but the assigned runner's assertion is framebuffer_probe, not that compositor-probe result. JNI619-643/868-902 forwards requests and uses checked UTF8 conversion for path. game_introspect2057-2082 wraps active native preview state as level_preview. Selected native preview state includes request_id/source/game/level fields; the host runner doesn't compare them. Preserve owned request validators and new-sample generation while keeping host request correlation as an existing remaining acceptance requirement.

Do not turn positive visible_pixels and advancing flip_serial into an exact scene oracle, guarantee of every requested graphics option or proof of physical display delivery. Whole runner's no-confirmation journal and unchanged configs remain the actual assigned preview contract.

## Declared fixture hashes versus fallback data

The previously traced recovery wrapper pre-resolves fixture, stripping_info/_deps, before run_test reads it. Selected run_test323-355 shows launcher-recovery path uses declared deps if present, otherwise generic Ensure-GameDataOnDevice. Whole Ensure-GameDataOnDevice1579-1647 always requests retail filenames, lists current device files, clears an evident D2 demo set and returns success on filename presence. Missing files are searched by name in preferred local paths; legacy push/cat/rm calls run with Continue and no explicit per-call exit verification in this function. It does not compare the fixture's eleven recorded hashes. This strengthens the need to preserve original metadata through admission and verify selected executed assets under existing source/staging owners; no payloads were read or hashed here. Broader Start-GameWithRetry/data provisioning behavior is not newly claimed complete.

## Synthesis direction

Retain six distinct setup/normal exit/preview/rebuild/repair interruption/restart contracts and actual durable journal/all-config/process/timing/readback checks. Simplify only through existing checked immutable staging, exact source identity, complete-attempt deadline, correlated snapshot and independently released runner infrastructure owners. Preserve current locks, native format authority, input release, generation matching, staged mode persistence and durable rollback marker. No proposed fixture merge, new framework, arbitrary waits or upstream policy expansion.

Next reconcile whole material BR definitions and existing prior plans, capture exact raw quality/rank rows and direct unpooled disposition, then consolidate replay and publish. No new finding/remediation/status/rating/runtime/original saving admitted. Two large prior-plan reads were clipped; they earn no whole-plan credit. Reuse exact previously reviewed plan packs or read bounded complete prose as needed before claiming reconciliation.
