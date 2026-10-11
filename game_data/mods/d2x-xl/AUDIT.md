# High-resolution pack construction audit

Updated 2026-10-11. The rebuilt six-pack release is published and its recommended downloads pass actual runtime checks in D1, D2 and D1-in-D2

Current results: all six packs have no rejected stock replacements or partial animation groups, including every D2 stock palette. Both UUD priorities and custom-mission extras pass gameplay checks. Publication replaced six high-res archives and three metadata assets; repeat publication changed nothing and verified all fifteen remote assets. This document retains the investigation history below; the initial failures are superseded by the final acceptance and publication evidence

## Reproduce coverage checks

```powershell
python game_data/mods/d2x-xl/audit_texture_coverage.py --output android/temp/hires-runtime-coverage.json
```

Requires Pillow for the independent PIG decoder. It checks all six generated texture packs against actual stock bitmap names, animation groups, indexed transparency and the renderer's KTX2/mask requirements. The report is stable when unchanged. Exit code 1 means rejected replacements or partially covered animation groups exist; it does not classify every finding as a conversion error

## Initial findings before repairs

| Pack   | Stock bitmap names reached | Replacements rejected by transparency requirements | Partially covered animations |
| ------ | -------------------------: | -------------------------------------------------: | ---------------------------- |
| D1 128 |                        591 |                                                  9 | None                         |
| D1 256 |                        591 |                                                 61 | None                         |
| D1 512 |                        591 |                                                  2 | None                         |
| D2 128 |                       1438 |                                                 20 | mtrl01                       |
| D2 256 |                       1438 |                                                209 | mtrl01                       |
| D2 512 |                       1437 |                                                 21 | mtrl01, box03b               |

These counts use the stock PC D1 PIG and D2 GROUPA PIG, without UUD semantic flag changes. Most rejections are RGB-only KTX2 entries for transparency-flagged bitmaps. The D1 128/512 packs also lack the required metl127 supertransparency mask. Full entry lists and actual indexed alpha/cutout presence are in the report

The packs additionally contain names absent from stock: 239 in each D1 pack, 235 in D2 128/256, and 175 in D2 512. Some are XL-only effects, other-game names or surplus animation frames. These are not proof of working features in Redux. PIG naming coverage is also an upper bound: it does not prove a bitmap is referenced by any level or rendered

The audit now partitions unmatched names using both games' actual PIGs. Each old D1 pack contains 206 other-game bitmap names, 24 surplus animation frames, three unframed animation names and six other names. D2 128/256 contain eight other-game names, 40 surplus frames, three unframed names and 184 other names; D2 512 has 124 in the last group. Categories are disjoint and cover every unmatched name; all previous coverage findings remain unchanged (`android/temp/hires-runtime-classified.json`). Repeating the audit preserves report bytes and modification time

The three unframed names are `arw01`, `door01` and `flare`. Original archives also contain the correctly framed alternatives (D2 256 uses `arw01!0`, which the converter normalizes). Thus their unmatched base files are not evidence of missing whole animations. The 512 flare alternatives have identical decoded pixels; arrow alternatives differ, and square door01 cannot substitute for an opening strip. Do not overwrite the correctly framed artwork just to eliminate an unmatched-name count. The remaining category includes XL effects and special images, so it requires runtime-path inspection rather than blanket deletion

Original D2 source inspection found 9-frame door18 and 16-frame wall01 strips, while the game has 8 and 5 frames respectively. Simply splitting sequential frames leaves later artwork unreachable and can omit the final door appearance. The 512 source mtrl01 strip has 12 frames while the game has 18. The source box03b images are 512x1025 and 512x513; the strict square-strip splitter does not recognize the extra row. These need source-aware conversion decisions and visual/runtime acceptance, not arbitrary padding of missing entries

Reproduced a merge conversion failure: ImageMagick reads the original exp13 TGA as opaque even though its pixels contain alpha. The merge pass used ImageMagick directly and omitted masks. It now uses the primary TGA decoder and strip splitter, carries masks into the target archive and rejects missing sources before updating that target. A real-source regression verifies exp13 alpha and misc037 image/mask alpha, dimensions and binary cutout edges. The generated release archives have not yet been rebuilt with this fix

```powershell
pwsh android/tests/test_d2xxl_texture_merge.ps1 -Etc2Tool android/temp/etc2-audit-build/Release/etc2tool.exe -Magick C:/local/imagemagick-7.1.2-26/magick.exe
```

`etc2tool` was built from the checked-in source with its pinned dependencies. The paths above are this audit's local build and installed ImageMagick; use equivalent tool paths elsewhere. The production converter still needs complete regeneration and animation coverage fixes before this finding is closed

The ImageMagick resize arguments also previously enlarged smaller source images: the original 64px exp13 frames became 256px in the merged pack without adding detail. Primary and merge resizing now treat the requested dimension as a maximum, including masks

## Animation and legacy-alpha repairs

The splitter now maps known mismatched strips onto the game's actual frame counts. Door openings and one-shot effects retain their initial and final frames; looping monitor/lava animations sample a complete cycle. This covers the affected D1/D2 doors, wall01, misc066, misc11, lava06, mtrl01 and mtrl03. The two-frame 512px box03b strip has a trailing transparent export row; conversion verifies and excludes that row. `test_d2xxl_texture_animation.ps1` passes 16 original-artwork cases, comparing every output frame and owned mask against explicit source-frame selections. Evidence: `android/temp/hires-animation-current-regression.log`, `android/temp/d2xxl-animation-test/run_98cc9e17107a4684ba7f3a12f1d8d82a`

An independent inventory of all four original archives found 14 32-bit TGAs whose meaningful alpha bytes are undeclared in the header. Examples include D1 door17/door31/fan01 and D2 metl135/misc055. The default decoder correctly treats undeclared alpha as unused, but that loses actual XL artwork. Production source conversion now explicitly preserves those bytes, matching [XL's four-channel image handling](https://github.com/arbruijn/d2x-xl/blob/master/gameio/tga.cpp). Generic decoding retains its strict default. Portable pixel/layout tests and the original-source merge test pass, including metl135 compressed as RGBA. Inventory: `android/temp/hires-undeclared-alpha.json`; logs: `hires-alpha-current-{pixels,layout}.log`, `hires-alpha-merge-regression.log`

Full-size strip testing also exposed excessive per-pixel PowerShell function overhead. Inlining the same exact-key comparison reduced the complete 16-case original-artwork regression to minutes while preserving its pixel hashes. The earlier test using the superseded decoder was stopped only after the current-code run passed

Primary conversion now builds beside the target and replaces it only after successful completion. Failed/empty input cannot destroy an existing archive. Stable ZIP entry times and byte comparison preserve an identical archive's bytes and modification time. Scratch directories are unique and restricted to `android/temp/d2xxl-convert`. The batch driver throws on failed texture/sound conversion. The existing layout regression now verifies successful repeatability and preservation of the prior archive after a mixed valid/invalid batch; it passes (`hires-atomic-final-layout.log`). Merge entry times are stable too (`hires-atomic-final-merge.log`)

Full six-pack regeneration is running into a separate `android/temp/hires-rebuilt` directory; its exact path is recorded in `hires-rebuilt-path.txt` and progress in `hires-complete-rebuild.log`. The public release has not yet been replaced. Re-run independent archive coverage and actual renderer/launcher acceptance on the rebuilt assets before publishing; the earlier table describes the old archives

## Additional original-file formats and residual transparency

The first complete rebuild stopped on D1 `blown05.tga`, whose true-color header leaves an unused color-map depth despite having no map. Whole-source preflight then found RLE true-color `missback.tga` and two 512px control strips whose complete 495-byte extension declares size 494. Production decoding now explicitly handles these XL source conventions and bounded RLE packets. Generic extension validation remains strict. All 1,658 original TGAs across the four source trees pass source-mode layout validation (1,660 files including two scratch derivatives); the 65,536 decoded pixels of original `missback.tga` match independent Pillow decoding. Pixel/layout regressions and scoped formatting pass (`hires-rle-{pixels,layout}.log`, `uud-release-final-format.log`)

The restarted rebuild completed the primary D1 128px archive. Its pre-merge audit has no partial animations and three RGB-only rejections: `rock156`, `rock178`, `rock260`. The stock images contain respectively one, three and one transparent pixels, while both original high-res resolutions are entirely opaque. These are different from the lost-alpha conversion bug. Evidence: `android/temp/hires-rebuilt-d1-128-coverage.json`. This archive is not final until the batch merge pass finishes

Original D2 muzzle strips such as `muzl02#0` also have no alpha or supertransparent key. They show bright effects on black. XL explicitly uses additive sprite blending for fireballs/explosions in [DrawObjectBitmap](https://github.com/arbruijn/d2x-xl/blob/master/render/objrender.cpp). Redux has optional additive effects, but its KTX2 eligibility guard rejects RGB replacements for stock transparency-flagged bitmaps before rendering. Do not infer missing source alpha here, manufacture arbitrary cutouts, or weaken the generic guard without testing normal and additive rendering. The repaired converter writes an archive-owned additive sprite material for these frames; the acceptance below covers its actual renderer behavior

The three opaque rocks now retain an explicit all-opaque alpha channel during primary and merge conversion. The engine's generic transparency guards remain unchanged. Real-source merge regression passes for these rocks alongside the alpha/cutout cases (`hires-opaque-rock-merge.log`). A paired renderer regression first fails with the old pack, then passes with corrected assets: stock holes are present, replacement GPU pixels are entirely opaque, and all three game modes load the 256px replacements. The complete fixture passes nine D1 and nineteen D2 cases (`opaque-rock-render-after/{d1,d2}.json`), including earlier priority, mask and mission-ownership checks

Validation copies under `android/temp/hires-opaque-rocks` combine old packs with the corrected rocks; they are not the final rebuilt release. All eighteen original rock conversions (three names, three sizes, two games) are retained there for comparison. The full batch started before this fix, so independently check all six completed packs and repair earlier outputs before publication

## Sound construction and source fidelity

The published D1 pack correctly contains 22.05 kHz `.r22` PCM, but `Get-D2xxlSoundFormat` still requested 11.025 kHz `.raw` for D1. A complete rebuild would silently downgrade it. The shared format helper now requests `.r22` at 22.05 kHz for both games, matching the updated Android loader and native D1-in-D2. The previous regression asserted the obsolete format and only searched source strings; it now creates actual source WAV archives and runs both conversions

The sound converter also previously deleted the existing DXA before conversion and returned success after individual WAV errors. It now builds beside the target, throws on missing/empty input or conversion failures, closes archives on failure, and replaces the previous archive only on success. Fixed ZIP timestamps and hash comparison preserve identical output bytes and modification times. Its unique scratch directories are under the repository with checked cleanup paths. `test_d2xxl_sound_format.ps1` verifies known PCM bytes/duration, both 22 kHz paths, repeatability, malformed-batch preservation and empty PCM rejection

Reproduce independent source/name validation with:

```powershell
python game_data/mods/d2x-xl/audit_sound_conversion.py --output android/temp/hires-sound-source-complete.json
```

The audit uses Python's WAV decoder, independently reconstructs the documented linear resampling, and compares every published PCM byte. It also reads stock D1 PIG and D2 S22 sound tables. All 113 D1 and 191 D2 WAVs match, with no sample-count/duration mismatch. Every stock sound name is covered (98 D1, 183 D2). The additional fifteen D1 and eight D2 names are original alternatives/XL effects with no stock sound slot; importing them does not activate those extra effects. A complete rebuild with the repaired converter also reproduces every published PCM entry exactly (`hires-sound-rebuild.log`, `hires-sound-source-complete.json`)

Byte fidelity does not evaluate resampling quality or prove event playback. Actual mixer consumption, rates, mission re-entry and both UUD/high-res orders are covered by the fifteen launcher combinations in the UUD audit. No sound release asset needed replacement for the converter repair

## Runtime evidence

`android/tests/test_hires_pack_release.ps1 -Serial emulator-5586 -Resolutions 256` passed all nine baseline/on/off cases in D1, D2 and D1-in-D2 using the newly packaged APK. It verifies real imports, level-one texture residency, mipmaps, presentation scope, Video Info status and disable/restart. Evidence: `android/temp/hires-release/run_b1a97570735f46e8a42a80a6fbcd93e1`

That narrow level-one result does not contradict the archive-wide failures above. Shared renderer priority, mask ownership and mission PG1/POG precedence are separately covered by the actual GLES fixture described in [the UUD audit](../xfing/AUDIT.md)

Final checks for the rock/sound stage pass: scoped formatting (`hires-rock-sound-format.log`, `hires-sound-audit-format.log`), the complete sound regression including empty PCM (`hires-sound-final-regression.log`), and both test catalogs (`hires-rock-sound-{automation,suite}-catalog.log`, 329 top-level entries / 387 support scripts). Repeating the independent source audit preserves bytes and modification time (`hires-sound-source-repeat.log`)

## Additive sprite compatibility and merge preservation

The converter now tags all eighteen original D2 muzzle frames with an exact `additive\n` sidecar beside each KTX2. The renderer checks image/marker ownership and the live non-rod vclip table, rejects wall textures, preserves the source RGB, applies additive blending with the existing sprite fade and restores the caller's blend state. Retail D1 records a zero vclip count while loading its full fixed table; eligibility handles this original-file convention explicitly (original PIG offset 22908 is zero; clip 11 contains muzzle bitmap 244)

`test_android_mod_renderer.ps1` now passes ten D1 and twenty-one D2 cases. Its sprite cases draw all eighteen D2 frames plus twelve shared frames in both D1 modes using actual `g3_draw_bitmap`, read back GPU pixels over black/gray backgrounds, and verify additive color, fades, blend restoration and a subsequent ordinary sprite. Missing, foreign-owner and malformed material hints fall back to stock; a wall marker cannot bypass the alpha guard. D1 uses the D2 artwork as a renderer fixture, not as a newly published D1 mod. Evidence: `android/temp/additive-sprite-render/{d1,d2}.json`, `hires-additive-render.log`

Actual source-artwork merge regression validates all three converted muzzle frames as RGB with owned material hints, while retaining the existing alpha/mask/opaque-rock checks (`hires-additive-merge.log`). Existing authored-mipmap/device-limit tests pass all 48 cases in each engine (`hires-additive-mipmaps.log`). Native builds introduce no new warnings; scoped formatting passed (`hires-additive-format.log`)

This is native renderer evidence, not a new APK or completed gameplay/publication acceptance. The complete six-pack rebuild remains in progress. Its parent process captured the older merge function, so inspect final outputs for corrected opaque rocks and sprite materials before replacing any public archive

Merge updates now stage a copy beside the target, reject duplicate entry paths (including case collisions), close streams on failure and replace only changed successful output. Regression injects a duplicate and a missing file after a valid first addition, proving prior bytes/mtime survive and no candidate is left behind; an empty merge also preserves bytes/mtime (`hires-additive-atomic-merge.log`). Final scoped formatting and both catalogs pass (`hires-additive-merge-format.log`, `hires-additive-{automation,suite}-catalog.log`)

All three D1 primary rebuilds are complete. Independent coverage finds no partial animation groups. The earlier 128/256 outputs each still need rock156/178/260 corrected; the 512 output has no rejected replacements. Stock-name counts are 526/364/526 before the scheduled cross-size merge. Report: `hires-d1-primary-additive-stage.json`. The batch is currently preparing the D2 128px source strips

## Draw diagnostics and gameplay acceptance

Android introspection now reports per-texture additive sprite draw counts and their actual source archive. Uploading a texture alone leaves its count at zero; a sprite draw increments it. Texture initialization and re-upload reset the count, including graphics-context recreation. The paired native renderer regression verifies these distinctions while retaining all pixel, fade, ownership and blend-restoration checks (10 D1 / 21 D2 cases; `android/temp/additive-sprite-draw-count/{d1,d2}.json`, `hires-additive-draw-count.log`)

The current APK builds successfully (`hires-combat-apk.log`) and was installed on task-owned emulator-5586. A normal launcher import followed by real enemy combat draws all three 256px `muzl02` frames from the imported archive. Evidence: `hires-release/run_582803ba6ecc4ffb81dacb72f1483385/sprite-enemy-pose.json` and its passing correlated result. This uses the temporary old-pack-plus-material-fixes validation archive, not the final rebuilt release. The initial movement-only probe failed because it never reached combat; player-owned muzzle flashes are deliberately hidden by the engine. The maintained runner now uses the verified view in segment 76 facing the stock level-one enemy in segment 35

The independent coverage audit accepts `--games` and `--sizes` to inspect completed primary outputs without reading an unfinished build. Reports identify actual archive SHA-256 and bytes, and case-colliding duplicate ZIP paths are rejected. D2 128px primary coverage passes with 1,444 stock names, no rejected replacements, no partial animations and all eighteen additive materials (`hires-d2-128-primary-coverage.json`). Repeating the audit preserves report bytes/mtime. This is primary-output evidence only; the full batch is still converting D2 256px and has not run its final merge

The maintained launcher runner passes the complete D2 256px baseline/enabled/disabled sequence plus the enemy sprite draw probe (`hires-additive-gameplay-final.log`, `hires-release/run_2c52a9f6d1834ba08b0df64847a288c5`). Each launcher phase passes 32 steps; the combat probe passes six and records six actual draws across all three replacement frames. Scoped formatting and both catalogs pass (`hires-combat-final-format.log`, `hires-combat-{automation,suite}-catalog.log`; 329 top-level entries / 387 support scripts). Final rebuilt-pack renderer, combination and publication acceptance remain outstanding

## Corrected D1 outputs and palette-wide eligibility

Replaced exactly rock156/178/260 in the earlier D1 128/256 primary outputs with the original-artwork conversions already validated by the renderer. Candidate ZIPs preserve entry order/metadata, validate every entry and CRC, and replace the target only after verification. Repair identities are recorded in `hires-primary-rock-repair.json`. All three D1 primary sizes now have zero rejected replacements and zero partial animations (`hires-d1-primary-corrected-coverage.json`); their stock-name counts remain 526/364/526 before merging

The current corrected merge is running for D1 separately while the original batch continues D2 conversion (`hires-d1-current-merge.log`). It adds 66 names from the 256px source to the other sizes and 326 names from the 512px source to 256px. Once complete, the original batch will see equal name sets and skip its older D1 merge body. Do not claim final D1 acceptance until the current merge finishes and its archives are re-audited

The reusable coverage audit now checks every D2 resolution against all six stock PIGs, recording the palette in each result. Flags and actual indexed cutout pixels are evaluated independently for groupa, alien1, alien2, fire, ice and water. The rebuilt D2 128px primary pack passes all six: 1,444 reachable stock names each, no rejected replacements or partial animations, and eighteen additive materials. Evidence: `hires-d2-128-six-palettes.json`; scoped formatting passes in `hires-six-palettes-format.log`. This does not replace the separate UUD seven-palette custom-mission gameplay acceptance

## Completed D1 merge and release-format checks

Current-code D1 merging has finished successfully (`hires-d1-current-merge.log`). All three archives now contain exactly the 805-name source union, with 591 stock names, no rejected replacements and no partial animations. Every KTX2 passes the release generator's mip bounds/format validation, and every ZIP passes CRC/path/duplicate checks. Reports: `hires-d1-final-coverage.json`, `hires-d1-final-format.json`

Archive sizes are 15,614,850 bytes (128px), 61,766,716 bytes (256px) and 231,569,830 bytes (512px). These are validated candidate files in the retained rebuild directory, not yet public release assets

The final merged 128/512 files pass all ten launcher scenarios across native D1 and D1-in-D2: baseline, enabled, removed, next resolution enabled, removed. Actual GPU residency/mipmaps, visible Video Info and D1 presentation checks pass, with zero loaded replacements after removal (`hires-d1-rebuilt-launcher.log`, `hires-release/run_113ea7ba99704b70952730198837f39e`). Recommended 256px launcher and UUD combination acceptance are running separately

D2 256px primary conversion also completed and passes all six stock palettes: 1,213 stock names, no rejected replacements/partial animations (`hires-d2-256-primary-six-palettes.json`). Its later merge is still required. The original full batch has entered D2 512px encoding; do not restart it

The recommended D1 256px candidate now passes all six baseline/enabled/removed launcher cases across native D1 and D1-in-D2 (`hires-d1-recommended-launcher.log`, `hires-release/run_cf29bac8147343c28056ec5d9d6bd103`). Enabled snapshots record 13 and 12 mipmapped replacements respectively; both removals return to zero. Together with 128/512 acceptance, all sixteen D1 candidate launcher scenarios pass. The same pipeline is now exercising both UUD priority orders, semantic/indexed patches, presentation and actual PCM consumption after mission re-entry; that matrix is not yet complete

The final D1 UUD matrix has now passed all ten scenarios across native D1 and D1-in-D2, including the host verifier's archive-byte/mixer comparison after mission re-entry (`hires-d1-rebuilt-uud-combinations.log`, `uud-combinations/run_1f9c79297fa545fe99b03dfd2d17ab67`). UUD-first retains corrected artwork alongside high-res textures; high-res-first can cover the visible UUD images while its semantic and indexed data remain active. Disabling restores the baseline

Actual GLES acceptance using the final D1 256px archive passes all ten D1 cases and all twenty-one D2-engine cases, including native D1-in-D2 (`hires-d1-final-render/{d1,d2}.json`, `hires-d1-final-render.log`). The paired fixture covers opaque rocks, additive materials/draw counters, both mod orders, owned masks, indexed fallback and mission overrides. Its D2 portion used the completed 256px primary archive, so this does not claim final merged D2 acceptance. No compiler warnings were reported. Final D1 candidates now have archive-wide and local runtime acceptance; publication/download verification remains pending

## Separate completed D2 256px candidate

To validate the recommended resolution without waiting for 512px encoding, independently compared the live 512px source inventory with the completed 128px primary: both contain exactly 1,589 image names. Copied the completed D1 candidates and D2 256px primary into a separate retained directory, recorded in `hires-final-candidates-path.txt`, and used the current merge implementation to add the 251 missing 512-source images to D2 256px. The running batch's primary file hash stayed unchanged (`hires-d2-256-candidate-merge.log`)

The resulting D2 256px archive contains the full 1,649-image source union, including 1,444 stock names. It passes all six palette eligibility/animation audits and release-format/CRC checks (`hires-d2-256-final-coverage.json`, `hires-d2-256-final-format.json`). Size: 118,747,893 bytes; SHA-256: `64e973bc24a77f3ed5a36da4f92584148c599b05374202f092c16937625c6060`. Compare its entries with the full batch after that batch's merge; do not overwrite this validated candidate blindly with the older process's output

Both final 256px candidates pass the paired actual GLES fixture (10 D1 / 21 D2-engine cases; `hires-final-256-render/{d1,d2}.json`, `hires-final-256-render.log`). D2 normal launcher baseline/enabled/removed plus real enemy additive draws pass against this completed candidate (`hires-d2-final-launcher.log`, `hires-release/run_582eb055cbf3494c99f51ab003cdc2fc`). Its UUD combinations and custom-mission gameplay are running separately

Updated recommended-popup sizes to the completed D1/D2 256px candidates and clarified that installed packs must be removed and downloaded again to update them. Overwriting a GitHub asset does not update imported copies automatically. The release generator records the same maintainer guidance in comments; public release prose remains unchanged. Scoped formatting passes (`hires-final-catalog-format.log`). A new catalog APK is building; no public assets have been replaced yet

Final D2 256px UUD combinations now pass all five phases, including actual mixer PCM, both priorities and mission re-entry (`hires-d2-final-uud-combinations.log`). Both priority orders also pass custom-mission levels 1/7 with eighteen extra surfaces, monitor destruction, lava animation/damage, green-door timing/passage and ordinary 256px texture 127 (`hires-d2-final-extras-uud-first.log`, `hires-d2-final-extras-hires-first.log`). The actual imported archive SHA-256 equals the final candidate above

The catalog APK packaged successfully, but an independent shared-worktree model-reader fix changed both native libraries. The fresh libraries pass the paired renderer again (10/21 cases; `hires-updated-apk-render/{d1,d2}.json`). All nine 256px baseline/enabled/removed launcher cases across the three modes and the actual enemy sprite draw probe also pass on the installed APK (`hires-updated-apk-launcher.log`, `hires-release/run_a556407c1ce44faa91de5fd7cec2f5bf`). Saved Video Info screenshots show active pack name/resolution and actual replacement counts; removal returns to zero. Final D2 128/512 acceptance, six-pack publication and downloaded replacement acceptance remain outstanding

## Final six-pack acceptance

All six final candidates pass ZIP integrity/path checks, KTX2 format/mip bounds, and complete stock eligibility/animation coverage (`hires-all-final-format.json`, `hires-all-final-coverage.json`). Each D1 resolution contains 805 images / 591 stock names; each D2 resolution contains 1,649 images / 1,444 stock names, with no rejected replacements or partial animation groups across all six D2 palettes

D2 128/512 candidates pass five launcher scenarios and two actual enemy sprite draw probes on the updated APK (`hires-d2-final-128-512-launcher.log`, `hires-release/run_9d4fa1260d5f4d83b4e091b53b0433d0`). Independent current-code conversion of all sixty merged additions at both sizes matches every resulting entry byte-for-byte (`validation/merge-comparison/result.json` in the final candidate directory). The 128px shield strip has one mip level because its narrow dimension is four pixels; it has no registered stock name. Oversized cockpit artwork is an intentional downscale exception and does not establish support for XL cockpit features

Published the six accepted high-res archives and three metadata assets; the other six mod assets were unchanged. `hires-final-publish.log` verifies all fifteen remote assets, and `hires-final-republish.log` records zero changes on repeat

The actual recommendation popup downloaded both UUD packs and both final 256px packs into a fresh file set. All four hashes and imported sizes/enabled states match the published manifest (`hires-published-download/downloaded-identities.json`). Direct runtime checks retain that file set's imported mods and pass fresh launch plus mission re-entry in all three modes: 14/23/14 mipmapped high-res textures for D1/D2/D1-in-D2, with UUD indexed and semantic patches active (`hires-published-download/runtime.log`, corresponding mode JSON files). The first attempt used an old test set whose registration had been reset; it failed before downloading, and the fresh-set run passed all nineteen popup steps

The full background rebuild also completed successfully. Its six-pack payload comparison matches the published candidates except for fifteen D2 256px PNG masks with different creation-time metadata; independently decoded RGBA pixels and dimensions match for every mask (`validation/merge-comparison/{complete-batch,mask-differences}.json` inside the final candidate directory). No artwork repair or replacement upload is needed for those metadata-only differences

Future primary and merge conversions suppress PNG date/time chunks using ImageMagick's [documented exclusion option](https://imagemagick.org/defines/). The real-source regression repeats both conversion paths and passes identical archive-byte checks plus unchanged primary output modification time, retaining its alpha/mask/material and failed-update checks (`hires-mask-determinism-acceptance.log`). This closes a reproducibility issue that the earlier no-mask fixture did not cover. Both final catalogs pass 329 top-level / 387 support entries; scoped completion formatting is recorded in `hires-completion-format.log`

## Unmatched artwork: additional source checks

Do not treat every unmatched entry as a broken name that should be renamed. Original 7z listings confirm both D2 source archives include correctly named `misc14#0` / `misc16#0` strips alongside `misc014#0` / `misc016#0`. The `misc016` pair is byte-identical; `misc014` differs in decoded pixels, so it is an unused alternative, not justification for overwriting the existing animation. Both D2 archives also contain the correctly named full `lava06#0` strip; the 256px archive's extra single `lava6#0` does not replace it. Corrected stale converter comments that incorrectly claimed otherwise

D1's 512px source includes both a 512px `exit02` and an unused 256px `exit02-256`; its separate 256px source already has `exit02`. `metl131` and `metl132` occur in neither stock PIG (unlike D1's valid metl133/134/135). D1's extra lava06 frames belong to D2's animation family, not D1's stock lava02 family. `xplasblob` has no registered stock name and must not be advertised as replacing plasma sprites merely because its suffix resembles `plasblob`

`hoard1` also has no stock PIG name. Redux's `d2/main/multi.c:init_hoard_data` reads orb frames directly from `hoard.ham` into dynamically allocated bitmaps, without registering `hoard1` filenames. Its ten KTX2 frames therefore have no replacement lookup route in that mode. The remaining XL effect/UI names (sparks, smoke, monsterball, bullet time, cockpit overlays and similar assets) have no corresponding Redux registration/loading path; their presence in an archive does not activate those XL features. Retained source alternatives/unsupported entries are inventory facts, not claims of runtime coverage
