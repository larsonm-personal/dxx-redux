# D2X-XL mission package support: engineering study

## Recommendation and scope

Extend the existing mission asset context with **mission-scoped external texture resolution first**. Prove that an authored TGA under `mods/<mission>/textures/` is selected for its owning mission and disappears when that mission is unloaded. Reuse the existing Android image decoder, importer, immutable publication and metadata browser. Do not flatten replacement files into a global texture directory or build a second importer.

This is a resource fidelity improvement, not an admission rule for XL levels. BoilingPoint supplies a real external-texture witness but has additional gameplay dependencies; successfully resolving its textures would not make the mission playable. Keep format, gameplay, resource closure and visual qualification as separate results. Per-level textures, sound banks/WAV, models and mission configuration are subsequent slices, not hidden requirements of the first change.

This report is research only. No runtime changes, new gameplay runs, decoder tests or visual comparisons were performed for it. Existing PRs #120-124 are untouched. The source and archive inspections below support the design; the acceptance tests are proposed work, not completed tests.

## Evidence baseline and confidence

| Input               | Pinned revision / scope                                                                                                   |
| ------------------- | ------------------------------------------------------------------------------------------------------------------------- |
| Redux               | `8acea2079c8c7f5724da52796854cee7e9f03bc1`, published `xl-feature-browser`, verified before study                         |
| Mission assets      | `larsonm-personal/dxx-redux-test-assets` at `d9fbeaf6eb059fd4504308bfd6364734775b7cba`                                    |
| XL source reference | `arbruijn/d2x-xl` at `2576a98b5de310e3ac7c9777df2677d1627b8bae`                                                           |
| Documentation       | [D2X-XL modding guide](https://www.descent2.de/d2x-modding), retrieved 2026-10-24; mutable, not a versioned specification |
| Archive sample      | All 19 `missions/d2xxl_downloads/*.7z`, plus six named ZIPs in the companion evidence manifest                            |

The [evidence manifest](xl_mission_package_evidence_20261024.json) records complete-archive SHA-256 values, file counts, extension counts, external mod entries, outer nested-archive entries and case-fold collisions. A local libarchive header walk read original archive bytes; 44 external `mods/` files were also streamed through SHA-256 with a 32 MiB per-entry audit bound. It did not recursively decode every HOG/DXA or execute any mission. A supplementary ZIP central-directory search selected the two DXA examples. No mission binaries or proprietary resource contents are included.

Labels used below: **observed** means archive headers or code inspected in this study; **source contract** means behavior in the pinned XL source, not a claim of every historical release; **proposal** means intended Redux work; **unverified** means a fixture or runtime experiment is still needed. Source links are pinned in the reference index. Repository guidance was read; no checkout `.agents/skills` directory was present.

## What the packages actually contain

An XL distribution is not a new ZIP codec. It can be a ZIP, 7z or wrapper containing familiar mission descriptors/HOGs plus a sibling `mods/` tree. Archive basename, mission title, descriptor stem, loaded HOG stem and level filename are distinct identities.

| Observed package               | Relevant layout / engineering value                                                                                                                                                                                                                                            |
| ------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `d2xxl_downloads/levelpack.7z` | 918 file entries, many missions under `missions/<author-or-category>/`; sibling `mods/` tree. SHA-256 `b4b519dc5755b795fd49cc3786a0421b96cd099dcc8f002925ddfd7c038b2719`                                                                                                       |
| BoilingPoint in that pack      | `missions/single/boilpnt.mn2`, `boilpnt.hog`; `mods/boilpnt/textures/` contains 29 TGA and one BMP. Examples: `ceil002.tga`, `door53#0.tga`, `force01#0.tga`, `redgoal#0.tga`; `ceil002.bmp` is a separate preserved file, not evidence that the XL texture loader selects BMP |
| Enemy Within in that pack      | `missions/single/ewithin.mn2` and `.hog`; `mods/ewithin/textures/COCKPITB.TGA` and `STATUSB.TGA`. Useful case-sensitive-filesystem and cockpit-resource witnesses                                                                                                              |
| Dragon Pit in that pack        | `missions/spotlight/drgpit.msn` and `.hog`; `mods/drgpit/sounds/boss01.wav`, `boss02.wav`. The `.msn` matters: this is not automatically a D2-only resource test                                                                                                               |
| Other pack mod data            | `mods/250/250.ham`; `mods/lostlvls/lostlvls.ham`, `.sng`, `.txb`. Data definitions, song tables and briefing text must not be treated as ordinary texture overlays                                                                                                             |
| `d2xxl_downloads/po2.7z`       | `mods/pluton2/pluton2.pmh`, `.pti`, `.s11`, `.s22`, and `descent.sng`. Preserve their names and classify them independently; never rename PMH to stock HAM                                                                                                                     |
| `d2xxl_downloads/eaf2.7z`      | `mods/eaf2/descent.sng`, exercising the current song-list alias path                                                                                                                                                                                                           |
| `EAF.zip`                      | Root `ENCOUN.HOG`, `ENCOUN.MN2`, `ENCOUN.TXT`: ZIP basename is not mission identity                                                                                                                                                                                            |
| `Disint_Beta_3.zip`            | Root `D2DISINT.HOG`, `d2disint.mn2`, `d2disint.txt`: mixed case and another archive/descriptor mismatch                                                                                                                                                                        |
| `Entropy.zip`, `ironblade.zip` | Uppercase root constituents; Ironblade uses `IRONBL01.MSN`/`.HOG`                                                                                                                                                                                                              |
| `Uneasy4.zip`, `levigen.zip`   | Outer members `Uneasy4.dxa` / `LEVIGEN.DXA`. These are actual nested-container witnesses; this survey does not certify their inner resource resolution                                                                                                                         |

No case-fold duplicate file paths were found in the 25 manifest samples. No directory components matching `level<digits>` or `slevel<digits>` were found in these outer manifests; nested contents and other distributions remain unqualified. No external ASE/OOF files were found in the sampled `mods/` trees. These missing witnesses require synthetic tests, not invented real-mission claims. The sample is not the entire downloadable XL ecosystem.

Wrapper directories and nested archives need separate handling. A distribution wrapper may contain several variants rather than one mission; automatically selecting a child can discard sibling mod resources. The current preferred-child import path copies the selected child into its own package. Determine whether resources belong inside that child or alongside it before allowing an XL wrapper to use this shortcut. Do not recursively flatten every archive or let archive enumeration order choose a mission. [R1, R2]

## Current Redux: preservation is ahead of lookup

The Android pipeline already has useful separation:

1. `MissionZip`/`ArchiveFiles` inspect ZIP/7z/RAR and identify mission sets. Preferred nested ZIP variants and DXA music handling are specific paths, not a universal recursive archive resolver.
2. `MissionZipExtractionStore` plans and publishes bounded extraction. Launch constituents move under the generated `missions/` namespace; other entries generally retain their paths. Generated `cache/` content is skipped except required constituents. It recognizes `mods/<descriptor-stem>/descent.sng`; a single eligible song list can produce a root `descent.sng` alias.
3. `MissionLaunchCatalog` associates resources with missions. Direct constituents win; same-stem sidecars and `mods/<descriptor-stem>/` follow; otherwise the nearest descriptor directory owns descendants, and unowned files become package-shared. It checks conflicting case-folded virtual paths.
4. `MissionLaunchPublication` copies/hardlinks selected resources into immutable `.mission_assets/<generation>/contexts/<token>` directories. It retains virtual paths and publishes descriptor aliases and a context manifest.
5. Native `android_mission_assets` mounts the context root and selected DXAs, resets baseline state, and tracks an activation generation. It does **not** turn `mods/foo/textures/x.tga` into a flat `x.tga` lookup. [R1-R4]

Consequences: root-level `mods/<descriptor-stem>/` ownership exists, but HOG aliases and arbitrary wrappers are not resolved by that string-prefix rule. Two descriptors sharing a basename or HOG need explicit association/ambiguity rules. An unmatched mod subtree must not become a silently active package-wide override. A preserved resource does not prove a decoder accepts it, that runtime searches its path, or that its owner is selected.

| Resource                      | Current inspected behavior                                                                                                                                          | Missing XL contract / qualification                                                                                                                                                                                               |
| ----------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| External wall textures        | Android supports KTX2, PNG, JPG and TGA candidates. D2 supplies flat bitmap names and `textures/d2[/sets/<pigset>]/...`; archive priority participates in selection | Mission/ordinal XL scope, exact-frame versus `#0` fallback, interaction with POG and animation/transparency fidelity. Non-Android fallback shown in `ogl.c` is flat PNG: do not advertise Android decoder coverage as host parity |
| POG / HXM                     | Existing classic level replacement paths, not a new archive format                                                                                                  | Extended XL POG RGBA/animation flags need separate qualification; mission-wide HXM scope and ordering are not implied by classic level HXM support                                                                                |
| ASE                           | `d2/xmodel` has fixed-name ASE loading and PNG/TGA skin support                                                                                                     | `xcfile.h::Open` ignores its directory argument and opens the filename through PhysFS. Upstream-looking folder fields do not establish functional mission/level lookup. Alias/model-number mapping and skin ownership need work   |
| OOF                           | No OOF loader identified in the inspected Redux D2 model implementation                                                                                             | New parser/render qualification, separate from routing ASE; absence of sampled files is not a support claim                                                                                                                       |
| Effects / sound banks         | Android raw effect replacements search `Sounds/<name>.r22` then `.raw`, with source priority and D1 scoping                                                         | That path does not implement `mods/.../sounds/*.wav` or XL `.s11/.s22` bank gating. Music WAV decoding elsewhere would not qualify effect-bank replacement                                                                        |
| Music                         | Existing SNG discovery/aliasing, nested DXA/HOG track extraction and user music machinery                                                                           | XL playlist and ordinal music precedence, secret mapping, option interactions and teardown must be integrated rather than added as another player                                                                                 |
| Wallpapers / effects / config | Files can survive import; ordinary UI and rendering systems exist                                                                                                   | No claim of XL `wallpapers/` selection, special effect replacement, RGB briefing commands, `global.ini` or per-level INI execution. Detection must remain distinct from use                                                       |
| HAM / V-HAM / PMH / PTI / PIG | Existing classic/enhanced data loading is valuable but gameplay-affecting                                                                                           | Resource admission, versions and dependencies require their own audited paths. Do not mount a companion `descent2.ham` early or reinterpret extensions to bypass admission                                                        |

Sources: texture paths/decoder [R5], models [R6], effect replacements [R7], music import [R8], mission resource boundary [R9].

## Pinned upstream contracts, by resource

### Identity and scope

`LoadLevel` calls `MakeModFolders(hogFileManager.MissionName(), nLevel)`. The mod name is derived from the active mission/HOG identity; built-ins map to `descent` or `descent2`, with a mod-enable option. Missing custom mod directories can fall back to built-in mod data. `LevelFolder` uses the **signed mission ordinal**, producing `level01/` or `slevel01/`; it does not use the level filename stem. Conversely, POG/HXM are keyed to the actual level filename. [U1, U2]

The mission parser accepts `hog=`, but the normal load path also clears/reopens a same-basename HOG. Alias behavior needs a fixture against the intended historical version; a descriptor directive alone does not prove which HOG supplies the resource identity. Redux should record descriptor identity, selected HOG identity and the resolved mod subtree separately, and reject ambiguity. ZIP name and display title must never be resource keys. [U3]

### Textures and POG

The source searches an exact bitmap/frame name through six candidates: level cache, level authored TGA, mission cache, mission authored TGA, base cache, base authored TGA. Only after that search does it retry an animation name with `#0`. Thus an exact mission frame may beat a level-wide `#0` fallback; simply searching both names per directory changes behavior. Cache files are generated products, not trusted authored inputs. [U4]

There is a likely pinned-source quirk: `0 < SearchHiresBitmap(...)` appears to reject candidate index zero (level cache). Record it for differential testing; do not deliberately reproduce a likely cache bug. The useful contract is authored-resource precedence and regeneration keyed by content, not compatibility with arbitrary distributed cache blobs.

Level POG replacements are installed after level textures, and bitmap paging returns early for existing buffers. XL POG can carry `BM_FLAG_TGA`, four-byte pixels and vertically stacked animation data; classic POG support is not proof of this extension. XL TGA processing also handles alpha, the RGB `(120,88,128)` super-transparency marker, frame masks and average colors. A generic TGA decode is necessary but insufficient for renderer parity. [U4, U5]

### Models and definition data

Mission HXM is loaded before level HXM. Hires model loading considers level then mission scopes, and tries descriptive aliases before `modelN`. ASE is attempted before OOF in the loading routine, but their mappings are separate: final mixed-format render precedence remains unverified. Do not collapse this into an unconditional universal ASE-over-OOF promise. [U2, U6]

ASE authored skin paths are stripped to basenames and resolved in the model scope, not the wall-texture scope. Missing skins can make model loading fail. A model-only resolver that silently borrows a same-named skin from another mission would be wrong. Definition files can affect robot/weapon behavior and dependencies; separate their admission from purely cosmetic assets. [U7]

### Effects and music

For custom sound loading, presence of the mission `.s22` or `.s11` bank selects low-resolution-bank handling and bypasses external WAV lookup. Without that bank, the WAV lookup tries level, mission, then option-dependent base sound folders, mapped by bank sound names. The code's fallback/verification paths and mixer resampling still require tests; do not implement an unconditional WAV-over-bank rule. [U8]

Music is a different subsystem. Its level selection can try HOG music first when `bFromHog` is set, then the mod playlist, mod ordinal OGG, user playlist, standard playlist/music and ultimately other configured fallback. Playlist parsing accepts OGG and FLAC and has secret-level handling. Decoder availability depends on the build. The mod music-folder discovery still has an OGG probe, so an all-FLAC package needs an explicit fixture before claiming automatic discovery. [U1, U9]

### Presentation, configuration and lifecycle

Mod wallpapers select `default.tga`, `levelXX.tga` or `slevelXX.tga`. Built-in fallback includes randomized loading-screen selection; that presentation policy should not consume simulation RNG in Redux. `UnloadLevelData` clears mod folders and temporary hires settings and frees textures, sounds, models, animations and effects. Resetting only the search path would leave stale decoded/GPU/audio objects alive. [U1, U10]

The guide additionally describes mission/level INI files inside HOGs and RGB briefing tags. They are detected capabilities to audit later, not authority to execute arbitrary imported configuration. This study does not establish complete command semantics or historical-version applicability.

### Documentation discrepancies

The [modding guide](https://www.descent2.de/d2x-modding) is helpful for author intent but contains errors and omissions. Its sound examples refer to model directories; the pinned loader uses sound directories. It describes mod music as OGG-only, while playlist code accepts FLAC. It requires dummy root assets to activate some folders; the pinned folder checks are not uniformly extension-gated. Its HAM naming prose and example also disagree. Prefer explicit source-backed rules with fixtures, preserving uncertainty for HOG aliases and mixed model formats. Do not turn documentation mistakes or cache quirks into compatibility requirements.

## Proposed design boundaries

Use one bounded resource-context record tied to the existing mission activation: package content revision, descriptor key, selected HOG key, game identity, mod root and signed level ordinal. Keep original resource paths for provenance; add lookup candidates rather than copying names into a shared root. Host inspection must be able to describe the same manifest without mounting/playing the mission.

Resolve ownership before precedence. Select only resources owned by the current mission plus deliberately package-shared data; never search every sibling `mods/` directory. For wrapper packages, identify the logical content root explicitly. Treat duplicate descriptor/HOG basenames, conflicting aliases and ambiguous shared ownership as errors with reasons. A package-relative lookup must not escape the immutable context even if an imported model, playlist or descriptor names an absolute path.

Preserve the existing classic D1/D2 and user global-mod precedence when no XL context is active. Define where a scoped XL candidate sits relative to global replacements, then test that rule on both games. Do not activate `mods/descent2` globally because a custom package contains it. D1-on-D2 sound/texture namespacing and LAN defaults remain unchanged.

Cache keys must include content revision, resource identity, scope/ordinal, decode options and relevant renderer/audio settings. Negative lookups also need invalidation. On level advance, secret entry/return, restore, mission switch, import replacement, failed load, teardown and renderer recreation: invalidate the relevant texture/model/sound cache, stop scoped audio, restore defaults and release mounts in the correct order. Reuse `android_mission_asset_reset_before/baseline` and generation tracking rather than a parallel lifecycle. [R4, R10]

Saves already carry mission asset identity through the Android shared state hooks. Extend/reuse that identity for deterministic resource reconstruction before loading game definitions; never save absolute extraction paths or GPU handles. Check save/replay package revision mismatch explicitly. Cosmetic changes should not alter SIM/RNG traces; HAM/HXM/config changes can, and require dependency hashes and new admission evidence. Do not claim loose hires textures are replay-portable merely because a save loads. [R10]

## Extraction and failure handling

Keep existing bounds: 512 MiB per entry, 2 GiB total extracted/source ZIP, 4,096 entries, 1,000:1 expansion ratio where compressed size is available, 1 MiB descriptors, 64 MiB metadata, 128 MiB combined read memory and a 256 MiB 7z decoder cap. The 16 MiB ZIP preamble cap supports bounded self-extracting wrappers. These are current limits, not recommended increases. Enforce cumulative budgets across any newly supported nested path, including actual streamed bytes, cancellation and unknown-size entries. [R11]

`normalizeArchivePath` is not by itself a security check. Output projection validation canonicalizes components and rejects case/NFC collisions and file/directory conflicts; mission path validation adds constraints. Review the complete extraction destination check before reusing a normalized path. Test absolute/drive/UNC paths, traversal after separator conversion, Unicode/case aliases, symlink/hardlink metadata, truncated streams and partial-publication rollback. Do not claim all archive backends handle link metadata identically without tests. [R1, R11]

Decoders need separate bounds on image dimensions, frame count, pixel multiplication, model counts/indices/materials, WAV chunks/sample count, playlist length and referenced-file expansion. A tiny archive can describe a huge decoded allocation. Never execute packaged code or configuration as part of browsing. Unsupported files can be preserved safely but must not be globally mounted as executable game definitions. Distinguish malformed required data (fail activation) from optional missing art (documented fallback), and report the chosen fallback rather than silently claiming full fidelity.

## Metadata and UI contract

Extend the existing structured compatibility/resource report, not a second Kotlin support table. Suggested per-resource fields: kind, original path, owner, scope, declared/inspected count, content hash, selected candidate, reason and evidence stage. Keep stages distinct:

- **Detected:** matching entry exists; bytes may be uninspected.
- **Preserved:** validated extraction/publication retained it.
- **Resolved:** a particular context lookup selected that entry.
- **Decoded:** the relevant decoder accepted it within bounds.
- **Qualified:** a specified render/audio/gameplay test passed; not a universal claim.
- **Unsupported / unknown:** explicit reason, including absent or old metadata.

A compact row could say `External textures: 29 - preserved, lookup unsupported`, then later `29 detected; 1 selected in this level`. Do not report 29 decoded/visible merely because 29 filenames end in `.tga`. Parsed-only and partial support remain orange; unsupported red; green only for the precisely qualified feature/build. Keep resource fidelity separate from per-level admission and the existing XL gameplay rows. Detail taps can show source scope and precedence without network calls, decoding or level execution. Count null means unknown, not zero. Duplicate candidates count separately from resolved logical resources.

## Implementation sequence and acceptance gates

| Slice                                       | Modules / dependencies                                                                                                                         | Concrete acceptance criterion                                                                                                                                                                                                                                                                                 |
| ------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 1. Mission textures                         | Existing `MissionLaunchCatalog`/publication context, native mission assets, `ogl.c`, Android texture reader; minimal shared metadata additions | Original BoilingPoint entries retain hash/path/owner; one static TGA resolves only inside that mission context and decodes with bounded dimensions. Synthetic same-name mission B and classic baseline select their own bytes after A unload. No gameplay admission change                                    |
| 2. Level/secret texture precedence          | Slice 1 plus signed ordinal notification and texture cache invalidation                                                                        | Synthetic mission deliberately maps level ordinal 1 to a differently named RL2; `level01` wins, `slevel01` wins on secret entry, return restores correct parent. Exact-frame search precedes `#0` across scopes. Explicit POG precedence test; extended RGBA POG stays unsupported until separately qualified |
| 3. WAV effects / banks                      | Stable context and reset contract; `android_mod_assets`, mixer, bank loader                                                                    | Original Dragon Pit WAVs are inspected for actual format, then admitted only if decoder/sample mapping is qualified. Synthetic bank-present/absent cases enforce bank gating; A/B/classic switches clear cached sound and active voices. Check D1 and D2 separately                                           |
| 4. Music and wallpapers                     | Existing music editor/playback/SNG machinery and menu background lifecycle                                                                     | SNG alias ownership remains correct for EAF2/PO2. Synthetic playlist/ordinal/HOG option precedence and secret mapping; no duplicate players, stale songs or simulation RNG use. Wallpapers qualified independently of music                                                                                   |
| 5. Mission HXM / ASE                        | Resource closure and definition reset; `xmodel`, `xcfile`, skin loader                                                                         | Synthetic alias/model-number and level/mission overrides, same-scope skins, missing-skin behavior, valid classic HXM and malformed bounds. Original ASE witness required before ecosystem claim. OOF remains a separate parser/render slice                                                                   |
| 6. Additional definitions / XL presentation | HAM/V-HAM/PMH/PTI and POG format audits, special effects/config/briefing                                                                       | Each file class gets explicit dependency closure, malformed fixtures and save/replay tests. Never rename PMH or enable unknown INI directives. Full mission qualification waits for unrelated gameplay gates too                                                                                              |

Slice 1 should support ordinary static authored TGA first, with animations/transparency explicitly partial until their semantic tests pass. It can reuse Android's decoder without promising new cross-platform decoder support; host inspection should still report the build-specific limitation. Decide a stable global-versus-scoped overlay rule before coding, and retain existing defaults outside that scope. Avoid a large generic scripting/resource framework.

### Efficient test matrix

Start with pure catalog/path tests using tiny synthetic files: archive name differs from descriptor/HOG, `hog=` alias, wrapper root, two missions sharing short names, uppercase files, duplicate/case/NFC paths, optional missing files and conflicting resource roots. Reuse extraction rollback tests and selected-child/DXA tests; do not rerun the whole corpus for each path change.

Then use one pinned original resource from BoilingPoint and the two uppercase Enemy Within cockpit entries for bounded import-to-publication-to-native-selection tests. These are resource tests, not BoilingPoint campaign runs. The 918-entry levelpack is a useful final multi-mission ownership/performance check. DXA samples cover existing nested behavior; synthetic sibling-resource wrappers cover the unobserved association case. No denied Belial runtime campaign is part of this plan.

For each resolver slice, add a small native selection trace/assertion with owner, ordinal, candidate and hash, then a real decoder test and one renderer/audio check where applicable. Use synthetic artwork in committed visual fixtures and CI screenshots; obtain original resources from the pinned private asset checkout. Test A -> B -> stock -> A, level -> secret -> return, native save/restore, failed activation rollback and resource revision replacement. Compare SIM/RNG only where meaningful; do not assert timing-dependent audio playback is deterministic.

Run focused JVM/native tests first, scoped Android compile next, then one existing emulator integration owner through completion. Register a new top-level owner only if an existing asset integration cannot express the test. Preserve classic and LAN regressions. Record exact build/head and selected asset hashes. A source review or successful import cannot substitute for actual resolution/rendering evidence.

## Open decisions and explicit limits

Before implementation, settle global-mod versus scoped replacement precedence, HOG-alias identity, ambiguous wrapper ownership and corrupt optional-resource fallback. Differential fixtures are still needed for mixed ASE/OOF, all-FLAC mod discovery, WAV resampling, absolute/case-sensitive upstream paths, candidate-zero cache behavior and extended POG. The reference is one source revision, not proof of semantics for every historical XL mission.

This study inspected code, archive headers and selected resource hashes. Scoped repository formatting passed; manifest validation checked 25 archive records, 44 resource hashes, extension totals and the existence of pinned source-link paths. To reproduce the inventory, enumerate non-directory libarchive headers for the manifest's 25 exact archive paths, hash each complete archive, group normalized suffixes, compare case-folded paths and stream the listed `mods/` members through SHA-256. Compare signed ordinals by directory components, not file basenames. This is an audit recipe, not an Android importer execution. It did not re-run the completed PR120-124 campaigns, perform new original-mission loads, inspect rendered screenshots or listen to sound. Their existing green checks do not certify the proposed package resolver. The first deliverable should be a narrowly tested texture path improvement with honest metadata, followed by independent resource slices.

## Source index

Redux links below are pinned to the study baseline; XL links are pinned to the reference revision. Link labels identify the functions discussed above.

- **R1**: [archive validation](https://github.com/larsonm-personal/dxx-redux/blob/8acea2079c8c7f5724da52796854cee7e9f03bc1/android/app/src/main/java/com/dxxredux/app/ArchiveFiles.kt), [extraction plan](https://github.com/larsonm-personal/dxx-redux/blob/8acea2079c8c7f5724da52796854cee7e9f03bc1/android/app/src/main/java/com/dxxredux/app/MissionZipExtractionStore.kt)

- **R2**: [preferred child import](https://github.com/larsonm-personal/dxx-redux/blob/8acea2079c8c7f5724da52796854cee7e9f03bc1/android/app/src/main/java/com/dxxredux/app/ModManager.kt), [mission inspection](https://github.com/larsonm-personal/dxx-redux/blob/8acea2079c8c7f5724da52796854cee7e9f03bc1/android/app/src/main/java/com/dxxredux/app/MissionZip.kt)

- **R3**: [ownership and resources](https://github.com/larsonm-personal/dxx-redux/blob/8acea2079c8c7f5724da52796854cee7e9f03bc1/android/app/src/main/java/com/dxxredux/app/MissionLaunchCatalog.kt), [context publication](https://github.com/larsonm-personal/dxx-redux/blob/8acea2079c8c7f5724da52796854cee7e9f03bc1/android/app/src/main/java/com/dxxredux/app/MissionLaunchPublication.kt)

- **R4**: [native context lifecycle](https://github.com/larsonm-personal/dxx-redux/blob/8acea2079c8c7f5724da52796854cee7e9f03bc1/android/app/src/main/cpp/shared/android_mission_assets.cpp)

- **R5**: [texture names](https://github.com/larsonm-personal/dxx-redux/blob/8acea2079c8c7f5724da52796854cee7e9f03bc1/d2/arch/ogl/ogl.c#L3050), [Android candidates](https://github.com/larsonm-personal/dxx-redux/blob/8acea2079c8c7f5724da52796854cee7e9f03bc1/android/app/src/main/cpp/shared/ogl_texture_android.c#L156)

- **R6**: [ASE model load](https://github.com/larsonm-personal/dxx-redux/blob/8acea2079c8c7f5724da52796854cee7e9f03bc1/d2/xmodel/xmodel.cpp), [directory-ignoring Open](https://github.com/larsonm-personal/dxx-redux/blob/8acea2079c8c7f5724da52796854cee7e9f03bc1/d2/xmodel/xcfile.h#L57), [model skins](https://github.com/larsonm-personal/dxx-redux/blob/8acea2079c8c7f5724da52796854cee7e9f03bc1/d2/xmodel/tga.cpp)

- **R7**: [raw sound overrides](https://github.com/larsonm-personal/dxx-redux/blob/8acea2079c8c7f5724da52796854cee7e9f03bc1/android/app/src/main/cpp/shared/android_mod_assets.cpp#L116)

- **R8**: [music discovery](https://github.com/larsonm-personal/dxx-redux/blob/8acea2079c8c7f5724da52796854cee7e9f03bc1/android/app/src/main/java/com/dxxredux/app/MissionZipMusic.kt), [bounded nested tracks](https://github.com/larsonm-personal/dxx-redux/blob/8acea2079c8c7f5724da52796854cee7e9f03bc1/android/app/src/main/java/com/dxxredux/app/MissionZipMusicStageManager.kt)

- **R9**: [mission definition admission](https://github.com/larsonm-personal/dxx-redux/blob/8acea2079c8c7f5724da52796854cee7e9f03bc1/d2/main/mission.c)

- **R10**: [baseline reset](https://github.com/larsonm-personal/dxx-redux/blob/8acea2079c8c7f5724da52796854cee7e9f03bc1/android/app/src/main/cpp/shared/android_mission_asset_reset.c), [save mission identity](https://github.com/larsonm-personal/dxx-redux/blob/8acea2079c8c7f5724da52796854cee7e9f03bc1/android/app/src/main/cpp/shared/state_android_shared.c#L800)

- **R11**: [limits and streamed budgets](https://github.com/larsonm-personal/dxx-redux/blob/8acea2079c8c7f5724da52796854cee7e9f03bc1/android/app/src/main/java/com/dxxredux/app/ExtractionLimits.kt), [ZIP source and preamble](https://github.com/larsonm-personal/dxx-redux/blob/8acea2079c8c7f5724da52796854cee7e9f03bc1/android/app/src/main/java/com/dxxredux/app/ArchiveInputStreams.kt)

- **U1**: [MakeModFolders and LevelFolder](https://github.com/arbruijn/d2x-xl/blob/2576a98b5de310e3ac7c9777df2677d1627b8bae/main/gamefolders.cpp#L602)

- **U2**: [mod and level resource ordering](https://github.com/arbruijn/d2x-xl/blob/2576a98b5de310e3ac7c9777df2677d1627b8bae/gameio/loadgame.cpp#L810), [active HOG identity](https://github.com/arbruijn/d2x-xl/blob/2576a98b5de310e3ac7c9777df2677d1627b8bae/gameio/loadgame.cpp#L970)

- **U3**: [hog directive and normal load](https://github.com/arbruijn/d2x-xl/blob/2576a98b5de310e3ac7c9777df2677d1627b8bae/gameio/mission.cpp#L963)

- **U4**: [exact/frame candidate search](https://github.com/arbruijn/d2x-xl/blob/2576a98b5de310e3ac7c9777df2677d1627b8bae/gameio/loadtextures.cpp#L655), [paging](https://github.com/arbruijn/d2x-xl/blob/2576a98b5de310e3ac7c9777df2677d1627b8bae/gameio/loadtextures.cpp#L919), [POG replacement](https://github.com/arbruijn/d2x-xl/blob/2576a98b5de310e3ac7c9777df2677d1627b8bae/gameio/loadtextures.cpp#L1021)

- **U5**: [TGA semantics](https://github.com/arbruijn/d2x-xl/blob/2576a98b5de310e3ac7c9777df2677d1627b8bae/gameio/tga.cpp#L130)

- **U6**: [model aliases and formats](https://github.com/arbruijn/d2x-xl/blob/2576a98b5de310e3ac7c9777df2677d1627b8bae/models/hiresmodels.cpp#L258)

- **U7**: [skin basename](https://github.com/arbruijn/d2x-xl/blob/2576a98b5de310e3ac7c9777df2677d1627b8bae/models/aseread.cpp#L621), [model skin scope](https://github.com/arbruijn/d2x-xl/blob/2576a98b5de310e3ac7c9777df2677d1627b8bae/gameio/tga.cpp#L1062)

- **U8**: [WAV lookup](https://github.com/arbruijn/d2x-xl/blob/2576a98b5de310e3ac7c9777df2677d1627b8bae/gameio/loadsounds.cpp#L202), [custom bank gating](https://github.com/arbruijn/d2x-xl/blob/2576a98b5de310e3ac7c9777df2677d1627b8bae/gameio/loadsounds.cpp#L391)

- **U9**: [playlist parsing](https://github.com/arbruijn/d2x-xl/blob/2576a98b5de310e3ac7c9777df2677d1627b8bae/audio/songs.cpp#L350), [level music order](https://github.com/arbruijn/d2x-xl/blob/2576a98b5de310e3ac7c9777df2677d1627b8bae/audio/songs.cpp#L667)

- **U10**: [UnloadLevelData](https://github.com/arbruijn/d2x-xl/blob/2576a98b5de310e3ac7c9777df2677d1627b8bae/gameio/loadgame.cpp#L684)
