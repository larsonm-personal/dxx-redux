# Plan: etc2tool logging + .dxa rebuild

## Phase 1: Add timing/progress logging to etc2tool [DONE]
- Added `<chrono>` for wall-clock timing
- Start message: `ETC2: <path> <WxH> <fmt> <N> mips`
- Per-mip timing: `mip N: WxH -> bytes (ms)`
- Final OK line now includes total elapsed ms
- All output to stdout with fflush() for immediate visibility

## Phase 2: Update conversion scripts for visible progress [DONE]
- convert_d2xxl_textures.ps1: progress every 10 textures (was 50)
- convert_all.ps1 Convert-AndAdd: added merge progress counter every 10

## Phase 3: Rebuild etc2tool [DONE]
- Built successfully, verified with 256x256 (14ms) and 512x512 (2437ms) test images
- Fixed KHRONOS_STATIC redefinition warning

## Phase 4: Full .dxa rebuild [DONE]
- Ran convert_all.ps1 with `Tee-Object` for visible + logged output
- Completed in ~1 hour (vs previous 8+ hour hung run)
- 2558 textures compressed, zero errors
- KTX2 magic bytes verified on sample entries

### Output sizes (KTX2)
| Pack | Entries | Size |
|------|---------|------|
| d1-128 | 287 ktx2 | 3.8 MB |
| d1-256 | 309 ktx2 | 36.7 MB |
| d1-512 | 337 ktx2 | 262.8 MB |
| d2-128 | 469 ktx2 | 5.7 MB |
| d2-256 | 568 ktx2 | 124.0 MB |
| d2-512 | 588 ktx2 | 454.7 MB |

Note: entry counts differ across sizes because animation strips are only
split into individual frames in the 128px pack (which uses ImageMagick).
The 256 and 512 packs store strips as single tall textures.
