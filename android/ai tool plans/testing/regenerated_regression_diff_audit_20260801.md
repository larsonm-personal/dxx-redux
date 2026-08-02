# Regenerated regression diff audit

## Goal

Classify the current generated regression-file changes, identify unexpected metadata regressions, and trace them to their generation sources without modifying the generated outputs during diagnosis.

## Plan

- [x] Inventory changed regression data by file type and distinguish formatting-only changes
- [x] Compare mission metadata fields against `HEAD`, with emphasis on mission names
- [x] Compare CD and music fingerprint manifests structurally and explain expected fingerprint changes
- [x] Trace suspicious metadata changes to generator inputs and selection logic
- [x] Scan remaining generated-data diffs for anomalies
- [x] Record findings and recommend narrowly scoped fixes

## Findings

- Generated-data inventory: 33 mission metadata files, 34 CD fingerprint manifests, 33 music sidecars, and the merged `known_discs.json5` changed.
- Thirteen CD manifests are formatting-only. Their compact records were rewritten by the current per-file pretty JSON writer. The other 21 change only full-song Chromaprints, except one AcoustID album correction; CD SHA-1 and duration fields remain stable.
- Twelve mission files are value-identical and differ only in object property order. Keeping the newly generated form is preferable to reverting it because this is the current direct generator order.
- The extracted-bundle mission target omits `missionDisplayName` even though the import scan has the correct descriptor title. This causes basename regressions for `Uneasy4`, `Trine1`, `trine2`, `TEW`, `KCXF2RMv11`, `castaway_redux`, `cererian_1.3`, `diehard`, `nefarious`, `Obsidian`, `plutonionOutbreak`, and the Ulterior targets.
- `drmsaga.mn2` declares both `zname = Dr. Moreau's Saga of Death` and `name=drmsaga`. The engine gives `name` priority when discovering the mission, so the regenerated `drmsaga` title agrees with runtime behavior and should remain.
- The non-extracted path now preserves full descriptor names and explains the valid improvements in `Countd2`, `Entropy2`, and `vignett2`. The level-name reader also explains the completed names in `HDVBETA2` and `outerrch11`; invalid non-ASCII Lunar names now fall back to level-file stems.
- Ulterior's D2X, DOS, and Rebirth descriptors each collect all three same-leaf-name `ULTERIOR.HOG` files. All three generated 26-level records are identical apart from filename and target index, so the extra records currently add no regression coverage and risk cross-variant contamination.
- Full-song fingerprints explain the intended Chromaprint churn and nine newly retained `known_discs` album tracks that the old 120-second comparison had deduplicated. Eleven known-disc durations were also synchronized, mostly by 10 to 25 ms.
- Full-song re-evaluation removed 26 AcoustID names from music sidecars and four from `known_discs.json5`. These are accepted as current lookup results; preserving old AcoustID metadata is not an accuracy requirement.
- `Extra_Missions` and `descent_maximum_fixed` now omit `coop_starts` for anarchy-only missions, and `KCXF2RMv11` now emits problem collections as arrays. Those changes are consistent with the current schema.
- Castaway and Ulterior route changes from boss objectives to reactor objectives deserve validation after the mission-set HOG association is fixed; the current multi-HOG contamination makes those route changes unsafe to bless as new baselines.

## Recommended fixes

1. Carry `mission.displayName` into `MissionZipExtractionStore.extractedTarget` so both storage modes publish the same descriptor title.
2. Associate descriptor assets within the descriptor's directory before falling back to leaf-name matching; decide whether truly identical platform variants should collapse to one metadata target.
3. Regenerate mission metadata after fixes 1 and 2, then review the remaining route deltas. Keep the current one-time canonical formatting rather than reverting it.

## Implementation phase

- [x] Preserve parsed mission display names in extracted-bundle metadata targets
- [x] Scope same-stem mission assets to the descriptor directory before using archive-wide fallback
- [x] Define safe duplicate mission-target handling: retain path-distinct variants and isolate their payloads
- [x] Extend existing mission ZIP coverage for extracted titles and variant isolation
- [x] Run focused tests, scoped formatting, and an emulator-free metadata target check
- [x] Record which generated files require regeneration and which current diffs remain valid

## Implementation results

- `MissionZipExtractionStore.extractedTarget` now carries the parsed descriptor display name into the level metadata target.
- Mission-set construction first matches mission payloads within the descriptor directory. Archive-wide leaf-name matching remains only as a compatibility fallback when that directory has no matching HOG.
- D2X, DOS, and Rebirth variants remain separate targets. Matching generated metadata is not sufficient evidence that path-distinct source payloads are identical.
- Focused `MissionZipTest` and `MissionZipExtractionStoreTest` runs passed under JDK 21 before and after scoped formatting. Scoped code quality also passed.
- Another mission metadata regeneration is required for `castaway_redux.json`, `cererian_1.3.json`, `diehard.json`, `KCXF2RMv11.json`, `nefarious.json`, `Obsidian.json`, `plutonionOutbreak.json`, `TEW.json`, `Trine1.json`, `trine2.json`, `ulterior_v1.0.6b.json`, and `Uneasy4.json`. The Ulterior routes must be reviewed after each variant is analyzed against only its own HOG. Castaway's boss-to-reactor route change also remains a manual review item.
- The full-song Chromaprint changes, current AcoustID removals, per-file canonical formatting, and runtime-consistent `drmsaga` title do not require rollback.
