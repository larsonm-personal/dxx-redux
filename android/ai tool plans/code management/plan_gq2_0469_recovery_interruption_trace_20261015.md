# Chunk 0469 recovery, probe and interruption checkpoint, 2026-10-15

Diagnosis and plans only. The previous goal turn made concrete progress: assigned-source gate closed and native/multiplayer consumer checkpoint audited PASS61. This checkpoint continues actual source tracing without closing the remaining four gates or publishing a terminal disposition.

## Recovery through actual renderer owners

apply_snapshot compares the first five live graphics options and calls their native setters with persist0 while applying=true. MSAA setter updates GameCfg.MsaaLevel and ogl_msaa_samples, issues a memory barrier and sets g_msaa_pending_apply. In both games, the start-frame path consumes pending runtime options before android_ogl_msaa_begin_frame; pending MSAA destroys the FBO. Shared destruction, read in the prior checkpoint, resets failure_latched even if the resource IDs are already zero, while retaining last_gl_error. Thus accepted MSAA0 restoration can clear the latch and preserve the failed allocation's diagnostic error without needing a successful new MSAA allocation. This is a concrete source explanation for the script's combined error1281/latchfalse/createfalse oracle.

Paired main-game draw branches skip automap and hidden idle, call graphics before_main_view, render, then call main_view_rendered. Selected native presentation admission requires successful presentation plus matching surface generation, EGL context and applied candidate revision before candidate_ready. This preserves current protections; no whole renderer, game_render_frame or EGL publication family was accepted here.

apply_snapshot detects mode changes/incomplete EGL or canvas state, reinitializes render buffers after successful gr_set_mode, and preserves staged launcher mode values when patching only current runtime graphics fields for persist. Return values from the five option setters are not individually consumed here; applying bypasses note-option trial admission and the full current error/transaction owner must be reconciled before diagnosing this as a separate defect. Paired gr.c selected tails skip texture caching during restoring mode; no whole mode-initialization acceptance.

## Exact probe scope and oracle

Read complete SavedState/helpers and complete android_ogl_msaa_probe/result getter. Probe requires a current EGL context/surface, surface size>=4, requested2/4/8, supported window format, compatible capability sample count, a single-sample window, successful setup/format queries, complete private targets and matching actual color/depth samples>=2.

The four stages draw and sample known colors on private/window control targets, then resolve a private multisample source to private and window destinations. Four quarter-position pixels per stage, tolerance8 per checked channel, and stage GL errors determine failure. This is diagnostic rendering before the next frame, not ordinary game content, arbitrary pixels or a presented swap. logical_size is reported, while target sizes come from the EGL surface.

finish restores saved read/draw framebuffer, renderbuffer, pack buffer/alignment/row/skip state, scissor, clear color and color mask; deletes private objects; compares the saved state and checks restore errors. State-restored does not mean framebuffer contents restored. Ordinary return paths call finish; allocation/JSON exceptions are not guarded by RAII in these bodies. Prior_errors and optional window_diagnostic_errors are recorded separately and do not themselves set first_failure; passed derives from first_failure plus finish's restore validation. Preserve this distinction under existing probe/cleanup owner reconciliation.

Automation msaa_color_probe calls the native function synchronously on Android OpenGL; result getter returns stored last_result with run serial, and introspection parses it. Assigned scripts assert passed/state_restored but do not compare run serials. Same-thread direct call provides a stronger producer link than merely polling a preexisting getter, without proving complete command/result publication identity or execution in this diagnosis.

## Native interruption and input

eligible requires game/front/SCREEN_GAME, no automap and an available unpaused native surface. Previously read event_tick cancels armed or preview state after eligibility is lost; prepare_main_view returns before consuming queued edits when ineligible. queue_option accepts normalized pending options while idle, even when automap is front, while rejecting prepare/challenge/restore. This supports deferred edits until returning to gameplay and coalescing an edit back to current settings.

android_pause_reasons includes graphics blockers. Multiplayer masks graphics/UI/background/user reasons out of simulation pause while input_allowed still rejects blocked ship input. On blocked entry publish clears Controls; on release it resets time and flushes inputs. Native try_open_automap requires graphics as the sole pause reason plus live eligibility restrictions, reads the configured map binding with Controls zeroed before/after, and invokes ordinary do_automap. Paired ReadControls invokes that exception before rejecting blocked input; it still permits ESC/PAUSE system-key handling. These are intentional controls/navigation behaviors to preserve.

The ordinary key action has two routes: key_command_map includes up/tab but not lctrl. With game_handler front, up/tab call game_handler directly through an EVENT_KEY_COMMAND; lctrl uses SDL down/up in one injected tap. Therefore the added assertions test native suppression and the configured native automap path, not Android UI key dispatch or held SDL key behavior. No direct-command-versus-held-key equivalence claimed.

Actual UI overlay acquisition remembers held controller keys, releases controller/axis/mixer state, sends touch cancellation, and suspends Video Info polling. MainActivity consumes suppressed keys until release and routes active modal keys into the overlay. The overlay defaults selectedOk from first_run_trial, handles navigation/accept/cancel, restores its input owner on dismiss, uses monotonic deadlines and schedules arming after a UI draw. Lifecycle cancels an active trial. Its key/motion handlers consume input while active. These selected functions do not prove whole controller/session/dispatch retirement, screen presentation or hardware behavior.

Paired event loops call graphics event_tick before automation/input, then publish pause and draw/present. No source-only claim that every frame receives the exact intended request. Actual LAN result/Fresh and fixture/staging boundaries remain next.

## Evidence and continuation

Exact raw-file fingerprints, physical ranges and body bytes are in gq2-recovery-interruption-progress-0469-20261015.json; a separate audit replays them and unchanged canonical/HEAD/product boundaries. No earlier package reused. Contexts include selected branches as explicitly bounded, not entire files/families.

Read recovery: guessed probe end735 exceeded actual719 and was recovered620-719; options end522 exceeded518 and was recovered508-518. First combined read stopped before options, which were then read separately. Guessed game_window_event_shared/DescentActivity names absent; actual event.c/MainActivity/gamecntl owners used. Large prior0137 plan/search output truncated: only locators and displayed text, no whole prior-plan or owner acceptance. Recover whole material plans/BR/quality before synthesis.

No product/test/helper edits or execution, builds, tests, probes, device/provider/network/media/payload/staging/commit. Canonical468DONE159TODO627 and all current findings/remediations/ratings unchanged. Source gate only closed; consumer/owner/disposition/publication remain open. Ledger unchanged, prior0468 mounted Drive proof historical, no new evidence append/backup. Next finish actual LAN admission/Fresh/results/profile/catalog/budgets and whole material existing owners/original plans/quality; then direct unpooled disposition, immutable publication and authorized backup. Broad queue and all remaining sweep/supplement/closure requirements stay active.
