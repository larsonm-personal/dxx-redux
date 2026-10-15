# Recommended mod documentation audit

Audited 2026-10-14 against the nine entries in `RecommendedModsDialog.kt` and the current `texture-packs-v1` GitHub release.

This is the audit snapshot before the documentation repairs. The [publication record](plan_publish_mod_docs_20261014.md) describes the completed update later that day, including new adaptation notes and published doc packs.

All nine local packs match the release's SHA-256 digests and sizes. Seven are Redux adaptations; two are unchanged upstream archives. Original documents are preserved in the five recommendations whose inspected upstream packages contain documents. The four D2X-XL recommendations derive from retained source archives containing no documents. Six adapted packs need substantially better Redux notes; UUD2SP already has useful adaptation notes.

## Results by recommendation

| Recommended pack                    | Original documentation                                                    | Redux documentation now                                                                                                             | Action                                                                                                         |
| ----------------------------------- | ------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------- |
| UUD1 Texture Fixes                  | `uud1tp.rtf`, 77,454 bytes; identical to the author's current download    | A 94-byte `README.md` says translated to DXA and refers to scripts without a link                                                   | Preserve RTF; replace our stub with pack-specific adaptation notes                                             |
| UUD2 Texture Fixes and Extras       | `UUD2TP.rtf`, 48,321 bytes; identical to the author's current download    | Same 94-byte stub                                                                                                                   | Preserve RTF; explain conversion, extra content, omitted demo/palette files and requirements                   |
| UUD2SP Sound Fixes and Restorations | `UUD2SP.rtf`, 17,399 bytes; identical to the author's current 1.4 archive | An 806-byte `README.md` describes unchanged sound banks, semantic HAM patches, requirements, ordering and combined-pack door timing | Keep this substance in a clearly named Redux note; add exact provenance and the omitted editor trailer         |
| D1 High-Res Level Textures 256px    | No document in either retained D1 source 7z                               | A 220-byte generated `README.md` plus a 1,885-byte generic `README.dxx-redux.md`                                                    | Consolidate our prose into a pack-specific conversion note; do not call either current file an original README |
| D2 High-Res Level Textures 256px    | No document in either retained D2 source 7z                               | A 243-byte generated `README.md` plus the same generic workspace README                                                             | As above, including D2-specific name/material adaptations                                                      |
| D1 High-Res Sounds                  | No document in retained `hires-sounds.7z`                                 | A 204-byte generated `README.md` plus a 1,319-byte generic workspace README                                                         | Explain actual sound conversion, output format and supported modes                                             |
| D2 High-Res Sounds                  | No document in retained `hires-sounds.7z`                                 | A 227-byte generated `README.md` plus the same generic workspace README                                                             | Explain source selection, downsampling, output format and excluded XL sounds                                   |
| D1 High-Res Menus and Fonts         | Original `README.TXT`, 603 bytes                                          | None                                                                                                                                | Leave the entire DXA untouched; whole archive equals upstream                                                  |
| Mac D1 Missing Sounds               | Original `README.TXT`, 709 bytes, and `Sounds/Move me.txt`, 297 bytes     | None                                                                                                                                | Leave the entire DXA untouched; whole archive equals upstream                                                  |

The source documents were compared as bytes, including original line endings and encoding. Their old installation instructions remain original text; launcher instructions can explain current installation separately.

## Original source verification

Fresh downloads linked by Xfing's [UUD1TP announcement](https://descentbb.net/viewtopic.php?f=22&p=391643), [UUD2TP announcement](https://www.descentbb.net/viewtopic.php?p=393747), and [UUD2SP announcement](https://descentbb.net/viewtopic.php?t=21386) contain the same RTF bytes as the corresponding published Redux packs. UUD2TP's nested `uud2tp.dxa` contains no additional documents. UUD2SP's fresh original archive also equals the retained original archive in this workspace.

The two Rebirth packs were read directly from the [official D1 add-on bundle](https://www.dxx-rebirth.com/d1x-rebirth_addons.zip), linked by the [DXX-Rebirth site](https://www.dxx-rebirth.com/). Their complete DXA bytes equal our release inputs, establishing that both documents and assets are unchanged. Historical individual download URLs now return 404; provenance should point to the maintained bundle and identify its inner entry.

| Original item                                     | SHA-256                                                            |
| ------------------------------------------------- | ------------------------------------------------------------------ |
| Author's `uud1tp.zip`                             | `373e6b90dfb979e2657d8cafbd9bb2ba27eadb3af7a9a65437bd08d519e36a00` |
| Author's `uud2tp.zip`                             | `bd2bdaf3d674238356972e8b64566bee038f1e6bff19f980c61532ee61345b6a` |
| Author's `UUD2SP1_4.dxa`                          | `d5a9b61f53ecc327bc25837f717516c9c1260a9e750452900481cb8d905b2569` |
| Original `uud1tp.rtf`                             | `42ae478f014008e9ee66a4679decced91b7c72abd348e3f57a22916658baeafe` |
| Original `UUD2TP.rtf`                             | `43fb7dd746332cab0af1f02267a9e294377c790d8000420b2c92cea404de73c5` |
| Original `UUD2SP.rtf`                             | `46160a60dab0bc2bdd5d3c84cd8f6fb4e758ea49a4108f94cdbe2a2e3bff57d6` |
| Upstream and published `d1xr-hires.dxa`           | `b30a164fa9c6dff05eda6d50f777777df843968d32053f5cc9453fe89d5bed19` |
| Upstream and published `d1xr-mac-demo-sounds.dxa` | `3ef1bf1dbcb1be8c3dde121ed53b0fec8a22340320937735073353fd0a1a990c` |

The D2X-XL source inventory covers these exact retained conversion inputs. They contain only images and WAV audio, including one JPG in D2 512px. No README, credits, license, or other document entry was found. This does not establish that every historical distribution or website lacked documentation; the five original XL downloads were not reacquired for this audit.

| Retained source          | Files | SHA-256                                                            |
| ------------------------ | ----- | ------------------------------------------------------------------ |
| `D1-textures-256x256.7z` | 300   | `e871f07125371bfdd861aa935d5ba974861e989045fefb7f6bb533bcc2ab8db0` |
| `D1-textures-512x512.7z` | 279   | `febf7e39b1cd32aa4cd4c94de9657dc7a429f5c73377d05cb109f51a595c7a26` |
| `D2-textures-256x256.7z` | 511   | `5a464b61f91f2b25d446f3f93086eebc8db8b5551acb78acf630c1b2641e6e50` |
| `D2-textures-512x512.7z` | 569   | `8b128f99ad512f142b4d4f7103da5e51be18bc85ef482d6374e68bd7ba83125a` |
| `hires-sounds.7z`        | 535   | `39cab9e2aed4c6ec9e3b811bbdf2c880d4acd87e4c66a9e1f80f1b25aef5ef6b` |

## What the adaptation notes need to explain

Use `README.dxx-redux.md` for our pack-specific notes. Preserve all original documents at their existing paths and bytes. Consolidate only files known to have been generated by Redux; do not replace an upstream README that happens to share a filename. Do not insert a Redux note into either unchanged Rebirth archive.

Each adapted pack's note should identify the original author/package and source, explain what we changed and why, state required Redux support and game data, document omissions and mod ordering where relevant, and link to the exact conversion source used. Credit the author's gameplay/artwork changes to the author and distinguish our packaging and compatibility work. Keep build commands and publication machinery in repository maintainer documentation.

### UUD1 Texture Fixes

Explain how the original replacement PIG and three RDL files become independently mountable bitmap/image replacements and semantic patches. Cover palette-index and transparency preservation, the author's door timing change, and the surface changes for First Strike levels 3, 17 and 20. These are translations of the author's changes, not new artwork or fixes invented by Redux. State the required D1 retail data and updated Android loader, supported D1/D1-in-D2 modes, and placement above high-resolution packs to retain UUD artwork. The existing stub conveys none of this.

### UUD2 Texture Fixes and Extras

Explain PNG/mask and indexed replacements for seven palette sets, added bitmap slots, and semantic HAM changes in place of full PIG/HAM replacement. State the matching baseline requirements and the need for the optional D1-in-D2 palette assets when a mission uses that palette. Explicitly identify `pogtest.hog`, `pogtest.mn2`, and `descent.256` as upstream content omitted from this global mod. Describe compatibility with UUD2SP and high-resolution pack ordering. Metadata currently contains much of this information, but the readable README does not.

### UUD2SP Sound Fixes and Restorations

Retain the current note's useful explanation. Both original sound banks are byte-identical to the author's archive, while the full HAM is replaced with 307 field changes. The one combined-pack alignment uses the author's TP+SP door clip 9 timing. Explain that the HAXMED editor trailer is intentionally omitted, not part of the runtime patch, and link the source and conversion revision. Keep the D2 retail v1.2 requirement and sound-priority guidance. This is an adapted package even though the audio banks themselves are untouched.

### D1 and D2 high-resolution textures

Explain TGA-to-ETC2/KTX2 conversion, mipmaps, animation strip splitting and frame-count mapping, transparent/supertransparent masks, game-specific paths and name corrections. The 256px output merges missing artwork from the 512px source with downscaling; it is not merely a format conversion of the 256px archive. D2 also has additive sprite material hints and selected name/variant fixes. Explain that XL-only features and unmatched artwork do not imply equivalent functionality in Redux.

The embedded `README.dxx-redux.md` is a stale snapshot of the whole conversion workspace guide. It includes irrelevant maintainer publication instructions and a relative `RELEASE.md` link whose target is absent from the DXA. Replace that generated content with a concise, self-contained note for each pack. Preserve artist attribution, and verify any expanded contributor list against the exact source versions rather than assuming a current combined community pack is the same input.

### D1 and D2 high-resolution sounds

Explain WAV-to-raw unsigned 8-bit mono conversion at 22,050 Hz and the `Sounds/*.r22` layout. Identify D1's source directory and D2's preferred 44 kHz source downsampled to 22,050 Hz. Document exclusion of XL-only sound IDs and relevant UUD2SP priority behavior for D2. The current notes say only that conversion happened; their shared workspace guide is mostly about textures. Redux-authored text also contains doubled carriage returns, which may be normalized when replacing our prose. Preserve original documents' line endings elsewhere.

## Publication and viewer requirements

For adapted packs, maintain the Redux note as a real source file read by the converter, rather than copying the workspace README or maintaining separate release prose. Put the original documents and that note in the converted archive. Generate the proposed `.readme.zip` companion from those exact final entries, so installed and recommended views agree.

For unchanged packs, publish a documentation companion by copying existing original documents only. Do not recompress, edit, rename, or inject notes into the mod itself. Keep current Redux installation guidance in the catalog/UI if needed. A future untouched upstream pack with no documents should remain untouched rather than receiving a manufactured README.

The document selection page should make original filenames visible and label our known note "Redux adaptation notes". Keep original documents equally accessible; do not merge their prose into ours. Any decoded preview is a viewing aid, not a replacement for the preserved file.

Before publishing documentation repairs, verify source-document hashes, unchanged game payload entry bytes, and identical whole-archive hashes for the two Rebirth packs. Update release hashes and catalog sizes for adapted packs whose documentation changes. Verify companion inventories against the final archives and rerun generation to check reproducibility. Document-only repairs should not rerun lossy texture or sound conversion.

## Evidence and remaining work

Source code reviewed: `RecommendedModsDialog.kt`; `prepare_texture_release.py`; `d2xxl_pack_lib.ps1`; `convert_all.ps1`; both D2X-XL converters; `repack_d2xxl_docs_and_layout.ps1`; the Xfing texture and sound converters; current archive metadata; and the existing source/runtime audit notes.

Machine-readable release/document inventories, upstream comparison results, downloaded UUD source archives, and the XL source inventory are under `android/temp/recommended-mod-docs-audit-20261014/`. The hash tables above retain the key original-source identities independently of that scratch directory.

This audit changes documentation and design only. Writing the seven final adaptation notes, repairing converter inputs/packaging, generating companion ZIPs, and publishing the validated assets remain implementation work. No mod archive or release asset was changed.
