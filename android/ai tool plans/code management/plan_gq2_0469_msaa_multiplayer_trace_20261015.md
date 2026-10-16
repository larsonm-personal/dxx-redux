# Chunk 0469 MSAA allocation and multiplayer consumer checkpoint, 2026-10-15

Diagnosis and plans only under plan_general_diagnosis_resume_20261009.md. This checkpoint advances actual consumer inspection; it does not finish consumer, owner, disposition or publication gates. Previous goal turn checked the reported merge conflict: target file clean on disk/index/HEAD, so no edit was needed. Current goal turn revalidated HEAD, source acceptance and ledger locations before continuing.

## Native allocation and recovery

Read explicit complete allocation, destroy/reset, begin/end/resolve, diagnostics and error-capture bodies in ogl_msaa_android.c, plus graphics renderer-failure, event-tick, prepare-main-view and selected store transitions. Exact physical ranges and raw bytes are captured in the progress artifact.

The INTROSPECT_ON one-shot injection resets its flag and passes color_width=-1 to the actual glRenderbufferStorageMultisample call. The code drains earlier GL errors, preserves the first creation error across subsequent creation checks, records framebuffer status/error and destroys failed resources before reporting msaa_framebuffer_failed. Unsupported format/sample/size paths can return before consuming the injection; execution on a capable target is still needed. The script expects 1281, but this read establishes the GL argument/error propagation rather than runtime driver behavior.

Destroy resets failure_latched and effective samples, but preserves last_create_status and last_gl_error. Failure cleanup then latches failure again. Thus the script's later failure_latched=false requires a subsequent recovery/reset path; the immediate failure body alone does not prove this outcome. Probe state restoration and actual option-to-renderer reinitialization remain to trace.

The store enum assigns IDLE0/ATTEMPT1/PREPARING2/CHALLENGE3/RESTORING4/PREVIEW5. prepare_main_view records a live candidate as PREVIEW before apply_snapshot and defers preparing the challenge until readiness/quiet time. renderer_failed captures the matching nonidle active trial's persisted phase and state before requesting cancellation. This explains why a live allocation failure may be phase5 rather than phase3. Full apply/draw/probe ordering still requires actual paired native consumers; no runtime success claimed.

Store cancellation persists RESTORING and patches accepted settings; complete_restore requires the same trial and RESTORING, patches accepted again, then writes IDLE. Event tick requires durable cancellation, successful apply_snapshot and successful complete_restore before clearing restore_pending and publishing current=target. A failed accept preserves old accepted settings and enters restore. These are meaningful safeguards to preserve, not proof of the lower file transaction or renderer callback families.

## Actual multiplayer helper

Read the whole 114-line test_graphics_multiplayer.ps1 and selected actual test_lan invocation/fixture branches. The helper is gated by GraphicsConfirmation; plain catalog registration is not evidence this profile executed.

It tests both serials, each through cancel, timeout and accept. All current/requested/accepted ten-field tuples are checked before/after, and the other peer is checked after each case. Accept on the first serial persists its TexFilt2 baseline while the second serial is tested; expected peer state accounts for this ordering.

During a challenge, two samples 350ms apart must remain unpaused/network/two-connected, retain trial ID and deadline, increase msaa.flip_serial and change the remote packet sequence. This adds actual presentation/network-progress observations beyond the JSON bodies. It is not an explicit whole simulation-state progress proof or a disk readback. Timeout relies on returning to idle with baseline restored.

Fixture generation restricts serial names to emulator-N and writes a ten-field accepted journal/config, but uses fixed /data/local/tmp names and unchecked Adb-Dev push/copy/mkdir calls. The actual facade, payload admission and Fresh observation ownership still require current boundary reconciliation; no new root admitted from this isolated helper read. Durable result waits depend on existing Get-DeviceAutomationResult semantics, not yet credited here.

## Acceptance and next work

The prior source audit PASS87 and exact source artifact identity were checked. Close only the assigned-source gate after current canonical/HEAD/product replay. Remaining four gates stay open and canonical0469 stays TODO.

Next trace actual paired native option/apply/draw/destroy and probe guards/results; automap eligibility/cancellation/deferred edits and challenge input/pause ownership; actual LAN staging/result/Fresh boundaries and profile/catalog/budgets; whole material existing owners/original plans/quality rows. Consolidate before direct unpooled disposition and immutable final publication/Drive backup.

Read recovery: Windows wildcard path failed and was replaced by rg over the actual shared directory with globs. Creation-check tail67-69 and prepare-main-view tail845-854 were recovered. Selected test_lan branches are not whole-file credit. No probe-source body was read in this checkpoint.

HEAD1b26c43cf928f9798513c0652a833c28f50a9e98/product empty; canonical source fingerprints replayed by separate audit. Local and mounted Drive ledger lengths both819705668; this is location/length evidence only, not a new backup or synchronization proof. Prior0468 terminal receipt remains historical. No evidence-ledger append, new finding/remediation/status/rating/runtime claim or inherited saving. Broad159 queue and all supplement/closure requirements remain. No product/test/helper edits or execution/build/test/device/provider/network/media/payload/staging/commit.
