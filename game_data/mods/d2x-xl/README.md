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

Sprite materials

The original D2 muzzle strips `muzl02`, `muzl03`, `muzl05`, `muzl06`, `muzl08` and `muzl09` contain additive RGB artwork on black. Their three converted frames each carry a `<image>.ktx2.blend` sidecar containing exactly `additive` followed by a newline. The updated Android renderer accepts this hint only from the selected image's archive and only for a non-rod vclip bitmap that is not also a wall texture. It adds the source color while respecting sprite fading and restoring the previous blend mode. Ordinary RGB transparency guards and supertransparency mask ownership still apply to other artwork

Release publication

`python game_data/mods/d2x-xl/prepare_texture_release.py --publish` validates and publishes the six texture packs, D1/D2 sound packs, D1 menus/fonts, Mac D1 missing-sound pack, UUD1/UUD2 texture fixes and UUD2SP sound fixes to the existing `texture-packs-v1` GitHub release. UUD texture inputs come from the sibling `xfing/dxx_tp/tmp/plain_texture_dxa/` conversion output; UUD2SP comes from `xfing/d2x_sp/uud2sp-sounds.dxa` (see its [conversion instructions](../xfing/d2x_sp/README.md)). Changed assets are replaced; identical uploads are skipped and all remote hashes are verified. The script generates [RELEASE.md](RELEASE.md) as a short recommendation header, download table and credits. Edit release descriptions in the script; omit `--publish` to regenerate notes and metadata locally. Maintainer guidance is in the script's opening comments
