# Daybreak campaign handoff implementation

Source: `C:\Users\first last\Downloads\Daybreak-campaign-handoff.md`

## Requirements and evidence

1. Preserve historical mission behavior while safely accepting longstanding content
   - Keep historical matcen captures distinct from normalized current metadata
   - Do not reinterpret contradictory serialized backlinks as author intent
   - Report compatibility repairs through `data_warnings`
2. Finish the current corpus and focused route qualification
   - Inspect warning-only differences separately from load/behavior changes
   - Treat missing level files, unsupported XL formats, and wall-clock limits as distinct outcomes
3. Audit every accepted sentinel before indexing runtime tables
   - Start with D2 boss-gated robots and the accepted `model_num == -1` sentinel
   - Add integration coverage for any repaired consumer
4. Qualify the corrected IFF writer
   - Run both engines' existing upstream odd-width roundtrip coverage
   - Run exact-size inputs under the sanitizer owner
5. Refresh coherent current-schema secret-area baselines through the existing two-engine owner
   - Preserve the full-document comparator
   - Review behavior changes separately from schema/warning additions
6. Integrate the historical matcen oracle foundation without rewriting its frozen expectations
   - Use the published PR78 head as evidence/source material
   - Validate current-schema metadata and historical behavioral references independently
7. Rebase or reproduce the independent PR79-PR81 changes only after the foundation is green
   - Preserve the user's merge ownership and do not commit, push, or merge
   - Check current remote workflow evidence before duplicating long-running Android work

## Verification gates

- Scoped code-quality check for changed sources
- D1 and D2 `test_upstream_compat`
- Relevant sanitizer suite with odd-width exact-size bitmap inputs
- Secret-area baseline owner for both engines and both data profiles
- Historical matcen reference tests and no-dispatch controls
- Full supplied-corpus metadata comparison and focused route checks
- Repository automation catalog validation for any new or changed test owner

## Coverage limits to preserve in the final report

- Unsupplied packs are not qualified
- Missing optional secrets are reported, not counted as successful loads
- Android save/resume, secret return, network continuation, and import-recovery claims require their actual owners
- A route process exit is not a route confirmation; a wall-clock timeout is not a simulation-frame failure

## Implemented locally

- Guarded D2 boss-gated robot creation against accepted unusable robot/model sentinels before table indexing, with no-mutation integration coverage
- Qualified exact-size odd-width IFF inputs under both D1 and D2 AddressSanitizer builds
- Refreshed both secret-area current-schema baselines through their existing two-engine owner
- Integrated the historical matcen capture/comparison foundation and exposed compact `data_warnings` through native and Kotlin metadata projections
- Restored mission-local mixed-case song-list lookup while preserving PhysFS mount priority and mission-to-stock switching
- Diagnosed PR81's first-play failure as an Android FORTIFY abort caused by a long mission basename, then bounded the legacy eight-character player/network/8.3 consumers in both engines
- Qualified PR81's actual interrupted public mission-replacement owner on an isolated integrated tree: the importer was force-stopped at a nonzero pre-publication extraction checkpoint, the prior mission remained playable, and an identical retry published and launched the replacement
- Qualified PR80's private rendered Vignettes secret-level continuation with the pinned retail/archive hashes: the native write and two independent cold restores produced byte-identical reports and four real authored births, while the no-dispatch control produced none
- Found and corrected PR80's Windows test-build portability issue in its isolated tree by passing explicitly mutable mission-name storage to the legacy loader
- Reproduced the PR80 save-continuation owner and PR81 interruption owner in the main working tree as uncommitted, git-visible changes, including their CI/catalog registrations
- Preserved all pre-existing corpus and simulation JSON edits; no commit, push, merge, or baseline auto-accept was performed

## Remaining external qualifications

- Run any desired secret-return/network-continuation campaigns through their separate owners
- Qualify unsupplied packs and optional external assets only when those inputs are made available

PR81 evidence: `temp/pr81-integrated/android/temp/public-first-play-20261016-222057-640/summary.json` and `interruption-summary.json`

PR80 main-tree evidence: `temp/authored-matcen-save-vignettes-main-final-20261016/results.json`
