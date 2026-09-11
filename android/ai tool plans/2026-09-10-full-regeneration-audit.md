# Full regeneration and route-change audit, 2026-09-10

## Result

Full host metadata regeneration and the full Guide-Bot simulation stage pass. All five lost simulation passes in the incoming diff are restored. No passing simulation was lost against either the incoming working-tree results or HEAD

| Baseline | Passing simulations |
| --- | ---: |
| HEAD | 2,124 |
| Incoming working tree | 2,153 |
| Published result | 2,162 |

The sweep processed 2,639 level entries: 2,549 native runs and 90 unavailable-level records. Remaining outcomes are 160 failed, 155 timeout, 146 unsupported, and 16 route mismatch. A successful batch does not certify these incomplete levels

## Final implementation

- Increase the host metadata wall-clock watchdog from 120 to 360 seconds, preserving explicit overrides. Uneasy4 completed serially in 80.4 seconds but needed 172.2 seconds under the final default eight-worker load. Its earlier timeout was infrastructure contention, not a routing failure
- Conditional shot visibility now receives the predicted reactor/countdown state. A reactor-linked shutter can admit the shot only after predicted destruction. Separate visibility-cache identities prevent pre-countdown and post-countdown witnesses from being reused interchangeably
- Before using guided assistance to approach a reactor or boss, try the complete ordinary primary approach, including reachable firing positions outside the target room. Restore the original planning state before a guided retry. The global key-search order and native work limits are unchanged
- If the preferred authored exit cannot be opened/reached, try the other authored exits from the same saved progression state
- Closed-door recovery now checks native flyability. Ironstar's authored OPENED flag makes its door physically passable despite a closed animation state; replanning there previously prevented crossing and exhausted path generation
- Native and Android route-cache generation is 38. Conditional-shot model version is 3. Headless exit-inventory diagnostics remain behind the existing trace switch

All new behavior is shared and uses level state; there are no mission-name branches. The broader global key-search experiments were discarded after they introduced unrelated failures

## Disposition of all 24 incoming metadata-level changes

Duplicate mission records count separately in this table

| Incoming change group | Records | Disposition |
| --- | ---: | --- |
| Saturn L4, standalone and levelpack | 2 | Removed the invalid pre-reactor guided shortcut; restored the original 14-step blue/gold/red-key route; both simulations pass |
| Chasm L3; Hydro L17/L18; Klassics L2 | 4 | Alternative authored exits restore completing metadata. Chasm and both Hydro levels now pass physically. Klassics remains unsupported because its asset set supplies no Guide-Bot type |
| Die Hard L6, three copies | 3 | Preserved the hidden-door and switch prerequisites needed to cross the final exit barrier; all copies pass |
| Die Hard L17, three copies | 3 | Preserved the ordinary switch shot after the boss opens its shutter; all copies pass |
| Die Hard L11, three copies; FFYL L24 | 4 | Firing-position changes retain the same semantic objectives and keys; no passing simulation is lost |
| D2Crossfire L3; KAK L4, two copies | 3 | Explicit exit-opening trigger prerequisites replace implicit assumptions; no keys or guided shots are removed |
| Chronolos secret -1; Phenomia secret -1; Cord-S2; LS Communications Center | 4 | Retained the newly resolved ordinary prerequisites and completing routes; all incoming simulation gains remain passing |
| Enemy Vignettes L15 | 1 | Retained the valid guided-shot removal: native ordinary fire reaches the boss from outside its room and the mine completes |

The incoming simulation diff had 34 gained passes, five lost passes, and three changed nonpassing outcomes. All 34 gains are preserved. Restoring Ironstar's three copies and Saturn's two copies fixes all five losses. Chasm L3, Hydro L17/L18, and Bitesize secret -1 add four further passes

## Every guided-shot change in the final metadata diff

- **Saturn L4, two copies:** invalid shortcut removed, all three keys restored, physical completion verified
- **Enemy Vignettes L15:** ordinary ranged boss shot verified; physical completion verified. This removal was already in the incoming diff and is preserved
- **Bitesize secret -1:** two guided-door steps replaced by an ordinary switch shot and ranged reactor shot. Both focused repeats complete in 893 frames
- **KMTL, levelpack target 143, level 0:** remains partial. The new diagnostic prefix proposes an ordinary reactor firing position instead of a guided door shot. The simulation times out before reaching that firing position; it does not verify the reactor shot. Its change from unsupported to timeout is not a new completion, and guided necessity remains unresolved

There are no guided-shot additions and no new "key not necessary" notes. The erroneous three-key omission notes in both Saturn copies are removed

## Additional metadata changes and limits

The complete per-level content comparison, including exact route positions, identifies 40 changed level records against the two baselines: the 24 incoming records above and 16 additional records

- Bitesize and KMTL are covered above
- Incontin remains passing, with a longer 19-step plan that includes a gold-key branch, versus the earlier 15-step plan. Its existing key-optimization work-budget note remains relevant; this is a route-quality limitation, not proof that the extra key is mandatory. The final simulation completes in 8,981 frames
- Ascent L19, EQ-set L12, and Plutonia L4 retain their objective counts with firing-position changes
- KAK L1 and Lagrange L1, each including its levelpack copy; D2Crossfire L1; Disintegration L6; and Reetus2 L15 remain partial with budget-limited diagnostics
- Ascent L23, Reetus2 L12, and Enemy Vignettes secret -3 remain failed. Their exhausted analyses no longer retain the placeholder start step

KMTL and Klassics are not claimed as physically fixed. Incontin's longer route is explicitly retained as an optimization limitation. Counterstrike secret -5 and TEW L9 received no targeted fixes

## Validation

- Final host metadata run: 138/138 archive/CD records pass, plus both built-in campaigns; 382.3 seconds
- Full simulation run: all 2,639 entries processed, exit 0; zero lost passes against either baseline
- Native suites: D1 42/42 and D2 49/49
- Unchanged demo replays: 15/15 on the final native code
- Android native builds: both games for arm64-v8a, armeabi-v7a, and x86_64
- Android metadata tests: 19/19, with fresh XML results and zero failures/errors
- Metadata worker timeout/crash/restart and parallel-result tests pass
- Scoped formatting/lint and the new `android/tests/test_route_regeneration_audit.ps1` pass. The integration runner regenerates eight archives and checks repeated full-radius completion plus preservation of existing guided certificates

## Evidence and publication

- Final metadata: `android/temp/full_audit_release_metadata`
- Final native simulations: `android/temp/full_audit_release_simulation`
- Final integration: `android/temp/test_route_regeneration_audit/20260910_161927`
- Final demo results: `temp/full_audit_release_demo_results`
- Logs, complete per-level comparisons, original diff, and SHA256 inventories: `temp/full_regeneration_audit`
- `final_metadata_comparison.json` includes the exact-content hashes and guided/key/status audit; `final_simulation_comparison.json` preserves both pre-publication baselines

Published 16 changed metadata files and 14 changed simulation files from the verified outputs. Every original input hash was checked before any replacement, and all published bytes were verified. Existing unrelated work was preserved
