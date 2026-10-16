# Chunk 0475 graphics consumer checkpoint, 2026-10-15

Diagnosis and plans only. Source gate closed; four later gates remain open. This checkpoint traces gauge/VBO and runtime texture wrapper/native consumers. No product edit, runtime reproduction, new finding/remediation/status/rating or original saving.

## Real probe coverage and gauge limits

The gauge script's probe_gles_vbo calls gles3_shim_probe_vbo_arrays and fails the run when false. The complete probe creates an8x8texture framebuffer, checks completeness, draws two VBO-backed triangles with zero and nonzero offsets and requires a white center pixel with no GL error. It then calls the complete ogl_2d_batch_probe, checks tracked versus actual buffer binding and deletion behavior, and restores captured array, matrices, program, framebuffer, texture, viewport, clear-color and enable state while deleting owned probe objects.

The batch probe compares exact8x8RGBA immediate and batched readbacks for empty, line, rectangle and alternating-mode scenarios. Nonempty scenarios issue800primitives, exceeding bounded1536vertex storage and exercising capacity/mode flush. It additionally requires nonblack blue at the checked central pixel and no GL error. Preserve this actual GPU readback oracle; the earlier source-only observation of no explicit script batch predicate must not become a claim that the entire fixture lacks batch pixel checks.

The probe uses synthetic geometry in a tiny framebuffer, not the actual cockpit image or presented display. Paired full-cockpit energy functions bracket obscuring lines with gr_begin/end_2d_batch; full/overfull energy>=100 yields empty line batches. Paired gr.c line/rectangle paths dispatch through ogl_2d_batch_draw. Status-bar energy uses a rectangle without an explicit begin/end scope in the inspected bodies, while D2 status afterburner has a line batch. Preserve these distinct bindings; do not claim every status-bar energy update itself uses an active batch. The subsequent energy/cockpit assertions and wait intervals remain useful state/survival coverage, not complete actual-cockpit pixel equality or measured performance.

Whole shared batch implementation uses fixed storage, mode/capacity flush, rectangle-to-triangle ordering and per-vertex colors. Immediate mode bypasses retained batch storage. The selected engine excerpts are not whole gauge/renderer algorithms or all driver state acceptance.

## Wrapper-generated runtime texture acceptance

Whole81linePowerShell wrapper read. It runs a prefix through _probe_end with LeaveRunning, reads GPU capabilities, and includes nonzero anisotropy transitions only when aniso_max>=2. It emits an explicit unavailable log otherwise. It writes generated scripts and runs shared run_test with300second timeout, checks child exit, and force-stops the package in finally.

It strips _confirm/_probe_end/_requires_aniso for full edits. Each annotated field updates an expected current tuple; changed non-all-off tuples add controller_input waiting for challenge/candidate_ready then DLEFT+A, and update expected accepted tuple. Every annotation adds an idle/current-field/accepted-field wait. This is real generated confirmation logic, stronger than direct template settings alone. The prefix probe retains annotations but relies on ordinary parser treatment and only reaches _probe_end; full transformed execution is the confirmation path.

All-off means TexFilt0/Aniso0/MSAA0; menu/HUD settings alone do not change that predicate. Unchanged accepted settings skip the challenge. The wrapper checks only each annotated field in the appended wait, not every tuple/texture/persisted byte. Get-GameIntrospection is called without Fresh; its body broadcasts, sleeps800ms and accepts object-shaped JSON unless Fresh explicitly cleared a file. Capability correlation and required ADB/force-stop status remain existing harness/observation owner acceptance, not a new wrapper issue or execution claim.

## Actual native dispatch and FBO evidence

SET_DEBUG graphics_option parses name:value, invokes android_graphics_set_option with persist0 and fails on a non-OK result. The selected setters still admit through graphics safety before changing native state; texture/anisotropy/MSAA publish pending application flags, while menu/HUD write their native config fields. Do not equate persist0 with bypassing safety admission or durable config completion. SET_DEBUG ends with immediate advance_step, so its authored post_delay_ms is not an honored pacing interval.

Both engine OGL states bind their game-local texture list and requested/applied config fields into the shared texture consumer. Actual start_frame and gr_flip paths invoke pending runtime options. Shared code walks texture records for filtering/anisotropy, generates effective filtering as needed, and synchronizes requested/applied levels; paired hooks reset texture bindings and retire MSAA FBO for pending recreation. Selected helper bodies are not every texture-type/filter implementation or cross-thread ownership acceptance.

Paired begin-frame calls shared MSAA binding; paired gr_flip resolves before framebuffer readback and later pending options. Successful shared creation records effective actual color samples, size and generation only after complete/equal color-depth samples>=2 with no GL error. Binding increments bound_frame_count on success. Resolve increments resolve_count and captures matching active frame serial only after blit and post-resolve checks succeed. Errors latch renderer failure. Diagnostics reports create_complete from status+liveFBO and last_frame_resolved from nonzero matching serials; introspection publishes these actual counters and effective dimensions/samples.

The D2 script's FBO predicates therefore observe concrete creation/binding/resolution work, not merely requested MSAA2. They do not prove display presentation, actual scene correctness, absence of all prior errors, or latest option/device generation by counters alone. Preserve current generation/serial/error checks and reconcile existing graphics/owner plans before any closure or simplification.

## Evidence and next

Exact contexts/fullfile identities are captured for independent replay. Whole wrapper81/sharedbatch124 plus completeVBOprobe779..950 were read; other files are explicit selected ranges. Unrelated whole ogl_batch_impl.h was read but not included as required gauge evidence. Guessed literal wildcard MSAA path failed on Windows; corrected directory -g lookup found actual ogl_msaa_android.c. No body credit for failed locators.

Next Guide-Bot debug/pose/physical frontier/route/connector/work counters and pilot hold threshold/selection/prompt/input release, then demo ready/catalog/default-download/TSF sample generation. Consolidate existing harness/graphics/engine-thread/JNI/source/worker owners and prior plans without speculative unrelated expansion. Canonical0475TODO, ledger and product unchanged; prior0474mountedDrive proof historical, no new backup/cloud-sync claim. Broad goal active.
