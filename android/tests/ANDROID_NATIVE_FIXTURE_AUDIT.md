# Android native-fixture owner audit
Source: `addf795999434115466046113be5e360d295d5fd`; source review only, 2026-10-15.

19 complete owners: 11 media-independent default candidates, 7 explicit external-data owners, 1 native-plus-gameplay owner.

## Media-independent default candidates
- `test_android_automap_route_policy`: D1/D2 68/68 trace rows. Production updater is linked with fixture-owned clock, recording/replay state and cache-load/readiness functions. The only file is its generated result trace.
- `test_android_cd_preview_completion`: D1/D2 11/11 trace rows. Writes preview-tail.bin and preview-tail.cue, runs nine controlled-endpoint cases and two actual OpenSL callback cases. Media-independent, but requires a working Android OpenSL output service.
- `test_android_config_policy`: D1/D2 58/58 trace rows. Creates descent.cfg and presence-only descent_ii.gog/descent_ii.inst markers, then exercises real config serialization/defaults. No game archives are decoded.
- `test_android_merge_cache`: D1/D2 124/124 trace rows. Uses in-memory bitmap/texture inputs to exercise actual merged-texture creation, reuse, eviction and cleanup.
- `test_android_merged_wrap`: D1/D2 240/240 trace rows. Uses generated compositor texture data and compares real GL wrapping/state restoration; default non-baseline mode only proposed.
- `test_android_music_completion`: D1/D2 21/21 trace rows. Allocates synthetic MIDI/HMP events and PCM in memory, controls synth output and ring publication, and invokes production producer/callback/lifecycle. No soundfont, song file or real audio device is required.
- `test_android_ogl_batches`: D1/D2 42/42 trace rows. Uses generated glyphs, text and line geometry through actual GLES ordered batching.
- `test_android_redbook_completion`: D1/D2 10/10 trace rows. Writes synthetic constant PCM sectors to redbook-tail.bin and exercises production reader/resampler/SDL producer/callback; the test supplies track state rather than reading external CUE/media.
- `test_android_texture_bindings`: D1/D2 80/80 trace rows. Generates ASE/TGA model/texture fixtures locally, then runs actual enhanced/native interleaving and GL ownership; default non-baseline mode only proposed.
- `test_android_texture_labels`: D1/D2 432/432 trace rows. Generated palette, bitmap pixels, glyphs and geometry. Real GLES3 drawing and framebuffer assertions across two context generations; no external media.
- `test_android_xmodel_mipmaps`: D1/D2 16/16 trace rows. Generates RGB/RGBA POT/NPOT texture/model inputs for actual enhanced-model mipmaps. Default non-baseline mode only proposed.

## Retain data requirements
- `test_android_d1_mod_patches`: Reads original campaign and PIG plus converted UUD1 patches and author levels 03/17/20; generated expectations are derived from original D1 AND D2 source packs, not synthetic replacement data.
- `test_android_ktx2_mipmaps`: Real authored KTX2 pack is mounted and decoded before synthetic LOD/fallback cases; may be public-provisionable, but no media-free exemption.
- `test_android_matcen_stations`: Native metadata fixture is synthetic, but the owner additionally requires full D1/D2 gameplay save/restore, declared pinned retail game dependencies and durable automation PASS/step completeness.
- `test_android_mod_bitmaps`: Compares production bitmap metadata/pixels against independently decoded author PIG and HAM sources; all original banks/palettes must remain intact.
- `test_android_mod_renderer`: Reads real original palettes/game properties and authored UUD/high-res images/masks. PackDirectory can select another external pack directory, not remove content requirements.
- `test_android_mod_save`: Synthetic saves still reference actual stock/UUD game definitions and original D1 palette/assets; no full-owner media-free admission.
- `test_android_mod_sounds`: Reads full stock, author and converted sound banks and compares PCM ownership; host prechecks also require author HAM and original combined texture+sound archive.
- `test_android_secret_origins`: Generates mission descriptors, but production D1/D2 mission loading and D2 properties_init still require pinned original HOG/HAM/S22 bytes.

## Shared build/runtime prerequisites
- PowerShell host with repository helper scripts, dependency_base.txt and adb; one explicitly selected online device/emulator of x86_64, arm64-v8a or armeabi-v7a ABI
- Complete matching Android Debug native build directory retained under android/app/.cxx/Debug/<hash>/<ABI> or explicit NativeBuildDir; matching CMakeCache ABI and actual libdxx-redux-d1.so/libdxx-redux-d2.so. APK-only artifacts are insufficient
- CMake >=3.22 (repository pin 3.31.6), Ninja from CMAKE_MAKE_PROGRAM, Android NDK toolchain (repository pin r30), matching Android platform; C++17
- Build both native engines unless caller explicitly selected NoEngineBuild; still configure/build each standalone D1 and D2 fixture. NoEngineBuild does not skip fixture builds
- Retained native _deps source headers for physfs, sdl12 and nlohmann_json, plus all required engine dependency shared libraries and libc++_shared.so
- adb push/execute in unique /data/local/tmp/dxx-<fixture>-<guid> directory, per-game 120-second native process deadline, pull trace JSON and enforce unchanged exact per-game record counts
- Repository master timeout is 600 seconds for every audited owner; actual build/runtime duration remains unmeasured

## Scope and limitations
- No PowerShell, CMake build, native test or Android process was executed for this audit
- Media-independent means all required runtime media inputs are generated or supplied in memory; it does not imply hardware-independent or runtime-qualified
- Proposed eligibility is exact default owner only; Baseline and arbitrary NativeBuildDir/NativeLibraryDir/NoEngineBuild/other caller variants are not silently admitted
- Master media preflight currently must explicitly exempt reviewed owners before a no-retail run; helper source audit alone does not change policy
- Case totals are trace rows per engine, not a count of all nested assertions; preserve complete fixture matrices

Detailed exact wrapper/native evidence, source line numbers, DataFiles and transitive inputs are in `android_native_fixture_owner_audit.json`.

## Renderer-specific requirement
- Texture-family and mod_renderer fixtures require ANDROID_USE_OPENGL=true (OPENGL/OPENGLMERGE), EGL/GLES3 and actual pbuffer/framebuffer support. Software-renderer libraries are insufficient.
- Optional baseline variants are also media-independent but are different contracts, sometimes with relaxed checks. They are not proposed for default-owner admission.
