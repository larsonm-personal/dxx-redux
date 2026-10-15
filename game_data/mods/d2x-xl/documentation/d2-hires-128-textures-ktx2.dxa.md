# D2 High-Res Level Textures 128px Redux adaptation notes

## Original artwork

Original D2X-XL community artwork credited to Aus-RED-5D, DizzyRox, MetalBeast, Novacron and Theftbot. Redux adapts the existing artwork for its renderer; these notes are written by the Redux project.

The retained source archives contain no original documentation files. Their exact identities are listed below; this file is not an original author README.

## What DXX-Revival changed

- Converted TGA images to GPU-compressed ETC2 textures in KTX2 containers, with mipmaps for distant rendering
- Split animation strips into separate frames and mapped affected strips to the game's frame counts, preserving endpoints for one-shot animations and sampling complete cycles for loops
- Preserved source alpha and converted the XL supertransparency color keys into separate masks
- Used game-specific texture paths and corrected selected source names to match the game's bitmap lookup

The 128px pack downsizes the 512px artwork and supplements missing artwork from the 256px source. Resizing uses linear-light Lanczos filtering and a small sharpening pass. The size is a maximum, not a promise that every texture has that resolution: smaller source images are not enlarged. D2 boss/arrow frame names are mapped to the game names. The green targeting variant is selected. Muzzle-flash frames carry additive blending hints so their black backgrounds blend correctly.

This is a texture adaptation, not an implementation of all D2X-XL features. XL-only and unmatched artwork does not become functional merely because it appears in the source pack. It does not replace menus, fonts, models, or sound effects.

## Using this pack

Use an Android Redux build with ETC2/KTX2, mask and applicable sprite-material support. Applies to D2. Enable only one D2 high-resolution texture resolution at a time, and restart the game session after changing mods. Put UUD texture fixes above this pack if you want their original-resolution corrected artwork to take precedence. Higher mods mask matching assets in lower mods.

## Conversion provenance

[Conversion implementation at the audited revision](https://github.com/larsonm-personal/dxx-redux/tree/c42522f9c1f4e795d717a564712e784cd32af33e/game_data/mods/d2x-xl) includes `convert_all.ps1`, `convert_d2xxl_textures.ps1`, and `d2xxl_pack_lib.ps1`.

Retained original source archives and SHA-256:

- `D2-textures-256x256.7z`: `5a464b61f91f2b25d446f3f93086eebc8db8b5551acb78acf630c1b2641e6e50`
- `D2-textures-512x512.7z`: `8b128f99ad512f142b4d4f7103da5e51be18bc85ef482d6374e68bd7ba83125a`
