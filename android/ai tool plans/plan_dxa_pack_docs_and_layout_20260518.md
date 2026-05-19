# DXA pack docs and layout plan

## Goal
- Add embedded documentation files to the Xfing and D2X-XL DXA packs
- Repack D2X-XL archives into a more legible subdirectory layout if the runtime can support it
- Avoid expensive texture re-encoding; only reuse already-generated payloads for D2X-XL repacks

## Tasks
- [x] Inspect current Xfing and D2X-XL pack builders plus runtime lookup paths
- [x] Add original RTF and brief README.md to the two Xfing texture DXAs
- [x] Add original and project readmes to D2X-XL DXAs
- [x] Implement D2X-XL repack/subdir layout without re-encoding textures
- [x] Add any runtime ignore/lookup handling needed for docs or subdirs
- [x] Validate repacked archives and compile touched engine/launcher code if needed