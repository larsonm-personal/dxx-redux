# Plan: Hires Transparency Polish, Performance Monitoring, Advanced Graphics

## Status key
- [ ] Not started
- [~] In progress
- [x] Done

---

## Phase 1: Hires super-transparency -- current state and remaining issues

### What's working
- [x] Mask pipeline: `convert_d2xxl_textures.ps1` detects key color #785880 (+/-13 tolerance) in 32-bit TGAs, generates `*_mask.png` files, packages into DXA alongside KTX2
- [x] Mask loading: `ogl_load_dxa_mask()` in d1/d2 reads mask PNGs from DXA, converts to single-byte buffer, uploads via BM_FLAG_TRANSPARENT palette path
- [x] Mask polarity: dark pixels (super-transparent) -> byte 255 -> BM_FLAG_TRANSPARENT maps to alpha=0 -> shader discards. Fixed polarity inversion
- [x] Mask shader: `tex2m` program multiplies `c.a * mask.a`, discards when < 0.02
- [x] `piggy_bitmap_get_flags()` accessor: returns real flags from GameBitmapFlags[] during cache pass when bm_flags is BM_FLAG_PAGED_OUT
- [x] Crash fix: null check on `bmovl->gltexture_mask` before dereferencing
- [x] Desktop safety: `real_flags` guarded by `#ifdef OGL_MERGE`, `ogl_load_dxa_mask` guarded by `#ifdef ANDROID`, all counters guarded. No impact on desktop builds
- [x] Generalization: mask loading triggers on any `BM_FLAG_SUPER_TRANSPARENT` bitmap, not just door35. 273 mask PNGs in the DXA, 200 masks load during cache

### Remaining: door35 edge artifacts
The door35 texture shows minor visual artifacts:
- Lower-left corner: a small extra bit of rock35 visible
- Some door edges: extra red color bleeding

**Root cause analysis**: Three contributing factors, all in the pipeline:

1. **Key color tolerance range** (+/-13 in each channel around R=120,G=88,B=128). Edge pixels in the original TGA may be anti-aliased blends between the key color and the door texture. Pixels that are close-but-outside the tolerance show as colored fringes; pixels close-but-inside get incorrectly masked.

2. **Mask resizing**: When `MaxDim` is set (e.g. 256), the mask PNG is resized with ImageMagick which introduces anti-aliased gray pixels at boundaries. The `> 128` threshold in ogl_load_dxa_mask roughly handles this, but sub-pixel accuracy is lost.

3. **ETC2 block compression**: The main KTX2 texture uses 4x4 block compression. At edges between key-color-zeroed pixels (RGBA=0,0,0,0) and door pixels, the block compressor blends, creating visible color fringes in decompressed output.

**Mitigation options** (ordered by effort):

- [x] **A. Pipeline: nearest-neighbor mask resize**. Use `-filter Point` in ImageMagick for mask PNGs so the binary mask stays crisp at any resolution. This is the highest-impact fix for the fuzziness.

- [x] **B. Pipeline: edge color flood-fill**. After zeroing key-color pixels, flood-fill their RGB from neighboring non-key pixels so ETC2 block compression doesn't average against black. This is a standard generic technique -- no per-texture tuning needed. Reduces color fringing at transparency boundaries.

- ~~**C. Wider tolerance**~~: Skipped -- needs per-texture tuning, want to keep the d2x-xl->rebirth import generic.

- ~~**D. Accept as minor artifact**~~: Not needed if A+B work.

---

## Phase 2: Performance monitoring and load measurement

### Goal
Expose CPU frame time + GPU utilization metrics in the video overlay so the user can compare texture pack sizes (base, 128, 256, 512) on real hardware and make recommendations.

### What already exists
- `g_current_fps` in game.c: 1-second rolling FPS counter (Android-only)
- `r_polyc`, `r_tpolyc`, `r_bitmapc`: per-frame draw counters (reset in ogl_start_frame)
- `ogl_get_texture_bytes()`: total GPU texture memory estimate
- `VideoInfoOverlay.kt`: shows FPS, hires count, tex memory, resolution
- `nativeGetVideoStats()` JNI: passes 11 int array from C to Kotlin

### What to add

#### 2A. Frame timing (CPU-side)
- [x] Add `g_frame_time_us` (microseconds for last frame) alongside `g_current_fps`
- [x] Add `g_frame_time_avg_us` (rolling average over last 60 frames)
- [x] Add `g_frame_time_max_us` (max over last 60 frames) -- shows stutter/spikes
- [x] Track `g_cache_time_ms` (time spent in ogl_cache_level_textures)
- [x] Expose all via nativeGetVideoStats extended array (indices 11-17)

Why microseconds: at 30fps a frame is 33ms. Per-frame timing at ms resolution is too coarse; us gives useful granularity for spotting 1-2ms differences between texture packs.

#### 2B. Draw call and state change counters
- [x] Add counters already partially there: `r_polyc` (total polys), `r_tpolyc` (textured polys), `r_bitmapc` (bitmap draws)
- [x] Add `r_texbinds` (texture bind calls per frame) -- main indicator of state change overhead
- [x] Expose via nativeGetVideoStats (r_polyc+r_tpolyc as draw polys)
- [ ] Add `r_shader_switches` (program use calls per frame) -- currently only 2 programs so this should be low
- [ ] Add `r_mask_draws` (super-transparent mask draws per frame) -- tracks mask overhead

#### 2C. GPU timing (GLES 3.0)
- [ ] Use `EXT_disjoint_timer_query` (widely supported on real hardware, may not work on emulator)
- [ ] Measure GPU time for the main render pass (ogl_start_frame to ogl_end_frame)
- [ ] Display as "GPU: Xms" in overlay
- [ ] Fallback: if extension not available, show "GPU: n/a"
- [ ] Note: the emulator (swiftshader) may not support this extension. Test on real hardware

#### 2D. Overlay enhancements
- [x] Add to VideoInfoOverlay: frame time (avg/max), draw calls, tex binds, cache time
- [ ] Add a "load bar" visual: green/yellow/red bar showing frame budget usage (33ms = 100% at 30fps)
- [ ] Show texture pack name and size (from DXA filename)
- [ ] Color-code metrics that are near budget limits

#### 2E. Benchmark mode
Deferred -- will be built around demo file playback. User will create demo files; benchmarks captured automatically during demo playback as part of regression tests (verify enemies killed, etc.).

---

## Phase 3: Light performance optimization

### Easy wins from code survey

#### 3A. Redundant GL state changes
In `ogl_start_frame()` + `ogl_end_frame()`, several GL state changes happen every frame that could be set-once:
- [ ] `glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)` -- set once at init unless changed
- [ ] `glEnable(GL_BLEND)` -- set once if never disabled
- [ ] `glCullFace(GL_FRONT)` -- set once
- [ ] Audit which GL state actually changes frame-to-frame vs is just re-set

#### 3B. Texture binding reduction
Currently every polygon binds its texture individually. Profile to see if adjacent polygons often share textures:
- [x] Add a "last bound texture" cache: skip glBindTexture if same handle
- [x] Track cache hit rate in r_texbinds counter
- [x] This is the single most impactful optimization for fill-rate-limited devices

#### 3C. Mipmap generation
- [ ] Verify mipmaps are generated for hires textures (glGenerateMipmap)
- [ ] With 512px textures, missing mipmaps = massive overshading on distant surfaces
- [ ] Check if `ogl_loadtexture` calls `glGenerateMipmap` for PNG/KTX2 paths

#### 3D. KTX2 mipmap chain
- [ ] etc2tool currently generates single-level KTX2 (no mip chain)
- [ ] For 512px textures, pre-generating mip levels in the .ktx2 would avoid runtime `glGenerateMipmap` cost
- [ ] This is a pipeline improvement, not a runtime change

#### 3E. Framebuffer format
- [ ] Currently using RGB565 (16-bit): `EGL_RED_SIZE 5, GREEN 6, BLUE 5`
- [ ] RGB565 is fast but may cause banding with some textures
- [ ] For quality mode: consider RGBA8888. Adds bandwidth cost but improves color accuracy
- [ ] Make this a setting

#### 3F. Shader efficiency
- [ ] Two shaders only (tex2, tex2m). Both are simple -- no obvious inefficiency
- [ ] For future: if adding post-processing, use a single fullscreen quad pass
- [ ] Consider if the tex2 (no mask) path can be made the common case during gameplay (it already is for most surfaces)

---

## Phase 4: Anti-aliasing and anisotropic filtering

### 4A. Anisotropic filtering (easy, high impact)
Already partially wired: `ogl_maxanisotropy` exists, `glTexParameterf(GL_TEXTURE_MAX_ANISOTROPY_EXT)` is called in ogl_loadtexture.

- [ ] Query `GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT` at init, store max supported level
- [ ] Add a setting: Off / 2x / 4x / 8x / 16x (capped by device max)
- [ ] Apply to all textures during ogl_loadtexture
- [ ] Add to launcher advanced settings
- [ ] Add cycle button in video overlay

Note: this is essentially free on modern GPUs for Descent's rendering complexity. Very high impact for long tunnel views.

### 4B. MSAA (medium effort, high visual impact)

Implementation plan:
- [ ] Create MSAA renderbuffer at EGL config time: add `EGL_SAMPLES, N` to config attributes
- [ ] Alternatively, create an MSAA FBO and resolve to default framebuffer (more control)
- [ ] Settings: Off / 2x / 4x / 8x (query `GL_MAX_SAMPLES`)
- [ ] EGL approach is simpler but less flexible; FBO approach allows runtime toggle
- [ ] Recommend: FBO approach so users can change MSAA without restarting

FBO MSAA steps:
1. Create color renderbuffer with `glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA8, w, h)`
2. Create depth renderbuffer with `glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH_COMPONENT16, w, h)`
3. Attach both to FBO
4. Render scene to MSAA FBO
5. `glBlitFramebuffer` to resolve to default framebuffer
6. Display

- [ ] Add to launcher advanced settings
- [ ] Add cycle button in video overlay
- [ ] Track performance impact via frame time counters

### 4C. Launcher advanced settings UI
- [ ] Add "Graphics" section to advanced settings page
- [ ] Anti-aliasing dropdown: Off / 2x / 4x / 8x MSAA
- [ ] Texture filtering dropdown: Bilinear / Trilinear / 2x AF / 4x / 8x / 16x AF
- [ ] Settings written to game config file, read by engine at startup
- [ ] Live controls: cycle buttons in video overlay for AA and AF

### 4D. Live overlay controls
- [ ] Add touchable cycle buttons to VideoInfoOverlay for:
  - AA level (cycle through supported MSAA levels)
  - AF level (cycle through supported anisotropy levels)
  - Texture pack (if multiple installed)
- [ ] Show current setting + performance impact in real time
- [ ] Changes take effect immediately (AF is per-texture-bind, MSAA requires FBO resize)

---

## Phase 5: Advanced graphics options -- practicality assessment

Based on actual codebase analysis, here's what's practical vs aspirational:

### Practical (low-medium effort, proven benefit)
| Feature | Effort | Benefit | Notes |
|---------|--------|---------|-------|
| Anisotropic filtering | Low | High | Already half-wired. Tunnels benefit enormously |
| MSAA | Medium | High | FBO approach well-understood. GLES 3.0 guarantees it |
| Gamma/brightness | Low | Medium | Simple post-process or glClearColor adjustment |

*Texture quality slider: not needed -- users install whatever texture pack they want.*
*Post-processing pipeline: deferred -- not needed as of now.*

### Achievable but significant effort
| Feature | Effort | Benefit | Notes |
|---------|--------|---------|-------|
| Post-processing pipeline | Medium | Medium | Need FBO (same as MSAA). Bloom, CRT filter, color grading possible. Descent's aesthetic suits CRT/scanline filters |
| Instancing | Medium | Low-Medium | Descent has many small objects but draw counts are already modest (~2000 polys/frame). Benefit is mainly for particle-heavy scenes |

### Impractical or low-value for this codebase
| Feature | Effort | Concern |
|---------|--------|---------|
| Shadow mapping | High | Engine has no shadow infrastructure. Light is per-vertex. Adding would require significant renderer changes and artist work for shadow-casting geometry. Not worth it for a faithful port |
| Occlusion queries | Medium | Engine already does portal-based visibility (segment rendering). GL occlusion queries would add latency (query results are async) and the engine's existing PVS is already efficient for indoor geometry |
| 2D array textures | High | Would require rewriting the texture binding model. Every polygon currently binds its own texture. Batching into texture arrays needs sorting, atlas management, and shader changes. Major refactor for modest gain |
| Uniform buffer objects | Low-Medium | Only 2 shaders, few uniforms. The overhead of per-draw uniform calls is negligible at current draw counts. Not worth the complexity |

### Summary
The high-value path is: **pipeline edge fixes -> perf counters -> texture bind cache -> anisotropic filtering -> overlay enhancements -> MSAA**.

---

## Phase 6: Implementation order

1. [x] Phase 1A+1B: Pipeline edge fixes (nearest-neighbor mask resize + edge color flood)
2. [x] Phase 2A-2B: Frame timing + draw counters
3. [x] Phase 3B: Texture bind cache (easy win, enables measurement)
4. [ ] Phase 4A: Anisotropic filtering (already 80% wired)
5. [x] Phase 2D: Overlay enhancements (basic metrics shown)
6. [ ] Phase 4B: MSAA via FBO
7. [ ] Phase 2C: GPU timing (real hardware only)
8. [ ] Phase 4C-4D: Launcher settings + live overlay controls
9. [ ] Phase 3A,3C-3F: Other optimizations as needed based on profiling data
