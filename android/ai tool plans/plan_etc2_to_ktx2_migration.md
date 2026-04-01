# Plan: Migrate ETC2 Textures from Custom Format to KTX2

## Motivation
The current .etc2 files use a custom 16-byte header that no viewer can open.
Switching to KTX2 (Khronos Texture 2.0) allows hand-verification via standard
viewers. The current textures may be malformed (possible cause of all-black 3D
textures on device).

## Decisions
- Container: KTX2 (per user request)
- Libraries: KTX-Software v4.4.2 (Khronos official) on both writer and reader
- Mipmaps: keep (d1 uploads all levels; future PC merge)
- Original dimensions: stored in KTX2 key-value metadata ("OrigWidth", "OrigHeight")
- File extension: .ktx2 (inside .dxa archives and on disk)

## Architecture

### Current flow
```
PNG/TGA -> etc2tool -> .etc2 (custom) -> .dxa (ZIP) -> PhysFS -> read_etc2_file -> glCompressedTexImage2D
```

### New flow
```
PNG/TGA -> etc2tool -> .ktx2 (standard) -> .dxa (ZIP) -> PhysFS -> read_ktx2_file -> glCompressedTexImage2D
```

## KTX2 format features used
- VkFormat: VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK (147) or
            VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK (151)
- pixelWidth/pixelHeight = pow2-padded dimensions (what glCompressedTexImage2D needs)
- numberOfMipmapLevels = mip_count
- Key-value metadata: "OrigWidth" (uint16 LE), "OrigHeight" (uint16 LE) for UV calc
- No supercompression (data is already ETC2 compressed)

## Changes

### Phase 1: etc2tool writer (tools/etc2tool/) -- DONE
- Add KTX-Software v4.4.2 via FetchContent (pin to git tag v4.4.2)
- Build with KTX_FEATURE_TOOLS=OFF, KTX_FEATURE_TESTS=OFF, KTX_FEATURE_DOC=OFF
- Replace custom .etc2 file writing with KTX2 via libktx:
  - ktxTexture2_Create() with VkFormat for ETC2
  - ktxTexture_SetImageFromMemory() for each mip level
  - ktxHashList_AddKVPair() for "OrigWidth" / "OrigHeight"
  - ktxTexture_WriteToNamedFile() or WriteToMemory()
- Output files are now .ktx2
- Keep etc2comp for actual compression (unchanged)
- Update etc2tool CLI: change default output extension to .ktx2

### Phase 2: Game reader (android/app/src/main/cpp/shared/) -- DONE
- Add KTX-Software v4.4.2 via FetchContent in android CMakeLists.txt
  - Build read-only: set(LIBKTX_VERSION_READ_ONLY ON)
  - BUILD_SHARED_LIBS=OFF (static)
  - KTX_FEATURE_TOOLS=OFF, KTX_FEATURE_TESTS=OFF, etc.
- In pngfile_stb.c: replace read_etc2_file() with read_ktx2_file()
  - Read full file via PhysFS into memory
  - ktxTexture2_CreateFromMemory() to parse
  - Extract format (map VkFormat -> our 0/1 enum), width, height, mip_count
  - Extract "OrigWidth"/"OrigHeight" key-value metadata
  - Pack per-mip data into [uint32_le size][data] buffer (same format ogl.c expects)
  - ktxTexture_Destroy() the KTX2 handle
  - Return populated struct
- Struct: rename etc2_file_data -> ktx2_tex_data (or keep for compat, rename later)

### Phase 3: ogl.c changes (d1/ and d2/) -- DONE
- Change filename lookup: "%s.etc2" -> "%s.ktx2"
- Change function call: read_etc2_file -> read_ktx2_file
- All GL upload code stays the same (same struct, same data format)
- D1 mip loop stays as-is (uploads all levels)
- D2 single-level upload stays as-is

### Phase 4: Header updates (d1/include/pngfile.h, d2/include/pngfile.h) -- DONE
- Rename struct and function declarations
- Or add new declarations alongside old ones with #ifdef

### Phase 5: Conversion scripts -- DONE
- convert_d2xxl_textures.ps1: change .etc2 -> .ktx2 in ZIP entry names
- convert_all.ps1: update Get-DxaEtc2Names -> work with .ktx2
- etc2tool invocation: output.etc2 -> output.ktx2

### Phase 6: Rebuild .dxa archives
- Run convert_all.ps1 with updated etc2tool to produce new .dxa files
- These contain .ktx2 files viewable in VS Code compressed texture viewer

### Phase 7: Build and test -- DONE (etc2tool + APK + code quality)
- etc2tool builds and produces valid KTX2 (verified magic bytes)
- APK builds cleanly with no new warnings
- Code quality checks pass
- Build etc2tool: cmake -B build && cmake --build build --config Release
- Build APK: assembleDebug
- Run code quality: run-code-quality.ps1 --fix
- Test on emulator with new .dxa files
- Manually verify a few .ktx2 files in VS Code viewer

## Key-value metadata format
KTX2 key-value pairs are:
- uint32 keyAndValueByteLength
- NUL-terminated key string
- value bytes (padded to 4-byte alignment)

We store:
- Key: "OrigWidth\0", Value: uint16 LE (original image width before pow2 padding)
- Key: "OrigHeight\0", Value: uint16 LE (original image height before pow2 padding)

These replace the orig_width/orig_height fields from the custom .etc2 header.

## KTX-Software FetchContent config (both CMakeLists)
```cmake
FetchContent_Declare(ktx_software
    GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software.git
    GIT_TAG        v4.4.2
    GIT_SHALLOW    TRUE
)
set(KTX_FEATURE_TOOLS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_DOC OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_JNI OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_PY OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(ktx_software)
```

## Risk notes
- KTX-Software is a large dependency. Build times will increase
- The library pulls in Basis Universal code even in minimal config
- On Android, static linking keeps APK size impact minimal (LTO strips unused)
- If KTX-Software proves too heavy, fallback: write a minimal KTX2 writer
  in etc2tool (spec is simple for uncompressed ETC2 data) and keep KTX-Software
  only on the etc2tool side. But user prefers library on both sides

## Status
- [ ] Phase 1: etc2tool KTX2 writer
- [ ] Phase 2: Game reader (pngfile_stb.c)
- [ ] Phase 3: ogl.c changes (d1 + d2)
- [ ] Phase 4: Header updates
- [ ] Phase 5: Conversion script updates
- [ ] Phase 6: Rebuild .dxa archives
- [ ] Phase 7: Build and test
