# PNG palette and menu background backport

Bring in deferred upstream `ceba2c7a712eb53102e707c316d222bacb580da8` while preserving Android replacement lookup, visual policy, filtering, transparency, and cache behavior

- [x] Trace upstream changes and local desktop/Android decoder and menu lifecycles
- [x] Add upstream's optional bitmap-name parameter and named scores background loading in both games
- [x] Expand desktop indexed PNGs to RGB/RGBA before upload, preserving low-bit-depth pixels and tRNS transparency
- [x] Add integration fixtures for palette colors, transparency, and menu texture reload/fallback
- [x] Run scoped quality checks, both Windows builds/tests, and Android arm64 native builds
- [x] Record completed scope, merge notes, and validation limits

## Design

Use upstream's three-argument `ogl_loadbmtexture_f()` signature. Request the `scores` replacement before drawing the framed background, including cached draws, so GL cache invalidation does not lose its explicit name. The existing loaded-texture early return avoids repeated lookup/upload

Expand palettes in the desktop libpng reader, not manually in the GL loader. The upstream manual RGB conversion loses tRNS alpha and leaves stale `paletted` metadata for mask generation. Libpng can expand 1/2/4/8-bit palettes and tRNS into consistent RGB/RGBA; refresh the output layout before allocating rows. Android already returns expanded pixels through stb_image and needs no decoder change

Reference: [upstream patch](https://github.com/dxx-redux/dxx-redux/commit/ceba2c7a712eb53102e707c316d222bacb580da8), [libpng transformation and row-layout documentation](https://libpng.org/pub/png/libpng-manual.txt)

Preserve the stock PCX for layout and fallback, local palette invalidation, replacement permissions, and texture filtering. Supertransparency follows expanded RGB's existing marker-color rule, consistent with the Android decoder

## Implementation and future merges

- Both games now request `scores` through the existing replacement loader. Desktop uses `scores.png`; Android retains its existing game-specific paths, extension precedence, compressed texture support, and visual policy
- Keep the named load on cached menu draws. Before reloading an invalidated menu texture, discard its stale descriptor so a removed replacement falls back to the PCX dimensions
- Keep decoder expansion instead of upstream's manual palette-copy loop. It handles packed indices, interlacing, and tRNS correctly and leaves the GL mask path with consistent channel metadata
- Keep the upload format derived from the decoded PNG's alpha channel. An opaque RGB replacement must not inherit an RGBA upload layout from the stock bitmap's transparency flags. Reinitialize that layout and the replacement dimensions when uploading into an invalidated texture descriptor
- Retain the read-buffer overflow/allocation checks and volatile row-pointer cleanup across libpng errors
- No broad engine refactor, new runtime dependency, or Android decoder change was needed. The upstream API signature and caller updates match the original change; the lifecycle and palette adaptations above deliberately differ

## Validation

- `run-windows-build.ps1 -Target both`: D1 and D2 Windows builds passed
- All 107 CTest tests passed: 50 D1, 57 D2
- Extended `test_upstream_compat` exercises actual desktop decoding of synthetic 1/2/4/8-bit indexed PNGs, each with/without transparency and interlacing; verifies RGB/RGBA pixels, partial alpha, implicit opaque palette entries, truecolor input, truncated input, and missing-file failure
- Both games passed the optional `test_upstream_compat --graphics` mode with a real desktop OpenGL context. It verifies the actual menu draw path, GPU texture pixels, alpha, cached handle reuse, cache invalidation, stock dimensions after removing a replacement, invalid-file fallback, multiplayer permissions, filtering, RGB upload stride, and supertransparency masks
- Android arm64 Debug native builds passed for both games. Existing unrelated warnings appeared during the initial broad rebuild; final incremental builds produced no new compiler warnings
- `python android/tests/test_replacement_texture_limits.py`: passed
- Scoped mixed-language quality invocation passed; the script excludes upstream engine source and native test fixtures, so the extended fixture was separately formatted and checked with pinned clang-format 20. `git diff --check` passed

To rerun graphics coverage after building, run `..\test_upstream_compat.exe --graphics` from each game's `buildd1/main/upstream-compat-fixtures` or `buildd2/main/upstream-compat-fixtures` directory. The test creates its own PNG/PCX fixtures and briefly opens a renderer window; no proprietary game data is needed. Normal CTest runs decoder coverage without requiring a display

Logs are under `temp/png-menu-*`. Android device rendering and Linux/macOS builds were not exercised. The two other original deferrals (D1 monitor handling and Autoselect Only Once) remain outside this change
