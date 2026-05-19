# Generic DXA Texture Paths 20260518

## Goal
Remove Xfing-specific and inaccurate palette naming from DXA-internal lookup paths and source-code texture lookup, while keeping patch files generic and descriptive.

## Tasks
- [x] Rename DXA texture root and D2 texture-set directories
- [x] Update manifests and verifier to use generic metadata paths
- [x] Update D2 engine lookup to use the generic texture-set path
- [x] Regenerate and verify DXAs
- [x] Run scoped quality and build checks

## Direction
- Use `textures/d2/sets/<set>/` for D2 texture replacements instead of `xfing-textures/d2/palettes/<palette>/`
- Use `metadata/manifest.json` for the archive manifest
- Use `patches/...` for RFC 6902 patch files and summaries
- Do not add `xfing` to source-code lookup paths

## Results
- Generated archives use `uud1tp-textures.dxa` and `uud2tp-textures.dxa`
- Archive lookup paths use `textures/`, `metadata/`, and `patches/`
- D2 texture sets use `textures/d2/sets/<set>/`, with extra bitmap entries named `idxNNNN.png`
- D2 source lookup is generic and does not contain Xfing-specific paths
- Conversion and verification completed for both archives
- Scoped code quality completed successfully
- Windows CMake build completed for D1 and D2
- CTest completed for `buildd1` and `buildd2`; no tests are registered in those build dirs
- Android D2 native CMake build completed for `arm64-v8a`
