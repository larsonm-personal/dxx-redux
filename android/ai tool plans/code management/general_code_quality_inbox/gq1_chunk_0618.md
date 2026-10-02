# GQ1-CHUNK-0618 frozen survey, resumed 2026-10-01

## Scope fingerprint

- `game_data/mods/d2x-xl/convert_d2xxl_textures.ps1`: L751-L1004; frozen blob `e63225c510e7dd40fb5716b3910805509c7eb0d6`
- `game_data/mods/d2x-xl/convert_progress_helpers.ps1`: L1-L72; frozen blob `bbbcc638041b248c9fd47174ecbc05098f7e3d85`
- `game_data/mods/d2x-xl/d2xxl_pack_lib.ps1`: L1-L135; frozen blob `e414b7a132aa0f174a347eabf55e759a9bff0854`
- `game_data/mods/d2x-xl/repack_d2xxl_docs_and_layout.ps1`: L1-L195; frozen blob `a2f06ebc7393d9b409f13ebfdab32ee5cac34faa`
- `game_data/mods/xfing/.gitignore`: L1-L8; frozen blob `a9c832423b611b0b136b54e4b1789f301a8770c3`

Scope SHA-256: `91f0cc1683e04762dd973538d0a039e2cfa181731a1bc0573fd5935d12e1cfac`

## Diff-minimization assessment

Retain portable bounded TGA repair and shared pack naming; complete exact sizing, collision rejection and archive publication

## Context and observations

Read all664 frozen primary lines: texture tail751..1004(254), progress72, packlib135, repacker195 and XFing ignore8. Complete current deltas read, including texture decoder extraction outside primary tail. Current blobsa8154edcef2b402f9a01132e731c34f4022d8725,bbbcc638041b248c9fd47174ecbc05098f7e3d85,e414b7a132aa0f174a347eabf55e759a9bff0854,a2f06ebc7393d9b409f13ebfdab32ee5cac34faa,a9c832423b611b0b136b54e4b1789f301a8770c3. Full current enclosing ConvertGameTextures pre-tail and complete ReadTGA bitmap wrapper/SplitStripTextures/ConvertWithMagick functions read, current decoder delta full but complete portable decoder/layout not reread. Full active BR-0636 and archived0638 read; full0169/0233/0628/0634/0637 reused617. Full maintained test_d2xxl_tga_pixels.ps1 and test_d2xxl_tga_layout.ps1 read. Portable test executed with a recursive cleanup guard checking resolved path stays in expected android/temp/d2xxl_tga_pixels root. First assertions PASS but guard script-scope variable missing during child cleanup led exit1 and retained only owned synthetic run; corrected unique global variable, full rerun exit0PASS and cleanup succeeded. Logs temp/gq618_pixels.log andtemp/gq618_pixels_retry.log. Explicit residue cleanup command rejected by automatic approval review, left initial owned run_0f7a5e3c0baa4da7b8ecbb729838e76d for retention, no broad cleanup. Testcalls requiredretention before newGUIDgeneration. Windowsbitmap/archive layout test NOT executed because its broader fullconversion fixture touches global fixed stages; portable PASS does not imply that integration result

All primary branch-added, shared packlib already owns exact sound/texture/mask names and shared progress checks7Zipstatus. Portable decoder extraction retains completed0638 validated exact type2 layout, origin/alpha/key mask and bitmap-lock ownership. Current ReadTGA catch disposes created bitmaps and strip clone/masks all finally dispose. Preserve that repair, do not reopen frozen earlier lockleak claim. Existing broader0634 remains for fallback bitmap resize ownership, archive and stage lifetimes; native input caps0321/0531 not substituted for cross-format sizing637. No inherited D1/D2 movement candidate; future shared branch transform owner can serve aggregate and primary without copying engine schema

Primary current tail already throws after per-texture errors and removes partial finalarchive, completed0638 narrower batch-status repair retained. Prior valid archive was deleted before conversion, successful first bytes visible beforecompletegeneration and allouter failures lack guaranteed archive/stage finalizers: existing0233/0634 still live. Missingarchive/subtree returnsnormal and broadaggregate169 remains; no claim every primary texture error still exits0. Main/mask/README entry streams close onlyordinarycontrol; failedclose/materialization can accumulate inpartialzip; complete final validation and guaranteed ownedcleanup pending. Current MaxSize branch still unguarded shrink geometry in ImageMagick, fallbackPNG directcopy andJPGdirectencoder, fallbackmask source-size and uncheckedmasknativeexit. Existing0637 exact main/mask dimensions and no-enlargementmatrix unchanged. No realraster resize, ETC2 encode, sourcearchiveextract or decoderheapstress performed

Repacker whole current unchanged fromfrozen: explicit/unrecognized/defaultmissing inputs omitted169, final target deleted before sourceopen233, fixedinplace .repack.tmp and sourceopen outsidefinally634. Relocation/generateddocumentationmapping has no canonicaluniqueness preflight636. Actual current whole CLI ran stable isolatedtemp/gq618_repack_fixture/input/d1-hires-256-textures-ktx2.dxa with byte-distinct foo.ktx2 andtextures/d1/foo.ktx2 to separateoutputdir: exit0 andclosedoutputcontains2 entriesnamedtextures/d1/foo.ktx2. temp/gq618_repack_probe.log/probeexit0. Syntheticentrypayloads deliberately names, no realKTX decoder/publicationclaimed; originalsourcepreserved, no originalindex/refs/corpusaffected. Extendexisting636 actualwholeCLI evidence, no duplicatefinding. Complete declared/actualinventory, slash/dot/case/Unicode/documentalias and field-map collisions mustrejectbefore any outputmutation. Ordinallogicalpath andsourceownership must agree with scanner/PhysFS actualconsumer

Pack documentation still claims ImageMagick fordownscaled128 basedonlyrequestedlabel; timestampsnotnormalizedinrepacker/sharedwriter, no complete tool/source/entrypolicyprovenance. Existing0628 truth/reproducibility owner unchanged. Controlled same-sourcebyte fixture for clockchanginghash recordedhistorically, notrerunhere. UTF8/noBOMsourcebuffers andparentignoredbinaries preserveexistingportableartifacts but do not close publicationor reproducibility. Analysis only, no productedits/nativebuild/Androidmountedpacktest

## Clean dimensions and evidence gaps

Complete frozen primary and complete current delta were read. Evidence above distinguishes executed fixtures from static source and test reads; no APK build, manifest merge, Android instrumentation or hostile-peer integration was run. No inherited D1/D2 reduction credit

Provisional impact rating: 47 (H/M/B/C/R = 23/0/7/10/7); proposed owner: BR-0636

Coverage outcome: ISSUES
