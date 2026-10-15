# D1 High-Res Level Textures 128px Redux adaptation notes

## Original artwork

Original D2X-XL community artwork credited to DizzyRox, Novacron and Aus-RED-5. Redux adapts the existing artwork for its renderer; these notes are written by the Redux project.

The retained source archives contain no original documentation files. Their exact identities are listed below; this file is not an original author README.

## What DXX-Revival changed

- Converted TGA images to GPU-compressed ETC2 textures in KTX2 containers, with mipmaps for distant rendering
- Split animation strips into separate frames and mapped affected strips to the game's frame counts, preserving endpoints for one-shot animations and sampling complete cycles for loops
- Preserved source alpha and converted the XL supertransparency color keys into separate masks
- Used game-specific texture paths and corrected selected source names to match the game's bitmap lookup

The 128px pack downsizes the 512px artwork and supplements missing artwork from the 256px source. Resizing uses linear-light Lanczos filtering and a small sharpening pass. The size is a maximum, not a promise that every texture has that resolution: smaller source images are not enlarged. D1 textures use D1-specific archive paths so the same pack can serve D1 and native D1-in-D2 without replacing D2 artwork.

This is a texture adaptation, not an implementation of all D2X-XL features. XL-only and unmatched artwork does not become functional merely because it appears in the source pack. It does not replace menus, fonts, models, or sound effects.

## Using this pack

Use an Android Redux build with ETC2/KTX2, mask and applicable sprite-material support. Applies to D1 and native D1-in-D2. Enable only one D1 high-resolution texture resolution at a time, and restart the game session after changing mods. Put UUD texture fixes above this pack if you want their original-resolution corrected artwork to take precedence. Higher mods mask matching assets in lower mods.

## Conversion provenance

[Conversion implementation at the audited revision](https://github.com/larsonm-personal/dxx-redux/tree/c42522f9c1f4e795d717a564712e784cd32af33e/game_data/mods/d2x-xl) includes `convert_all.ps1`, `convert_d2xxl_textures.ps1`, and `d2xxl_pack_lib.ps1`.

Retained original source archives and SHA-256:

- `D1-textures-256x256.7z`: `e871f07125371bfdd861aa935d5ba974861e989045fefb7f6bb533bcc2ab8db0`
- `D1-textures-512x512.7z`: `febf7e39b1cd32aa4cd4c94de9657dc7a429f5c73377d05cb109f51a595c7a26`
