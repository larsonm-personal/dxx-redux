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

`python game_data/mods/d2x-xl/prepare_texture_release.py --publish` prepares, validates and publishes the six texture packs, D1/D2 sound packs, D1 menus/fonts, Mac D1 missing-sound pack, UUD1/UUD2 texture fixes and UUD2SP sound fixes to the existing `texture-packs-v1` GitHub release. UUD texture inputs come from the sibling `xfing/dxx_tp/tmp/plain_texture_dxa/` conversion output; UUD2SP comes from `xfing/d2x_sp/uud2sp-sounds.dxa` (see its [conversion instructions](../xfing/d2x_sp/README.md)). Changed assets are replaced; identical uploads are skipped and all remote hashes are verified. The script generates [RELEASE.md](RELEASE.md) with mod and documentation downloads. Omit `--publish` to prepare and validate locally

Documentation preparation

Maintain the eleven adapted packs' user-facing notes in `documentation/<mod-filename>.md`. Release preparation replaces only recognized Redux-generated placeholders with that pack's `README.dxx-redux.md`. It preserves every other entry's path and uncompressed bytes, including original author documents and gameplay payloads. It does not rerun image/audio conversion. Converter outputs stay intact; final documented DXAs are staged under `<output>/packs/`. Publish these staged packs through the script, rather than uploading conversion outputs directly

`documentation-policy.json` pins original document hashes, both untouched Rebirth archive hashes, and the audited generated placeholders that may be replaced. An unrecognized placeholder fails preparation instead of silently deleting possible author content. When converter-generated prose changes, review its ownership before updating the corresponding allowed hash. The policy applies to final documentation assembly; no legacy launcher reader or data migration is involved

Each of the thirteen mods gets `<mod-filename>.readme.zip`, containing the final pack's document paths and exact bytes. Companions use fixed ZIP timestamps and deterministic ordering. The two Rebirth DXAs are never repacked; their companions contain only original documents. `mod-packs.json` schema 2 records each companion's size/hash, source pack hash, and individual document identities. `SHA256SUMS.txt` covers both mods and companions. `documentation-preservation.json` records local preparation evidence

Use `--output <directory>` to choose a release workspace; the default is `android/temp/hires-release-assets`. Repeating preparation leaves identical outputs and modification times unchanged. Mod and companion assets upload before metadata and release notes; interrupted replacement is retryable, not atomic. After changing notes, refresh the byte sizes in `RecommendedModsDialog.kt` from the generated `mod-packs.json`

Run the fixture integration checks with `python -m unittest discover -s game_data/mods/d2x-xl -p test_mod_documentation.py -v`. They cover original document and gameplay preservation, untouched upstream archives, invalid paths, unexpected README contents, companion bounds, and repeat preparation. Full release preparation additionally checks every real pack and every companion
