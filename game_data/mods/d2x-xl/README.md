DXA texture packs

This workspace contains the D2X-XL high-resolution texture and sound pack conversion scripts, source archives, and generated DXA files.

D2
- d2-hires-512-textures-ktx2.dxa: High-resolution textures from d2x-xl by Aus-RED-5D, DizzyRox, MetalBeast, Novacron, Theftbot
- d2-hires-256-textures-ktx2.dxa: High-resolution textures from d2x-xl by Aus-RED-5D, DizzyRox, MetalBeast, Novacron, Theftbot
- d2-hires-128-textures-ktx2.dxa: Downscaled from the 512x512 textures from d2x-xl by Aus-RED-5D, DizzyRox, MetalBeast, Novacron, Theftbot

D1
- d1-hires-512-textures-ktx2.dxa: High-resolution textures from d2x-xl by DizzyRox, Novacron, Aus-RED-5
- d1-hires-256-textures-ktx2.dxa: High-resolution textures from d2x-xl by DizzyRox, Novacron, Aus-RED-5
- d1-hires-128-textures-ktx2.dxa: Downscaled from the 512x512 textures from d2x-xl by DizzyRox, Novacron, Aus-RED-5

DXA sound packs
- d2-hires-sounds.dxa
- d1-hires-sounds.dxa

These DXA files are converted from the d2x-xl hires texture and sound packs stored under `d2x-xl/`.
Texture packs use KTX2 containers with ETC2 compression for mobile/Android.
The conversion scripts `convert_all.ps1`, `convert_d2xxl_textures.ps1`, and `convert_d2xxl_sounds.ps1` repackage the original 7z archives into DXA format for dxx-redux on Android.
