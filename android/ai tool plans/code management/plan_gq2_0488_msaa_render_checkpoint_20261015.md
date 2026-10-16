# Chunk 0488 production MSAA/render checkpoint, 2026-10-15

Diagnosis only; serial work, no product/test/helper edits or execution. Consumer and owner gates remain open

The receipt gq2-msaa-render-progress-0488-20261015.json in temp/general_cleanup_20261006 preserves 22 current contexts / 2261 overlapping lines and whole BR-0647. Whole files read: ogl_msaa_android.c, ogl_msaa_android.h, android_gpu_policy.h and android_egl_surface.c. Other caller ranges remain explicit partial context; no extra assigned queue coverage

Sample policy selects the smallest common color/depth sample count at least the requested count. It does not negotiate downward. Keep this distinct from the script's effective_samples >=2 assertion for requested4. Runtime color-format refresh handles actual window bits changing from the cached capability format. Capability query rejects unsupported, nonlinear, non-unsigned-normalized and already multisampled window targets, and publishes query failure as unavailable

Production allocation checks individual GL errors, framebuffer completeness and matching effective color/depth counts at least two. Unsupported sizes/formats, create, bind and resolve failures latch failure and notify graphics safety. Pending MSAA changes destroy the old FBO, allowing recreation for changed sample settings. Retired contexts forget old GL names without deleting objects allocated in the new context. FBO generation advances only on successful creation; it is distinct from window/context generations

Outermost 3D passes bind the shared FBO. The first MSAA-backed pass clears color; subsequent cockpit passes clear depth while retaining the main scene. Resolve waits for depth zero. Bind and resolve counters advance after error-free operations; last_frame_resolved requires the resolved serial to equal the latest bound serial. These are stronger than desired sample count alone

The paired swap wrappers call shared EGL handling. That handler can skip disallowed presentation or paused/missing windows, recreate a changed window surface, and sends the actual eglSwapBuffers result to graphics safety. The MSAA flip_serial increments after wrapper return even on skipped or failed swaps. Menu/scene probe success is measured before the swap. Neither flip_serial nor probe success alone is proof of a successfully displayed frame. Initial context-created diagnostics increment before makeCurrent; cumulative counts alone do not prove a usable context

Both native menu callers clear subfunction on the unscaled probe copy, suppressing duplicate draw callbacks in the reference rendering. The scaled contents helper copies items and menu geometry before rendering and frees its private items. Both FOV visual-only paths restore render-list state; D2 also restores the rendered-object list. Visibility-verification reference passes are discarded separately when that diagnostic mode is enabled

Introspection publishes desired samples separately from production effective/create/latest-resolve/error/counter diagnostics and the three probe reports. Capability JSON has its own copied report; this does not establish a multi-provider atomic snapshot

Read whole BR-0647 FIXED, SHA256 f36ca36153a8b2bf9b4df066fa340bdba44289bcb3df243948666575c778bdfc. Preserve its repair and status. Its historical device and injected-failure validation limits remain historical; no fresh runtime validation was performed. No new finding or priority derived from that prior record

Next: exact-current safety/controller/harness/assertion evidence, whole other owners and quality/prior dispositions, then direct unpooled assessment and final publication. Canonical queue remains 487 DONE / 140 TODO of 627; no evidence-ledger append, fresh Drive verification, runtime pass or inherited diff saving
