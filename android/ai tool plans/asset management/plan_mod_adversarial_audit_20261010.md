# Mod construction and runtime adversarial audit

## Objective

Audit texture and other recommended mods for expected behavior that silently fails. Research additional candidates, including D2X-XL mods. Add functioning UUD1TP/UUD2TP recommendations, verify their fixes and extra textures, and establish their interaction with high-resolution packs

## Acceptance work

- [x] Inventory current source archives, conversion steps, generated contents, metadata and published assets; identify omissions and unsupported features
- [x] Trace runtime asset lookup, game/palette scope, format precedence, load order, caching, mission overrides and disable/reload behavior
- [x] Audit source-to-output UUD1TP/UUD2TP texture fixes, extra texture registration and semantic patches (HAM, levels, animations, transparency)
- [x] Reproduce and fix material failures with meaningful host/native/emulator coverage; do not equate archive validation or mounting with visible effect
- [x] Verify D1, D2 and D1-in-D2 supported cases, representative third-party extra-texture use, and both mod orders with high-resolution packs
- [x] Research additional candidates from primary author/project pages and record compatibility and conversion requirements with links
- [x] Add verified UUD packs to repeatable publication and launcher recommendations with truthful compatibility/conflict descriptions; verify downloads and imported runtime use
- [x] Run scoped formatting/builds and required catalogs for any new automation; audit every requirement against current evidence before completion

## Initial state

- The prior ten-pack release and six-item launcher catalog exist, along with earlier runtime diagnostics and integration fixtures; their previous results are starting points, not acceptance of new combinations
- Existing Xfing conversion outputs are `uud1tp-textures.dxa` and `uud2tp-textures.dxa`; source workspace also contains original/combined texture and sound packages
- Worktree contains unrelated ongoing edits; preserve them and keep this audit scoped

## Evidence and findings

- Added independent PIG/RLE/palette/image audit: `game_data/mods/xfing/audit_texture_conversion.py`. Reports preserve unchanged bytes/mtime
- Confirmed and fixed three unreachable replacement names (GROUPA 1477/2315, FIRE 1477); regenerated D2 archive, all included images/masks and names match source
- Before/after evidence: `android/temp/uud-source-audit-before.json`, `uud-source-audit-after.json`; conversion log `uud-convert-audit.log`, existing archive verifier `uud-archive-verify.log`
- Confirmed D1 source metadata change is wall animation 9 play_time 104857 -> 65536; missing from conversion. D1 surface patch is rejected by launcher scope parsing and lacks engine application
- Seventh D2 palette is omitted; source audit intentionally remains failing. Six converted palettes have no remaining pixel/name mismatches
- Findings and runtime acceptance checklist: `game_data/mods/xfing/AUDIT.md`
- Primary-source candidate research and compatibility triage: `game_data/mods/MOD_CANDIDATES.md`
- No recommendation or publication change yet; remaining functionality must be implemented and verified
- Scoped code-quality run passed (`android/temp/uud-audit-format.log`). Its Python stage excluded the new untracked file, so managed Ruff was also run directly: format/check passed after sorting imports
- Repeated independent audit produced identical bytes and preserved report modification time. Existing archive verifier passed both packages; independent audit exits 1 as expected for absent D2 DESCENT palette

## Next implementation work

- Implement atomic D1 level surface application in the shared engine path, with D1-in-D2 native assets and launcher conflict scope support. Preserve source checks; do not simply suppress launcher errors
- Convert/apply D1 wall animation 9 timing and bitmap semantic changes, then verify actual loaded values
- Establish source coverage for D2 DESCENT palette and extra-slot bitmap flags/data fallback
- Replace format-first texture lookup with explicitly tested mod/mission precedence, including ownership of supertransparency masks
- Reuse the author's demonstration levels plus native/emulator diagnostics for all runtime cases before adding/publishing UUD recommendations

## D1 semantic patch implementation

- Shared Android consumer `android_d1_mod_patches.cpp` applies D1 timing and level surfaces, also under `d1-mods/` for D1-in-D2 native assets. Both engines have guarded load hooks
- Converter now emits the actual D1 timing delta and rejects any other unrepresented PIG-prefix changes. Level operations include SHA-256 source tests; all changes stage before mutation
- Native paired fixture passes 15 cases per engine, comparing every surface texture against original author RDLs and exercising rejected operations/hash mismatch/disable reload. Source changes are 15 + 55 + 2 surfaces
- Launcher scope conflict tests pass (9 ModManagerDetailsTest cases total); APK build successful. Logs: `uud-d1-native-build.log`, `uud-d1-fixture.log`, `uud-d1-gradle.log`; paired trace directory `uud-d1-native-fixture`
- Scoped formatting and both automation catalogs pass (`uud-d1-format.log`, `uud-d1-automation-catalog.log`, `uud-d1-suite-catalog.log`)
- Found and downloaded authentic 2015 seventh-palette baseline from Pumo's primary announcement. Later Dropbox copy equals UUD2 and cannot serve as baseline. README records original source hashes
- D2 seven-palette regeneration completed; converter now refreshes source HAM on every run and uses actual optional-palette baseline metadata instead of the unrelated stock D1 PIG
- Independent audit now exits 0: 121 D1 and 1118 D2 images/masks match actual source pixels and runtime names; seventh palette has 173 changes + 30 extra slots. Evidence `uud-source-audit-seven-palettes.json` and `uud-seven-palettes-archive-verify.log`
- Final native fixture cleanup rerun passes with no leftover allocation warning. Emulator-5586 was started for this task; other devices were not used
- Final seven-palette scoped formatter passed (`uud-seven-palettes-format.log`); regenerated audit is stable with unchanged bytes/mtime
- Next concrete focus: actual D2 extra bitmap CPU data/flags and monitor/lava/door semantics (22 of 30 extra slots are transparent, while current virtual registration sets flags zero and has no pixels). Also resolve mod priority across KTX2/PNG before final APK import combinations and publication

## Indexed bitmap and HAM acceptance

- Added generated raw indexed POG overlays and shared Android consumer; hooks cover D1, D2 palettes and native D1-in-D2. Validation stages all entries before mutation, checks runtime names/indices, preserves source flags/average colors and restores original ownership on teardown
- D1-in-D2 tracks mission-custom bitmap slots during preparation so global mods cannot replace them at publication
- Independent PIG audit now verifies indexed POG contents and emits source-derived native expectations. Native paired fixtures pass 120 D1 and 1111 D2 bitmaps across seven palettes; page-out, mission override/unload and malformed input rejection pass
- Actual D2 execution exposed a completely rejected HAM patch: embedded NUL filename suffixes in converter tests did not match the engine. Fixed canonical strings and removed six combined TP+SP overrides from standalone TP. Native comparison of every texture/vclip/eclip/wall record against the author's full HAM now passes
- D1 semantic fixture expanded from 15 to 17 cases per engine, adding all indexed bitmap hashes and real PG1 precedence in both D1 modes; passed traces in `uud-d1-indexed-fixture`
- Added data-patch application counts to historical Mod Details usage, with D2 HAM success recorded alongside D1 metadata and indexed banks
- Archive packer now compares actual entry contents excluding only generatedUtc; unchanged output preserves archive bytes/mtime/timestamp. Changed output builds beside the existing archive with stable entry times before replacement. Full regeneration repeatability check and final checks underway
- Full regeneration repeatability check passed for both packs: `uud-indexed-repeat-evidence.json`. Final native indexed fixture passes eight cases per engine, including palette RGB refresh and disabling HAM without retaining extra slots. D1 semantic fixture passes 17 cases per engine
- Final scoped formatting and native builds pass with no new warnings. Kotlin ModUsageTest + ModManagerDetailsTest pass 10 tests (`uud-indexed-gradle.log`); both catalogs pass (323 top-level entries, 384 support scripts); archive verifier passes both updated packs (`uud-indexed-archive-verify.log`)
- Native build libraries are current in the isolated hires validation output; latest changes have not yet been packaged into an APK or imported through the launcher. No UUD release/catalog changes have been made
- Remaining: rendered high-res/UUD precedence and masks in both orders, real launcher imports/restarts and third-party extra-texture mission, broader high-res source/animation audit, combination sound/presentation checks, then UUD catalog/release publication

## Rendered priority and mission ownership

- Reproduced format-first priority and mission override failures through actual GLES output; fixed shared candidate ranking and mission bitmap ownership checks in both engines
- Masks now require the selected image's archive. Bitmap inspection permits a keep-all mask only when source indices contain no supertransparent pixels; all 35 affected stock D2 door frames pass actual GPU checks
- Renderer fixture passes 8 D1 and 17 D2 cases, including D1-in-D2, both real pack orders, mask ownership, indexed fallback and actual PG1/POG mission overrides. Evidence: `uud-render-after/{d1,d2}.json`
- KTX2 mipmap regression passes 48 cases per engine; native builds, scoped formatting, APK packaging and both catalogs pass (324 top-level entries, 384 support scripts)
- Current launcher acceptance uses the newly packaged APK on task-owned emulator-5586. Remaining work includes UUD imports/combinations and author demo/third-party extras, broader construction checks, then recommendations/publication

## Launcher acceptance and broader archive findings

- All nine 256px baseline/on/off launcher runs pass on the current APK across D1, D2 and D1-in-D2, including Video Info status. Evidence: `hires-release/run_b1a97570735f46e8a42a80a6fbcd93e1`
- Added `test_uud_mod_combinations.ps1` and support script/verifier for real imports, UUD alone, both high-res orders, PCM consumption, presentation, mission re-entry and disable. Scoped formatting and both catalogs pass (325 top-level, 385 support); actual integration runs in progress
- Added independent high-res archive coverage audit. Found RGB-only replacements rejected for transparency (61 D1 and 209 D2 entries at 256px), missing D1 metl127 masks at 128/512, incomplete D2 animations and unmatched names. Findings and original source dimensions are recorded in `game_data/mods/d2x-xl/AUDIT.md`; report `hires-runtime-coverage.json` intentionally fails until resolved/classified
- Selected original D2 source images are extracted under `android/temp/hires-audit-source/d2-{256,512}`. Host etc2tool build under `android/temp/etc2-audit-build` is being prepared to reproduce current conversion, especially the merge path's loss of alpha/masks

- All 15 UUD launcher combination scenarios pass in D1, D2 and D1-in-D2. Evidence: `uud-combinations/run_bd20c2e1af17468e9143dbbf7c1d0a4d`. UUD-first retains corrected images; high-res-first can cover every visible UUD image while its data patches remain active. Author demo extras and D1 levels 3/17/20 still need end-to-end acceptance
- Built the current host etc2tool. Reproduced original TGA alpha loss in ImageMagick and fixed the merge path to reuse the primary decoder/splitter and preserve masks. Real exp13/misc037 regression passes, including rejection of absent artwork without modifying the target archive. Production archives have not been regenerated
- Also found unintended upscaling in the merge/primary ImageMagick resize arguments; changed image and mask resizing to a maximum dimension. Final regression now checks 64px source stays 64px while 256px cutout image/mask downsizes together to 128px; final validation in progress
- Final merge regression, existing portable TGA pixel/layout tests, scoped formatting and both catalogs pass. Evidence: `hires-merge-regression.log`, `hires-tga-{pixels,layout}-regression.log`, `hires-merge-final-format.log`, `hires-merge-{automation,suite}-catalog.log` (326 top-level entries, 385 support scripts)
- Next acceptance work: author demo/third-party extra-texture gameplay and D1 surface levels via launcher, repair mismatched high-res animation frame mapping and regenerate packs, then add/publish verified UUD recommendations and verify downloaded assets. Goal remains active; neither release assets nor UUD recommendation catalog changed in this stage

## Full mission semantics and source conversion repairs

- All 72 D1 surface changes pass through actual launcher imports in both engines (`uud-surfaces/run_2a6a5ea678424aa2bb3b776c859c8d24`). Corrected one-second clip 9 and ordinary opening pass separately in stock level 18, both modes (`uud-surfaces/run_55a36ad679b7417b9d8fee5393890f01`)
- Original author's demo passes real texture residency and spider-monitor destruction across all seven palettes (`uud-mission-author-palettes.log`, water separately `uud-mission-render.log`)
- Added deterministic derivative fixture preserving author geometry/objects while placing all 18 added texture references and a real green door. All seven palettes pass real UUD GPU residency, animated lava/trickle, door opening/passage, monitor destruction and lava damage (`uud-mission-extra-palettes.log`, `uud-gameplay/run_8672799c0fd344628b81713bb8b1459c`)
- Extra-mission combinations with high-res in both orders on water/DESCENT are in progress. Initial spawn-only high-res assumption was invalid because those surfaces were deliberately replaced by UUD extras; the test now visits unchanged texture 127 explicitly. No engine fix was inferred from that harness failure
- Fixed high-res strip mapping for actual game frame counts, retaining door/one-shot endpoints and whole loop cycles, plus the verified transparent extra export row in box03b. Sixteen real-source pixel/mask mapping cases pass (`hires-animation-current-regression.log`)
- Found 14 original TGAs with undeclared but meaningful alpha. Explicit XL-source decoding preserves it; generic strict decoding stays intact. Portable pixel/layout and actual merge RGBA regressions pass. Per-pixel exact-key comparison was inlined after whole-source testing exposed excessive function-call overhead
- Primary conversion preserves prior archives after errors and identical regeneration; unique repo-local workspaces replace shared system-temp paths. Stable merge timestamps and batch failure propagation added. Layout/merge regressions and scoped formatting pass; full six-pack rebuild is running (`hires-complete-rebuild.log`, output path in `hires-rebuilt-path.txt`)
- Both catalogs pass after additions: 329 top-level entries, 387 support scripts (`uud-final-{automation,suite}-catalog.log`). Current final formatting of combination/timing runner changes still needs a scoped pass
- Remaining: finish extra-mission combinations, independently audit rebuilt packs and close residual failures, validate rebuilt renderer/launcher behavior, then add/publish UUD recommendations and verify downloaded assets. Goal remains active; live release and recommendation catalog are still unchanged

- Final extra-mission combination matrix passes: UUD-first and high-res-first on water and DESCENT palettes, checking actual 256px ordinary wall residency plus all 18 UUD references and monitor/lava/door gameplay. Evidence: `uud-gameplay/run_9d0975c0f7aa4d148ae27efb7bb2c386` and `uud-gameplay/extras-hires-first/run_214bdd773749460e94c012579e174fe3`. Runner output retention now separates variants/orders to preserve each matrix's evidence
- Final scoped runner formatting passed (`uud-combination-final-format.log`). Full high-res rebuild is still live, writing `android/temp/hires-rebuilt/run_cc5b57cd5d814e308bee6db511aa5758`; poll its existing process rather than restarting. No emulator tests remain running. Next: audit the completed rebuild, resolve remaining rejected/partial assets, then final runtime/recommendation/publication work

## UUD publication and full-source format acceptance

- Published both verified UUD archives, their combined manifest and SHA list. All fifteen remote assets verified; repeat `--publish` changed zero assets. The existing ten mod archives were preserved. Logs: `uud-release-publish.log`, `uud-release-republish.log`
- Updated eight-item recommendation catalog packaged successfully (`uud-recommendations-apk.log`). Actual popup downloads/imports passed and both entries showed Installed with accurate decimal sizes. Downloaded bytes match the source/gameplay-tested hashes (`uud-popup-download-result.json`, `uud-popup-downloaded-sha256.txt`, `uud-popup.png`)
- Downloaded copies then passed fresh-session and mission re-entry gameplay in all three supported modes, with actual UUD GPU residency plus indexed/semantic patches (`uud-downloaded-runtime/{d1,d2,d1-in-d2}.json`, `uud-downloaded-runtime.log`). Current APK is installed only on task-owned emulator-5586; no new APK release was published
- First full rebuild failed on unused true-color map depth in original blown05. Whole-source preflight additionally found valid RLE missback and two source extensions with an off-by-one size field. Explicit XL-source mode handles these without relaxing generic extension checks. All 1,660 source TGAs validate; missback's 65,536 decoded pixels independently match Pillow. Portable regressions and scoped formatting pass (`hires-rle-{pixels,layout}.log`, `uud-release-final-format.log`)
- Restarted full rebuild is live in session 85339, log `hires-complete-rebuild-rle.log`, same retained output directory. D1 128 primary archive completed with no partial animations and three remaining opaque-rock RGB eligibility rejections. Merge has not run yet. Continue polling this process; do not restart because it is slow
- Original rock156/178/260 high-res artwork is opaque; stock counterparts contain one/three/one transparent pixels. Original D2 muzzle strips instead rely on additive black-background effects. Redux has optional additive blending but rejects these RGB replacements before rendering. These need explicit compatibility treatment and actual rendering checks, not fabricated alpha or a blanket eligibility bypass
- Extended coverage report to partition all unmatched names against both stock games and animation groups. Original coverage findings remain unchanged, and categories partition every unmatched name (`hires-runtime-classified.json`). Unknown names still need inspection for special runtime paths
- Remaining: finish high-res rebuild/merge, resolve and classify residual coverage failures, verify rebuilt assets through actual renderer/launcher combinations, update sizes/manifests and replace public high-res assets. Full goal remains active

## Opaque rock and sound regeneration repairs

- Opaque source rocks now retain an explicit opaque alpha channel in both primary and merge ImageMagick paths. No renderer eligibility guard was weakened. Real-source merge regression passes; all 18 game/size/name conversions remain fully opaque (`hires-opaque-rock-merge.log`, `hires-opaque-rock-validation.log`)
- GPU fixture reproduces old-pack rejection and passes with corrected assets in all three game modes, comparing original holes with fully opaque replacement pixels. Full renderer fixture passes nine D1 and nineteen D2 cases (`opaque-rock-render-after/{d1,d2}.json`). Its runner accepts `-PackDirectory` for regenerated candidates. Validation copies use old packs plus corrected rocks and must not be mistaken for the final rebuilt release
- Found stale D1 sound generator contract: 11 kHz `.raw` despite the verified public pack and updated shared Android loader using 22 kHz `.r22`. Fixed the helper and replaced the obsolete source-string test with actual source-WAV/archive conversion checks
- Sound conversion now preserves prior output after errors, rejects missing/empty sources, uses isolated repo-local workspaces, closes output on failure, and preserves bytes/mtime on identical builds. Known PCM/duration, malformed-batch preservation and repeatability pass; final empty-source regression is being checked
- Added reusable independent `audit_sound_conversion.py`: all 304 original WAVs reproduce published PCM exactly, and all 98 D1 / 183 D2 stock sound names are covered (`hires-sound-source-complete.json`). Fifteen D1 and eight D2 additional names are unused alternatives/XL effects, not activated stock sounds
- Complete actual sound regeneration passes with zero errors and identical published PCM payloads (`hires-sound-rebuild.log`). Its temporary comparison README is not release metadata; do not publish those comparison archives
- Full texture batch remains live in session 85339, currently D1 512 conversion. Because the process began before the opaque-rock fix, audit all six final outputs and correct any earlier RGB rocks using the original sources before release. Do not restart the expensive batch solely for this three-image repair
- Scoped formatting of conversion/native fixture changes passed (`hires-rock-sound-format.log`). Final formatting/source-audit repeatability and existing catalogs are pending after the new sound audit/regression refinements. The full goal remains active

- Final sound regression (including empty PCM), scoped formatting and both catalogs pass: `hires-sound-final-regression.log`, `hires-sound-audit-format.log`, `hires-rock-sound-{automation,suite}-catalog.log` (329 top-level / 387 support). Repeated independent source audit preserves bytes/mtime (`hires-sound-source-repeat.log`). All non-rebuild processes are terminal; only full texture batch 85339 remains live
- Source tree count clarified: 1,658 original TGAs in the four archives (300 + 279 + 511 + 568), plus two scratch derivatives in the earlier 1,660-file preflight. Updated audit wording accordingly

## Additive sprite rendering

- Added explicit archive-owned additive KTX2 sprite material hints for all 18 original D2 muzzle frames. Shared Android rendering resolves non-rod vclip membership, excludes walls, preserves fade and restores prior blend mode; ordinary alpha/mask guards remain
- Actual D1 execution exposed the original retail PIG's zero vclip count despite a populated fixed table. Confirmed source offset 22908 and clip 11; added the same bounded D1 convention used by D1-in-D2's reader
- Native renderer passes 10 D1 / 21 D2 cases, including all 42 sprite frame draws across D1, D2 and D1-in-D2, pixel comparisons on two backgrounds, fade/state restoration, ordinary subsequent sprite, malformed/foreign markers and wall rejection (`additive-sprite-render/{d1,d2}.json`, `hires-additive-render.log`)
- Original-art merge tests validate actual RGB muzzle frames and exact material hints (`hires-additive-merge.log`). Existing mipmap/device-limit fixture passes 48 cases per engine (`hires-additive-mipmaps.log`). Native builds and scoped formatting pass (`hires-additive-engine-build.log`, `hires-additive-format.log`)
- Found merge helper permits duplicate ZIP paths and modifies the target directly. Added candidate-file updates with duplicate rejection and cleanup; failure-preservation regression is running. Full rebuild session 85339 is still live and uses its previously captured merge body
- Remaining: final merge checks/formatting, full six-pack coverage and repairs, APK/gameplay checks of regenerated assets, publication and downloaded-asset acceptance. No new texture assets or APK published in this stage; goal remains active

- Final atomic merge regression passes: duplicate or missing input after an earlier addition leaves prior bytes/mtime intact, removes the candidate and closes the ZIP; empty merge preserves identity (`hires-additive-atomic-merge.log`). Final formatting and both catalogs pass (329 top-level / 387 support)
- D1 primary coverage now inspected for all three sizes: no partial animations; earlier 128/256 still need three opaque-rock corrections, 512 has none (`hires-d1-primary-additive-stage.json`). Full batch has entered D2 source-strip preparation
- Inspected unmatched source alternatives against original 7z listings and both PIGs, plus dynamic hoard bitmap loading. Correct canonical misc14/misc16/lava06 artwork already exists, so do not overwrite it with extra aliases. Corrected stale source comments; added detailed unsupported/alternative findings to AUDIT.md
- Only session 85339 remains the long-running conversion. Native APK/gameplay acceptance for additive materials and final rebuilt packs has not yet run; no release changes in this stage

## Actual sprite draws and primary coverage

- Added actual additive draw counters with archive/name attribution, reset on initialization and re-upload. Native renderer regression retains all pixel/material/priority checks and now proves upload alone is not a draw, three fixture draws are counted and re-upload resets the count. Both engines pass (10 D1 / 21 D2 cases; `additive-sprite-draw-count/{d1,d2}.json`, `hires-additive-draw-count.log`)
- Current APK build passes (`hires-combat-apk.log`) and is installed on task-owned emulator-5586. Read-only robot positions in introspection locate real combat without injecting test objects
- Initial movement-only sprite probe did not encounter an enemy; own-ship flashes are deliberately suppressed. Replaced it with a verified view in segment 76 facing the stock level-one enemy in segment 35
- Maintained D2 256px launcher baseline/enabled/disabled and actual enemy muzzle draw checks all pass: `hires-additive-gameplay-final.log`, `hires-release/run_2c52a9f6d1834ba08b0df64847a288c5`. All three 256px muzzle frames were drawn from the imported archive. This uses the temporary validation pack (old release plus corrected rocks/materials), not the final rebuild
- Scoped formatting and both existing catalogs pass (`hires-combat-final-format.log`, `hires-combat-automation-catalog.log`, `hires-combat-suite-catalog.log`; 329 top-level entries / 387 support scripts). All validation processes are terminal; only full conversion 85339 remains live
- D2 128px primary audit passes: 1,444 stock names, zero rejected replacements or partial animations, all eighteen additive material hints. Audit now supports partial game/size selections and records archive SHA-256/bytes; duplicate case-colliding ZIP names are rejected. Repeated report generation preserves bytes/mtime (`hires-d2-128-primary-coverage.json`)
- Full batch 85339 is confirmed live, converting D2 256px (540/1398 observed). Preserve its process. It still has D2 512px and cross-size merge ahead; its captured old merge body requires independent final output inspection and repair
- Remaining: finish all six packs, correct earlier D1 128/256 opaque rocks and any outdated merge output, inspect complete coverage, validate final renderer/launcher/UUD combinations using the new candidate-directory options, update catalog sizes/metadata, publish and verify downloaded replacements. No new texture release assets were published in this stage; full goal remains active

## Repairing completed packs while the batch continues

- Replaced the three earlier RGB rocks in each D1 128/256 primary archive using prepared original-source RGBA conversions. Atomic ZIP rewrite verifies all entries/CRC and preserves metadata/order. `hires-primary-rock-repair.json` records old/new hashes and the six changed entries. All D1 sizes now pass primary eligibility/animation coverage (`hires-d1-primary-corrected-coverage.json`)
- Started current-code D1 merge in session 7488 (`hires-d1-current-merge.log`, scratch runner `android/temp/merge_current_d1.ps1`). Source sets: 479 names at 256px, 739 at 128/512; union 805. Add 66 to 128/512 and 326 to 256. This owns the D1 candidate outputs until terminal; do not run another mutation against them
- The original full batch 85339 remains live on D2 256px. Once current D1 merge succeeds, equal name sets cause the older parent's D1 merge to skip. D2 still needs current-code final merge inspection/repair after its 512px primary completes
- Expanded the reusable audit to all six D2 stock palette PIGs. Rebuilt 128px passes every palette with 1,444 stock names, zero rejected replacements/partial groups, eighteen additive materials (`hires-d2-128-six-palettes.json`). Scoped formatting passes (`hires-six-palettes-format.log`); repeated report generation is checked separately in `hires-six-palettes-repeat.log`
- Next: poll both existing sessions, audit all three completed D1 merged outputs, then run D1/D1-in-D2 candidate launcher/renderer checks while D2 continues. Final six-pack renderer/UUD combination checks, catalog sizes, publication and downloaded-asset validation remain required. No public texture assets changed in this stage

## Final D1 archives and runtime acceptance

- Current-code D1 merge session 7488 completed successfully. All sizes have exactly 805 image names, including 591 stock textures. Complete eligibility/animation and release-format/CRC checks pass (`hires-d1-final-coverage.json`, `hires-d1-final-format.json`)
- Final candidate sizes: 128px 15,614,850; 256px 61,766,716; 512px 231,569,830 bytes. Update the recommended popup's D1 256px size to the final value when preparing publication; the public pack remains unchanged for now
- All ten final 128/512 launcher scenarios pass across native D1 and D1-in-D2, including visible status, actual GPU residency/mipmaps, presentation and removal/restart. Session 13905 is terminal; evidence `hires-d1-rebuilt-launcher.log`, `hires-release/run_113ea7ba99704b70952730198837f39e`
- Session 43689 now runs D1/D1-in-D2 256px baseline/on/off, followed sequentially by both modes' complete UUD combination matrices. Logs: `hires-d1-recommended-launcher.log`, `hires-d1-rebuilt-uud-combinations.log`. It owns emulator-5586 until terminal; do not run overlapping device tests
- D2 256px primary completed and passes all six stock palette audits (1,213 stock names, zero rejected/partial; `hires-d2-256-primary-six-palettes.json`). Batch 85339 remains live on D2 512px encoding (9/1589 observed). D2 cross-size merging and its final acceptance are outstanding
- Remaining: finish the active D1 runtime pipeline; complete D2 conversion/current-code merge and audit; paired final renderer, D2 launcher/UUD/custom-mission combinations; update sizes/metadata, publish and verify downloaded replacements. Application changes still require the next distributed APK; this task has published only mod assets so far

- D1 256px launcher stage completed: all six baseline/on/off cases pass across both engines, with 13/12 actual mipmapped textures when enabled and zero after removal. Evidence: `hires-d1-recommended-launcher.log`, `hires-release/run_cf29bac8147343c28056ec5d9d6bd103`. All sixteen final D1 launcher cases across the three sizes now pass
- Session 43689 remains live in its UUD combination stage (`hires-d1-rebuilt-uud-combinations.log`); first baseline passed. Preserve exclusive use of emulator-5586 until this pipeline exits. Full D2 batch 85339 is still live at 141/1589 in its 512px primary, with zero conversion errors observed. No publication changes in this stage

## Completed D1 combination and renderer gates

- Pipeline 43689 completed successfully. All ten UUD combination scenarios across D1/D1-in-D2 pass the final archive-byte/mixer verifier, including both priorities, semantic/indexed patches, presentation, re-entry and disabling. Evidence: `hires-d1-rebuilt-uud-combinations.log`, `uud-combinations/run_1f9c79297fa545fe99b03dfd2d17ab67`
- Native renderer session 30918 also completed successfully with the final D1 256px candidate and the D2 256px primary candidate: 10 D1 / 21 D2-engine cases, including native D1-in-D2. All actual pixel/material/priority/mask/mission checks pass; no compiler warnings. Evidence: `hires-d1-final-render/{d1,d2}.json`, `hires-d1-final-render.log`. D2 still requires final merged-archive acceptance
- Only full batch 85339 remains live (D2 512px encoding, 277/1589 observed). Emulator-5586 is free; the test app was force-stopped before the completed native fixture
- Final D1 candidates now have complete static checks, sixteen launcher scenarios, ten UUD scenarios and actual renderer checks. Do not publish them as proof of final D2 correctness. Remaining: complete D2 conversion and current-code merge/repair, all-six final audits, D2 renderer/launcher/UUD/custom-mission acceptance, catalog sizes, publication, and downloaded-asset validation. No public texture assets changed in this stage

## Final recommended-resolution candidates

- Verified the live D2 512px input inventory equals the completed 128px primary's 1,589 image names. Prepared a separate candidate directory without mutating the full batch's outputs; pointer `android/temp/hires-final-candidates-path.txt` resolves to `android/temp/hires-final-candidates/run_8f785f4f95054dff9729b1796158e34e`
- That directory contains all three already verified D1 packs plus the completed merged D2 256px pack. Session 8485 is terminal success; current-code merge added 251 images and verified the original D2 primary hash stayed unchanged (`hires-d2-256-candidate-merge.log`). No D2 128/512 files are staged there yet
- Final D2 256px: 118,747,893 bytes, SHA-256 `64e973bc24a77f3ed5a36da4f92584148c599b05374202f092c16937625c6060`, 1,649 images and 1,444 stock names. All six palette eligibility/animation checks and release-format/CRC checks pass (`hires-d2-256-final-{coverage,format}.json`)
- Session 15836 owns emulator-5586. Its paired final-256 renderer stage passed 10/21 cases (`hires-final-256-render`); its D2 baseline/on/off and actual enemy-muzzle draw stage passed (`hires-d2-final-launcher.log`, `hires-release/run_582eb055cbf3494c99f51ab003cdc2fc`). It continues sequentially with D2 UUD combinations and extras gameplay in both orders on levels 1/7. Logs: `hires-d2-final-uud-combinations.log`, `hires-d2-final-extras-uud-first.log`, `hires-d2-final-extras-hires-first.log`
- Popup sizes now match completed candidates (D1 61,766,716; D2 118,747,893 bytes). Added concise remove/re-download update guidance to the popup and generator comments. Scoped formatter session 71244 completed successfully (`hires-final-catalog-format.log`)
- APK packaging session 91195 is live (`hires-final-catalog-apk.log`). It verifies both native-library hashes remain identical to the gameplay-tested build. Do not install it during session 15836's tests; retain it for final published-download acceptance
- Full batch 85339 remains live on D2 512px encoding. After it finishes, inspect all six outputs, compare the separately completed D2 256px candidate with the batch output, and correct only demonstrated problems. Stage validated D2 128/512 beside the final candidates; do not replace a verified candidate blindly. Publication and downloaded-asset validation remain outstanding, and public release assets are still unchanged

## Final D2 combinations and current APK validation

- Pipeline 15836 completed successfully. The final D2 256px candidate passes all five UUD combination phases, including source PCM/mixer verification and mission re-entry (`hires-d2-final-uud-combinations.log`, `uud-combinations/run_6e759820857f4cecbbf0f017c1e12fab`)
- Both extra-mission priority orders pass levels 1/7: all eighteen extra surfaces, spider-monitor destruction, animated lava/trickle, lava damage, green-door timing/passage and ordinary high-res texture 127. Evidence: `uud-gameplay/extras-uud-first/run_e38cfd8470e9448bb601238c77788b37`, `uud-gameplay/extras-hires-first/run_afee7bbdece246189a145182356d9543`. The imported D2 archive SHA-256 matches the final candidate
- APK packaging succeeded (`hires-final-catalog-apk.log`), but the post-build native identity guard detected another worktree change. The existing `d1_shareware_model.h` sparse-gun-index fix rebuilt D1 polyobj and D1-in-D2 shareware objects; preserve that independent change. This was a validation guard failure, not a Gradle build failure
- Current native libraries pass the paired renderer again (10 D1 / 21 D2 cases; `hires-updated-apk-render/{d1,d2}.json`). The new APK is installed and all nine 256px launcher scenarios plus the actual enemy sprite draw probe pass (`hires-updated-apk-launcher.log`, `hires-release/run_a556407c1ce44faa91de5fd7cec2f5bf`). Process 26064 is terminal. Saved Video Info screenshots show the active pack name/resolution and replacement count; all three removal cases return to zero
- Preserved earlier D1 128/512 launcher evidence inside the final candidate directory at `validation/d1-128-512` before further run retention. Full batch process 20880 completed D2 512px primary conversion and remains live in cross-size merging; no restart or public texture replacement in this stage

## All six final candidates accepted locally

- Staged the completed D2 128/512 outputs after their merge targets finished, checking source/copy hashes before and after. All six final candidates pass archive CRC/path/format/mipmap bounds and all stock palette eligibility/animation checks (`hires-all-final-format.json`, `hires-all-final-coverage.json`). D1: 805 images / 591 stock names; D2: 1,649 images / 1,444 stock names, no rejected or partial groups
- D2 128px is 40,338,095 bytes, SHA-256 `0c8fa175d4387318db244deca5b9060c81064cec71c7be1d016af5e0973c2d4b`; 512px is 394,203,185 bytes, SHA-256 `743e122e8c7dd6646521399e18dfcc942fe53c1b88498cd809643f531e492315`. The 128px pack's single-level shield strip is an unused XL name; the retained cockpit dimensions are intentional converter exceptions, not reachable stock level textures
- D2 128/512 launcher integration passes all five baseline/on/off scenarios and both actual enemy sprite probes on the new APK (`hires-d2-final-128-512-launcher.log`, `hires-release/run_9d4fa1260d5f4d83b4e091b53b0433d0`). Session 31523 is terminal success
- Independently reconverted the sixty 256-source-only additions with current code at both target resolutions. All 120 resulting entries are byte-identical to the completed batch output (`validation/merge-comparison/result.json` inside the final candidate directory); session 62022 is terminal success
- Copied the six hash-verified candidates atomically into the production archive paths. Publisher session 37625 is running (`hires-final-publish.log`). The released D2 256px input is the independently completed and fully accepted current-code candidate; the older batch continues its separate 256px merge in session 85339 and does not own production paths
- Remaining: finish publication, prove repeat publication changes nothing, download recommended high-res packs through the popup and verify their actual imported runtime/re-entry in all three modes, then complete the requirement-by-requirement audit. Prepared `android/temp/hires-published-download/downloaded-runtime.jsonc` to exercise the download set directly without replacing its imported mods; it has not run before publication

## Completion audit, 2026-10-11

| Requirement                                              | Final evidence and result                                                                                                                                                                                                                                                                                                                                                                                                                |
| -------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Source inventory and silent construction failures        | High-res audit records 1,658 original TGAs, alpha/format/animation/opaque-rock/additive repairs and unsupported aliases. All six final packs pass format/CRC checks and 21 game/resolution/palette coverage combinations. Sound audit independently matches all 304 source WAVs and all 98/183 stock sound names                                                                                                                         |
| Runtime lookup, priorities, ownership and disable/reload | Current native renderer passes 10 D1 / 21 D2-engine cases including D1-in-D2, both priorities, actual mission PG1/POG precedence, owned masks, pixels, blend restoration and draw counts. All three modes pass final 256px on/off status and GPU residency checks; all other sizes have launcher acceptance                                                                                                                              |
| UUD fixes and extra textures                             | Independent source checks cover 121 D1 and 1,118 D2 images/masks, indexed data and all seven palettes. Native HAM/surface/timing checks and actual 72-surface D1 gameplay, one-second door opening, seven-palette author/extra missions and both final high-res orders pass. Final combination logs: `hires-d1-rebuilt-uud-combinations.log`, `hires-d2-final-uud-combinations.log`, `hires-d2-final-extras-{uud-first,hires-first}.log` |
| Additional candidate research                            | `game_data/mods/MOD_CANDIDATES.md` records eight candidates from primary author/project pages, including XL textures/effects/models/wallpapers and UUD2SP, with concrete compatibility work and no unsupported recommendation claims                                                                                                                                                                                                     |
| Repeatable GitHub release and recommendations            | `hires-final-publish.log`: 9 changed assets, 6 unchanged, all 15 verified. `hires-final-republish.log`: 0 changed, all 15 verified. Final live API check matches all 12 mod identities and generated header/table/credits. The eight-item popup includes both UUD packs and accurate final 256px sizes                                                                                                                                   |
| Actual published downloads and gameplay                  | Fresh popup run passes 19 steps. Four downloaded UUD/high-res archives match remote hashes, import sizes and enabled state. The exact imported set passes fresh launch and mission re-entry in D1, D2 and D1-in-D2, with 14/23/14 mipmapped high-res replacements and actual UUD indexed/semantic patches (`hires-published-download/downloaded-identities.json`, `runtime.log`, mode JSON files)                                        |
| Builds, regression checks and catalogs                   | Current APK build succeeds and its native renderer/launcher revalidation passes. Both completion catalogs pass 329 top-level / 387 support entries. Mask repeatability regression passes both actual conversion paths and all prior alpha/mask/material/failure-preservation cases (`hires-mask-determinism-acceptance.log`). Scoped final formatting is recorded in `hires-completion-format.log`                                       |

- Full background batch 85339 completed successfully. All final payloads match the chosen release inputs except fifteen PNG mask date chunks; independent RGBA comparison proves every differing mask has identical dimensions/pixels. Current converters now exclude those timestamp chunks. This prospective metadata-only repair does not require replacing already accepted artwork or rebuilding the APK
- The first download attempt referenced an old set whose registration had been reset by the test harness. The final run creates `hires-release-download-20261011`, uses `-PreserveState`, and exercises that same downloaded set without reimporting host packs. No app change was inferred from the harness setup failure
- Public release: `https://github.com/larsonm-personal/dxx-redux/releases/tag/texture-packs-v1`. Only mod assets were published. Native/launcher fixes require the next distributed APK; the tested APK was installed on emulator-5586 only. Existing installed packs need removal/re-download to obtain updated release assets
