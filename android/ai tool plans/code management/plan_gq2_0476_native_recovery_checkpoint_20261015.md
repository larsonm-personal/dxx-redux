# Chunk 0476 native recovery and script admission checkpoint, 2026-10-15

Partial trace following wrapper checkpoint. All selected ranges explicitly captured; only staged-restart script is claimed whole-file. Source gate remains closed, four later gates open. No product/helper/test edits or execution/runtime credit.

## Script preparation and dependency boundary

Read complete Resolve-TestScript function2373-2504: JSONC parse, per-game/param vars, shareware handling, recursive string/array/object substitution with missing/cycle rejection, game filtering before value resolution, strict output serialization/validation. It strips _info, writes fixed .resolved/basename, and returns source unchanged on missing/parse failure. Preserve recursive escaping and validation; fixed path and fail-open parse remain existing source/staging/admission-owner topics.

The recovery wrapper first resolves fixture and passes that path to run_test. Therefore successful pre-resolution strips the fixture's _info/_deps before run_test inspects ScriptPath. Selected run_test85-150/212-290 and whole Get-ScriptDeps1192-1224 confirm declarative dependencies come from original supplied ScriptPath metadata. The eleven hashes in the assigned fixture must not be credited as executed-byte verification on this wrapper path. Later generic provisioning may provide data; its identity contract is still to reconcile with existing owners. No actual payload read/hash or new duplicate finding.

test_target1-122 selects environment/default serial; physical target defaults to nsdtest and requires isolated package. Broadcast argument rewriting adds package when absent for com.dxxredux actions. Preview runner uses it; recovery runner restricts emulator and its raw wrapper adb is separate. Preserve isolation and explicit target propagation.

Whole staged-restart script navigates fresh gameplay, waits challenge with current TexFilt2/accepted1, presses/releases B and waits idle/current/requested/accepted1/unpaused. This is the normal-exit follow-up, unlike assigned accepted-restart's no-prompt assertion. Preserve scenario distinction.

## Durable journal and config publication

graphics_safety_store495-585 locks and normalizes patch snapshots, calls config batch for all safety fields. decide checks trial identity/nonidle; acceptance only during challenge before deadline and only commits new accepted tuple if record publication succeeds. Failed OK retains old accepted tuple and restores. Cancel writes RESTORING marker before config publication; marker remains for renderer acknowledgment. complete_restore re-patches accepted tuple and writes idle only after success.

recover distinguishes live owner by PID plus process creation/session identity. Current snippet174-203 uses Windows creation time or Linux proc starttime, protecting against PID reuse. Recovery leaves another live owner's attempt alone; abandoned/restoring record is durably marked and completed before staged flush. clean_exit validates owner PID/session, rolls back challenge/preparing/preview/restoring, but an unvalidated state clears ownership and flushes staged config rather than silently accepting. This explains staged2/accepted1 after menu exit.

config_paths66-85 always includes root and adds D1/D2 configs only for existing engine directories. Three-mirror fixture checks therefore have meaningful provisioned-directory preconditions; avoid claiming unconditional creation of every engine directory.

graphics_config_transaction1-41/350-480 stages all target files and originals before replacing any, validates keys/duplicates/counts, rolls back previously published files on replace failure and all on parent-directory sync failure. Failed rollback retains backup where needed. Debug pause is enabled only on Android introspection builds, keyed to TexFilt value captured when armed; atomically consumes once and sleeps after first successful publication. It occurs before the later per-directory fsync loop. The journal phase marker plus restart repair, rather than a pretend atomic multi-file rename, supplies interruption recovery. Private-file read/write/replace/sync internals not fully reviewed in this checkpoint.

## Engine rollback, EGL and UI process recovery

android_graphics_safety120-160 eligibility requires front gameplay window, SCREEN_GAME, no automap and an available unpaused surface. Thus menu staging does not by itself arm gameplay confirmation.

apply_snapshot225-345 suppresses recursive trial creation, applies option deltas, compares mode keys and also detects lost EGL or canvas dimensions even if Game_screen_mode has not changed. It rebuilds mode/render buffers on either condition; only success updates mode. Optional persistence preserves launcher-staged mode fields while patching live options. Initialization calls store recovery; shutdown calls clean_exit. Full presented-frame correlation continues beyond selected boundary and still requires trace.

decide/event_tick530-745 retains trial ID/candidate-ready checks, publishes decision outcome and sets restore_pending for restore/error outcomes. It drops state/file locks before GL recovery, retries durable restore, applies accepted snapshot and clears marker only on successful renderer completion. It polls staged changes, cancels on clock/lifecycle/eligibility loss, and handles first-run retry state. The selected tail begins prepare_main_view and records candidate durably before live GL work; remaining candidate application/presentation path still unread.

Paired gr.c mode and EGL setup ranges show canvas resize before ogl_init_window, Android before_mode hook, ogles_destroy before android_egl_surface_initialize and renderer_failed on initialization error. EGL debug count35-64/163-214 decrements and fails actual initialization;512 failures include attempts to restore640x480. Shared apply_snapshot renderer_incomplete check addresses canvas/context mutation before successful mode update. Keep these compact paired hooks and shared policy, avoiding broad upstream refactoring.

MainActivity2230-2275 wires overlay to native state/decision and releases held controller/axis/mixer/touch input when modal acquires input.2320-2375 publishes fatal error, starts setup with recovery message, finishes touch capture and kills current process. Exceptions in fatal-error publication are caught; startActivity ordering is explicit and is not proof of guaranteed kill if that call throws. Do not invent acceptance beyond source.

SetupActivity798-820 calls NativeGraphicsSafety.recover before continuing launch, returns error on0 and updates render-resolution preference/generation when recovery returns2. Broader setup launch/codec ownership remains existing review territory.

jni_main355-414 initializes graphics safety before ordinary main, then shutdown after return. Separate whole preview-start function445-552 shares engine admission/UTF8 validation/global ref but invokes main with preview request without ordinary graphics-safety initialization; it marks engine terminating and kills process on exit. Preserve process isolation and admission rather than enabling confirmation in previews. Native preview render/probe and request generation still require selected consumers.

## Continue

Finish actual prepare_main_view/presented correlation and preview framebuffer/introspection routing, check required generic fixture provisioning source identity without reading payloads, and reconcile material existing BR owners/prior plans. No new owner, rating, remediation, runtime acceptance or original-file reduction assigned here. A literal wildcard locator failed and was corrected with directory plus -g; a clipped mixed locator output earned no body credit, and needed store/helper/JNI bodies were reread explicitly.
