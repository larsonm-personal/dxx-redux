# Plan: Pre-compressed ETC2 in DXA files

## Design

### .etc2 file format (custom, simple)
Binary file with a header followed by mip levels:
```
Header (16 bytes):
  magic:     4 bytes  "ETC2"
  width:     uint16   logical width (pixels)
  height:    uint16   logical height (pixels)
  format:    uint8    0 = RGB8_ETC2, 1 = RGBA8_ETC2_EAC
  mip_count: uint8    number of mip levels (1 = base only)
  reserved:  6 bytes  zero (future use)

For each mip level (largest first):
  data_size: uint32   byte size of compressed data
  data:      [bytes]  ETC2 compressed block data
```

### Offline compressor tool
- C++ command-line tool: `etc2tool` 
- Links etc2comp at max effort (100)
- Reads PNG/JPG/TGA via stb_image
- Outputs .etc2 with full mip chain
- Built as a desktop tool (Windows), not part of APK

### DXA builder changes
- convert_d2xxl_textures.ps1: run etc2tool on each texture, store .etc2 in ZIP
- convert_all.ps1: minor updates for new file sizes/descriptions

### Engine changes (d1/d2 ogl.c)
- In ogl_loadbmtexture_f: try .etc2 extension first (before .png/.jpg/.tga)
- New function: read_etc2() that reads the .etc2 file via PhysFS, returns
  pre-compressed data ready for glCompressedTexImage2D
- Remove entire runtime ETC2 compression block from ogl_loadtexture

### Build changes
- Remove etc2comp FetchContent from CMakeLists.txt
- Remove etc2_compress.cpp and etc2_compress.h from build
- Remove EtcLib link from arch_ogl targets

## Status
- [x] Offline etc2tool (tools/etc2tool/ -- builds with cmake, tested)
- [x] DXA builder updates (convert_d2xxl_textures.ps1, convert_all.ps1)
- [x] Engine .etc2 loading (pngfile_stb.c read_etc2_file, d1/d2 ogl.c)
- [x] Remove runtime ETC2 (etc2_compress.cpp/h deleted, CMakeLists.txt cleaned)
- [x] Rebuild DXAs (all 6 texture packs rebuilt, 0 errors except 1 pre-existing RLE TGA)
- [x] Build + lint (APK builds, clang-format passes)
