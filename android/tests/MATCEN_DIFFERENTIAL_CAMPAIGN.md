# Historical/current matcen differential campaign

## Reference and limits

Historical gameplay is Redux `bc19490fb58d0db5f6ba0fd440db33b59b9861fe`, with the observational adapter `ffdb397624d4f61d5d2129ef705402f867b14942`. It is not the original 1995 executable. The current gameplay pin is `6513579581cc4a37a47e3a95dacdb1049a8c35f3`; private assets are pinned to `5fb5b199f9f8c670cff7b0b4ea7086c8505bbefa`.

Independent review found no gameplay changes or resolver backport in the historical adapter. The engines are independent historical/current implementations, but share the observer design: equality cannot prove the observer has no common omissions. Historical decoded triggers are captured after validation; current triggers are captured after compatibility repair. Exact input bytes and hashes are retained separately. Rejected historical levels have no historical loaded-world oracle.

## Completed load comparison

All 1,303 original level/variant entries are accounted for:

- 1,296 fresh historical/current admitted pairs; every complete raw matcen observation matches
- Six current-only admissions after historical rejection; these are not gameplay parity passes
- One advertised KCXF2RM secret level is absent from its pinned archive

The paired observations include 3,393 generated matcens, 3,401 disk records, 16,035 triggers and 27,986 ordered targets. Four levels contain duplicated matcen targets. Authored records, eligible masks/models, effective ordinal/station bindings, trigger type, source wall/side, target order and duplicates match. Stripping the observer block reproduces each of the 1,296 original uninstrumented historical metadata reports exactly.

Four initial missing-level results were corrected by verifying the original loose files and their mount layout; they are included in the paired count. Available ewithin levels are included. Castaway is absent from the inventory.

Separate metadata changes are preserved, not hidden: 94 levels lose robot-summary entries caused solely by unused animation offsets whose joint count is zero. Direct comparison of the authored 480-byte robot records proves that canonicalizing only those unused offsets makes the records identical; retained robot properties, models and matcen masks are unchanged. Current metadata also adds compatibility diagnostics (614 nonempty warning lists and 682 empty lists).

## Controlled lifecycle fixture

Opt-in native command:

```
test_upstream_compat --authored-matcen-lifecycle BASE EXTRA MISSION LEVEL capture.json DIFFICULTY [--no-dispatch]
```

Run from a new writable working directory. The output name is relative to the PhysFS write directory. Difficulty must be 0 through 4. The existing `--authored-matcen-stations` command and its frozen report format are unchanged.

The same fixture bytes run on both engine revisions. Each case starts from a fresh native level load, uses seed 1234 and frame time 1092, and retains the loaded player position and original observed trigger target order. It records four activation attempts with 2,400 station frames per attempt, plus an immediate active retrigger. It checks normal three-life exhaustion and the native D2 highest-difficulty exception without rewriting lives, timers, enabled state, trigger links, masks, kills or native robot limits.

Reports preserve per-round activation lights, station state, lives/capacity/timers, RNG state/calls, player position/orientation, birth frames/types/positions/origins, and native cap timing/counters. Each generated robot is retired without inventing a kill. Zero-birth rounds accompanied by a global cap remain inconclusive production outcomes; exact state equality can still be established.

This is a controlled station loop: obstructing target-segment robots are removed, and only station updates advance. It does not test navigation, physical wall crossing, ordinary combat/congestion, morph/object lifetime progression, save/load or cooperative play. One-shot/mixed/disabled/unsafe cases are classified separately. Imported D1-in-D2 lifecycle validation fails closed when source-action evidence is unavailable.

## Execution and controls

The 20-job validation pilot passed all paired comparisons: 45 authored trigger executions and 594 historical births, with exact same-engine repeats and both no-dispatch controls. Ten mutations of actual traces were all rejected, including valid alternate robot type, frame, position/segment, ordinal/station/mask mapping, repeated-center target order/removal, and missing births. Both current repeats were mutated identically so this was not merely a repeat-equality check. Identical segment-and-side duplicate pairs were unavailable in that selected mutation witness and are not claimed.

The expanded campaign is in progress: 925 startup-preflight-safe trigger-bearing levels, 5,490 trigger records, all five difficulties, up to 4,625 level/difficulty jobs and 27,750 fresh processes. Archive/anomaly representatives run first, then remaining difficulty-2 cases, then the other difficulties. These are planned denominators, not completed behavioral coverage. Failed admissions, logically unbound stations, invalid robot masks, no triggers, excluded trigger kinds, empty masks and cap-limited rounds remain separate outcomes.

Historical repeat stability is checked before dependent current comparisons. The Python helpers validate each report, retain exact allocation identities for same-engine repeats, and omit only explicitly incidental object/signature identities from cross-engine semantic comparison. Raw traces, commands, input manifests, binary/source hashes and failure artifacts are retained privately outside Git. Per-job archives are byte-verified before unpacked generated copies are retired. No expected trace is automatically refreshed, no private assets are published, and no new resolver behavior is introduced.
