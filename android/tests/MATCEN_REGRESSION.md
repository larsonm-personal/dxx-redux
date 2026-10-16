# Matcen compatibility references

The reference engine is `bc19490fb58d0db5f6ba0fd440db33b59b9861fe`, immediately before the mission-security changes. This is preferable to a calendar-based fork: an earlier snapshot predates the October 8 `disable_matcens` station-index correction. D1/D2 `fuelcen.c` and `switch.c` gameplay are unchanged between this reference and `e193bdb8afca598057a58af8f1ad4346d80dcff6`.

The reference build receives only the observational capture patch. Its exact patch revision and generated file hashes must accompany published references. New output is compared with those references; regeneration never approves its own output.

## Evidence layers

The additive per-level `matcen_regression` block travels through the existing native metadata exporters and mission-metadata projection:

- Original segment matcen/station fields, before `matcen_create` overwrites them, alongside generated ordinal.
- Disk record ordinal, original segment/station backlinks, robot masks, hit points and interval, before runtime relinking.
- Decoded disk trigger type/flags and ordered links, preserving duplicates. These fields have passed the existing version conversion; they are not claimed to be raw serialized bytes. Wall associations are explicitly runtime associations.
- Current segment-to-record-to-station joins and their bounds/type/segment status.

`capture_status` describes observation completeness, independently of `admission`. Rejected/missing levels cannot masquerade as successful empty factories. The historical parser lacks the newer checked-read scope; its admission result remains the historical loader's result. No new admission policy is backported.

These are fresh-load observations, not a save/restore state format. Counts and arrays are bounded. The observer does not clear or change live robot-center tables, consume randomness, allocate in gameplay hooks, activate factories or select mappings.

A generated ordinal outside the declared disk count but inside the allocated global array is `logical_disk_unbound`. It can retain a previous level's robot mask. Cold and warm observations must remain distinct; stale global contents are not an author-intent reference. A physical array violation is a separate classification.

## Focused corpus

| Family | Cases | Required invariant |
|---|---|---|
| Public normal | D1 shareware L3; D2 demo L2 | Nonempty real production and ordered trigger evidence |
| Missing records | Orion/ORION-D2 L7 | Preserve ordinal, distinguish cold/warm unbound slots |
| Interleaved placeholders | Galmond2 L2/L3/L11; Ironblade L3 | No compaction or stealing a defined record |
| Stale backlinks | Vignettes L13/L17/L20/L21 | Preserve defined ordinal mapping despite redundant backlink |
| Secret mapping | Vignettes secret L1 | Preserve extra records and distinguish defined/unbound joins |

The compatibility oracle is defined historical behavior, not a guess about author intent. Disk hit points/interval are evidence, not an assertion that the engine uses them as production cadence: the native loop also uses player distance, RNG and morph timing.

The official public D2 demo L2 is itself a crucial conflicting-backlink witness: generated ordinal 0 / segment 50 uses disk record 0 with backlink 263 and mask `[0,192]`; ordinal 2 / segment 263 uses record 2 with backlink 50 and mask `[33554432,64]`. Reordering records to follow backlinks would swap the eligible robots in official data.

## Existing behavior coverage and extension

`public_matcen_stations_fixture.hpp` and `run_public_matcen_stations.py` already test forty native public cases: actual births, masks, frame sequence, two activations, difficulty/lives/capacity, pause during morph, one-round policy, energy preservation, repeated-process determinism and omitted-activation failure. Their controlled fixture clears obstructing authored robots and retires births; it does not claim ordinary combat or congestion coverage.

Additional trigger witnesses must dispatch the real trigger path and preserve ordered multi-target/duplicate semantics. Negative controls must reject swapped masks/ordinals/stations/targets, omitted links, wrong spawn segment, and an absent positive birth. A wall link is only `linked_from_wall`; dispatch in a fixture and reachability through an actual player route are separate facts.

## Explicit subsequent checks

Before any relinker is qualified, add or execute the applicable existing owners for save/load, cooperative/network factory identity and deterministic demo/replay state, plus cold/warm level transitions. A metadata match alone does not qualify these paths. Historical logically unbound or physically unsafe bindings are inspected without invoking unsafe consumers. Do not turn undefined memory behavior into a golden requirement.

The Daybreak proposal's later ordinal-preserving rule supersedes its initial evidence-voting remapper: keep every defined generated-ordinal-to-disk-record binding; any new fallback must be unambiguous and must not steal or reorder a valid binding. Gameplay normalization itself is outside this capture-only change.

## Captured behavioral witnesses

The historical native engine executed 19 original authored trigger cases across the focused levels, producing 165 robot births. A second isolated run reproduced each full trace; no-dispatch controls produced no births. Five additional trigger cases were skipped because a target ordinal was outside the loaded runtime record count. Both Orion level-7 cases contain no matcen trigger links. These are separate outcomes, not successful zero-spawn cases.

The authored fixture dispatches `check_trigger_sub` using the level's original ordered links. It records original masks, activation order, lives/capacity/timers and each birth's frame, robot type, source station and segment. It clears obstructing authored robots and retires spawned robots, as disclosed in each trace; the native global robot cap remains enforced. This is an isolated native production witness, not a player navigation, congestion or combat test.

Six same-process metadata sequences loaded target → another level → target. Orion L7, ORION-D2 L7 and Ironblade L3 changed masks in logically unbound global slots after the prior level. The three selected Galmond2 sequences did not change those masks with their selected primer. These observations demonstrate why a cold zero mask cannot be treated as stable author intent. They are not golden requirements to preserve accidental stale data.

The public fixture retains its forty production-policy cases and adds ten ordered-dispatch/control cases. Its historical golden compares actual production timing/types and mappings, beyond repeating the same new binary. The public D2 demo's conflicting backlinks are an explicit negative control against backlink-driven mask swapping.

### Current admission prerequisite

At `e193bdb8afca598057a58af8f1ad4346d80dcff6`, the eleven private graph witnesses remain rejected by the existing graph validator. Their rejected observation blocks are retained, but cannot satisfy the loaded-reference comparator. The same revision also rejects the original pinned public `d2demo.ham` because unused robot slots contain `model_num = -1`. Historical D2 tests pass; current D2 startup remains a real prerequisite failure. Neither failure is skipped or repaired by this capture patch.

## Running comparisons

Build the existing `dxx-redux-d1-headless-metadata`, `dxx-redux-d2-headless-metadata`, metadata-worker and `test_upstream_compat` targets. The reference branch uses Release builds and `-DDXX_MATCEN_CAPTURE_ONLY_MAIN` in C++ compiler flags to isolate its explicit fixture entry points; it does not stub or backport gameplay. Normal current builds do not need that macro.

Generate actual metadata with the existing native metadata command or the existing metadata regeneration owner into a separate output directory. Preserve hierarchical mission staging and the exact archive variant. On Windows, use `-NoRegressionCopy` when invoking the existing metadata regeneration owner so comparison cannot overwrite references.

```sh
python android/helpers/compare_matcen_metadata.py \
  game_data/mission_files/Vignettes.json /separate-output/Vignettes.json
```

The comparator exits 0 only when selected defined bindings match with no unresolved rows, 2 for matching defined rows with historical unbound rows requiring review, and 1 on mismatch/missing/rejected capture. It compares only levels carrying the additive reference block; other existing mission metadata stays unchanged.

Run one original authored trigger witness with the supplied native game, mission and level, an explicit mission archive staging directory, and its checked-in historical trace:

```sh
python android/tests/run_authored_matcen_stations.py \
  --executable /build/d1/main/test_upstream_compat \
  --base /private/base/d1 --extra /private/staged/Vignettes \
  --game d1 --mission vignette --level 13 \
  --metadata game_data/mission_files/Vignettes.json \
  --expected android/tests/fixtures/matcen-authored/vignette-level13.json \
  --inputs android/tests/fixtures/matcen-authored/vignette-level13.inputs.json \
  --output-dir /separate-output/vignette-level13
```

That runner validates frozen startup evidence before launching, runs repeat and no-dispatch controls in fresh directories, and compares with the historical trace. Skips, no authored trigger and native-cap suppression remain review-required. It never generates or replaces expected files. Assets are required locally or in the existing trusted private-asset environment; public PR jobs run source controls and pinned public fixtures only.

Reference provenance is recorded in `fixtures/matcen_reference_provenance.json`: historical gameplay revision, capture-only adapter revision/tree/patch, native binary hashes, immutable asset revision, exact input-tree hashes and each selected capture. The adapter branch is a reference source, not an integration target. The modern implementation pull request targets `cmake`.
