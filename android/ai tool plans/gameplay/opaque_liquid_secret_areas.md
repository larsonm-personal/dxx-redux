# Opaque liquid secret areas

## Scope and status

Planning survey completed 2026-09-07; implementation started 2026-09-10

User request: recognize optional secret pockets behind opaque fly-through lava/water, including the floor pocket near the start of Obsidian level 13, while excluding see-through liquids and ordinary level geometry

This document consolidates the initial conversation proposal and investigates its two prerequisites: texture loading parity and stable results across animation, rescans, and save restoration

- [x] Trace the current scanner and compare opaque/transparent illusion-wall behavior in D1 and D2
- [x] Inspect Obsidian 13 geometry, objects, and local texture headers read-only
- [x] Trace gameplay, metadata-worker, preview, replacement, animation, cache, and save paths
- [x] Define proposed changes and deferred validation below
- [x] Start implementation with A1 effective bitmap flag ownership in both games
- [x] Implement conservative liquid detection, semantic snapshots, inventory retention, and identity saves
- [x] Add native regression cases and cache completeness/dependency handling
- [ ] Validate native behavior and extend unsupported D1-in-D2 texture mappings
- [ ] Compile and run host/device validation in a separately authorized implementation task

The 2026-09-07 follow-up changed only this plan. Initial implementation retained the no-compilation/no-emulator constraints. During the subsequent regression investigation, the user explicitly authorized all necessary work; see the validated follow-up below

### Implementation progress, 2026-09-10

- Implemented A1 in paired `d1/main/piggy.c` and `d2/main/piggy.c`: `piggy_bitmap_get_flags()` uses stored file flags only for PIG-backed entries with a nonzero file offset, and resident bitmap flags otherwise
- Audited the callers in both games' wall transparency checks, OpenGL paths, and shared merged-wall diagnostics. They request effective flags, so the correction belongs in the accessor rather than a separate secret-scanner workaround
- Audited D2 POG loading and native D1 PG1/DTX loading: both clear the bitmap file offset and assign resident replacement flags. D1 custom removal restores the original file offset; D2 PIG reload restores base offsets and stored flags. The accessor now follows these ownership transitions without changing the loaders or raw file-state accessor
- Base PIG flags remain available during page-out. Resident overrides can now differ from their original PIG entry in either direction, including transparency and super-transparency
- Continued implementation after the request for broader scope: added the conservative liquid-pocket pass, texture header snapshots, animation-frame union, load-boundary retention, identity saves, cache invalidation, and regression coverage
- A2/A3 adjustment: the secret classifier reads immutable bitmap header metadata through the native loaders' existing parsers instead of loading pixels or altering renderer state in every worker. Native D1 overlays PG1 then DTX onto base file flags; native D2 reads the selected palette PIG then the level POG. This removes dependence on paging, previous-level replacement buffers, and skipped presentation for supported mappings. General gameplay asset-loading changes are outside this implementation
- D1 header readers now bound bitmap/sound offsets before narrowing them to their existing integer representation; the metadata adapter rejects incomplete sources
- B1: inspect all eclip frames, not the current frame; reject ambiguous/dynamic effect mappings. Apply base transparency and overlay super-transparency rules, additionally rejecting ordinary overlay holes when the underlying base may be transparent. Resolve relevant facts before scanning so uncertain data cannot produce traversal-order-dependent results
- Detection: both faces must be active, opaque liquid illusion walls and physically passable. Preserve the door/trigger pass, then add reward-bearing pockets with no progression. Reject triggers, special segments, narrow/blocked connections, malformed edges, onward walls, ordinary alternate access, and overlap with existing secrets. Merge successive internal sheets, retain multiple external entrances, and keep the 30-secret overflow policy
- B2: successful native `load_level()` calls clear the canonical inventory and hash authored level bytes. The first scan captures membership; route refreshes retain membership and found state. Actual reloads of the same filename rebuild
- B3/B4: D1 version 16 and D2 version 30 use a fixed 282-byte little-endian section: 4-byte count, 8-byte game-scoped level identity, then 30 slots of 8-byte region identity plus 1-byte found status. Region identities use sorted segment membership. Validate before applying; match identities and clear unmatched entries. Old positional sections remain readable through the legacy automap approximation, with liquid pockets unfound. D2's D1-save importer accepts versions 15 and 16, validates the new section, and does not transfer identities between interpretations
- A4: `secret_areas_complete` prevents incomplete results entering the Kotlin result cache. Result schema v4 and synchronized route generation 39 invalidate prior results. Archive cache keys include installed top-level semantic assets from both data directories, including non-groupa PIGs. Loose/directory sources bypass this cache until their dependency closure is represented
- Introspection and headless dumps include `liquid_only` and string region identities for membership/discovery review
- Added `test_secret_area_liquids` to extract CMake/CTest. It calls the real scanner and save codec for positives, transparent/asymmetric/blocked negatives, progression/trigger exclusions, alternate access, nested sheets, multiple entrances, legacy overlap, candidate limits, identity reordering/membership changes, legacy seen-vs-entered behavior, byte ordering, truncation, and malformed records. Kotlin cases cover PIG changes and incomplete-cache rejection
- Source-only validation: 28 serialization, save-validation, and persistence contract checks passed after formatting. Scoped `run-code-quality.ps1 -Fix` and `git diff --check` passed; inherited-file exclusions in the quality runner still apply. Native C/CTest and Kotlin tests are authored but unrun; no compilation, emulator, headless game execution, metadata regeneration, or baseline acceptance was performed

Remaining limits before declaring the bug fixed:

- D1-in-D2 replacement mapping and D1 PPIG custom mapping are deliberately unsupported by the snapshot reader. Relevant liquid levels fail closed and report incomplete metadata; native D1 PG1/DTX and native D2 PIG/POG are implemented
- Unknown/dynamic liquid texture facts disable the additional liquid pass for that level. Existing hidden-door/trigger secrets remain available
- Native compilation, runtime parity, disk/rewind save round trips, and Obsidian 13 membership/visual checks below remain necessary. No expected totals or mission baselines were regenerated

Related background: [original secret-area study](../mixed%20batch,%20umbrella/study_secret_area_autolabel_20260606.md). The existing review finding `BR-0213` already owns index-only secret discovery persistence; this plan develops that prerequisite rather than creating another finding

## Regression follow-up, 2026-09-10

The user subsequently authorized compilation and native execution. The earlier no-build restriction and deferred-validation notes above describe the previous implementation pass.

- Fixed signed trigger sentinel handling (`sbyte` uses -1, not 255); the previous guard rejected ordinary liquid walls
- Added an explicit MSVC packing guard around shared secret structures. Engine headers leave byte packing active; the new 64-bit identities otherwise produced different layouts in pure C and engine callers, crashing detailed secret dumps
- Refined the blanket switch exclusion for a single `TT_OPEN_WALL` target proven to be a sealed reward-only leaf. It must contain no progression items, special segments, further triggers, or onward connections. Other controls and boundary triggers remain rejected
- Native Obsidian 13 now yields six secrets: unchanged four existing secrets plus segment 154 (Mega missile/Phoenix cannon) and 282 (Cloak). Transparent 58/71 and 334/347 water is excluded. The switch in 154 opens the separate optional reward room 153; it is not a required route switch
- Added a native integration runner `android/tests/test_secret_area_liquid_obsidian.py` and a packing regression to the real scanner/codec test; registered that test in both native game CTest suites
- Cache generation 40 invalidates results produced by the broken trigger check
- See [Uneasy4 regression investigation](uneasy4_timeout_and_liquid_regression_20260910.md) for the related startup timeout, baseline comparison, and final validation

## Proposed detection policy

1. Add active, opaque, fly-through liquid illusion walls as candidate boundaries. Require a valid child/reverse connection and conservative physical passability. Initially restrict this addition to engine material classifications for water/lava, rather than all illusion walls or hardcoded texture numbers
2. Classify both faces of a portal consistently. Initially reject asymmetric or uncertain pairs rather than allowing component construction to depend on traversal direction
3. Remove these candidate boundaries from ordinary traversal and identify hidden regions. An ordinary alternate entrance disqualifies a region; multiple hidden entrances to the same region count once
4. Retain the scanner's reward requirement and exclusions for keys, contained key drops, hostages, and reactors. Also prove that a new liquid pocket is optional with respect to exits and progression switches, including effects outside its component. Do not substitute a shortest-route sample for proof of optionality
5. Empty pools, robot-only scenery, visible underwater rooms, and normal liquid corridors should not qualify. An enemy guarding an optional reward need not disqualify it
6. Merge/review adjoining and nested liquid boundaries so sheets inside an existing secret neither create extra secrets nor detach its rewards. Keep the existing 30-secret limit as a fallback, not the main false-positive filter

Current cause: `secret_area_scan.c:is_ordinary_edge()` treats active `WALL_ILLUSION` walls as ordinary passages, while `is_secret_boundary_edge()` recognizes hidden doors and qualifying trigger-opened boundaries only

Read-only Obsidian 13 evidence from `game_data/mission_files/Obsidian.zip`, level `o3tdepot.rl2`:

| Portal, engine numbering | Authored data | Proposed role |
| --- | --- | --- |
| Segment 144 side 3 -> 154, walls 18/19 | Active illusion pair, texture 403, one-room pocket with Mega missile and Phoenix cannon | Positive candidate |
| Segment 203 side 3 -> 282, walls 25/26 | Active illusion pair, texture 403, one-room pocket with Cloak | Positive candidate |
| Segment 58 side 4 -> 71, walls 10/11 | Active illusion pair, texture 401, robotmaker segment beyond it | Transparent negative case |
| Texture 353 illusion pairs around segments 0/12/13 and 334/335/338 | Transparent panels in normal geometry | Transparent negative cases |

The survey used the locally available Vertigo `descent2.ham` and Mac `water.pig` headers to resolve texture names/flags. Texture 403 maps to opaque `water01` frames; texture 401 maps to transparent `water05` frames. None of those frames is replaced by this level's POG. These are static asset findings, not runtime verification against the exact gameplay installation. Which positive pocket matches the user's remembered entrance still needs visual confirmation later

## Prerequisite A: effective texture metadata and loading parity

### Confirmed source findings

| Owner | Current behavior | Consequence for the proposed scanner |
| --- | --- | --- |
| `d2/main/gameseq.c:LoadLevel`, around lines 927-977 | Normally selects the level palette/PIG, applies D2 POG or D1-in-D2 assets, loads robot replacements, then scans | Correct general ordering for ordinary D2 gameplay, but PIG selection is currently skipped with presentation |
| `d1/main/gameseq.c:LoadLevel`, around lines 706-757 | Scans immediately after `load_level()`, before `load_custom_data()` | D1 can classify using base or previous-level custom texture state |
| `headless/headless_metadata_dump_main.cpp:dump_level` and `jni_level_metadata.cpp` level analysis | Initialize `groupa.pig`, then call `load_level()`, load D2 robot data, and scan without the corresponding per-level PIG/POG or D1 custom setup | Native metadata workers do not yet have the same effective surface inputs as gameplay |
| `shared/android_level_preview.cpp`, around lines 1752-1769 | Selects the preview palette before scanning, but does not call the level bitmap replacement loader here | Preview has a separate partial setup path to cover |
| `d1/main/wall.c:check_transparency` and `d2/main/wall.c:check_transparency` | Without an overlay, reads base `BM_FLAG_TRANSPARENT`; with an overlay, reads overlay `BM_FLAG_SUPER_TRANSPARENT` | An ordinary transparent overlay pixel is not itself proof that the room beyond is visible |
| D1/D2 `piggy_bitmap_get_flags()` | Returns `GameBitmapFlags[]` for every `GameBitmaps[]` entry | Paged-out base textures retain inspectable flags, but some resident overrides are not represented correctly |
| `d2/main/piggy.c:load_bitmap_replacements`, around lines 1789-1791 | Sets replacement `bm_flags` and clears its PIG offset, without updating `GameBitmapFlags[]` | The current transparency accessor can still report the base PIG flags after a POG replacement |
| `d1/main/custom.c:load_pigpog`, around lines 322-325 | Similarly sets resident bitmap flags and clears the PIG offset without updating `GameBitmapFlags[]` | The flag-source issue also exists for native D1 custom textures |

The accessor mismatch is established by source inspection; no runtime manifestation was tested in this task

### Proposed implementation

**A1. Resolve flags in the bitmap owner**

Prefer a small paired correction to `piggy_bitmap_get_flags()`:

- For a bitmap backed by the current PIG, read the stored file flags, including when paged out
- For a resident override or generated bitmap whose PIG offset is zero, read its resident `bm_flags`
- Preserve existing behavior for non-`GameBitmaps[]` bitmaps
- Validate bitmap indices before obtaining pointers in the new scanner adapter

This follows the existing ownership distinction and avoids maintaining another replacement flag table in the scanner. Keep `piggy_bitmap_get_file_flags()` as a file-state API; callers that save/restore file state must not silently receive a different meaning

Audit the accessor's callers before implementation because this correction also affects engine doorway/visibility behavior. Do not make a scanner-only workaround which disagrees with rendering. Cover base page-in/page-out, D2 POG, native D1 PG1/DTX, and D1-in-D2 replacement paths

**A2. Establish one semantic asset-preparation order**

The required order is: load authored level data, select/reset the correct base texture source, apply mission/level overrides and definition changes, capture effective texture facts, then build the canonical secret inventory

- D2: select the PIG implied by `Current_level_palette`, with the existing demo fallback and D1-in-D2 rules, independently of whether presentation is skipped
- Native D1: perform the existing custom-data removal/load sequence before secret scanning. Keep PG1 then DTX then HX1 precedence
- Workers: invoke equivalent native asset preparation before scanning each level, including cleanup on reused-worker transitions
- Preview: add equivalent replacement preparation before its canonical scan
- Keep presentation, font remapping, texture upload, and loading screens outside metadata-only preparation. `load_palette(..., 1, 1)` is not a pure metadata helper: it can still remap fonts and clear menu backgrounds
- Keep new orchestration in shared native code with small engine hooks. Reuse the existing engine readers; do not introduce a Kotlin/Python implementation of PIG, POG, or animation formats

For the first implementation, reuse the existing replacement loaders in workers even if they load pixel payloads. A header-only path is a possible later optimization only if measurement warrants it; it must share the engine's header decoding and source precedence rather than becoming a second parser

**A3. Reset overrides in the correct order**

`free_bitmap_replacements()` only frees replacement storage. It does not restore bitmap headers, pointers, or file offsets. `piggy_new_pigfile()` deliberately bypasses its same-PIG early return while `Bitmap_replacement_data` exists

Consequently, do not implement worker cleanup as "free POG, then reload the same PIG": the reload could be skipped and leave stale replacement state. Reuse the existing valid reload-before-free lifecycle, or explicitly restore headers/offsets in the native owner before releasing overrides. Cover level A with a POG -> level B without one, including when both use the same PIG and when their filenames match in different mission mounts

For native D1, use `custom_remove()` and preserve its original-bitmap restoration. For D1-in-D2, preserve `d1_custom_remove()` and the existing effect/wall-animation replacement restoration, rather than treating it as ordinary D2 POG loading

**A4. Represent uncertainty explicitly**

The new adapter should return an effective classification such as unknown, ordinary/visible, or concealed liquid, rather than treating missing metadata as zero flags and therefore opaque. A missing, invalid, or unresolved required texture source must not create a secret

Record why the new category was skipped. Existing door/trigger secret detection can remain available, but incomplete liquid classification must not be published as a complete reusable metadata result

### Cache consequences

`LevelMetadataResultCache.kt:sourceFiles()` currently hashes the main archive/level inputs and, for D2, only `descent2.hog`, `descent2.ham`, and `groupa.pig` as base files. Its loose-source list does not generally include level replacement sidecars. Archive-contained POG changes are already covered by hashing the archive; loose sidecars and a changed `water.pig` are not covered by that rule

Plan the following alongside A2:

- Extend the cache dependency manifest to cover effective base PIGs and mission/level replacement sources, including source precedence and appearance/disappearance of sidecars
- Prefer an engine-produced dependency list/digest so the launcher does not duplicate format knowledge. Since cache lookup precedes native analysis, store and revalidate the prior dependency manifest, including override candidates that were absent, or bypass cache lookup for sources whose complete dependencies cannot yet be proven
- Account for all files read by the chosen preparation path, including native D1 custom definition files; verify mount/staging availability instead of assuming a path on the host is available in the worker
- Invalidate existing metadata results when enabling the scanner change. Use the existing synchronized `ROUTE_ANALYSIS_CACHE_GENERATION` / `ROUTE_METADATA_CACHE_GENERATION` mechanism and document both copies; choose the next value at implementation time because other work is active
- Do not assume the native route topology hash identifies secret inventory inputs: it does not hash effective liquid opacity. Secret inventory caching, if introduced, needs its own relevant input digest. Only expand route-cache dependencies where corrected texture semantics actually affect route/visibility results
- Regenerate checked-in secret counts after validation. Do not migrate disposable Android metadata caches

## Prerequisite B: stable classification, inventory, and discoveries

### Confirmed source findings

- `shared/effect_runtime_shared.c:effect_apply_bitmap_state()` changes `Textures[]` with the current animation frame and can switch to `crit_clip` after reactor destruction
- `secret_area_prepare_current_level()` still performs a secret scan; it merely avoids expensive route planning. It is not a deferred secret initialization operation
- `secret_area_rescan_current_level()` rebuilds the secret list from current globals and clears found state. In addition to load/worker callers, route confirmation invokes it after presentation frames may have elapsed
- The scanner orders entries by distance, entrance segment/side, and lowest member segment. Adding a newly recognized nearby pocket can renumber unrelated existing secrets
- `secretarea.c:secret_area_restore_saved_found()` trusts positional found bits whenever only the total count matches. Equal counts do not establish equal membership or ordering
- `secret_area_write_runtime_state()` writes an integer count followed by 30 bytes of positional found bits
- Both `state.c` validators currently skip exactly `sizeof(int) + SECRET_AREA_MAX_GENERATED` for that save section
- Save restore loads the authored level through `StartNewLevelSub()` before restoring runtime mutations. It first derives approximate found state from `Automap_visited`, then the newer secret runtime section can override it
- `Automap_visited` means seen/rendered, not necessarily entered. It cannot reconstruct exact historical discovery when candidate identities change

### Proposed implementation

**B1. Classify animation families, not the current frame**

Build a bounded, per-level table of surface facts after A2. Resolve animated textures through the native `Effects[]` / vclip definitions, applying effective replacement flags to every referenced frame. Do not enumerate animations by texture-name suffixes or read just the current `Textures[]` entry

- Require the initial surface to remain opaque across every valid ordinary animation frame
- Preserve the engine's base/overlay transparency semantics; for independently animated layers, conservatively consider any frame combination the engine could display, without simulating elapsed time
- A transparent frame, invalid frame index/count, inconsistent effect mapping, or unsupported overlay combination makes the new category ineligible
- Use initial level wall flags and texture assignments. An initially disabled illusion is ordinary geometry, not a hidden boundary. Verify its passability against engine doorway behavior; the scanner's current illusion-off test must not be copied as the source of truth
- For this conservative first pass, skip liquid candidates whose concealment depends on a later reactor-critical, destructive, or trigger-driven visual transition that has not been modeled. Do not let the currently active critical frame choose the classification
- Keep live `WALL_IS_DOORWAY()` behavior dynamic for gameplay. The stable secret inventory uses a snapshot of authored semantics, not a replacement for collision or rendering

This makes initial classification independent of frame timing, save-restored animation phase, bitmap paging, and the previous level's last displayed frame

**B2. Keep the canonical inventory separate from live route refreshes**

Create the inventory once after semantic asset preparation and before save-restored walls, removed pickups, or simulation normalization can affect it. Retain the resulting `Secret_area_state` membership and labels for that level

Use an explicit successful-level-load lifecycle marker, not just mission filename or level number: a reload of the same level must rebuild, while a route refresh in the same loaded level must preserve membership and discoveries

Audit all current rescan callers: paired gameplay loaders, JNI metadata worker, host metadata dump, preview, and route confirmation. Refactor only the secret-inventory portion so route callers can still request their existing canonical/live route work without clearing secrets. For an explicit rebuild, transfer found state by identity as described below

Do not retain a second full copy of the entire mine solely for secrets if the existing bounded result can be retained. Clear/capture it at the true load boundary and let live route/visibility state continue to change independently

**B3. Separate display numbers from persistent identity**

Retain distance-based display ordering for the UI. Add an identity for each canonical region, independent of its display index, entry distance, label position, reward counts, and found state

Recommended identity inputs:

- Level identity: game/interpretation mode and the authored level data identity, established at load time, not the mutable route snapshot hash or a filename alone
- Region identity: canonical sorted member segment numbers under that level identity

A bounded fixed-width fingerprint can encode this key, using explicitly ordered fields rather than struct memory. Detect duplicate identities within an inventory and reject ambiguous restoration. Keep scanner/cache generation separate from region identity so an algorithm update that adds another region does not unnecessarily discard discoveries for unchanged regions

Store a bounded list of identity/found pairs in a new native save section version. Restore by matching identities, never by equal totals. A new region starts unfound. A removed, split, or merged region does not inherit discovery through a guessed overlap. An unchanged region retains discovery when only its display number changes

Texture changes invalidate classification/cache inputs, but an unchanged region in the same authored level can still match its identity. Changed level data invalidates the level identity conservatively

**B4. Preserve native save readability and handle legacy uncertainty honestly**

Allocate the next D1/D2 native save versions during implementation, after reconciling concurrent changes. Keep the old section reader for existing native saves and teach the shared reader/validator which version is being read. Update both paired validators, shared declarations, endian handling, and rewind-memory serialization together

Old positional saves contain no identity, so exact mapping after a scanner change is not recoverable from their count alone. Consume their old bytes without assigning them by index to the expanded list. Retain only the documented approximate automap fallback for existing categories; newly introduced liquid-only regions should default unfound for legacy saves. Do not maintain a frozen legacy scanner to guess old identities

For new identity-bearing saves, a valid identity section must replace the earlier automap fallback, including clearing unmatched regions. Otherwise a newly generated region could remain marked found from visibility despite failing identity matching

Validate the complete section before applying it; reject truncation, out-of-range counts, invalid found values, duplicate identities, and unsupported layouts without partially overwriting discoveries. Preserve native old-save readability, while leaving Android metadata cache formats disposable

## Deferred validation and implementation order

Implementation above is present; build/runtime checks in this section remain unperformed

1. Compile both games; run extract `secret_area_liquid_tests` and existing `secret_area_scan_budget_tests`
2. Run Kotlin result-cache tests and paired native disk/rewind save checks
3. Validate exact gameplay assets and headless/JNI/preview parity, including repeated loads and frame changes
4. Inspect Obsidian 13 membership, entrances, rewards, and transparent negatives; run the D1/D2 baseline harness
5. Extend unsupported mappings using validated native facts; review baseline changes before regenerating metadata

| Future check | Required result |
| --- | --- |
| Base texture, paged out vs resident | Same effective visibility flags |
| Opaque base -> transparent POG, and the reverse | Engine and scanner agree with the effective replacement |
| Native D1 PG1/DTX and D1-in-D2 overrides | Correct precedence and restored base state |
| POG level A -> no-POG level B -> A, same and different PIGs | No inherited flags, freed pointers, or prior-level classifications |
| Same mission/level filename in different mounts | Correct source selection and dependency invalidation |
| Gameplay, JNI worker, host dump, preview, and skipped presentation | Same canonical surface classification and inventory for identical assets |
| Changed non-groupa PIG, changed/added/removed loose sidecar | Cache miss or validated reanalysis, never stale secret totals |
| Every animation frame, different effect times, page-out, and reactor state | Identical initial inventory; uncertain dynamic concealment excluded |
| Route confirmation/refresh after pickups or wall changes | Canonical secrets and found state preserved |
| Insert earlier secret; reorder with equal count | Discovery follows unchanged region identity |
| Region added, removed, split, merged, or level data changed | No discovery transferred by position or guessed overlap |
| Legacy save with equal count but different candidates | No direct positional restore into the new list; new liquid pockets unfound |
| Save after emptying a secret; restore and D2 secret-level return | Initial inventory survives and matched found state restores |
| Native disk and rewind-memory round trips, swapped fields, truncated sections | Correct bounded parsing and atomic state application |
| Obsidian 13 positive pockets and transparent negatives | Expected membership, contents, and one count per pocket |
| Empty liquid scenery, ordinary underwater loot, required switches/exits, nested sheets | No false secret inflation or reward fragmentation |

Extend meaningful native scanner/serialization coverage and the existing D1/D2 baseline harness. `test_secret_area_serialization_contracts.py` now checks explicit versioned layouts; native byte-level execution remains required. Review full region membership, entrances, and display ordering, not only totals

Keep future diagnostics concise and normalized: segment/side pair, texture source and frame set, effective opacity/material classification, rejection reason, region identity, and matched/unmatched restore result. Do not automatically accept regenerated baselines or raise the 30-secret cap to hide unexpected growth
