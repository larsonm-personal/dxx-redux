# D1/D2 Diff-Minimization Campaign Catalog, 2026-07-11

## Purpose

This is the working inventory for reducing branch-owned churn in files inherited from `upstream/main` under `d1/` and `d2/`. It combines the lessons and measured outcomes from earlier rounds with a fresh line-level survey after the latest feature work.

The objective is not to minimize total repository lines. The objective is to reduce merge-conflict and review surface in original engine files while keeping behavior, game-specific policy, file formats, packet layouts, and desktop support intact.

This catalog is intended to be processed in bounded tranches. After each tranche, live metrics and rankings must be refreshed because concurrent feature work can invalidate both line numbers and old recommendations.

## Live baseline

- Comparison base: `upstream/main`
- Whole D1/D2 surface: 341 changed files, `+49958/-3886`
- Upstream-original files under `d1/main` and `d2/main`: 162 modified files, approximately `+28138/-3385`
- Upstream-original files outside `main`: 145 modified files, `+8526/-501`
- Two branch-added non-`main` files and the large branch-added D1-in-D2 and input-demo sinks are excluded from the upstream-conflict priority calculation
- `arch/ogl` alone accounts for `+4997`, or about 58.6 percent of non-`main` additions
- The aggregate is a live inventory, not the clean delta for the just-completed game-control tranche. Concurrent pathing work added D1/D2 changes during that tranche.

## What the successful prior rounds actually did

### 1. Optimized the inherited-file conflict surface

The strongest passes removed branch-owned implementation bodies from files that also exist upstream. Moving 200 lines from each `gamecntl.c` into one Android-owned source is useful even when the shared source contains most of those lines, because upstream merges stop colliding with two engine copies.

Raw branch size is therefore a secondary metric. The primary metrics are:

- additions and deletions remaining in upstream-original D1/D2 files
- duplicated behavior removed from the two engine trees
- number and size of hooks left at engine-owned call sites
- whether shared code has one clear owner and a conventional declaration/implementation boundary

### 2. Classified files before ranking them

Every survey must distinguish:

- upstream-original modified files, which drive merge cost
- branch-added sink files, which may be large but do not conflict with upstream versions
- substantive feature implementations, which should not be relocated merely to improve a number
- duplicated Android or branch-owned mechanisms, which are the best extraction candidates
- stale historical candidates whose code has since moved or materially changed

This avoids selecting `input_demo_hooks.c`, D1-in-D2 translation files, or D2 escort work merely because they dominate a line-count table.

### 3. Shared mechanism and retained local policy

Successful boundaries usually have one of four shapes:

1. A direct shared function when D1 and D2 types and behavior are already identical.
2. A compact state structure when shared code needs several values but should not reach into private translation-unit globals.
3. One or two callbacks when drawing, persistence, transport, or private struct access must remain local.
4. A compile-time D1/D2 adapter when the semantic operation is the same but an engine signature differs.

The shared implementation must remain materially smaller and clearer than the two bodies it replaces. A callback table larger than the duplicated block is a failed extraction even if the metric improves.

### 4. Preserved exact ordering before improving behavior

Diff-only tranches preserve request priority, GL bind order, packet mutation order, save-field order, window closure, diagnostic timing, and failure handling. Any behavior correction found during extraction is recorded as a separate follow-up unless it is required for type safety or compilation.

This rule matters in the current backlog. Examples include the GLES 3 versus GLES 1 context recreation discrepancy and D1/D2 fatal-error drift. First isolate the duplicated mechanism; then make a separately reviewable correction.

### 5. Used normal translation units

Prior cleanup removed direct `.c` inclusion and macro-templated wrappers. New shared code should use a declaration header plus a separately compiled `.c` or `.cpp` source. Per-game adapters should be ordinary functions or compile-time branches, not implementation includes.

### 6. Kept tranches independently verifiable

Each tranche should have:

- a small plan with explicit guardrails
- exact baseline metrics for the target files
- one conceptual extraction boundary
- scoped formatting or a documented formatter-environment limitation
- both-game builds on affected platforms
- focused runtime or host tests for centralized behavior
- `git diff --check`
- exact after metrics and a reranked queue

### 7. Added test controls only when they expose production behavior

The game-control tranche added a debug-only request injection path, but the injected flags are consumed by the production handler. This is preferable to a test-only reimplementation. Similar future seams should expose state or a trigger while keeping the actual operation in production code.

## Measured examples from completed rounds

### OGL phases 1 through 36

- A long sequence of narrow extractions reduced combined inherited OGL churn by 3,567 lines.
- The sequence ended with D1 OGL at approximately `+1508/-50` and D2 OGL at approximately `+1590/-49` at that time.
- Successful pieces included texture-state helpers, cache helpers, upload support, render diagnostics, merged-wall helpers, and MSAA creation/destruction.
- The lesson is to keep taking isolated seams from a large interleaved file, not to attempt a wholesale OGL rewrite.

### metl154 cleanup

- The broad surface moved from approximately `+21018/-791` to `+20620/-801`.
- The two render files each shed roughly 171 lines of churn.
- Generic helpers and diagnostics were renamed or centralized while wall-selection and render-order policy remained local.

### Newmenu, three slices

- Text wrapping and allocation cleanup removed 274 lines of combined inherited churn.
- Menu-scale primitives removed another 80 lines.
- Touch diagnostic formatting and rate limits removed another 38 lines.
- Total inherited-file reduction: 392 lines.
- The work stopped when remaining drawing and hit-testing bodies required private `newmenu` and `listbox` access. That stopping decision remains valid until a natural scaled-render callback exists.

### State metadata trailer readers

- Two exact reader families moved to `state_android_shared`.
- Reduction: 156 additions from the two `state.c` files.
- Disk-backed saves and memory-backed rewind reads retained the same underlying read API.

### Songs

- The paired Android track-control block moved to shared code.
- Reduction: 780 additions from inherited files.
- D1 `songs.c` moved from about `+427` to `+49`; D2 moved from about `+450` to `+68`.
- Related headers moved from approximately 12 additions each to one declaration each.

### PhysFS initialization and sync

- Shared initialization removed 220 additions from the paired engine files.
- A later upstream synchronization removed about 30 more lines of churn.
- This demonstrates why extraction and upstream resynchronization should be separate measured steps.

### HMP memory playback

- The shared path removed 237 additions.
- The inherited headers became clean and each source retained only about 12 branch additions.

### Game-control save/load and meta-action dispatch

- The pause-window and in-game dispatch bodies moved into `android_meta_actions.c`.
- Reduction: 162 additions from each `gamecntl.c`, 324 total.
- D1 moved from `+455/-9` to `+293/-9`; D2 moved from `+728/-13` to `+566/-13`.
- The focused emulator script passed 45 of 45 steps in both games, covering auto-minimize, live difficulty, pause, save, load, game menu, and return to gameplay.

### Other completed structural waves

- Coop save/warp helpers, automatic networking, input-demo replay cores, playsave helpers, state rewind/save orchestration, and conventional shared-source conversion have already been processed in earlier rounds.
- Their old checklist entries must not be treated as fresh candidates without a live code audit.

## Ranked live queue

Estimates below are removal from upstream-original D1/D2 files, not net repository deletion. Estimates overlap where explicitly noted and must not be summed blindly.

### P0. Active campaign bookkeeping

#### C00. Game-control save/load dispatch

- Status: implementation and focused validation complete
- Files: `d1/main/gamecntl.c`, `d2/main/gamecntl.c`, `android_meta_actions.{c,h}`
- Exact payoff: 324 additions removed from the two engine files
- Remaining work: keep the plan and this catalog synchronized; do not reopen save formats or rewind policy in this seam

### P1. Largest ownership-clean seams

#### C01. D2 inline input-demo diagnostic probes

- Files and live regions:
  - `d2/main/controls.c:61-256`, about 196 lines
  - `d2/main/physics.c:57-412`, about 356 lines
  - `d2/main/collide.c:112-278`, about 167 lines
  - `d2/main/render.c:1225-1358` plus state near `204-208`, about 139 lines
  - `d2/main/ai.c:309-390`, about 82 lines
  - D1/D2 `fvi.c` far-miss diagnostics, about 110 combined lines
- Estimated inherited-file reduction: 995-1,030 lines
- Why it qualifies: the bodies are branch-owned replay/diagnostic instrumentation embedded in upstream-original engine files; analogous code already lives in input-demo helpers
- Proposed boundary: one or more `input_demo_engine_probes.{c,h}` sources split by motion, collision, and render ownership
- Required adapters: keep controls and physics threat-window state in distinct context structures; preserve D1/D2 FVI gate differences
- Risk: medium because diagnostics observe deterministic engine state and can accidentally perturb timing or ordering
- Validation: a known D2 replay with diagnostic logging, deterministic final-state comparison, paired FVI record/replay, Android and Windows builds
- Do not combine with branch-added sink-file cleanup in the same tranche

#### C02. Playsave launcher bridge

- Status: audited; defer mechanical movement until file-integrity fixtures and atomic shared I/O exist
- Files: `d1/main/playsave.c:1800-2198`, `d2/main/playsave.c:1416-1667`
- Raw region: 651 lines of Android launcher PLR/PLX reading and rewriting
- Header surface: another 28 D1 and 32 D2 additions can move to the shared declaration header
- Estimated inherited-file reduction: 480-580 lines including headers after retaining compact local descriptors
- Proposed boundary: extend `playsave_android_shared` with generic stdio, synchronization, validation, and PLX-section manipulation
- Keep local: canonical D1/D2 file layouts, field offsets, version rules, and compact format descriptors
- Important differences: D1 uses fixed-width PLR records and keeps several values in PLX; D2 has version-dependent name widths and stores cockpit, autolevel, headlight, and interleaved weapon order in PLR
- Concrete D1 corruption cases found by the audit: a valid shareware v8 PLR is patched 40 bytes too late; registered files with the legacy eight-byte cruft are patched eight bytes too early; shareware plus cruft is patched 32 bytes too late
- Concrete D2 corruption cases: v17-v19 control/weapon writes target 60/61 bytes too late; v20 weapon order is one byte late; byte-swapped v24 counts/version are decoded incorrectly by the launcher helper even though the engine accepts them
- File-integrity gaps: short files can be extended and reported as patched; multi-field writes are non-atomic; most write/flush/sync results are ignored; 4 KiB PLX rewrites can truncate; malformed sections can consume later content; missing sections may be appended after the outer `[end]`
- Safe tranche sequence: atomic PLX cockpit update (80-100), D1 weapon/PLX convergence (95-115), binary transaction plus scalar preferences (120-145), then dynamic keysettings/D2 weapon order and header cleanup (210-245)
- Risk: medium-high because corrupting player files is unacceptable
- Validation gate: byte-for-byte unchanged originals on every injected failure; version/shareware/cruft/byte-swap layout matrices; large and malformed PLX fixtures; then engine preferences, controller comparison, autoselect persistence, and double-launch integration tests
- Endpoint note: the identical `plr_filename_is_selectable` blocks are a separate 20-40-line follow-up after the transactional work

#### C03. Coop statistics, peer status, and restore inventory packets

- Status: implementation and Android build validation complete
- Files: D1 and D2 `main/multi.c`, `main/multi.h`, and `shared/coop/coop_multi_status.{c,h}`
- Exact inherited-file result: 179 additions removed from each `multi.c`, 29 from each `multi.h`, and one CMake source line added per game, for a net reduction of 414 inherited-file additions
- Shared growth: 231 lines; net repository source reduction: 183 lines
- Boundary: the public functions now live directly in one unconditional shared coop source, with no wrapper bodies
- Behavior covered: kill-stat bookkeeping, peer-status packets, 78-byte inventory restore packets, and rejoin cleanup
- Keep desktop-visible statistics unconditional; keep Android transport guards only around Android-specific sends
- Risk: medium due packet layout and reconnect semantics
- Validation: the moved 178-line implementation and 29-line declaration body compare exactly after newline normalization; all six Android game/ABI targets compiled and linked the new source; packet layout and call sites were unchanged
- Environment limitation: Windows configuration reached an uncached FetchContent dependency and could not use the sandboxed network, so desktop build validation remains environment-blocked

#### C04. OGL runtime texture controls

- Status: complete
- Files: D1 and D2 `arch/ogl/ogl.c`, plus `ogl_texture_android.{c,h}`
- Exact inherited-file result:
  - D1 moved from `+2046/-65` to `+1895/-65`
  - D2 moved from `+2139/-64` to `+1988/-64`
  - 151 additions were removed from each inherited file, 302 total
- Shared growth: 183 lines; net repository source reduction: 119 lines
- Boundary: one pointer-backed shared runtime state now owns effective-filter calculation, selective bound-texture filtering, and pending anisotropy/TexFilt dispatch; MSAA pending handling remains local with no callback or framebuffer-lifecycle overlap
- Existing helper reuse: `android_ogl_get_texture_bytes`, `android_ogl_apply_anisotropy_all`, and `android_ogl_apply_texfilt_all` now serve the engine paths
- Ordering preserved: anisotropy, raw TexFilt, requested/applied synchronization, then local MSAA, from both `start_frame` and menu-only `gr_flip`
- Validation: all six Android game/ABI links passed; the focused unified script passed 43 of 43 steps in both games with per-game counts of `gr_flip=2`, `start_frame=8`, anisotropy applies `=2`, requests `=7`, and effective reapplies `=1`; D2 MSAA FBO and pause viewport tests passed; the APK passed v1/v2/v3 signature and 16 KiB alignment verification
- Environment limitations: the Windows wrapper was blocked before source compilation by managed external vcpkg/toolchain access, and the scoped formatter could not execute its external clang-format binary in the sandbox

#### C05. Android EGL lifecycle

- Status: complete
- Files: D1 and D2 `arch/ogl/gr.c`, plus `android_egl_surface.{c,h}`
- Exact inherited-file result: D1 moved from `+243/-1` to `+80/-1`; D2 moved from `+244/-1` to `+81/-1`; 326 additions removed
- Boundary: pointer-backed display/config/surface/context state with only the existing texture-smash and texture-cache callbacks
- Preserved policy: initial context creation remains GLES 3 and context-loss recreation remains GLES 1; renderer and later capability-query log order is unchanged
- Residual: the largest remaining added hunk in either inherited file is 21 lines
- Risk: medium due context ownership and resume behavior
- Validation: all six Android game/ABI links passed; the 53-step launch/automap flow passed in D1 and D2 with background/resume, `egl_recreate_count=2`, and post-resume rendering

#### C06. Automap secret/objective metadata labels

- Status: complete
- Files: D1 and D2 `main/automap.c`, plus `automap_metadata_overlay.{c,h}`
- Exact inherited-file result: D1 moved from `+319/-20` to `+161/-21`; D2 moved from `+505/-23` to `+348/-23`; after two CMake entries, 313 additions were removed
- Boundary: one shared label renderer returns four telemetry counts through integer pointers; segment visibility predicates also moved directly
- Kept local: D2 markers, the private active-map structure, edge lists/culling, and introspection copying
- Risk: low-medium; code is nearly identical and current metadata overlay tests are strong
- Validation: all six Android links passed; the 53-step objective-overlay flow passed in D1 and D2, and the 29-step D2 secret-reveal flow reported nonzero labels, segments, and visible secret edges

#### C07. Android scaled linear font renderer

- Status: complete
- Files: D1 and D2 `2d/font.c`, plus `android_font_scale.{c,h}`
- Exact inherited-file result: both font files moved from `+214/-1` to `+87/-1`; after two 2D CMake entries, 252 additions were removed
- Boundary: the exact 127-line renderer moved into one translation unit and calls the two existing external font helpers directly, so no callback layer was needed
- Shared mechanism: glyph expansion, bitmap scaling, underline, masked/unmasked drawing, color, centered text, controls, and spacing
- Kept local: the separate cross-platform generic linear-font fallback and both unchanged call sites
- Risk: medium because menu readability depends on pixel-exact output
- Validation: all six Android links passed; D1 completed the full menu/automap launch path; D2 passed readable-tiny help (24 steps), controls readability (50), and menu scale (16)

#### C08. HUD robot, hostage, secret, and coop counts

- Status: implementation and dual-platform build validation complete
- Files: `d1/main/gauges.c:782-904`, `d2/main/gauges.c:792-919`
- Estimated inherited-file reduction: 230-245 lines
- Proposed boundary: `hud_counts_shared.c` for count computation and common rendering
- Adapter: a compact local wrapper for private score-display state
- Preserve: D2 secret/negative-level behavior that displays only secret count
- Risk: low-medium
- Validation: exposed count computation plus both-game assertions for robots, hostages, secrets, and aggregate coop kills
- Completed result: D1 `gauges.c` moved from 226 to 126 additions and D2
  from 282 to 177.  After one CMake line per game, the inherited-file reduction
  is 203 additions.  Private score animation and inset policy remain in compact
  local wrappers; D2 alone passes its negative-level `secret_only` flag.
- Build result: Windows D1/D2 and all three Android ABI links pass.

#### C09. OGL MSAA and framebuffer lifecycle

- Status: safe shared lifecycle slice complete
- Files: both `arch/ogl/ogl.c`
- Regions: begin/bind in `ogl_start_frame`, frame-depth end handling, readback resolve, window backing clear, overlay preparation, and existing create/destroy wrappers
- Estimated inherited-file reduction: 200-250 lines
- Existing seam: `ogl_msaa_android.{c,h}`
- Proposed boundary: extend the existing state with begin-frame, resolve-readback, clear-window, and overlay-preparation operations
- Preserve: GL bind order, frame-depth rules, resolve timing, and orthographic restoration
- Risk: medium
- Validation: MSAA smoke, keyboard and pause viewport tests, merged-wall snapshot, graphics preferences
- Overlap note: do not double-count tiny framebuffer helpers later if absorbed here
- Completed result: after C15, each OGL file shed another 33 additions, 66 total; shared code now owns begin/end nesting, create-or-reuse binding, resolve timing/error handling, and backing/overlay target binding
- Intentional residual: clear-mask policy, viewport, orthographic shader state, blend/alpha/cull state, and render context remain local; moving those would exceed the natural MSAA boundary
- Validation: all Android ABIs and Windows D1/D2 passed; focused emulator tests are deferred due to unrelated active automation

#### C10. SDL mixer Android diagnostics

- Status: complete
- Files: `d1/arch/sdl/digi_mixer.c +177/-11`, `d2/arch/sdl/digi_mixer.c +173/-11`
- Regions: handwritten audio declarations, chunk-duration calculation, init diagnostics, latency polling, and SFX-start timing
- Estimated inherited-file reduction: 200-230 lines
- Proposed boundary: a proper `SDL_androidaudio.h` plus `androidaud_log_mixer_init`, probe start, and chunk note/log helpers
- Keep local: D1/D2 sample conversion and rate variables
- Known differences: D1 log tag and requested/source-rate variable names
- Risk: low-medium
- Validation: a focused fire-weapon SFX test asserting audio probe counters, plus existing fire-primary and fire-secondary scripts
- Completed result: D1 moved from 177 to 86 additions and D2 from 173 to
  82.  After four conditional CMake lines per game, 174 inherited additions
  were removed.  Driver diagnostics now have one public header and shared
  chunk/init/latency/start logging implementation.

### P2. Exact duplicated feature kernels

#### C11. Live difficulty runtime

- Status: implementation and Android build validation complete
- Files: `d1/main/gamecntl.c:159-265`, `d2/main/gamecntl.c:221-327`
- Exact duplicate: 108 lines per game, 216 total
- Responsibilities: clamping/history, eligibility, coop host authority, persistence, replay recording, network broadcast, and HUD notification
- Proposed boundary: an unconditional `difficulty_runtime_shared.c` with relocated public symbols
- Design caution: this is cross-platform engine behavior rather than Android-only behavior. Accept only if the shared translation unit can own the same headers/globals directly without a large operations table.
- Risk: medium
- Validation: current live-difficulty assertion, input-demo replay roundtrip, and two-emulator host-authority propagation
- Completed result: the exact 108-line body moved out of each `gamecntl.c`;
  after one CMake line per game, 214 inherited additions were removed.

#### C12. Input-demo direct-command dispatcher

- Files: D1 `gamecntl.c:1500-1608`; D2 command work near `1222-1413` and replay integration near `2300-2357`
- Estimated inherited-file reduction: 180-220 lines
- Proposed boundary: common command iteration, event validation, error handling, death abort, and difficulty commands
- Adapter: a compact operations table for D2 guidebot, marker, weapon-drop, and flag commands
- Preserve: D2 unloads failed replay while D1 currently logs and returns failure
- Risk: medium-high due deterministic replay semantics
- Validation: direct-command recording/replay suites for both games and exact final-state comparisons

#### C13. Deterministic effect timeline

- Status: implementation and Android build validation complete
- Files: both `main/effects.c:40-123`
- Exact duplicate: 85 lines per game, 170 total
- Proposed boundary: `effect_runtime_shared.c` with relocated public clamping, loop, bitmap, and reset-to-elapsed-time functions
- Risk: low-medium
- Validation: table-driven time-boundary host tests and replay/save restore with animated effects
- Completed result: both `effects.c` files moved from 87 to 2 additions;
  after CMake wiring, 168 inherited additions were removed.

#### C14. Kconfig offscreen scaled-render kernel

- Status: implementation and dual-platform build validation complete
- Files: both `main/kconfig.c:601-700`
- Exact inherited-file reduction: 148 additions
- Proposed boundary: extend `android_menu_scale` with one offscreen-draw callback
- Shared ownership: allocation, canvas setup, scaling, crop/blit, cleanup, and restoration
- Keep local: `kconfig_draw_contents` and private item state
- Strategic value: creates the natural callback boundary needed before revisiting remaining newmenu direct-render bodies
- Risk: medium
- Validation: keyboard-stage, controls-readability, menu-scale, and pause-menu viewport scripts
- Completed result: D1 moved from 365 to 291 additions and D2 from 377 to 303; shared code now owns compute, allocation, scaled state, background, subcanvas, crop/blit, telemetry, cleanup, and scroll clamping
- Build validation: all Android ABIs and Windows D1/D2 passed; focused emulator coverage is deferred while an unrelated guidebot test owns the sole emulator

#### C15. OGL viewport and keyboard gap

- Status: implementation and dual-platform build validation complete
- Files: D1 `arch/ogl/ogl.c:199-282`, D2 `202-285`
- Exact inherited-file reduction: 136 additions
- Shared behavior: axis scaling, drawable-size policy, cached viewport, and keyboard-gap clear
- Proposed boundary: `ogl_viewport_android.{c,h}` with cached logical/physical coordinates and keyboard offset
- Preserve: application from `gr_flip`, since menus render outside normal frame begin/end
- Risk: low-medium
- Validation: keyboard viewport, D2 pause viewport, menu scale, and both-game autoselect
- Completed result: D1 moved from 1,895 to 1,827 additions and D2 from 1,988 to 1,920; one pointer-backed shared state owns scaling and cache mechanics while the existing local wrapper supplies screen and keyboard policy
- Validation note: all Android ABIs and Windows D1/D2 passed; focused emulator tests are deferred due to unrelated active automation

#### C16. Coop restore client-ID remapping

- Files: `d1/main/state.c:2334-2413`, `d2/main/state.c:3120-3199`
- Exact duplicate: 81 lines per game, about 150-156 removable after wiring
- Proposed boundary: the existing coop-save layer
- Preserve: player/object mutation order and callsign fallback
- Risk: medium
- Validation: restore/remap host and joiner tests, D1 counterpart, and new/missing joiner case

#### C17. Texmerge owner and diagnostic state

- Status: complete
- Files: both `main/texmerge.c:60-159` plus small call sites
- Exact inherited-file reduction: 190 additions
- Proposed boundary: extend `merged_wall_debug` with a compact owner-state structure and set/reset/log operations
- Adapter: one state field in the private texture cache, not exposure of the whole cache
- Risk: low-medium; diagnostic-only but render ordering matters
- Validation: merged-wall snapshot, door45 regression, and D1/D2 render smoke
- Completed result: each `texmerge.c` moved from 133 to 38 additions; the shared record owns first/last face and frame state, and the shared logger preserves event filtering, severity, names, and field order
- Build validation: all Android ABIs and Windows D1/D2 compiled and linked; the existing per-event call sites and D1/D2-specific flush tags remain local

#### C18. Missing coop-start fanout

- Status: implementation and Android build validation complete
- Files: `d1/main/gameseq.c:187-250`, `d2/main/gameseq.c:280-343`
- Exact duplicate: 65 lines per game, 130 total
- Proposed boundary: relocate the helper into `coop_start_positions.c`
- Risk: low-medium
- Validation: mission with too few coop starts; assert generated positions are valid, distinct, and deterministic in both games
- Completed result: 64 additions left each `gameseq.c`; after CMake wiring,
  the inherited-file reduction is 126 additions.  Assignment, counters, and
  logging remain local while the exact search order is shared.

### P3. Smaller clean seams and prerequisites

#### C19. Texture extension lookup, profiling, and D2 DXA mask regression

- Status: DXA mask sub-tranche complete; generic extension/profile lookup remains queued
- Files: both `arch/ogl/ogl.c`, plus `ogl_texture_android.{c,h}`
- Exact common regions: KTX2 profiling and extension lookup, about 95-105 combined lines
- D2-only regrown region: local `ogl_load_dxa_mask`, exactly 37 added lines at live lines 3297-3333
- Optional path-policy separation: another 40-50 lines if D1 `textures/d1` and D2 pig/set mapping remain explicit parameters
- Proposed boundary: `android_texture_lookup.{c,h}` or narrow additions to existing texture/profile helpers
- Known correctness issue: the shared helper declares and calls `ogl_loadtexture` with nine parameters while the live engine definitions take eight. This is cross-translation-unit C ABI undefined behavior hidden by permissive calling conventions.
- Safe API: add a typed eight-argument `android_ogl_loadtexture_fn` callback to `android_ogl_load_dxa_mask`, delete the direct shared `extern`, and pass each game's local `ogl_loadtexture` at all four call sites.
- Risk: low if split into two tranches: first correct and reuse the DXA mask helper, then extract generic lookup loops
- Validation: D1/D2 custom textures, D1-in-D2 custom textures, and merged-wall texture tests
- Completed DXA result: repaired the callback ABI, removed exactly 37 D2 additions, linked both games on all Android ABIs, and observed 167 D2 mask loads in a passing 33-step mod-loading run

#### C20. Masked bitmap scaler

- Files: `d1/2d/bitblt.c +51/-9`, `d2/2d/bitblt.c +57/-9`, and both `include/gr.h`
- Exact region: `scale_line_masked` and `gr_bitmap_scale_to_masked`, 46-49 additions per game
- Estimated inherited-file reduction: 97 additions, including inherited-header declarations
- Proposed boundary: move implementation into `android_menu_scale.c`, its only caller
- Risk: very low
- Validation: readable-tiny, menu-scale, and controls-readability coverage
- Status: complete
- Completed result: one private Android-owned implementation, no inherited-header API, D1 `+51/-9` to `+5/-9`, D2 `+57/-9` to `+8/-9`, and exactly 97 inherited additions removed
- Validation result: all-ABI dual-game links plus D2 menu scale 16/16, D2 readable-tiny 24/24, and D1 autoselect 74/74 on an isolated emulator

#### C21. Kconfig launcher-settings construction

- Files: `d1/main/kconfig.c:2100-2176`, `d2/main/kconfig.c:2145-2229`
- Estimated inherited-file reduction: 120-135 lines
- Proposed boundary: common pair/default population driven by per-game override descriptors
- Preserve: materially different indices, including afterburner, automap, and weapon-cycle mappings
- Risk: medium because a bad descriptor silently changes controls
- Validation: controller comparison, default bindings, and D1/D2 launcher settings roundtrip

#### C22. Startup resume and pilot arguments

- Status: implementation and dual-platform build validation complete
- Files: D1 `inferno.c:211-275`; D2 common regions `229-252` and `280-318`
- Exact inherited-file reduction: 130 additions after CMake wiring
- Proposed boundary: an unconditional startup-resume helper
- Keep local: D2 classic-demo dump parsing
- Risk: low-medium
- Validation: missing-pilot resume in both games plus invalid and missing save paths
- Completed result: both `inferno.c` files shed 66 additions; command lookup, Android pilot selection, and restore orchestration now have one unconditional shared owner
- Runtime validation is deferred due to unrelated active guidebot automation on the sole emulator

#### C23. State restore-tail helpers

- Status: reranked and final mechanical count slice complete
- Files and independent slices:
  - player-count normalization: D1 `2460-2483`, D2 `3246-3272`
  - player flight reset: D1 `2499-2516`, D2 `3290-3307`
  - trailer postprocessing: D1 `2528-2553`, D2 `3318-3358`
  - collision-delay source selection: D1 `2561-2583`, D2 `3366-3388`
- Estimated inherited-file reduction: 90-130 lines depending on accepted slices
- Proposed boundary: extend existing state/coop shared helpers one slice at a time
- Keep local: D2 guidebot ownership and route handling
- Risk: medium
- Validation: disk save, rewind checkpoint, coop restore, and collision-delay fixtures
- Live finding: player flight reset was already shared, trailer processing is materially game-specific, and collision-delay selection is a 20-line-per-game follow-up
- Completed count result: 15 additions removed from each `state.c`, 30 total, by moving live Netgame count normalization into the existing coop restore layer
- Endpoint: remaining clean C23 slices are now individually in the requested 20-40-line range

#### C24. Shader information-log helpers

- Files: the byte-identical D1/D2 `arch/ogl/oglprog.c`, each `+132/-23`
- Region: `ogl_log_shader_info`, `ogl_read_shader_info_log`, and `ogl_read_program_info_log`
- Estimated inherited-file reduction: 80-90 lines
- Proposed boundary: a small cross-platform `ogl_shader_log.c` compiled into both prefixed OGL targets
- Keep local: shader sources, uniforms, cutoffs, and program creation
- Risk: low
- Validation: merged-wall snapshot, two-pass probe, and door45 GPU regression

#### C25. Jukebox sidecar paths and display names

- Files: both `arch/sdl/jukebox.c +54/-2`
- Regions: sidecar path construction and decoded-name/basename fallback
- Estimated inherited-file reduction: 70-75 lines
- Proposed boundary: `track_names` helpers for playlist sidecar loading and display-name fallback
- Risk: low
- Validation: host path/name fallback fixtures and a custom-source integration script

#### C26. Android fatal-error bridge

- Files: both `misc/error.c` and `include/dxxerror.h`
- Estimated inherited-file reduction: 40-45 from source bodies, 55-65 if Assert/Int3 overrides also move
- Proposed boundary: a clearly noreturn Android fatal writer/finisher API
- Known drift: D2 logs and calls `_exit(1)` after `android_finish_and_exit`; D1 does not
- Tranche rule: define intentional behavior explicitly rather than accidentally preserving conflicting control flow
- Risk: low-medium
- Validation: host crash-file writer and a deliberate debug-build fatal path if practical

### P4. High-coupling or lower-confidence backlog

#### C27. Host migration core

- Files: `d1/main/multi.c:2795-2851`, `d2/main/multi.c:2915-2973`
- Estimated inherited-file reduction: 105-110 lines
- Common behavior: election, reset, object ownership, powerup caps, JSON output, and notification
- Keep local: D2 later ownership transfer
- Risk: high; defer until automated host-disconnect coverage is reliable

#### C28. Android virtual-gamepad registration

- Files: both `arch/sdl/joy.c`, around 79 identical Android lines in `joy_init`
- Estimated inherited-file reduction: 60-100 lines
- Proposed boundary: a shared layout descriptor plus small add-axis/add-button callbacks
- Concern: prior guidance correctly kept SDL adapter logic local. Accept only if the descriptor is clearly smaller and does not expose private arrays.
- Risk: medium
- Validation: joystick menu, axis mapping, controller comparison, keyboard defaults

#### C29. D2 classic-demo JSON serialization

- File: `d2/main/newdemo.c:3974-4493`
- Potential first payoff: 250-350 lines of pure serialization
- Concern: parser state is heavily coupled to private `nd_*` variables and this is D2-only, so merge-surface payoff must justify a descriptor or snapshot model
- Risk: high
- Validation: classic demo dump fixtures and exact JSON comparison

#### C30. Remaining newmenu direct render and touch bodies

- Potential payoff: 250-350 combined lines
- Current blocker: private `newmenu` and `listbox` layouts make callbacks larger than the duplicated code
- Revisit condition: complete C14 first and confirm a generic scaled-render callback naturally owns the common orchestration
- Risk: medium-high

#### C31. Render FOV policy

- Files: D1 `render.c:99-179`, D2 `108-198`
- Potential payoff: 80-90 lines for pure clamp/state/effective-FOV policy
- Keep local: two-pass rendering and game-specific render order
- Risk: medium

#### C32. Net UDP resync request

- Files: near D1 `net_udp.c:3260` and D2 `3314`
- Raw duplicate: roughly 32 lines per game; expected actual reduction only 35-45 combined after transport adapters
- Risk: high relative to payoff
- Status: defer until the queue is near the 20-40-line endpoint

## Deliberate non-candidates

- Branch-added `input_demo_hooks.c`, D1-in-D2 translation files, and D2 custom metadata sinks do not drive the inherited-file metric.
- D2 escort and guidebot route work is substantive feature behavior and currently overlaps active pathing work.
- Deterministic RNG streams in `maths/rand.c` are cross-platform engine behavior and remain authoritative there.
- D2 MVE audio conversion is substantive decoder behavior.
- The 240-278-line inline ETC2/KTX2 upload transaction remains too interleaved with allocation, flags, masks, profiling, and game-specific lookup order. Extract pure lookup helpers, not the transaction.
- Merged-wall `g3_draw_tmap_2` instrumentation already delegates heavily to shared code and still depends on private point signatures and draw order.
- The generic linear-font fallback is cross-platform and tightly coupled to private font macros.
- CMake target-prefix changes are required to build both games in one tree.
- Broad net-UDP extraction and the old welcome-player slot-selection recommendation are stale. Networking requires a fresh narrow boundary each time.
- Remaining private newmenu drawing and hit testing stay local until C14 produces a natural boundary.
- Save serialization layouts and canonical config schemas stay in engine-owned files.

## Small cleanup defects found during the live survey

These should be bundled only after adjacent feature tranches or when the main queue reaches small residuals:

- D2 regrew a local DXA mask loader after an older OGL phase said both games used the shared helper.
- `ogl_texture_android.c` has a nine-argument `ogl_loadtexture` declaration/call versus eight-argument live definitions.
- Two shared OGL runtime helpers are declared and implemented but unused.
- Both `joy.c` files print the same too-many-axes warning twice.
- `d2/maths/rand.c` has an include inside a function.
- Both OGL `gr.c` files contain non-ASCII em-dash characters contrary to repository guidance.
- Both `xmodel/aseread.cpp` files shrink an upstream diagnostic buffer from 64 to 40 bytes without a clear reason; reverting to upstream is safer.
- `android_surface.h` lives only under `d2/arch/include` although both games consume it.
- D1 and D2 fatal-error behavior has unnecessary drift.

## Expected endpoint after the ranked queue

Once C01 through roughly C26 have either been completed or rejected with a concrete boundary analysis, the best credible inherited-file seams should be near the requested stopping size:

- `game.c` frame metrics, about 30-35 lines per game
- `gamerend.c` debug texture overlay, about 28-32 lines per game
- `console.c` Android log forwarding, about 20-22 lines per game
- `piggy.c` pageout diagnostics, about 23-28 lines per game
- `config.c` first-run/default/runtime graphics slices, about 20-35 lines
- `playsave.c` filename selector predicate, about 18 lines per game
- `titles.c` joystick-close and skippable-loop fragments, mostly 7-22 lines
- remaining newmenu geometry/reorder helpers, about 20-40 lines each if C14 succeeds
- `pngfile.h` ETC2 type/API relocation, about 32 additions
- `loadgl.h` repeated macro undefinitions, about 31 lines
- display-size accessors, Android Assert/Int3 override, and `rbaudio.h` declarations, about 20 lines each
- `physfsx.c` at 12 D1 and 21 D2 additions and `hmp.c` at 12 per game, already below the threshold

At that point, further extraction should require a stronger justification than line count. Small high-risk networking, file-format, or private-struct adapters should remain local.

## Processing protocol

For each candidate:

1. Reconfirm the live lines and exact D1/D2 differences.
2. Record target-file numstat against `upstream/main`.
3. Prove the proposed shared API is smaller and clearer than the removed bodies.
4. Write a tranche plan with behavior and ownership guardrails.
5. Implement only that seam using normal translation units.
6. Run scoped quality checks, both-game builds, focused tests, and `git diff --check`.
7. Record exact inherited-file reduction and any environment limitations.
8. Mark the candidate complete, rejected, deferred, or superseded here.
9. Refresh the live queue before selecting the next candidate.

## Initial execution order

The first pass should favor clear ownership and high payoff while avoiding concurrent pathing files:

1. Close C00 validation and metrics. Complete.
2. Take C19's isolated D2 DXA mask regression/signature repair. The detailed audit confirmed a typed callback removes the ABI mismatch while deleting exactly 37 D2 additions.
3. Take C20 masked bitmap scaling as a very-low-risk prerequisite cleanup.
4. Choose between C04 OGL runtime controls and C03 coop packet/statistics based on which focused runtime environment is available.
5. Process C05 EGL lifecycle and C15 viewport separately so failures remain attributable.
6. Process C07 font scale, then C14 kconfig scale, before reconsidering newmenu.
7. Process C02 playsave only after fixture coverage is in place.
8. Keep C01 as a separate diagnostic-ownership campaign with smaller motion/collision/render sub-tranches rather than one thousand-line change.

This order is expected to change after each live rerank.
