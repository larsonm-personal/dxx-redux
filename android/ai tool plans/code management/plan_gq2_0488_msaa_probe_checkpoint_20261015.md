# Chunk 0488 native probe checkpoint, 2026-10-15

Diagnosis only; no product/test/helper edits or execution. This is a partial consumer checkpoint, not a completed chunk

Receipt temp/general_cleanup_20261006/gq2-msaa-probe-progress-0488-20261015.json SHA256 f319f3a1647ebb317a5c8c7b4c5cbd78bed0b40353dd1dbbb025b28cc037509c preserves 13 explicit current contexts / 1247 overlapping lines with full-file and body hashes. Context ranges are the coverage boundary; partial enclosing functions do not earn whole-function credit. Loading-background code L426-L544 in the probe implementation is not reviewed here

Automation dispatch restricts scene requests to 1..3 passes. Color probe execution is synchronous; menu and scene requests reset their report/serial and complete from renderer hooks. The script's graphics_option uses the direct setter with persist0, distinct from graphics_queue_option's queue with persist1/debounce1

Known-color probing creates private single/multisample renderbuffers, validates completeness and matching color/depth samples at least two, and checks four quadrant colors with tolerance eight through control and resolve stages. It also writes the default window framebuffer. SavedState restores and compares framebuffer/renderbuffer bindings, pixel pack binding/settings, scissor, clear color and color mask. This does not establish complete GL-state restoration or preservation of prior window pixels

Menu/scene observation reads the current draw target; a nonzero renderbuffer target is copied to a private single-sample target without invoking the production resolve. Native menu expectations come from flat opaque non-dark palette patches spread over a 3x4 grid, with at least four points required. The scaled producer samples its rasterized bitmap around the real blit. Unscaled sampling renders a private reference copy; caller suppression of event callbacks still needs inspection

Scene probing writes three 5x5 markers in the first qualifying pass and reads their survival after every pass and before swap. A smaller later viewport establishes saw_subview. Requested multi-pass completion also requires a subview; missing passes fail after 120 observed flips. Script deadlines are separate. These are marker-composition checks, not general scene correctness or proof that a requested sample count was achieved

Both games sample scene passes in ogl_end_frame before resetting the viewport and ending the MSAA frame. Final menu/scene reads happen before ogl_swap_buffers_internal. Probe success therefore does not prove successful EGL presentation. The FOV base-pass discard is conditional on visibility verification; ordinary custom-FOV rendering uses separate CPU visibility collection and a visual-only pass

Remaining: production MSAA create/bind/resolve/failure/context-generation lifecycle, actual swap result, whole menu/FOV caller boundaries, sample policy implementation, complete introspection producers, exact-current safety/controller/harness evidence, whole existing owners and prior plans, then direct unpooled disposition. No new finding/priority/status/runtime/original saving or canonical queue credit. Primary consumer gate remains open; no canonical evidence append or fresh Drive backup
