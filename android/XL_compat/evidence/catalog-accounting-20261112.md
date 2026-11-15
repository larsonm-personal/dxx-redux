# Catalog accounting, November 12, 2026 continuation window

Recounted the immutable `android/tests/xl_readiness_inventory.json`, SHA-256
`bf4356a7a5fbe64832a40d7065ea02d8b81276ebd8bbba6874012f7851fa0cf8`.
All 1,111 unique IDs remain: 1,020 normal, 91 secret, 23 archives and 356
archive/descriptor pairs. No basename or byte-based deduplication was applied.

Single-player eligibility remains unresolved for this bounded baseline: zero
proven exclusions, 1,111 entries in the conservative interim denominator, and
1,000 successes needed for 90%. Directory names are not exclusion evidence.
No full playability numerator is established. Zero fully qualified entries in
this join means missing qualification evidence, not that all levels fail.

The exact-ID join uses the validated frozen receipt from
[private run 43357326772](https://github.com/larsonm-personal/dxx-redux/actions/runs/43357326772),
candidate `2edc3875f534d2c2dc12247b6e9f4abf05c7d8cc`, asset inventory commit
`9dad21bd65ba05665ac6fe315f37fe7461e85725`. Four entries loaded and ticked for
120 frames with XL inactive; four failed to load; 1,103 have no observation in
this receipt. These are historical candidate results, not tests of current source.
The four failures are Boiling Point level 7, Dinter level 1, Sphere level 2 and
Warp Shockwave level 6. Their receipt lacks causal fingerprints; later blockers
remain unknown. Assisted navigation is separately recorded and adds no successes.

All rows retain archive hash, descriptor hash, ordinal, mission and level name.
Render/control, connected play, save/continue, completion/transition and replay
remain independently unqualified in this join. Four entries have observed
classic/imported runtime, zero observed XL-active, and 1,107 unknown runtime.
Resource profile, level byte hash and eligibility require further provenance.
The sanitized receipt does not identify the tested executable hash.

| Archive             | Entries | Normal | Secret | Descriptors |
| ------------------- | ------: | -----: | -----: | ----------: |
| BoilingPoint-XL.zip |       7 |      7 |      0 |           1 |
| D1-levelpack.7z     |      17 |     17 |      0 |          17 |
| bahagad.7z          |      18 |     16 |      2 |           2 |
| diehard.7z          |      19 |     19 |      0 |           1 |
| dinter-1.5.7z       |       1 |      1 |      0 |           1 |
| eaf.7z              |       5 |      5 |      0 |           1 |
| eaf2.7z             |      10 |      8 |      2 |           1 |
| entropy.7z          |       5 |      4 |      1 |           1 |
| harqyjia.7z         |       5 |      4 |      1 |           1 |
| ironblade.7z        |       8 |      7 |      1 |           1 |
| ironstar.7z         |      11 |      8 |      3 |           1 |
| levelpack.7z        |     866 |    805 |     61 |         314 |
| maximum.7z          |      36 |     30 |      6 |           2 |
| norep.7z            |       1 |      1 |      0 |           1 |
| orionneb.7z         |       8 |      7 |      1 |           1 |
| phobos-e.7z         |       6 |      4 |      2 |           1 |
| po2.7z              |      11 |     10 |      1 |           3 |
| revodrav.7z         |      30 |     24 |      6 |           1 |
| saturn.7z           |       7 |      6 |      1 |           1 |
| sphere-1.6.zip      |       5 |      5 |      0 |           1 |
| tt.7z               |       1 |      1 |      0 |           1 |
| vignettes.7z        |      27 |     25 |      2 |           1 |
| wshock20.7z         |       7 |      6 |      1 |           1 |

The per-ID join and reproducible accounting invocation are retained in
`temp/xl-g2-window-20261112/catalog-accounting.json` and `catalog_accounting.py`.
The script calls the existing trusted receipt validator and never changes the
catalog, expected readiness or source inputs. The existing four-HOG static
survey and 131-hash research survey are not joined merely by filename: their
archive/member identities still need matching to catalog provenance. Historical
source observations must not be represented as current runtime rejection.

No marginal set-of-fixes gain is yet measurable. Four first load failures are
four entries, regardless of how many causes later surface. Unknown later blockers
remain visible; no package or entry is dropped because it is difficult or untested.
Next: verify archive/descriptor/level provenance for the existing static surveys,
attach their overlapping feature sets as historical inspection evidence, and
prove eligibility exclusions from descriptors and source scope.

## Verified authored eligibility supplement

All 23 immutable archive sizes and SHA-256 values match the fixed catalog.
The existing safe archive extractor and `xl_readiness.levels` parser verified all
356 descriptor hashes and their complete advertised ordinal/member lists. The
[retained descriptor evidence](catalog-eligibility-20261112.json) preserves these
identities and declaration line numbers; no raw catalog row was removed.

Of 1,111 entries, 1,109 explicitly declare `type = normal`. Stadium
(`levelpack.7z/missions/spaz/stadium.mn2:1`) declares `type = anarchy` on line 2;
exclude this one entry from the original single-player scope. Azure Catacombs XL
(`levelpack.7z/missions/darkflamewolf/acat-xl.mn2:1`) instead declares
`type = D2X-XL`, so keep its eligibility unresolved and included. These are authored
scope declarations, not proof of engine admission, connected gameplay or runtime
XL activation. In particular, `normal` does not establish playability.

This supersedes the initial zero-exclusion denominator above: **1,110 included,
999 required for 90%**, with one proven authored-scope exclusion and one unresolved
entry retained. The fully qualified numerator remains unknown. The loaded/ticked
sample, historical candidate identity and independent qualification dimensions
above are unchanged. Reproducible local audit and extracted evidence remain at
`temp/xl-g2-window-20261112/catalog-eligibility.py` and `catalog-eligibility/`.
The audit hashes source archives again after extraction. Source inputs and pinned
catalog bytes are unchanged. This does not claim the native menu parser accepts
every descriptor's authored type declaration.
