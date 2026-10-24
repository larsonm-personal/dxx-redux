# Dot survey reconciliation

Reviewed 2026-10-23 from the user-supplied conversation and
`Downloads/d2xxl-compatibility-evidence.zip`. The archive's own report says
2026-10-24; retain that source date without treating it as our local review date

`dot-survey.zip` preserves the original nine entries byte-for-byte, including
REPORT.md, REPRODUCE.md, features.json, xl-levels.csv, four exploratory scripts and
their manifest. It contains decoded measurements and scripts, not mission
binaries. Archive SHA-256:
`5b8ae8e1359d5ecaa31736c16188ff4fde0a233d54d425eb93cea3959fc85edd`

## What was checked

The stable [audit summary](audit-summary.json) records the recomputed measurements
and fresh native pilot comparison. The summary never grants playability

- All eight manifest hashes match; exact ZIP member allowlist checked
- JSON and CSV agree on each path/hash, versions and main table counts
- Recomputed 131 records / 131 distinct hashes / 56 HOGs; version histogram
  11:1, 15:46, 17:24, 18:2, 19:1, 21:1, 23:56
- Geometry endpoint assertions: 131; object endpoint assertions: 129; the two
  unresolved object sections are `playgdxl.rl2` and Boiling Point `subzero.rl2`
- Recomputed capacity excesses: walls 21 levels, objects two, wall triggers two;
  no segment/vertex excess at current local limits
- Recomputed 212 object triggers in 22 levels, 53 equipment generators in 11,
  and 330/151/1335 particle/lightning/sound records in the 129 decoded sections
- Recomputed wall action counts including 99 masters, 108 teleports, 281 boosts;
  chromatic vertex/side/texture tables occur in 124/58/129 levels
- The pilot's source bytes match the survey hash exactly. Native inventory and
  survey agree on versions, offsets, main counts, wall-type/cloak histograms and
  chromatic color counts. This is an independent parser cross-check of selected
  fields, not independent verification of all reference semantics
- Local source confirms 9000/36000 geometry, 255/1000/100 wall/object/trigger
  capacities, repair-center shield behavior and HMQ dispatch

Run the repository-owned verifier without executing the research scripts:

```powershell
python android/XL_compat/evidence/audit_survey.py
```

For a pilot cross-check, first generate fresh JSON with `xl_inspect` from the
original `d2leva-1.rl2`, then pass both files:

```powershell
python android/XL_compat/evidence/audit_survey.py --native-inventory <inventory.json> --pilot-level <d2leva-1.rl2>
```

The verifier emits stable JSON and preserves `playable: false`. The optional
comparison checks the original level hash; generate the native report from that
same file. It does not cryptographically bind an arbitrary supplied JSON report
to those bytes. Full scan reproduction remains the original REPRODUCE.md workflow
with authorized assets and pinned libarchive-c, not a production dependency

## What remains reported rather than independently reproduced

No full archive rescan, reference-engine run or playthrough was performed for this
reconciliation. The 131 archive files / 2464 level-member occurrences, one D2X
archive, 48 XL descriptors, broad resource counts, and historical PO2 PMH details
remain attributed to the survey. Its raw `inventory.json` was intentionally not
included. Those counts cannot be reconstructed completely from `features.json`

The survey's source pins are Redux
`1cadb4d3e5a39bd7fa08926df2e53725b1851b64`, assets
`d9fbeaf6eb059fd4504308bfd6364734775b7cba`, and XL fork
`2576a98b5de310e3ac7c9777df2677d1627b8bae`. Do not equate its source semantics with
our different pinned local XL/ar12 reference or with current official XL 2.0

The scripts were read, not executed. They are exploratory offset/size parsers,
not hardened production readers: fixed table/layout assumptions, raw pre-v21
function enums, partial object decoding and endpoint checks need version-specific
validation. The export of a missing object-trigger list as zero is not by itself
proof of absence. Parser failures must remain unknown, never empty supported data

Other retained limitations: odyssee.zip nested-HOG boundary failure, unresolved
teleport.hog descriptor, excluded object sections, broader asset counts mixing
classic and XL content, and different denominators from the historical 59
unsupported advertised levels. Neither parser exceptions nor nine Belial player
starts establish engine crash causality. No hub-world/non-cubic claims follow

## Changes to our strategy

Keep Ahayweh Gate as the bounded first pilot. The same-hash corroboration supports
that choice but uncovers another specific reference check: walls 6-17 have
serialized cloak 21, walls 81-82 have 31, all type 3. Test segments 99-105,
84 and 226 for visibility, collision, projectiles and activation. Nonzero fields
are not yet proof of an XL-only material or a stock rendering defect

Split navigation, generators, wall materials, effect/audio objects and resource
scope into explicit browser rows; expand event semantics and capacity text.
Keep new rows unknown until native evidence exists. Add cross-cutting save/replay,
network and A -> stock -> B cleanup gates. Preserve separate loading, gameplay
and fidelity claims. Grayscale is an intermediate option, not fulfillment of the
user's requested lighting-verifiable pilot

The [catalog](../EXTENSIONS.md) gives fixture priorities. The active
[pilot plan](../../ai%20tool%20plans/mission%20metadata/d2xxl_first_mission_pilot_20261023.md)
retains ordinary gameplay, lighting and production-admission gates. The survey
does not expand the first milestone to all XL versions or modern hub persistence
