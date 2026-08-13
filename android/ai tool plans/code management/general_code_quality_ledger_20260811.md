# General Code-Quality Ledger

This is the only canonical file for review progress, findings, dispositions, and closure

## Frozen campaign snapshot

- Generated: 2026-08-12 01:56:57 UTC
- Campaign ID: `GQ1`
- Target ref at generation: `upstream/main` -> `6e76c5d8dafd02dc32a1c6312ce9440dc95b1aca`
- Review base: `fb555eec75e1ed12c8348805ab335afb4c721b06`
- Review head: `7877ad30d05887b8e19869ed4c50075e41e2f88e`
- Diff command: `git diff fb555eec75e1ed12c8348805ab335afb4c721b06 7877ad30d05887b8e19869ed4c50075e41e2f88e`
- Changed paths: 3136
- Diff lines: +810516/-4360
- Generated review chunks: 750
- Targeted historical recheck units: 50
- Preflight, sweep, and closure units: 19
- Initial total coverage calls: 819
- Process: `android/ai tool plans/code management/general_code_quality_worker_process.md`
- Durable detailed evidence: `android/ai tool plans/code management/general_code_quality_evidence_ledger_20260811.md`
- Linked inherited-diff subtrack: `android/ai tool plans/code management/d1d2_diff_minimization_ledger_20260811.md`
- Historical finding sources: `branch_adversarial_review_ledger.md` and `branch_adversarial_review_ledger.done.md`

Review the frozen commits even if the live branch moves. `GQ1-CLOSE-001` must account for every later commit before the campaign can finish

## Queue meaning and status legend

The 750 `GQ1-CHUNK-*` rows are deterministic read-only coverage chunks, not 750 predetermined code edits. Each chunk must produce an explicit coverage record and zero or more findings or investigations. Product edits are formed later as separate `GQR-*` remediation chunks, each handled by one fresh `gpt-5.6-sol` worker at medium reasoning effort

- `[ ] TODO`: not reviewed
- `[-] ACTIVE`: claimed by the current call
- `[x] DONE`: reviewed and recorded, including explicit no-finding results
- `[x] SKIP`: disposition recorded with a concrete reason and substitute validation
- `[!] BLOCKED`: cannot be completed without named evidence or authority

Only one call may edit this file at a time. Replace the state in place and put coverage, finding, or investigation IDs in the Result column. A clean result still requires a `GQC-*` record

## Inventory summary

| Kind | Paths | Added | Deleted |
|---|---:|---:|---:|
| artifact | 22 | 56411 | 0 |
| authored-config | 135 | 8564 | 2 |
| authored-source | 836 | 243467 | 4202 |
| build-script | 201 | 36430 | 152 |
| dependency-lock | 1 | 2910 | 0 |
| documentation | 27 | 2261 | 4 |
| generated-fixture | 125 | 258906 | 0 |
| historical-plan | 1361 | 126108 | 0 |
| other-data | 22 | 252 | 0 |
| test-source | 406 | 75207 | 0 |

## Why the first queue was too small

The nine DMR1 decisions remain valid extraction work, but they were produced by a different algorithm than a general code-quality survey:

- DMR1 inspected 325 inherited modified D1/D2 paths and 37 branch-added D1/D2 sinks, while this generation inventories 3,136 branch-changed paths across the repository
- DMR1 searched primarily for paired Android-owned bodies that could leave inherited files behind a materially smaller API
- It normalized observations directly into likely product edits instead of first creating deterministic coverage units, atomic findings, investigations, and explicit clean records
- Its 40-400-line preference and 20-40-line stopping threshold are appropriate for extraction economics but were incorrectly allowed to suppress small correctness, warning, diagnostic, test, portability, dead-code, and documentation work
- High-coupling and branch-added files were classified as retained policy or sinks instead of being internally reviewed for quality
- Historical completion was treated as a family-level exclusion rather than evidence to check for regrowth, adjacent defects, or incomplete fixes
- Hundreds of paths were collapsed into prose residual groups with no path/range-level completion record
- Tests, automation, scripts, builds, release logic, Kotlin/JNI lifecycle, server security, artifacts, generated data, and PR hygiene were outside the survey's discovery scope

The historical comparison is decisive: the first adversarial campaign covered 2,638 paths with 599 mechanical chunks and 617 total calls, yielding 673 distinct findings and 54 explicit no-finding queue results. DMR1 produced six active extraction candidates, three deferrals, and one final audit. Its queue answered `which remaining inherited extractions are worth implementing`, not `what quality work exists on this branch`

Evidence:

- `general_code_quality_evidence_ledger_20260811.md`: historical rubric and 32 omitted dimensions
- `general_code_quality_evidence_ledger_20260811.md`: measured ledger comparison and scalable schema
- `general_code_quality_evidence_ledger_20260811.md`: initial live findings, historical rechecks, and domain coverage
- This ledger's frozen path and range manifest under `Mechanical batch path lists`

Queue size is reported as a tuple rather than one ambiguous number:

`819 total coverage calls / 160 completed issue records / 10 completed clean records / 60 weighted diff-minimization decisions (14 candidate decisions, 10 unique candidates, 1 already completed externally; 18 retain, 1 defer, 27 no-effect) / 4 open investigations / 174 open findings / 2 terminal findings / 163 remediation chunks (2 done)`

Beginning with `GQ1-CHUNK-0108`, the user-set effort weighting is 80 percent diff minimization and 20 percent general quality. Each weighted unit must produce one `GQD-*` decision before secondary findings are normalized. The decision measures branch-caused inherited paths, hunks and lines and is `CANDIDATE`, `RETAIN`, `NO_INHERITED_EFFECT`, or `DEFER`. Actionable D1/D2 candidates reconcile with DMR1 before any new owner is admitted

The two completed DMR1 remediation chunks remain linked separately and removed 175 isolated inherited additions

## Impact-ranked remediation order

This snapshot rates all 163 formed `GQR-*` product-fix chunks. It is the implementation order within the process's 80/20 lanes, not an effort estimate. A score is the maximum finding-owner score in the remediation chunk. Completed work retains its confirmed score for historical comparison but is skipped during dispatch. Prerequisites and active-writer overlap can delay dispatch without changing impact

At this snapshot, the first ten-slot 80/20 dispatch tranche is:

- Diff-minimization lane: `GQR-0162` (`DONE`), `GQR-0143` (`DONE`), `GQR-0142` (`DONE`), `GQR-0156` (`DONE`), `GQR-0157` (`DONE`), `DMR1-CHUNK-003`, `GQR-0159` (`DONE`), `GQR-0161` (`DONE`)
- General-quality lane: `GQR-0002`, `GQR-0024`

Each lane is descending by score and the documented tie-breakers. Run the live overlap/prerequisite audit before claiming a row

| Rank | Score | Band | H/M/B/C/R | Fix | State | Leading finding | Scope |
|---:|---:|---|---|---|---|---|---|
| 1 | 82 | IMMEDIATE | 23/35/7/10/7 | `GQR-0162` | `DONE` | `GQF-0175` | Extract paired headless CMake target policy |
| 2 | 75 | HIGH | 23/28/7/10/7 | `GQR-0143` | `DONE` | `GQF-0156` | Consolidate paired menu/window debug accessors |
| 3 | 57 | MEDIUM-HIGH | 12/21/7/10/7 | `GQR-0142` | `DONE` | `GQF-0155` | Finish paired Android PhysFS init extraction |
| 4 | 57 | MEDIUM-HIGH | 12/21/7/10/7 | `GQR-0156` | `DONE` | `GQF-0169` | Consolidate paired Redbook Android declarations |
| 5 | 57 | MEDIUM-HIGH | 12/21/7/10/7 | `GQR-0157` | `DONE` | `GQF-0170` | Finish shared HMP wrapper extraction |
| 6 | 57 | MEDIUM-HIGH | 12/21/7/10/7 | `GQR-0159` | `DONE` | `GQF-0172` | Consolidate Android mixer init diagnostics |
| 7 | 57 | MEDIUM-HIGH | 12/21/7/10/7 | `GQR-0161` | `DONE` | `GQF-0174` | Move secret-area serialization into its adapter |
| 8 | 56 | MEDIUM-HIGH | 32/0/7/10/7 | `GQR-0002` | `DONE` | `GQF-0007` | Bound DXA mask-name construction and add exact-boundary coverage |
| 9 | 56 | MEDIUM-HIGH | 32/0/7/10/7 | `GQR-0024` | `DONE` | `GQF-0037` | Route SAF URI strings through strict standard-UTF-8 JNI conversion |
| 10 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQR-0006` | `DONE` | `GQF-0005` | Constrain exported automation receivers without breaking intended tests |
| 11 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQR-0011` | `DONE` | `GQF-0024` | Unify and cryptographically verify production Android dependency acquisition |
| 12 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQR-0034` | `TODO` | `GQF-0047` | Enforce physical ISO output containment |
| 13 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQR-0038` | `TODO` | `GQF-0051` | Enforce one peak-live-memory budget for STi2 method 15 |
| 14 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQR-0042` | `TODO` | `GQF-0055` | Require route proof before every reconnect state mutation |
| 15 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQR-0043` | `TODO` | `GQF-0056` | Version and domain-separate reconnect transcripts/generations |
| 16 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQR-0044` | `TODO` | `GQF-0057` | Bound unauthenticated reconnect verification before JNI/JCA |
| 17 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQR-0048` | `TODO` | `GQF-0061` | Carry one budget through complete CD composition |
| 18 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQR-0063` | `TODO` | `GQF-0076` | Enforce one peak-live-memory budget for Inno metadata decode |
| 19 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQR-0065` | `TODO` | `GQF-0078` | Make Inno version admission overflow-free |
| 20 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQR-0070` | `TODO` | `GQF-0083` | Bound aggregate Inno solid-chunk decode work |
| 21 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQR-0087` | `TODO` | `GQF-0100` | Require valid ZIP structure before lifting prompt limits |
| 22 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQR-0090` | `TODO` | `GQF-0103` | Apply one extraction budget to direct Setup ZIP import |
| 23 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQR-0091` | `TODO` | `GQF-0104` | Enforce peak-live-memory policy in Kotlin bounded reads |
| 24 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQR-0095` | `TODO` | `GQF-0108` | Prevent weaker fallback after RAR policy rejection |
| 25 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQR-0096` | `TODO` | `GQF-0109` | Bound RAR enumeration before materialization |
| 26 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQR-0105` | `TODO` | `GQF-0118` | Carry one catalog budget through nested music containers |
| 27 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQR-0106` | `TODO` | `GQF-0119` | Preserve nested streaming size and expansion accounting |
| 28 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQR-0112` | `TODO` | `GQF-0125` | Bind bounded extraction to an admitted Python runtime |
| 29 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQR-0114` | `TODO` | `GQF-0127` | Supervise bounded extractor process trees |
| 30 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQR-0115` | `TODO` | `GQF-0128` | Reject unsafe extractor output types |
| 31 | 54 | MEDIUM-HIGH | 32/0/2/10/10 | `GQR-0005` | `TODO` | `GQF-0001` | Remove tracked runtime/scratch artifacts and establish narrow recurrence policy |
| 32 | 53 | MEDIUM-HIGH | 32/0/7/10/4 | `GQR-0039` | `TODO` | `GQF-0052` | Make assigned JNI acquisitions and allocations exception-safe |
| 33 | 53 | MEDIUM-HIGH | 32/0/7/10/4 | `GQR-0045` | `TODO` | `GQF-0058` | Bound and cancel complete-track fingerprint work |
| 34 | 53 | MEDIUM-HIGH | 32/0/7/10/4 | `GQR-0102` | `TODO` | `GQF-0115` | Bind mission scan to one extraction source generation |
| 35 | 53 | MEDIUM-HIGH | 32/0/7/10/4 | `GQR-0128` | `TODO` | `GQF-0141` | Preserve graphics transaction originals through rollback |
| 36 | 53 | MEDIUM-HIGH | 32/0/7/10/4 | `GQR-0160` | `TODO` | `GQF-0173` | Make SDL audio callback lifetime entry-safe |
| 37 | 53 | MEDIUM-HIGH | 23/0/10/10/10 | `GQR-0144` | `TODO` | `GQF-0157` | Make bounded-extractor quota tests reason-exact |
| 38 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQR-0018` | `TODO` | `GQF-0031` | Make declared CD audio identity/counts executable regression oracles |
| 39 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQR-0020` | `TODO` | `GQF-0033` | Add non-mutating combined-oracle semantic freshness validation |
| 40 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQR-0055` | `TODO` | `GQF-0068` | Register audio enumeration regression coverage |
| 41 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQR-0140` | `TODO` | `GQF-0153` | Make D2 Mac extraction oracle content-exact |
| 42 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQR-0145` | `TODO` | `GQF-0158` | Register bounded-extractor Python regression |
| 43 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQR-0146` | `TODO` | `GQF-0159` | Require source-bearing extraction regression specs |
| 44 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQR-0149` | `TODO` | `GQF-0162` | Make extraction workflow assertions behavioral |
| 45 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQR-0150` | `TODO` | `GQF-0163` | Bind app-private extraction staging to source identity |
| 46 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQR-0151` | `TODO` | `GQF-0164` | Make descriptor-only SAF success observation-gated |
| 47 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQR-0154` | `TODO` | `GQF-0167` | Publish final audio samples before producer completion |
| 48 | 47 | MEDIUM | 23/0/4/10/10 | `GQR-0109` | `TODO` | `GQF-0122` | Make sequential-port prediction conservative |
| 49 | 47 | MEDIUM | 23/0/7/10/7 | `GQR-0016` | `TODO` | `GQF-0029` | Regenerate and bind the stale Anniversary fingerprint |
| 50 | 47 | MEDIUM | 23/0/7/10/7 | `GQR-0017` | `TODO` | `GQF-0030` | Version and bind physical-disc fingerprint generations |
| 51 | 47 | MEDIUM | 23/0/7/10/7 | `GQR-0019` | `TODO` | `GQF-0032` | Replace size-based combined-component collision selection with explicit ownership |
| 52 | 47 | MEDIUM | 23/0/7/10/7 | `GQR-0021` | `TODO` | `GQF-0034` | Enforce typed complete mission fingerprint cache/publication schema |
| 53 | 47 | MEDIUM | 23/0/7/10/7 | `GQR-0025` | `TODO` | `GQF-0038` | Replace HFS wrapper fixed-buffer joins with checked path ownership |
| 54 | 47 | MEDIUM | 23/0/7/10/7 | `GQR-0027` | `TODO` | `GQF-0040` | Make flattened HFS collision policy explicit and order-independent |
| 55 | 47 | MEDIUM | 23/0/7/10/7 | `GQR-0031` | `TODO` | `GQF-0044` | Align PKG no-audio free-space checks with selected outputs |
| 56 | 47 | MEDIUM | 23/0/7/10/7 | `GQR-0032` | `TODO` | `GQF-0045` | Make PKG JNI progress stable and cumulative |
| 57 | 47 | MEDIUM | 23/0/7/10/7 | `GQR-0035` | `TODO` | `GQF-0048` | Define ISO normalized version/collision semantics |
| 58 | 47 | MEDIUM | 23/0/7/10/7 | `GQR-0036` | `TODO` | `GQF-0049` | Implement or explicitly reject ISO extended/interleaved layouts |
| 59 | 47 | MEDIUM | 23/0/7/10/7 | `GQR-0040` | `TODO` | `GQF-0053` | Bind Inno extraction to its analyzed source generation |
| 60 | 47 | MEDIUM | 23/0/7/10/7 | `GQR-0064` | `TODO` | `GQF-0077` | Require strict setup-header LZMA terminal state |
| 61 | 47 | MEDIUM | 23/0/7/10/7 | `GQR-0066` | `TODO` | `GQF-0079` | Preserve and assemble Galaxy multipart groups |
| 62 | 47 | MEDIUM | 23/0/7/10/7 | `GQR-0067` | `TODO` | `GQF-0080` | Model the complete Windows destination namespace |
| 63 | 47 | MEDIUM | 23/0/7/10/7 | `GQR-0075` | `TODO` | `GQF-0088` | Enforce ISO declared-volume containment |
| 64 | 47 | MEDIUM | 23/0/7/10/7 | `GQR-0076` | `TODO` | `GQF-0089` | Make bounded CUE titles preserve valid UTF-8 |
| 65 | 47 | MEDIUM | 23/0/7/10/7 | `GQR-0078` | `TODO` | `GQF-0091` | Bind PKG extraction to a collision-resistant source generation |
| 66 | 47 | MEDIUM | 23/0/7/10/7 | `GQR-0083` | `TODO` | `GQF-0096` | Preserve complete resume-save mission identity |
| 67 | 47 | MEDIUM | 23/0/7/10/7 | `GQR-0084` | `TODO` | `GQF-0097` | Move resume-save discovery off the Compose looper |
| 68 | 47 | MEDIUM | 23/0/7/10/7 | `GQR-0089` | `TODO` | `GQF-0102` | Bind ZIP extraction to the validated central entry set |
| 69 | 47 | MEDIUM | 23/0/7/10/7 | `GQR-0100` | `TODO` | `GQF-0113` | Reject stored ZIP collisions before registration |
| 70 | 47 | MEDIUM | 23/0/7/10/7 | `GQR-0103` | `TODO` | `GQF-0116` | Eliminate repeated synchronous mission freshness hashing |
| 71 | 47 | MEDIUM | 23/0/7/10/7 | `GQR-0118` | `TODO` | `GQF-0131` | Validate DOS demo output before first publication |
| 72 | 47 | MEDIUM | 23/0/7/10/7 | `GQR-0126` | `TODO` | `GQF-0139` | Retire stale audio generations on no-audio success |
| 73 | 47 | MEDIUM | 23/0/7/10/7 | `GQR-0147` | `TODO` | `GQF-0160` | Put host-only extraction tests in the host tier |
| 74 | 47 | MEDIUM | 23/0/10/10/4 | `GQR-0023` | `TODO` | `GQF-0036` | Bound SAF manifest decoded allocation and duplicate work |
| 75 | 47 | MEDIUM | 23/0/10/10/4 | `GQR-0059` | `TODO` | `GQF-0072` | Bound and cancel HFS metadata and ancestry work |
| 76 | 47 | MEDIUM | 23/0/10/10/4 | `GQR-0072` | `TODO` | `GQF-0085` | Make Inno output-name uniqueness preflight bounded |
| 77 | 47 | MEDIUM | 23/0/10/10/4 | `GQR-0108` | `TODO` | `GQF-0121` | Bound NAT simulator live mappings and tasks |
| 78 | 47 | MEDIUM | 23/0/10/10/4 | `GQR-0122` | `TODO` | `GQF-0135` | Bound every Mac demo external process |
| 79 | 47 | MEDIUM | 23/0/10/10/4 | `GQR-0124` | `TODO` | `GQF-0137` | Bound mission music archive nesting |
| 80 | 45 | MEDIUM | 23/0/2/10/10 | `GQR-0003` | `TODO` | `GQF-0008` | Restore always-active HUD layout test oracles |
| 81 | 45 | MEDIUM | 23/0/2/10/10 | `GQR-0004` | `TODO` | `GQF-0009` | Restore always-active escort policy test oracles |
| 82 | 45 | MEDIUM | 23/0/2/10/10 | `GQR-0007` | `TODO` | `GQF-0011` | Replace ambiguous optional menu selection with stable semantics |
| 83 | 45 | MEDIUM | 23/0/2/10/10 | `GQR-0012` | `TODO` | `GQF-0025` | Repair quick-test catalog migration and add target-resolution coverage |
| 84 | 45 | MEDIUM | 23/0/2/10/10 | `GQR-0080` | `TODO` | `GQF-0093` | Add a successful production STi2 method-14 oracle |
| 85 | 45 | MEDIUM | 23/0/2/10/10 | `GQR-0130` | `TODO` | `GQF-0143` | Make CUE extension-filter tests exact |
| 86 | 45 | MEDIUM | 23/0/2/10/10 | `GQR-0131` | `TODO` | `GQF-0144` | Validate HMP output through production wrappers |
| 87 | 45 | MEDIUM | 23/0/2/10/10 | `GQR-0136` | `TODO` | `GQF-0149` | Prove fingerprint input-byte and stream parity |
| 88 | 45 | MEDIUM | 23/0/2/10/10 | `GQR-0137` | `TODO` | `GQF-0150` | Validate raw fingerprint fixtures exactly |
| 89 | 45 | MEDIUM | 23/0/2/10/10 | `GQR-0152` | `TODO` | `GQF-0165` | Register and bound the WAV parser regression |
| 90 | 44 | MEDIUM | 23/0/4/10/7 | `GQR-0022` | `TODO` | `GQF-0035` | Restore evidence-backed deterministic AcoustID candidate selection |
| 91 | 44 | MEDIUM | 23/0/4/10/7 | `GQR-0028` | `TODO` | `GQF-0041` | Preserve valid SOW append prefixes on later failure |
| 92 | 44 | MEDIUM | 23/0/4/10/7 | `GQR-0037` | `TODO` | `GQF-0050` | Scan ISO volume descriptor sequences correctly |
| 93 | 44 | MEDIUM | 23/0/4/10/7 | `GQR-0046` | `TODO` | `GQF-0059` | Preserve lossless fingerprint ranking evidence |
| 94 | 44 | MEDIUM | 23/0/4/10/7 | `GQR-0047` | `TODO` | `GQF-0060` | Make exact-collision duration decisions symmetric |
| 95 | 44 | MEDIUM | 23/0/4/10/7 | `GQR-0050` | `TODO` | `GQF-0063` | Make CUE FILE grammar strict and source ordering shared |
| 96 | 44 | MEDIUM | 23/0/4/10/7 | `GQR-0051` | `TODO` | `GQF-0064` | Reject oversized CUE physical lines without synthetic parsing |
| 97 | 44 | MEDIUM | 23/0/4/10/7 | `GQR-0052` | `TODO` | `GQF-0065` | Define and enforce CUE track identity rules |
| 98 | 44 | MEDIUM | 23/0/4/10/7 | `GQR-0057` | `TODO` | `GQF-0070` | Make Windows CD fingerprint paths Unicode-safe |
| 99 | 44 | MEDIUM | 23/0/4/10/7 | `GQR-0058` | `TODO` | `GQF-0071` | Validate the complete HFS catalog parent graph before projection |
| 100 | 44 | MEDIUM | 23/0/4/10/7 | `GQR-0060` | `TODO` | `GQF-0073` | Accept standards-conforming multi-node HFS allocation maps |
| 101 | 44 | MEDIUM | 23/0/4/10/7 | `GQR-0061` | `TODO` | `GQF-0074` | Require complete classic-HFS catalog record schemas |
| 102 | 44 | MEDIUM | 23/0/4/10/7 | `GQR-0068` | `TODO` | `GQF-0081` | Bind Inno chunks to validated split-volume sources |
| 103 | 44 | MEDIUM | 23/0/4/10/7 | `GQR-0074` | `TODO` | `GQF-0087` | Scope or remove ISO `zero` directory suppression |
| 104 | 44 | MEDIUM | 23/0/4/10/7 | `GQR-0077` | `TODO` | `GQF-0090` | Parse exact XAR field structure |
| 105 | 44 | MEDIUM | 23/0/4/10/7 | `GQR-0088` | `TODO` | `GQF-0101` | Select a fully valid ZIP EOCD candidate |
| 106 | 44 | MEDIUM | 23/0/4/10/7 | `GQR-0092` | `TODO` | `GQF-0105` | Share song filename capacity with both engines |
| 107 | 44 | MEDIUM | 23/0/4/10/7 | `GQR-0093` | `TODO` | `GQF-0106` | Key contained-track sidecars by member identity |
| 108 | 44 | MEDIUM | 23/0/4/10/7 | `GQR-0094` | `TODO` | `GQF-0107` | Separate sidecar freshness from identification completeness |
| 109 | 44 | MEDIUM | 23/0/4/10/7 | `GQR-0098` | `TODO` | `GQF-0111` | Validate loadable mission level payloads |
| 110 | 44 | MEDIUM | 23/0/4/10/7 | `GQR-0099` | `TODO` | `GQF-0112` | Preserve qualified mission descriptor identities |
| 111 | 44 | MEDIUM | 23/0/4/10/7 | `GQR-0111` | `TODO` | `GQF-0124` | Contain mission-batch diagnostic capture failures |
| 112 | 44 | MEDIUM | 23/0/4/10/7 | `GQR-0119` | `TODO` | `GQF-0132` | Share strict CUE sector geometry with Mac extraction |
| 113 | 44 | MEDIUM | 23/0/4/10/7 | `GQR-0120` | `TODO` | `GQF-0133` | Validate Mac extraction CUE INDEX fields |
| 114 | 44 | MEDIUM | 23/0/4/10/7 | `GQR-0121` | `TODO` | `GQF-0134` | Bind Mac extraction cache to supervisor policy |
| 115 | 44 | MEDIUM | 23/0/4/10/7 | `GQR-0123` | `TODO` | `GQF-0136` | Regenerate and enforce Mac demo oracle provenance |
| 116 | 44 | MEDIUM | 23/0/4/10/7 | `GQR-0125` | `TODO` | `GQF-0138` | Make tracklist selector conflicts explicit |
| 117 | 44 | MEDIUM | 23/0/4/10/7 | `GQR-0134` | `TODO` | `GQF-0147` | Reject reserved Windows ISO output components |
| 118 | 44 | MEDIUM | 23/0/7/10/4 | `GQR-0009` | `TODO` | `GQF-0016` | Limit compiler-process cleanup to owned children |
| 119 | 44 | MEDIUM | 23/0/7/10/4 | `GQR-0026` | `TODO` | `GQF-0039` | Give HFS installer scratch files exclusive attempt ownership |
| 120 | 44 | MEDIUM | 23/0/7/10/4 | `GQR-0033` | `TODO` | `GQF-0046` | Make direct PKG and standalone ISO publication transactional |
| 121 | 44 | MEDIUM | 23/0/7/10/4 | `GQR-0056` | `TODO` | `GQF-0069` | Bind each CD fingerprint run to one immutable BIN generation |
| 122 | 44 | MEDIUM | 23/0/7/10/4 | `GQR-0062` | `TODO` | `GQF-0075` | Preserve the PE resource leaf bound through offset-table parsing |
| 123 | 44 | MEDIUM | 23/0/7/10/4 | `GQR-0069` | `TODO` | `GQF-0082` | Route solid chunks by complete decoded prefix |
| 124 | 44 | MEDIUM | 23/0/7/10/4 | `GQR-0071` | `TODO` | `GQF-0084` | Validate Galaxy external size before final publication |
| 125 | 44 | MEDIUM | 23/0/7/10/4 | `GQR-0081` | `TODO` | `GQF-0094` | Move STi2 fixed entry catalogs off native stack |
| 126 | 44 | MEDIUM | 23/0/7/10/4 | `GQR-0082` | `TODO` | `GQF-0095` | Make level-metadata runtime initialization transactional |
| 127 | 44 | MEDIUM | 23/0/7/10/4 | `GQR-0085` | `TODO` | `GQF-0098` | Bind conditional save deletion to one file generation |
| 128 | 44 | MEDIUM | 23/0/7/10/4 | `GQR-0086` | `TODO` | `GQF-0099` | Freeze one SAF mounted-source generation |
| 129 | 44 | MEDIUM | 23/0/7/10/4 | `GQR-0097` | `TODO` | `GQF-0110` | Preserve staged RAR source ownership |
| 130 | 44 | MEDIUM | 23/0/7/10/4 | `GQR-0117` | `TODO` | `GQF-0130` | Serialize extraction publication by destination |
| 131 | 44 | MEDIUM | 23/0/7/10/4 | `GQR-0127` | `TODO` | `GQF-0140` | Bound native mission fingerprint processes |
| 132 | 44 | MEDIUM | 23/0/7/10/4 | `GQR-0129` | `TODO` | `GQF-0142` | Close graphics transaction descriptors on sync failure |
| 133 | 44 | MEDIUM | 23/0/7/10/4 | `GQR-0135` | `TODO` | `GQF-0148` | Use production heap ownership in CUE/ISO tests |
| 134 | 44 | MEDIUM | 23/0/7/10/4 | `GQR-0155` | `TODO` | `GQF-0168` | Decode replay direct commands once per frame |
| 135 | 43 | MEDIUM | 23/0/10/6/4 | `GQR-0029` | `TODO` | `GQF-0042` | Give SOW output leaves no-follow regular-file ownership |
| 136 | 43 | MEDIUM | 23/0/10/6/4 | `GQR-0054` | `TODO` | `GQF-0067` | Restrict fingerprint CLI enumeration to owned regular inputs |
| 137 | 42 | MEDIUM | 23/0/2/10/7 | `GQR-0008` | `TODO` | `GQF-0012` | Pin NAT testbed base image reproducibly |
| 138 | 42 | MEDIUM | 23/0/2/10/7 | `GQR-0110` | `TODO` | `GQF-0123` | Bind TinySoundFont updates to incremental rebuilds |
| 139 | 42 | MEDIUM | 23/0/2/10/7 | `GQR-0116` | `TODO` | `GQF-0129` | Make the POSIX CUE/ISO entry point executable |
| 140 | 42 | MEDIUM | 12/0/10/10/10 | `GQR-0153` | `TODO` | `GQF-0166` | Complete and guard the server configuration template |
| 141 | 41 | MEDIUM | 23/0/4/10/4 | `GQR-0132` | `TODO` | `GQF-0145` | Make invalid fingerprint database configuration fail closed |
| 142 | 41 | MEDIUM | 12/5/7/10/7 | `GQR-0158` | `TODO` | `GQF-0171` | Remove unused paired FP environment includes |
| 143 | 40 | MEDIUM | 23/0/7/6/4 | `GQR-0053` | `TODO` | `GQF-0066` | Make CUE parsing length-aware and reject embedded NUL |
| 144 | 40 | MEDIUM | 23/0/7/6/4 | `GQR-0107` | `TODO` | `GQF-0120` | Lease staged music generations across preview use |
| 145 | 39 | MEDIUM | 12/0/7/10/10 | `GQR-0163` | `TODO` | `GQF-0176` | Remove the orphan JNI skeleton |
| 146 | 36 | MEDIUM | 12/0/7/10/7 | `GQR-0010` | `DEFERRED` | `GQF-0019` | Evaluate paired ETC2 self-test extraction |
| 147 | 36 | MEDIUM | 12/0/7/10/7 | `GQR-0014` | `TODO` | `GQF-0027` | Finish audio URI persistence schema migration |
| 148 | 36 | MEDIUM | 12/0/7/10/7 | `GQR-0148` | `TODO` | `GQF-0161` | Remove stale extraction failure state |
| 149 | 34 | LOW | 12/0/2/10/10 | `GQR-0015` | `TODO` | `GQF-0028` | Remove no-op crash Activity compatibility shim |
| 150 | 34 | LOW | 12/0/2/10/10 | `GQR-0101` | `TODO` | `GQF-0114` | Remove or restore `isMissionHog` ownership |
| 151 | 34 | LOW | 12/0/2/10/10 | `GQR-0133` | `TODO` | `GQF-0146` | Remove unused CUE test helpers |
| 152 | 33 | LOW | 12/0/4/10/7 | `GQR-0013` | `TODO` | `GQF-0026` | Remove unsupported launcher file-layout migration state |
| 153 | 33 | LOW | 12/0/4/10/7 | `GQR-0030` | `TODO` | `GQF-0043` | Correct SOW filtered progress population accounting |
| 154 | 33 | LOW | 12/0/4/10/7 | `GQR-0073` | `TODO` | `GQF-0086` | Define truthful Inno solid-chunk progress |
| 155 | 33 | LOW | 12/0/4/10/7 | `GQR-0104` | `TODO` | `GQF-0117` | Include generated aliases in storage and progress totals |
| 156 | 33 | LOW | 12/0/4/10/7 | `GQR-0113` | `TODO` | `GQF-0126` | Preserve zero-length HFS files |
| 157 | 33 | LOW | 12/0/4/10/7 | `GQR-0138` | `TODO` | `GQF-0151` | Make fingerprint assertions evaluate operands once |
| 158 | 33 | LOW | 12/0/4/10/7 | `GQR-0141` | `TODO` | `GQF-0154` | Make empty music generations explicit |
| 159 | 32 | LOW | 12/0/7/6/7 | `GQR-0049` | `TODO` | `GQF-0062` | Resolve or reject Windows drive-relative output identity |
| 160 | 31 | LOW | 12/0/2/10/7 | `GQR-0139` | `TODO` | `GQF-0152` | Initialize GOG LZMA trailing-data fixtures |
| 161 | 29 | LOW | 12/0/4/6/7 | `GQR-0041` | `TODO` | `GQF-0054` | Propagate native JSON/output stream failures |
| 162 | 29 | LOW | 12/0/4/6/7 | `GQR-0079` | `TODO` | `GQF-0092` | Make zero-length SOW extraction allocation-independent |
| 163 | 0 | REFERENCE | 0/0/0/0/0 | `GQR-0001` | `DONE` | - | Add a capacity-aware shared texture extension lookup and exact boundary tests |

## Terminal coverage-chunk impact annotations

This table rates every terminal `GQ1-CHUNK-0001` through `GQ1-CHUNK-0167` exactly once by the highest-impact live canonical fix it supports. `PRIMARY` is the first completed GQ chunk representing a GQ finding in this table; `REFERENCE` contributes duplicate or extension evidence and must not cause another implementation. `BR-*` and `DMR1-*` owners remain reference-only because their canonical work lives in those ledgers

| Rank | Score | Band | H/M/B/C/R | Coverage chunk | Diff decision | Canonical fix owner | Role | Annotation |
|---:|---:|---|---|---|---|---|---|---|
| 1 | 82 | IMMEDIATE | 23/35/7/10/7 | `GQ1-CHUNK-0165` | `CANDIDATE` | `GQF-0175` | `PRIMARY` | Paired headless executable target construction |
| 2 | 75 | HIGH | 23/28/7/10/7 | `GQ1-CHUNK-0127` | `CANDIDATE` | `GQF-0156` | `PRIMARY` | Paired menu/window debug accessors |
| 3 | 75 | HIGH | 23/28/7/10/7 | `GQ1-CHUNK-0132` | `CANDIDATE` | `GQF-0156` | `REFERENCE` | Paired menu/window debug accessors |
| 4 | 75 | HIGH | 23/28/7/10/7 | `GQ1-CHUNK-0135` | `CANDIDATE` | `GQF-0156` | `REFERENCE` | Paired menu/window debug accessors |
| 5 | 59 | MEDIUM-HIGH | 32/0/7/10/10 | `GQ1-CHUNK-0016` | `PRE-0108` | `BR-0073` | `REFERENCE` | Resolve LGPL obligations for the embedded XADMaster-derived extractors |
| 6 | 59 | MEDIUM-HIGH | 32/0/7/10/10 | `GQ1-CHUNK-0040` | `PRE-0108` | `BR-0073` | `REFERENCE` | Resolve LGPL obligations for the embedded XADMaster-derived extractors |
| 7 | 59 | MEDIUM-HIGH | 32/0/7/10/10 | `GQ1-CHUNK-0041` | `PRE-0108` | `BR-0073` | `REFERENCE` | Resolve LGPL obligations for the embedded XADMaster-derived extractors |
| 8 | 59 | MEDIUM-HIGH | 32/0/7/10/10 | `GQ1-CHUNK-0042` | `PRE-0108` | `BR-0073` | `REFERENCE` | Resolve LGPL obligations for the embedded XADMaster-derived extractors |
| 9 | 59 | MEDIUM-HIGH | 32/0/7/10/10 | `GQ1-CHUNK-0044` | `PRE-0108` | `BR-0073` | `REFERENCE` | Resolve LGPL obligations for the embedded XADMaster-derived extractors |
| 10 | 59 | MEDIUM-HIGH | 32/0/10/10/7 | `GQ1-CHUNK-0141` | `NO_INHERITED_EFFECT` | `BR-0194` | `REFERENCE` | Bootstrap certificates before enabling the nginx TLS site |
| 11 | 57 | MEDIUM-HIGH | 12/21/7/10/7 | `GQ1-CHUNK-0125` | `CANDIDATE` | `GQF-0155` | `PRIMARY` | Paired Android PhysFS initialization residue |
| 12 | 57 | MEDIUM-HIGH | 12/21/7/10/7 | `GQ1-CHUNK-0137` | `CANDIDATE` | `GQF-0155` | `REFERENCE` | Paired Android PhysFS initialization residue |
| 13 | 57 | MEDIUM-HIGH | 12/21/7/10/7 | `GQ1-CHUNK-0153` | `CANDIDATE` | `GQF-0169` | `PRIMARY` | Paired Redbook Android extension declarations |
| 14 | 57 | MEDIUM-HIGH | 12/21/7/10/7 | `GQ1-CHUNK-0155` | `CANDIDATE` | `GQF-0170` | `PRIMARY` | Paired HMP Android wrapper residue |
| 15 | 57 | MEDIUM-HIGH | 12/21/7/10/7 | `GQ1-CHUNK-0158` | `CANDIDATE` | `DMR1-CHUNK-003` | `REFERENCE` | Extract paired texture-overlay draw bodies |
| 16 | 57 | MEDIUM-HIGH | 12/21/7/10/7 | `GQ1-CHUNK-0159` | `CANDIDATE` | `GQF-0172` | `PRIMARY` | Residual paired Android mixer init logging |
| 17 | 57 | MEDIUM-HIGH | 12/21/7/10/7 | `GQ1-CHUNK-0164` | `CANDIDATE` | `GQF-0174` | `PRIMARY` | Paired secret-area save serialization helpers |
| 18 | 57 | MEDIUM-HIGH | 12/21/7/10/7 | `GQ1-CHUNK-0167` | `NO_INHERITED_EFFECT` | `GQF-0174` | `REFERENCE` | Paired secret-area save serialization helpers |
| 19 | 56 | MEDIUM-HIGH | 32/0/7/10/7 | `GQ1-CHUNK-0009` | `PRE-0108` | `GQF-0037` | `PRIMARY` | SAF URI and MIDI path/JSON JNI text bridges |
| 20 | 56 | MEDIUM-HIGH | 32/0/7/10/7 | `GQ1-CHUNK-0015` | `PRE-0108` | `GQF-0037` | `REFERENCE` | SAF URI and MIDI path/JSON JNI text bridges |
| 21 | 56 | MEDIUM-HIGH | 32/0/7/10/7 | `GQ1-CHUNK-0019` | `PRE-0108` | `GQF-0037` | `REFERENCE` | SAF URI and MIDI path/JSON JNI text bridges |
| 22 | 56 | MEDIUM-HIGH | 32/0/7/10/7 | `GQ1-CHUNK-0023` | `PRE-0108` | `GQF-0037` | `REFERENCE` | SAF URI and MIDI path/JSON JNI text bridges |
| 23 | 56 | MEDIUM-HIGH | 32/0/7/10/7 | `GQ1-CHUNK-0093` | `PRE-0108` | `GQF-0037` | `REFERENCE` | SAF URI and MIDI path/JSON JNI text bridges |
| 24 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0013` | `PRE-0108` | `GQF-0047` | `PRIMARY` | ISO output directory traversal and final opens |
| 25 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0018` | `PRE-0108` | `BR-0497` | `REFERENCE` | Authenticate and bound dynamic host-proxy peer admission |
| 26 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0021` | `PRE-0108` | `GQF-0061` | `PRIMARY` | Complete CD extraction attempt budget |
| 27 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0030` | `PRE-0108` | `GQF-0051` | `PRIMARY` | STi2 method-15 decode scratch |
| 28 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0031` | `PRE-0108` | `GQF-0078` | `PRIMARY` | Inno version admission arithmetic |
| 29 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0032` | `PRE-0108` | `GQF-0076` | `PRIMARY` | Inno metadata LZMA peak-live memory |
| 30 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0033` | `PRE-0108` | `GQF-0083` | `PRIMARY` | Inno aggregate repeated solid-chunk decoding |
| 31 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0034` | `PRE-0108` | `GQF-0083` | `REFERENCE` | Inno aggregate repeated solid-chunk decoding |
| 32 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0036` | `PRE-0108` | `GQF-0047` | `REFERENCE` | ISO output directory traversal and final opens |
| 33 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0043` | `PRE-0108` | `GQF-0051` | `REFERENCE` | STi2 method-15 decode scratch |
| 34 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0049` | `PRE-0108` | `BR-0080` | `REFERENCE` | Validate ship-status player and weapon indices before caching or display |
| 35 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0054` | `PRE-0108` | `GQF-0100` | `PRIMARY` | ZIP early-marker prompt bypass |
| 36 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0056` | `PRE-0108` | `GQF-0108` | `PRIMARY` | RAR policy rejection fallback |
| 37 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0061` | `PRE-0108` | `GQF-0118` | `PRIMARY` | Nested mission music container catalogs |
| 38 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0062` | `PRE-0108` | `GQF-0119` | `PRIMARY` | Streaming descriptor nested music budget |
| 39 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0063` | `PRE-0108` | `BR-0090` | `REFERENCE` | Implement the server's keypair fallback for clients without Play Games authentication |
| 40 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0065` | `PRE-0108` | `BR-0106` | `REFERENCE` | Fail closed when Google authentication is not configured |
| 41 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0066` | `PRE-0108` | `BR-0108` | `REFERENCE` | Reject incomplete TLS configuration instead of serving plaintext |
| 42 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0067` | `PRE-0108` | `BR-0090` | `REFERENCE` | Implement the server's keypair fallback for clients without Play Games authentication |
| 43 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0068` | `PRE-0108` | `BR-0121` | `REFERENCE` | Enforce the friend-state authorization invariant on accept, presence, and join |
| 44 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0069` | `PRE-0108` | `BR-0127` | `REFERENCE` | Give every WebSocket connection generation-safe session ownership |
| 45 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0070` | `PRE-0108` | `BR-0132` | `REFERENCE` | Authenticate each relay endpoint and preserve its exact lobby slot |
| 46 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0071` | `PRE-0108` | `BR-0137` | `REFERENCE` | Fail closed when ban enforcement cannot read the database |
| 47 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0073` | `PRE-0108` | `BR-0115` | `REFERENCE` | Bound WebSocket messages and every client-controlled field before work |
| 48 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0075` | `PRE-0108` | `BR-0115` | `REFERENCE` | Bound WebSocket messages and every client-controlled field before work |
| 49 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0076` | `PRE-0108` | `BR-0116` | `REFERENCE` | Validate and cap ICE candidates before storing or expanding them |
| 50 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0079` | `PRE-0108` | `GQF-0024` | `PRIMARY` | android/app/src/main/cpp/CMakeLists.txt` production dependency fetches |
| 51 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0083` | `PRE-0108` | `GQF-0125` | `PRIMARY` | Bounded extraction Python runtime |
| 52 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0084` | `PRE-0108` | `GQF-0127` | `PRIMARY` | Bounded extractor descendant ownership |
| 53 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0088` | `PRE-0108` | `GQF-0061` | `REFERENCE` | Complete CD extraction attempt budget |
| 54 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0090` | `PRE-0108` | `GQF-0128` | `PRIMARY` | Extractor link and special-file outputs |
| 55 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0092` | `PRE-0108` | `GQF-0078` | `REFERENCE` | Inno version admission arithmetic |
| 56 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0095` | `PRE-0108` | `GQF-0061` | `REFERENCE` | Complete CD extraction attempt budget |
| 57 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0106` | `PRE-0108` | `GQF-0076` | `REFERENCE` | Inno metadata LZMA peak-live memory |
| 58 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0119` | `NO_INHERITED_EFFECT` | `GQF-0061` | `REFERENCE` | Complete CD extraction attempt budget |
| 59 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0121` | `NO_INHERITED_EFFECT` | `GQF-0119` | `REFERENCE` | Streaming descriptor nested music budget |
| 60 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0122` | `NO_INHERITED_EFFECT` | `GQF-0118` | `REFERENCE` | Nested mission music container catalogs |
| 61 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0128` | `CANDIDATE` | `GQF-0127` | `REFERENCE` | Bounded extractor descendant ownership |
| 62 | 56 | MEDIUM-HIGH | 32/0/10/10/4 | `GQ1-CHUNK-0130` | `NO_INHERITED_EFFECT` | `GQF-0125` | `REFERENCE` | Bounded extraction Python runtime |
| 63 | 53 | MEDIUM-HIGH | 32/0/4/10/7 | `GQ1-CHUNK-0074` | `PRE-0108` | `BR-0148` | `REFERENCE` | Track and complete connectivity checks per player pair |
| 64 | 53 | MEDIUM-HIGH | 32/0/7/10/4 | `GQ1-CHUNK-0017` | `PRE-0108` | `GQF-0052` | `PRIMARY` | Assigned JNI array/string acquisitions and MIDI byte-array creation |
| 65 | 53 | MEDIUM-HIGH | 32/0/7/10/4 | `GQ1-CHUNK-0024` | `PRE-0108` | `GQF-0058` | `PRIMARY` | Complete-track disc and compressed-file fingerprinting |
| 66 | 53 | MEDIUM-HIGH | 32/0/7/10/4 | `GQ1-CHUNK-0025` | `PRE-0108` | `GQF-0058` | `REFERENCE` | Complete-track disc and compressed-file fingerprinting |
| 67 | 53 | MEDIUM-HIGH | 32/0/7/10/4 | `GQ1-CHUNK-0026` | `PRE-0108` | `GQF-0058` | `REFERENCE` | Complete-track disc and compressed-file fingerprinting |
| 68 | 53 | MEDIUM-HIGH | 32/0/7/10/4 | `GQ1-CHUNK-0037` | `PRE-0108` | `GQF-0052` | `REFERENCE` | Assigned JNI array/string acquisitions and MIDI byte-array creation |
| 69 | 53 | MEDIUM-HIGH | 32/0/7/10/4 | `GQ1-CHUNK-0048` | `PRE-0108` | `BR-0029` | `REFERENCE` | Marshal overlay game-state access through the engine thread |
| 70 | 53 | MEDIUM-HIGH | 32/0/7/10/4 | `GQ1-CHUNK-0050` | `PRE-0108` | `BR-0029` | `REFERENCE` | Marshal overlay game-state access through the engine thread |
| 71 | 53 | MEDIUM-HIGH | 32/0/7/10/4 | `GQ1-CHUNK-0059` | `PRE-0108` | `GQF-0115` | `PRIMARY` | Mission extraction `ScanResult` generation |
| 72 | 53 | MEDIUM-HIGH | 32/0/7/10/4 | `GQ1-CHUNK-0094` | `PRE-0108` | `GQF-0141` | `PRIMARY` | Graphics configuration rollback backup |
| 73 | 53 | MEDIUM-HIGH | 32/0/7/10/4 | `GQ1-CHUNK-0118` | `NO_INHERITED_EFFECT` | `GQF-0115` | `REFERENCE` | Mission extraction `ScanResult` generation |
| 74 | 53 | MEDIUM-HIGH | 32/0/7/10/4 | `GQ1-CHUNK-0161` | `RETAIN` | `BR-0078` | `REFERENCE` | Make native engine admission atomic through process termination |
| 75 | 53 | MEDIUM-HIGH | 23/0/10/10/10 | `GQ1-CHUNK-0001` | `PRE-0108` | `BR-0007` | `REFERENCE` | Make the Play Store credential sample match the supported key format |
| 76 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQ1-CHUNK-0002` | `PRE-0108` | `BR-0008` | `REFERENCE` | Enforce recorded source hashes before extraction regression tests |
| 77 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQ1-CHUNK-0003` | `PRE-0108` | `BR-0008` | `REFERENCE` | Enforce recorded source hashes before extraction regression tests |
| 78 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQ1-CHUNK-0004` | `PRE-0108` | `BR-0008` | `REFERENCE` | Enforce recorded source hashes before extraction regression tests |
| 79 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQ1-CHUNK-0005` | `PRE-0108` | `BR-0008` | `REFERENCE` | Enforce recorded source hashes before extraction regression tests |
| 80 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQ1-CHUNK-0006` | `PRE-0108` | `BR-0008` | `REFERENCE` | Enforce recorded source hashes before extraction regression tests |
| 81 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQ1-CHUNK-0007` | `PRE-0108` | `BR-0008` | `REFERENCE` | Enforce recorded source hashes before extraction regression tests |
| 82 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQ1-CHUNK-0080` | `PRE-0108` | `BR-0160` | `REFERENCE` | Bound and isolate CUE/ISO test runs |
| 83 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQ1-CHUNK-0082` | `PRE-0108` | `BR-0162` | `REFERENCE` | Fail test batches that execute no archive |
| 84 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQ1-CHUNK-0085` | `PRE-0108` | `BR-0165` | `REFERENCE` | Derive the default mission archive directory from the repository |
| 85 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQ1-CHUNK-0086` | `PRE-0108` | `BR-0169` | `REFERENCE` | Return a failing status when any batch extraction fails |
| 86 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQ1-CHUNK-0087` | `PRE-0108` | `BR-0169` | `REFERENCE` | Return a failing status when any batch extraction fails |
| 87 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQ1-CHUNK-0089` | `PRE-0108` | `BR-0169` | `REFERENCE` | Return a failing status when any batch extraction fails |
| 88 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQ1-CHUNK-0099` | `PRE-0108` | `BR-0160` | `REFERENCE` | Bound and isolate CUE/ISO test runs |
| 89 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQ1-CHUNK-0100` | `PRE-0108` | `BR-0160` | `REFERENCE` | Bound and isolate CUE/ISO test runs |
| 90 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQ1-CHUNK-0101` | `PRE-0108` | `BR-0160` | `REFERENCE` | Bound and isolate CUE/ISO test runs |
| 91 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQ1-CHUNK-0102` | `PRE-0108` | `BR-0160` | `REFERENCE` | Bound and isolate CUE/ISO test runs |
| 92 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQ1-CHUNK-0103` | `PRE-0108` | `BR-0160` | `REFERENCE` | Bound and isolate CUE/ISO test runs |
| 93 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQ1-CHUNK-0108` | `NO_INHERITED_EFFECT` | `BR-0158` | `REFERENCE` | Report absent extraction fixtures as skipped or failed |
| 94 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQ1-CHUNK-0109` | `NO_INHERITED_EFFECT` | `BR-0158` | `REFERENCE` | Report absent extraction fixtures as skipped or failed |
| 95 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQ1-CHUNK-0113` | `NO_INHERITED_EFFECT` | `BR-0160` | `REFERENCE` | Bound and isolate CUE/ISO test runs |
| 96 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQ1-CHUNK-0114` | `NO_INHERITED_EFFECT` | `BR-0158` | `REFERENCE` | Report absent extraction fixtures as skipped or failed |
| 97 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQ1-CHUNK-0116` | `NO_INHERITED_EFFECT` | `BR-0160` | `REFERENCE` | Bound and isolate CUE/ISO test runs |
| 98 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQ1-CHUNK-0129` | `NO_INHERITED_EFFECT` | `BR-0008` | `REFERENCE` | Enforce recorded source hashes before extraction regression tests |
| 99 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQ1-CHUNK-0131` | `CANDIDATE` | `BR-0008` | `REFERENCE` | Enforce recorded source hashes before extraction regression tests |
| 100 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQ1-CHUNK-0133` | `NO_INHERITED_EFFECT` | `GQF-0153` | `PRIMARY` | D2 Mac native extraction output oracle |
| 101 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQ1-CHUNK-0134` | `NO_INHERITED_EFFECT` | `BR-0169` | `REFERENCE` | Return a failing status when any batch extraction fails |
| 102 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQ1-CHUNK-0136` | `NO_INHERITED_EFFECT` | `BR-0008` | `REFERENCE` | Enforce recorded source hashes before extraction regression tests |
| 103 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQ1-CHUNK-0138` | `NO_INHERITED_EFFECT` | `GQF-0158` | `PRIMARY` | Bounded extractor Python regression registration |
| 104 | 50 | MEDIUM-HIGH | 23/0/7/10/10 | `GQ1-CHUNK-0151` | `RETAIN` | `GQF-0167` | `PRIMARY` | Shared TSF/PCM producer completion ordering |
| 105 | 47 | MEDIUM | 23/0/7/10/7 | `GQ1-CHUNK-0008` | `PRE-0108` | `BR-0623` | `REFERENCE` | Share one complete portable source-manifest policy |
| 106 | 47 | MEDIUM | 23/0/7/10/7 | `GQ1-CHUNK-0010` | `PRE-0108` | `BR-0021` | `REFERENCE` | Honor and propagate documented extraction cancellation |
| 107 | 47 | MEDIUM | 23/0/7/10/7 | `GQ1-CHUNK-0011` | `PRE-0108` | `BR-0021` | `REFERENCE` | Honor and propagate documented extraction cancellation |
| 108 | 47 | MEDIUM | 23/0/7/10/7 | `GQ1-CHUNK-0012` | `PRE-0108` | `BR-0021` | `REFERENCE` | Honor and propagate documented extraction cancellation |
| 109 | 47 | MEDIUM | 23/0/7/10/7 | `GQ1-CHUNK-0014` | `PRE-0108` | `BR-0021` | `REFERENCE` | Honor and propagate documented extraction cancellation |
| 110 | 47 | MEDIUM | 23/0/7/10/7 | `GQ1-CHUNK-0027` | `PRE-0108` | `BR-0021` | `REFERENCE` | Honor and propagate documented extraction cancellation |
| 111 | 47 | MEDIUM | 23/0/7/10/7 | `GQ1-CHUNK-0028` | `PRE-0108` | `GQF-0040` | `PRIMARY` | Flattened HFS loose-file projection |
| 112 | 47 | MEDIUM | 23/0/7/10/7 | `GQ1-CHUNK-0035` | `PRE-0108` | `BR-0021` | `REFERENCE` | Honor and propagate documented extraction cancellation |
| 113 | 47 | MEDIUM | 23/0/7/10/7 | `GQ1-CHUNK-0038` | `PRE-0108` | `BR-0021` | `REFERENCE` | Honor and propagate documented extraction cancellation |
| 114 | 47 | MEDIUM | 23/0/7/10/7 | `GQ1-CHUNK-0051` | `PRE-0108` | `BR-0082` | `REFERENCE` | Validate metadata-backed save bodies before offering Resume |
| 115 | 47 | MEDIUM | 23/0/7/10/7 | `GQ1-CHUNK-0052` | `PRE-0108` | `BR-0417` | `REFERENCE` | Repair last-save selection after deleting an empty set |
| 116 | 47 | MEDIUM | 23/0/7/10/7 | `GQ1-CHUNK-0057` | `PRE-0108` | `GQF-0113` | `PRIMARY` | Stored ZIP collision validation timing |
| 117 | 47 | MEDIUM | 23/0/7/10/7 | `GQ1-CHUNK-0058` | `PRE-0108` | `GQF-0030` | `PRIMARY` | Physical-disc and mission fingerprint generation and cache reuse |
| 118 | 47 | MEDIUM | 23/0/7/10/7 | `GQ1-CHUNK-0072` | `PRE-0108` | `BR-0111` | `REFERENCE` | Bounds-check complete STUN attributes before slicing |
| 119 | 47 | MEDIUM | 23/0/7/10/7 | `GQ1-CHUNK-0081` | `PRE-0108` | `BR-0174` | `REFERENCE` | Gate external reference extractors to supported hosts |
| 120 | 47 | MEDIUM | 23/0/7/10/7 | `GQ1-CHUNK-0091` | `PRE-0108` | `BR-0174` | `REFERENCE` | Gate external reference extractors to supported hosts |
| 121 | 47 | MEDIUM | 23/0/7/10/7 | `GQ1-CHUNK-0096` | `PRE-0108` | `GQF-0091` | `PRIMARY` | PKG analysis-to-extraction generation identity |
| 122 | 47 | MEDIUM | 23/0/7/10/7 | `GQ1-CHUNK-0097` | `PRE-0108` | `BR-0236` | `REFERENCE` | Report and roll back partial all-pilot preference writes |
| 123 | 47 | MEDIUM | 23/0/7/10/7 | `GQ1-CHUNK-0110` | `NO_INHERITED_EFFECT` | `GQF-0091` | `REFERENCE` | PKG analysis-to-extraction generation identity |
| 124 | 47 | MEDIUM | 23/0/7/10/7 | `GQ1-CHUNK-0111` | `NO_INHERITED_EFFECT` | `BR-0021` | `REFERENCE` | Honor and propagate documented extraction cancellation |
| 125 | 47 | MEDIUM | 23/0/7/10/7 | `GQ1-CHUNK-0120` | `NO_INHERITED_EFFECT` | `GQF-0030` | `REFERENCE` | Physical-disc and mission fingerprint generation and cache reuse |
| 126 | 47 | MEDIUM | 23/0/10/10/4 | `GQ1-CHUNK-0053` | `PRE-0108` | `GQF-0036` | `PRIMARY` | saf_manifest_parser.c` retained strings and duplicate lookup |
| 127 | 45 | MEDIUM | 23/0/2/10/10 | `GQ1-CHUNK-0104` | `PRE-0108` | `BR-0181` | `REFERENCE` | Compare the complete fpcalc reference fingerprint |
| 128 | 45 | MEDIUM | 23/0/2/10/10 | `GQ1-CHUNK-0105` | `PRE-0108` | `BR-0662` | `REFERENCE` | Keep mailbox assertions active in registered builds |
| 129 | 45 | MEDIUM | 23/0/2/10/10 | `GQ1-CHUNK-0115` | `NO_INHERITED_EFFECT` | `BR-0182` | `REFERENCE` | Exercise the production StuffIt parser in corpus tests |
| 130 | 45 | MEDIUM | 23/0/2/10/10 | `GQ1-CHUNK-0154` | `RETAIN` | `BR-0224` | `REFERENCE` | Fail closed on unknown automation actions and debug fields |
| 131 | 45 | MEDIUM | 23/0/2/10/10 | `GQ1-CHUNK-0156` | `RETAIN` | `BR-0231` | `REFERENCE` | Stop replay when direct-command policy fails |
| 132 | 45 | MEDIUM | 23/0/2/10/10 | `GQ1-CHUNK-0160` | `RETAIN` | `BR-0396` | `REFERENCE` | Register route snapshot and cache tests with CTest |
| 133 | 45 | MEDIUM | 23/0/2/10/10 | `GQ1-CHUNK-0163` | `RETAIN` | `BR-0396` | `REFERENCE` | Register route snapshot and cache tests with CTest |
| 134 | 44 | MEDIUM | 23/0/4/10/7 | `GQ1-CHUNK-0020` | `PRE-0108` | `GQF-0059` | `PRIMARY` | Fingerprint matcher score serialization and best-CD selection |
| 135 | 44 | MEDIUM | 23/0/4/10/7 | `GQ1-CHUNK-0022` | `PRE-0108` | `GQF-0063` | `PRIMARY` | CUE `FILE` directive grammar |
| 136 | 44 | MEDIUM | 23/0/4/10/7 | `GQ1-CHUNK-0039` | `PRE-0108` | `GQF-0070` | `PRIMARY` | Windows `fingerprint_cd` CUE and BIN paths |
| 137 | 44 | MEDIUM | 23/0/4/10/7 | `GQ1-CHUNK-0046` | `PRE-0108` | `BR-0077` | `REFERENCE` | Propagate loaded level failures to the metadata result |
| 138 | 44 | MEDIUM | 23/0/4/10/7 | `GQ1-CHUNK-0047` | `PRE-0108` | `BR-0077` | `REFERENCE` | Propagate loaded level failures to the metadata result |
| 139 | 44 | MEDIUM | 23/0/4/10/7 | `GQ1-CHUNK-0055` | `PRE-0108` | `GQF-0105` | `PRIMARY` | Song-list token versus engine filename capacity |
| 140 | 44 | MEDIUM | 23/0/4/10/7 | `GQ1-CHUNK-0117` | `DEFER` | `GQF-0106` | `PRIMARY` | Container-track sidecar identity |
| 141 | 44 | MEDIUM | 23/0/4/10/7 | `GQ1-CHUNK-0123` | `NO_INHERITED_EFFECT` | `GQF-0111` | `PRIMARY` | Mission ZIP loadable-level admission |
| 142 | 44 | MEDIUM | 23/0/4/10/7 | `GQ1-CHUNK-0124` | `NO_INHERITED_EFFECT` | `GQF-0111` | `REFERENCE` | Mission ZIP loadable-level admission |
| 143 | 44 | MEDIUM | 23/0/4/10/7 | `GQ1-CHUNK-0126` | `NO_INHERITED_EFFECT` | `GQF-0111` | `REFERENCE` | Mission ZIP loadable-level admission |
| 144 | 44 | MEDIUM | 23/0/4/10/7 | `GQ1-CHUNK-0147` | `RETAIN` | `BR-0289` | `REFERENCE` | Preserve resolution-scaled point primitives in GLES3 |
| 145 | 44 | MEDIUM | 23/0/4/10/7 | `GQ1-CHUNK-0149` | `RETAIN` | `BR-0300` | `REFERENCE` | Remove or restore the dead D1-in-D2 level-start mode |
| 146 | 44 | MEDIUM | 23/0/4/10/7 | `GQ1-CHUNK-0157` | `CANDIDATE` | `BR-0201` | `REFERENCE` | Restore the replay floating-point environment on every platform |
| 147 | 44 | MEDIUM | 23/0/4/10/7 | `GQ1-CHUNK-0166` | `RETAIN` | `BR-0588` | `REFERENCE` | Correlate introspection reads with the current request |
| 148 | 44 | MEDIUM | 23/0/7/10/4 | `GQ1-CHUNK-0029` | `PRE-0108` | `GQF-0075` | `PRIMARY` | Inno PE resource offset-table leaf |
| 149 | 44 | MEDIUM | 23/0/7/10/4 | `GQ1-CHUNK-0045` | `PRE-0108` | `GQF-0095` | `PRIMARY` | Level-metadata process-global initialization retry |
| 150 | 44 | MEDIUM | 23/0/7/10/4 | `GQ1-CHUNK-0098` | `PRE-0108` | `GQF-0069` | `PRIMARY` | CD fingerprint BIN source generation |
| 151 | 44 | MEDIUM | 23/0/7/10/4 | `GQ1-CHUNK-0107` | `PRE-0108` | `GQF-0082` | `PRIMARY` | Inno solid-chunk decoded prefix and terminal routing |
| 152 | 44 | MEDIUM | 23/0/7/10/4 | `GQ1-CHUNK-0150` | `RETAIN` | `BR-0222` | `REFERENCE` | Publish input-demo artifact sets transactionally |
| 153 | 44 | MEDIUM | 23/0/7/10/4 | `GQ1-CHUNK-0152` | `RETAIN` | `GQF-0168` | `PRIMARY` | Replay current-frame direct-command acquisition |
| 154 | 42 | MEDIUM | 23/0/2/10/7 | `GQ1-CHUNK-0139` | `RETAIN` | `BR-0610` | `REFERENCE` | Fail an invalid explicit vcpkg root before automatic fallback |
| 155 | 42 | MEDIUM | 23/0/2/10/7 | `GQ1-CHUNK-0140` | `RETAIN` | `BR-0610` | `REFERENCE` | Fail an invalid explicit vcpkg root before automatic fallback |
| 156 | 33 | LOW | 12/0/4/10/7 | `GQ1-CHUNK-0060` | `PRE-0108` | `GQF-0117` | `PRIMARY` | Generated `descent.sng` alias bytes |
| 157 | 0 | REFERENCE | 0/0/0/0/0 | `GQ1-CHUNK-0064` | `PRE-0108` | - | `NONE` | Clean coverage; no live fix |
| 158 | 0 | REFERENCE | 0/0/0/0/0 | `GQ1-CHUNK-0077` | `PRE-0108` | - | `NONE` | No live canonical fix remains |
| 159 | 0 | REFERENCE | 0/0/0/0/0 | `GQ1-CHUNK-0078` | `PRE-0108` | - | `NONE` | Clean coverage; no live fix |
| 160 | 0 | REFERENCE | 0/0/0/0/0 | `GQ1-CHUNK-0112` | `NO_INHERITED_EFFECT` | - | `NONE` | Clean coverage; no live fix |
| 161 | 0 | REFERENCE | 0/0/0/0/0 | `GQ1-CHUNK-0142` | `NO_INHERITED_EFFECT` | - | `NONE` | Clean coverage; no live fix |
| 162 | 0 | REFERENCE | 0/0/0/0/0 | `GQ1-CHUNK-0143` | `RETAIN` | - | `NONE` | Clean coverage; no live fix |
| 163 | 0 | REFERENCE | 0/0/0/0/0 | `GQ1-CHUNK-0144` | `RETAIN` | - | `NONE` | Clean coverage; no live fix |
| 164 | 0 | REFERENCE | 0/0/0/0/0 | `GQ1-CHUNK-0145` | `NO_INHERITED_EFFECT` | - | `NONE` | Clean coverage; no live fix |
| 165 | 0 | REFERENCE | 0/0/0/0/0 | `GQ1-CHUNK-0146` | `RETAIN` | - | `NONE` | Clean coverage; no live fix |
| 166 | 0 | REFERENCE | 0/0/0/0/0 | `GQ1-CHUNK-0148` | `RETAIN` | - | `NONE` | Clean coverage; no live fix |
| 167 | 0 | REFERENCE | 0/0/0/0/0 | `GQ1-CHUNK-0162` | `RETAIN` | - | `NONE` | Clean coverage; no live fix |

Rating snapshot: 2026-08-12. Recompute the affected owner and all reference rows after any severity, confidence, measured inherited payoff, prerequisite, or lifecycle change

## Review queue

| ID | State | Phase | Risk | Kind | Path | Assigned scope | Result |
|---|---|---|---|---|---|---|---|
| GQ1-PREFLIGHT-001 | [x] DONE | preflight | critical | pr-scope | `complete diff` | PR composition, provenance, generated artifacts, secrets, licenses, and split strategy | `GQC-0011`; extended `GQF-0001` through `GQF-0004`, `GQF-0013`; added `GQF-0024`, `GQI-0001` |
| GQ1-PREFLIGHT-002 | [x] DONE | preflight | high | architecture | `complete diff` | Subsystem map, intended behavior, ownership boundaries, and highest-risk data flows | `GQC-0012`; evidence extensions and historical links, no new root |
| GQ1-PREFLIGHT-003 | [x] DONE | preflight | high | change-history | `complete diff` | Commit clusters, superseded approaches, partial migrations, and abandoned compatibility paths | `GQC-0013`; added `GQF-0025` through `GQF-0028` |
| GQ1-CHUNK-0001 | [x] DONE | source | critical | authored-config | `2 related paths` | 45 review lines under android | `GQC-0014`; duplicate `BR-0007`, one attached ASCII cleanup note |
| GQ1-CHUNK-0002 | [x] DONE | source | critical | authored-config | `12 related paths` | 533 review lines under game_data/CD images | `GQC-0015`; added `GQF-0029`, extended five `BR-*` roots |
| GQ1-CHUNK-0003 | [x] DONE | source | critical | authored-config | `2 related paths` | 237 review lines under game_data/CD images | `GQC-0016`; added `GQF-0030`, extended `BR-0008`, `BR-0010`, `BR-0188` |
| GQ1-CHUNK-0004 | [x] DONE | source | critical | authored-config | `7 related paths` | 549 review lines under game_data/CD images | `GQC-0017`; added `GQF-0031`, extended six `BR-*` roots |
| GQ1-CHUNK-0005 | [x] DONE | source | critical | authored-config | `8 related paths` | 551 review lines under game_data/CD images | `GQC-0018`; extended `GQF-0030`, `GQF-0031`, and seven `BR-*` roots |
| GQ1-CHUNK-0006 | [x] DONE | source | critical | authored-config | `9 related paths` | 584 review lines under game_data/CD images | `GQC-0019`; eight evidence extensions, no new root |
| GQ1-CHUNK-0007 | [x] DONE | source | critical | authored-config | `game_data/combined launches/Descent II plus Vertigo (USA)/extract_regression.json5` | L1-L35 | `GQC-0020`; added `GQF-0032`, `GQF-0033`, extended six `BR-*` roots |
| GQ1-CHUNK-0008 | [x] DONE | source | critical | authored-config | `10 related paths` | 271 review lines under game_data/music | `GQC-0021`; added `GQF-0034`, `GQF-0035`, extended `GQF-0030`, `BR-0623` |
| GQ1-CHUNK-0009 | [x] DONE | source | critical | authored-source | `2 related paths` | 307 review lines under android/app/src/main/cpp | `GQC-0022`; added `GQF-0036`, `GQF-0037`, extended `BR-0084` |
| GQ1-CHUNK-0010 | [x] DONE | source | critical | authored-source | `2 related paths` | 340 review lines under android/app/src/main/cpp | `GQC-0023`; added `GQF-0038` through `GQF-0040`, extended `BR-0021` |
| GQ1-CHUNK-0011 | [x] DONE | source | critical | authored-source | `2 related paths` | 528 review lines under android/app/src/main/cpp | `GQC-0024`; added `GQF-0041` through `GQF-0043`, extended three owners |
| GQ1-CHUNK-0012 | [x] DONE | source | critical | authored-source | `2 related paths` | 490 review lines under android/app/src/main/cpp | `GQC-0025`; added `GQF-0044` through `GQF-0046`, extended `BR-0021`, `BR-0070` |
| GQ1-CHUNK-0013 | [x] DONE | source | critical | authored-source | `2 related paths` | 548 review lines under android/app/src/main/cpp | `GQC-0026`; added `GQF-0047` through `GQF-0050`, extended `GQF-0046`, `BR-0021` |
| GQ1-CHUNK-0014 | [x] DONE | source | critical | authored-source | `2 related paths` | 321 review lines under android/app/src/main/cpp | `GQC-0027`; four evidence extensions, no new root |
| GQ1-CHUNK-0015 | [x] DONE | source | critical | authored-source | `2 related paths` | 420 review lines under android/app/src/main/cpp | `GQC-0028`; added `GQF-0052`, broadened `GQF-0037`, extended five `BR-*` roots |
| GQ1-CHUNK-0016 | [x] DONE | source | critical | authored-source | `2 related paths` | 78 review lines under android/app/src/main/cpp | `GQC-0029`; added `GQF-0051`, extended four owners, duplicate `BR-0073` |
| GQ1-CHUNK-0017 | [x] DONE | source | critical | authored-source | `3 related paths` | 554 review lines under android/app/src/main/cpp | `GQC-0030`; added `GQF-0053`, `GQF-0054`, extended three owners |
| GQ1-CHUNK-0018 | [x] DONE | source | critical | authored-source | `3 related paths` | 215 review lines under android/app/src/main/cpp | `GQC-0031`; added `GQF-0055` through `GQF-0057` |
| GQ1-CHUNK-0019 | [x] DONE | source | critical | authored-source | `3 related paths` | 517 review lines under android/app/src/main/cpp | `GQC-0032`; added `GQF-0058`, extended three owners |
| GQ1-CHUNK-0020 | [x] DONE | source | critical | authored-source | `3 related paths` | 369 review lines under android/app/src/main/cpp | `GQC-0033`; added `GQF-0059`, `GQF-0060`, extended `BR-0041` |
| GQ1-CHUNK-0021 | [x] DONE | source | critical | authored-source | `3 related paths` | 456 review lines under android/app/src/main/cpp | `GQC-0034`; added `GQF-0061`, `GQF-0062`, extended four owners |
| GQ1-CHUNK-0022 | [x] DONE | source | critical | authored-source | `4 related paths` | 418 review lines under android/app/src/main/cpp | `GQC-0035`; added `GQF-0063` through `GQF-0066` |
| GQ1-CHUNK-0023 | [x] DONE | source | critical | authored-source | `6 related paths` | 548 review lines under android/app/src/main/cpp | `GQC-0036`; five evidence extensions, no new root |
| GQ1-CHUNK-0024 | [x] DONE | source | critical | authored-source | `android/app/src/main/cpp/extract/extract_cd.c` | L1-L600 | `GQC-0037`; five evidence extensions, no new root |
| GQ1-CHUNK-0025 | [x] DONE | source | critical | authored-source | `android/app/src/main/cpp/extract/fingerprint_audio.c` | L1-L421 | `GQC-0038`; added `GQF-0067`, `GQF-0068`, `GQI-0002`; extended two owners |
| GQ1-CHUNK-0026 | [x] DONE | source | critical | authored-source | `android/app/src/main/cpp/extract/fingerprint_cd.c` | L1-L459 | `GQC-0039`; added `GQF-0069`, `GQF-0070`; extended six owners |
| GQ1-CHUNK-0027 | [x] DONE | source | critical | authored-source | `android/app/src/main/cpp/extract/hfs_reader.c` | L1-L600 | `GQC-0040`; added `GQF-0071`, `GQF-0072`, `GQI-0003`; extended two owners |
| GQ1-CHUNK-0028 | [x] DONE | source | critical | authored-source | `android/app/src/main/cpp/extract/hfs_reader.c` | L601-L1200 | `GQC-0041`; added `GQF-0073`, `GQF-0074`; extended `GQI-0003` and two owners |
| GQ1-CHUNK-0029 | [x] DONE | source | critical | authored-source | `android/app/src/main/cpp/extract/inno_reader.c` | L1-L600 | `GQC-0042`; added `GQF-0075`, linked archived `BR-0052` |
| GQ1-CHUNK-0030 | [x] DONE | source | critical | authored-source | `android/app/src/main/cpp/extract/inno_reader.c` | L601-L1200 | `GQC-0043`; added `GQF-0076` through `GQF-0078` |
| GQ1-CHUNK-0031 | [x] DONE | source | critical | authored-source | `android/app/src/main/cpp/extract/inno_reader.c` | L1201-L1800 | `GQC-0044`; added `GQF-0079`, `GQF-0080`; extended `GQF-0078` |
| GQ1-CHUNK-0032 | [x] DONE | source | critical | authored-source | `android/app/src/main/cpp/extract/inno_reader.c` | L1801-L2400 | `GQC-0045`; added `GQF-0081` through `GQF-0083`; extended `GQF-0076`, `BR-0021` |
| GQ1-CHUNK-0033 | [x] DONE | source | critical | authored-source | `android/app/src/main/cpp/extract/inno_reader.c` | L2401-L3000 | `GQC-0046`; added `GQF-0084`; extended `GQF-0083`, `BR-0021` |
| GQ1-CHUNK-0034 | [x] DONE | source | critical | authored-source | `android/app/src/main/cpp/extract/inno_reader.c` | L3001-L3525 | `GQC-0047`; added `GQF-0085`, `GQF-0086`; extended four owners |
| GQ1-CHUNK-0035 | [x] DONE | source | critical | authored-source | `android/app/src/main/cpp/extract/inno_reader.h` | L1-L217 | `GQC-0048`; extended five owners, no new root |
| GQ1-CHUNK-0036 | [x] DONE | source | critical | authored-source | `android/app/src/main/cpp/extract/iso9660_reader.c` | L1-L600 | `GQC-0049`; added `GQF-0087`, `GQF-0088`; extended four owners |
| GQ1-CHUNK-0037 | [x] DONE | source | critical | authored-source | `android/app/src/main/cpp/extract/jni_disc_import.c` | L1-L513 | `GQC-0050`; added `GQF-0089`; extended four owners and one duplicate |
| GQ1-CHUNK-0038 | [x] DONE | source | critical | authored-source | `android/app/src/main/cpp/extract/pkg_reader.c` | L1-L600 | `GQC-0051`; added `GQF-0090`, `GQF-0091`; extended five owners |
| GQ1-CHUNK-0039 | [x] DONE | source | critical | authored-source | `android/app/src/main/cpp/extract/sow_extract.c` | L1-L600 | `GQC-0052`; added `GQF-0092`; extended three owners |
| GQ1-CHUNK-0040 | [x] DONE | source | critical | authored-source | `android/app/src/main/cpp/extract/sti2_extract.c` | L1-L600 | `GQC-0053`; extended two owners, one closed verification, one duplicate |
| GQ1-CHUNK-0041 | [x] DONE | source | critical | authored-source | `android/app/src/main/cpp/extract/sti2_extract.c` | L601-L1200 | `GQC-0054`; added `GQF-0093`; two closed verifications, one duplicate |
| GQ1-CHUNK-0042 | [x] DONE | source | critical | authored-source | `android/app/src/main/cpp/extract/sti2_extract.c` | L1201-L1800 | `GQC-0055`; duplicate `GQF-0093`, two closed verifications, one duplicate |
| GQ1-CHUNK-0043 | [x] DONE | source | critical | authored-source | `android/app/src/main/cpp/extract/sti2_extract.c` | L1801-L2400 | `GQC-0056`; added `GQF-0094`; extended five owners, one closed verification |
| GQ1-CHUNK-0044 | [x] DONE | source | critical | authored-source | `android/app/src/main/cpp/extract/stuffit_extract.c` | L1-L492 | `GQC-0057`; extended six owners, two duplicates, no new root |
| GQ1-CHUNK-0045 | [x] DONE | source | critical | authored-source | `android/app/src/main/cpp/jni_level_metadata.cpp` | L1-L600 | `GQC-0058`; added `GQF-0095`; two closed verifications |
| GQ1-CHUNK-0046 | [x] DONE | source | critical | authored-source | `android/app/src/main/cpp/jni_level_metadata.cpp` | L601-L1200 | `GQC-0059`; extended `BR-0077`, no new root |
| GQ1-CHUNK-0047 | [x] DONE | source | critical | authored-source | `android/app/src/main/cpp/jni_level_metadata.cpp` | L1201-L1251 | `GQC-0060`; extended `BR-0077`, two closed verifications |
| GQ1-CHUNK-0048 | [x] DONE | source | critical | authored-source | `android/app/src/main/cpp/jni_main.c` | L1-L600 | `GQC-0061`; extended `BR-0078`, `BR-0029`, `GQF-0052` |
| GQ1-CHUNK-0049 | [x] DONE | source | critical | authored-source | `android/app/src/main/cpp/jni_main.c` | L601-L1200 | `GQC-0062`; extended five owners, two closed verifications |
| GQ1-CHUNK-0050 | [x] DONE | source | critical | authored-source | `android/app/src/main/cpp/shared/android_music_control.c` | L1-L447 | `GQC-0063`; extended `BR-0029`, `GQF-0089`; three closed verifications |
| GQ1-CHUNK-0051 | [x] DONE | source | critical | authored-source | `android/app/src/main/cpp/jni_resume_save.cpp` | L1-L600 | `GQC-0064`; added `GQF-0096`, `GQF-0097`; extended `BR-0082` |
| GQ1-CHUNK-0052 | [x] DONE | source | critical | authored-source | `android/app/src/main/cpp/jni_resume_save.cpp` | L601-L1154 | `GQC-0065`; added `GQF-0098`; extended `BR-0417` |
| GQ1-CHUNK-0053 | [x] DONE | source | critical | authored-source | `android/app/src/main/cpp/shared/physfs_archiver_saf.c` | L1-L524 | `GQC-0066`; added `GQF-0099`; extended `GQF-0036` |
| GQ1-CHUNK-0054 | [x] DONE | source | critical | authored-source | `2 related paths` | 354 review lines under android/app/src/main/java | `GQC-0067`; added `GQF-0100` through `GQF-0104` |
| GQ1-CHUNK-0055 | [x] DONE | source | critical | authored-source | `2 related paths` | 221 review lines under android/app/src/main/java | `GQC-0068`; added `GQF-0105` through `GQF-0107`; extended `BR-0089` |
| GQ1-CHUNK-0056 | [x] DONE | source | critical | authored-source | `android/app/src/main/java/com/dxxredux/app/ArchiveFiles.kt` | L1-L478 | `GQC-0069`; added `GQF-0108` through `GQF-0110`, `GQI-0004` |
| GQ1-CHUNK-0057 | [x] DONE | source | critical | authored-source | `android/app/src/main/java/com/dxxredux/app/MissionZip.kt` | L1-L539 | `GQC-0070`; added `GQF-0111` through `GQF-0114` |
| GQ1-CHUNK-0058 | [x] DONE | source | critical | authored-source | `android/app/src/main/java/com/dxxredux/app/MissionZipAudioFingerprintCache.kt` | L1-L354 | `GQC-0071`; extended four owners, no new root |
| GQ1-CHUNK-0059 | [x] DONE | source | critical | authored-source | `android/app/src/main/java/com/dxxredux/app/MissionZipExtractionStore.kt` | L1-L600 | `GQC-0072`; added `GQF-0115`, `GQF-0116`; extended `BR-0103` |
| GQ1-CHUNK-0060 | [x] DONE | source | critical | authored-source | `android/app/src/main/java/com/dxxredux/app/MissionZipExtractionStore.kt` | L601-L654 | `GQC-0073`; added `GQF-0117` |
| GQ1-CHUNK-0061 | [x] DONE | source | critical | authored-source | `android/app/src/main/java/com/dxxredux/app/MissionZipMusic.kt` | L1-L600 | `GQC-0074`; added `GQF-0118`; extended three owners |
| GQ1-CHUNK-0062 | [x] DONE | source | critical | authored-source | `android/app/src/main/java/com/dxxredux/app/MissionZipMusicStageManager.kt` | L1-L571 | `GQC-0075`; added `GQF-0119`, `GQF-0120` |
| GQ1-CHUNK-0063 | [x] DONE | source | critical | authored-source | `android/app/src/main/java/com/dxxredux/app/multiplayer/PlayGamesAuth.kt` | L1-L102 | `GQC-0076`; duplicate `BR-0090`, no new root |
| GQ1-CHUNK-0064 | [x] DONE | source | critical | authored-source | `android/app/src/main/res/xml/data_extraction_rules.xml` | L1-L9 | `GQC-0077`; clean, archived `BR-0414` remains closed |
| GQ1-CHUNK-0065 | [x] DONE | source | critical | authored-source | `2 related paths` | 271 review lines under server/src | `GQC-0078`; extended `GQF-0014`, `BR-0106` through `BR-0108` |
| GQ1-CHUNK-0066 | [x] DONE | source | critical | authored-source | `2 related paths` | 146 review lines under server/src | `GQC-0079`; five historical/current duplicates, no new root |
| GQ1-CHUNK-0067 | [x] DONE | source | critical | authored-source | `2 related paths` | 591 review lines under server/src | `GQC-0080`; eight exact duplicates, no new root |
| GQ1-CHUNK-0068 | [x] DONE | source | critical | authored-source | `3 related paths` | 555 review lines under server/src | `GQC-0081`; six duplicates, extended `BR-0135`, `BR-0125` |
| GQ1-CHUNK-0069 | [x] DONE | source | critical | authored-source | `3 related paths` | 445 review lines under server/src | `GQC-0082`; duplicates `BR-0127` through `BR-0131` |
| GQ1-CHUNK-0070 | [x] DONE | source | critical | authored-source | `3 related paths` | 495 review lines under server/src | `GQC-0083`; seven exact duplicates |
| GQ1-CHUNK-0071 | [x] DONE | source | critical | authored-source | `server/src/db.rs` | L1-L532 | `GQC-0084`; six duplicates, four evidence extensions |
| GQ1-CHUNK-0072 | [x] DONE | source | critical | authored-source | `server/src/nat_sim.rs` | L1-L475 | `GQC-0085`; added `GQF-0121`; extended `BR-0109`, `BR-0112`, two duplicates |
| GQ1-CHUNK-0073 | [x] DONE | source | critical | authored-source | `server/src/ws_handler.rs` | L1-L600 | `GQC-0086`; 13 duplicates/evidence refreshes |
| GQ1-CHUNK-0074 | [x] DONE | source | critical | authored-source | `server/src/ws_handler.rs` | L601-L1200 | `GQC-0087`; added `GQF-0122`; extended `BR-0148` |
| GQ1-CHUNK-0075 | [x] DONE | source | critical | authored-source | `server/src/ws_handler.rs` | L1201-L1800 | `GQC-0088`; six duplicates/extensions, no new root |
| GQ1-CHUNK-0076 | [x] DONE | source | critical | authored-source | `server/src/ws_handler.rs` | L1801-L2400 | `GQC-0089`; seven duplicates/evidence refreshes |
| GQ1-CHUNK-0077 | [x] DONE | source | critical | authored-source | `server/src/ws_handler.rs` | L2401-L2951 | `GQC-0090`; seven duplicates/evidence extensions |
| GQ1-CHUNK-0078 | [x] DONE | source | critical | build-script | `android/app/src/main/cpp/extract/.gitignore` | L1-L1 | `GQC-0091`; clean, historical owners reconciled |
| GQ1-CHUNK-0079 | [x] DONE | source | critical | build-script | `android/app/src/main/cpp/extract/CMakeLists.txt` | L1-L600 | `GQC-0092`; added `GQF-0123`; extended `GQF-0024`, `GQF-0068`, `BR-0158` |
| GQ1-CHUNK-0080 | [x] DONE | source | critical | build-script | `android/app/src/main/cpp/extract/CMakeLists.txt` | L601-L681 | `GQC-0093`; extended `BR-0160`, `BR-0236` |
| GQ1-CHUNK-0081 | [x] DONE | source | critical | build-script | `android/get_deps/helpers/get_7zip.ps1` | L1-L95 | `GQC-0094`; extended `BR-0174`, `BR-0159` |
| GQ1-CHUNK-0082 | [x] DONE | source | critical | build-script | `2 related paths` | 398 review lines under android/helpers | `GQC-0095`; added `GQF-0124`; duplicated four `BR-*` roots |
| GQ1-CHUNK-0083 | [x] DONE | source | critical | build-script | `3 related paths` | 597 review lines under android/helpers | `GQC-0096`; added `GQF-0125`, `GQF-0126`; extended `GQF-0072`, `GQF-0040` |
| GQ1-CHUNK-0084 | [x] DONE | source | critical | build-script | `3 related paths` | 336 review lines under android/helpers | `GQC-0097`; added `GQF-0127` through `GQF-0129`; extended `BR-0160`, duplicated `BR-0007` |
| GQ1-CHUNK-0085 | [x] DONE | source | critical | build-script | `android/helpers/run_mission_zip_batch.ps1` | L1-L600 | `GQC-0098`; regrowth `BR-0161`, duplicated three roots |
| GQ1-CHUNK-0086 | [x] DONE | source | critical | build-script | `2 related paths` | 462 review lines under game_data | `GQC-0099`; added `GQF-0130`; extended `BR-0169`, duplicated `GQF-0016` |
| GQ1-CHUNK-0087 | [x] DONE | source | critical | build-script | `game_data/extract_dos_demos.ps1` | L1-L283 | `GQC-0100`; added `GQF-0131`; extended four owners |
| GQ1-CHUNK-0088 | [x] DONE | source | critical | build-script | `game_data/extract_mac_cd.ps1` | L1-L468 | `GQC-0101`; added `GQF-0132` through `GQF-0134`; extended eight owners |
| GQ1-CHUNK-0089 | [x] DONE | source | critical | build-script | `game_data/extract_mac_demos.ps1` | L1-L213 | `GQC-0102`; added `GQF-0135`, `GQF-0136`; extended four owners |
| GQ1-CHUNK-0090 | [x] DONE | source | critical | build-script | `game_data/fingerprint_mission_zip_music.ps1` | L1-L600 | `GQC-0103`; added `GQF-0137`, `GQF-0138`; extended `GQF-0128` |
| GQ1-CHUNK-0091 | [x] DONE | source | critical | build-script | `game_data/fingerprint_mission_zip_music.ps1` | L601-L958 | `GQC-0104`; added `GQF-0139`, `GQF-0140`; extended three owners |
| GQ1-CHUNK-0092 | [x] DONE | source | critical | documentation | `android/app/src/main/cpp/extract/INNO_READER_CAPABILITIES.md` | L1-L56 | `GQC-0105`; extended `GQF-0078`, `GQF-0018` |
| GQ1-CHUNK-0093 | [x] DONE | source | critical | test-source | `11 related paths` | 572 review lines under android/app/src/main/cpp | `GQC-0106`; extended three owners, duplicated `BR-0597` |
| GQ1-CHUNK-0094 | [x] DONE | source | critical | test-source | `2 related paths` | 448 review lines under android/app/src/main/cpp | `GQC-0107`; added `GQF-0141`, `GQF-0142`; extended `GQF-0075`, duplicated `BR-0158` |
| GQ1-CHUNK-0095 | [x] DONE | source | critical | test-source | `2 related paths` | 477 review lines under android/app/src/main/cpp | `GQC-0108`; added `GQF-0143`; extended `BR-0160`, `GQF-0061`, `GQF-0062` |
| GQ1-CHUNK-0096 | [x] DONE | source | critical | test-source | `3 related paths` | 160 review lines under android/app/src/main/cpp | `GQC-0109`; extended `GQF-0090`, `GQF-0091`, `GQF-0046`; historical `BR-0024` closed |
| GQ1-CHUNK-0097 | [x] DONE | source | critical | test-source | `5 related paths` | 436 review lines under android/app/src/main/cpp | `GQC-0110`; added `GQF-0144`; extended `GQF-0054`, `BR-0236` |
| GQ1-CHUNK-0098 | [x] DONE | source | critical | test-source | `5 related paths` | 328 review lines under android/app/src/main/cpp | `GQC-0111`; added `GQF-0145`; extended `BR-0046`, `GQF-0069` |
| GQ1-CHUNK-0099 | [x] DONE | source | critical | test-source | `android/app/src/main/cpp/extract/test_cue_iso.c` | L1-L600 | `GQC-0112`; extended `BR-0160` |
| GQ1-CHUNK-0100 | [x] DONE | source | critical | test-source | `android/app/src/main/cpp/extract/test_cue_iso.c` | L601-L1200 | `GQC-0113`; added `GQF-0146`; extended `BR-0160` |
| GQ1-CHUNK-0101 | [x] DONE | source | critical | test-source | `android/app/src/main/cpp/extract/test_cue_iso.c` | L1201-L1800 | `GQC-0114`; added `GQF-0147`; extended `BR-0160` |
| GQ1-CHUNK-0102 | [x] DONE | source | critical | test-source | `android/app/src/main/cpp/extract/test_cue_iso.c` | L1801-L2400 | `GQC-0115`; added `GQF-0148`; extended `GQF-0046`, `BR-0160` |
| GQ1-CHUNK-0103 | [x] DONE | source | critical | test-source | `android/app/src/main/cpp/extract/test_cue_iso.c` | L2401-L3000 | `GQC-0116`; extended `GQF-0063`, `BR-0160` |
| GQ1-CHUNK-0104 | [x] DONE | source | critical | test-source | `android/app/src/main/cpp/extract/test_fingerprint.c` | L1-L559 | `GQC-0117`; added `GQF-0149` through `GQF-0151`; extended `BR-0181` |
| GQ1-CHUNK-0105 | [x] DONE | source | critical | test-source | `android/app/src/main/cpp/extract/test_game_file_extensions.c` | L1-L20 | `GQC-0118`; extended `BR-0662`, archived `BR-0041` |
| GQ1-CHUNK-0106 | [x] DONE | source | critical | test-source | `android/app/src/main/cpp/extract/test_gog_fd.c` | L1-L600 | `GQC-0119`; extended six current owners |
| GQ1-CHUNK-0107 | [x] DONE | source | critical | test-source | `android/app/src/main/cpp/extract/test_gog_fd.c` | L601-L1200 | `GQC-0120`; added `GQF-0152`; extended three owners |
| GQ1-CHUNK-0108 | [x] DONE | source | critical | test-source | `android/app/src/main/cpp/extract/test_hfs.c` | L1-L600 | `GQD-0001`, `GQC-0121`; no inherited effect; extended three owners and `GQI-0003` |
| GQ1-CHUNK-0109 | [x] DONE | source | critical | test-source | `android/app/src/main/cpp/extract/test_hfs.c` | L601-L1140 | `GQD-0002`, `GQC-0122`; no inherited effect; evidence extensions only |
| GQ1-CHUNK-0110 | [x] DONE | source | critical | test-source | `android/app/src/main/cpp/extract/test_pkg_toc_bounds.c` | L1-L600 | `GQD-0003`, `GQC-0123`; no inherited effect; evidence extensions only |
| GQ1-CHUNK-0111 | [x] DONE | source | critical | test-source | `android/app/src/main/cpp/extract/test_sow_integrity.c` | L1-L567 | `GQD-0004`, `GQC-0124`; no inherited effect; extended three owners |
| GQ1-CHUNK-0112 | [x] DONE | source | critical | test-source | `android/app/src/main/cpp/extract/test_sow_real_media.cmake` | L1-L77 | `GQD-0005`, `GQC-0125`; no inherited effect; clean, `BR-0024` remains closed |
| GQ1-CHUNK-0113 | [x] DONE | source | critical | test-source | `android/app/src/main/cpp/extract/test_sti2.c` | L1-L600 | `GQD-0006`, `GQC-0126`; no inherited effect; extended `GQF-0150`, `BR-0160` |
| GQ1-CHUNK-0114 | [x] DONE | source | critical | test-source | `android/app/src/main/cpp/extract/test_sti2.c` | L601-L1050 | `GQD-0007`, `GQC-0127`; no inherited effect; added `GQF-0153`, extended three owners |
| GQ1-CHUNK-0115 | [x] DONE | source | critical | test-source | `android/app/src/main/cpp/extract/test_stuffit_corpus.cpp` | L1-L600 | `GQD-0008`, `GQC-0128`; no inherited effect; extended `BR-0182`, `BR-0586` |
| GQ1-CHUNK-0116 | [x] DONE | source | critical | test-source | `android/app/src/main/cpp/extract/test_stuffit_corpus.cpp` | L601-L1120 | `GQD-0009`, `GQC-0129`; no inherited effect; extended two owners |
| GQ1-CHUNK-0117 | [x] DONE | source | critical | test-source | `2 related paths` | 212 review lines under android/app/src/test/java | `GQD-0010`, `GQC-0130`; deferred ~16-line paired seam; added `GQF-0154` |
| GQ1-CHUNK-0118 | [x] DONE | source | critical | test-source | `4 related paths` | 549 review lines under android/app/src/test/java | `GQD-0011`, `GQC-0131`; no inherited effect; existing-owner evidence only |
| GQ1-CHUNK-0119 | [x] DONE | source | critical | test-source | `5 related paths` | 461 review lines under android/app/src/test/java | `GQD-0012`, `GQC-0132`; no inherited effect; existing-owner evidence only |
| GQ1-CHUNK-0120 | [x] DONE | source | critical | test-source | `android/app/src/test/java/com/dxxredux/app/MissionZipAudioFingerprintCacheTest.kt` | L1-L403 | `GQD-0013`, `GQC-0133`; no inherited effect; retained 4-line natural consumer boundary |
| GQ1-CHUNK-0121 | [x] DONE | source | critical | test-source | `android/app/src/test/java/com/dxxredux/app/MissionZipMusicStageManagerTest.kt` | L1-L486 | `GQD-0014`, `GQC-0134`; no inherited effect; extended `GQF-0119`, `GQF-0120` |
| GQ1-CHUNK-0122 | [x] DONE | source | critical | test-source | `android/app/src/test/java/com/dxxredux/app/MissionZipMusicTest.kt` | L1-L357 | `GQD-0015`, `GQC-0135`; no inherited effect; linked prior paired songs decisions |
| GQ1-CHUNK-0123 | [x] DONE | source | critical | test-source | `android/app/src/test/java/com/dxxredux/app/MissionZipTest.kt` | L1-L600 | `GQD-0016`, `GQC-0136`; no inherited effect; extended `GQF-0111`, `GQF-0112` |
| GQ1-CHUNK-0124 | [x] DONE | source | critical | test-source | `android/app/src/test/java/com/dxxredux/app/MissionZipTest.kt` | L601-L706 | `GQD-0017`, `GQC-0137`; no inherited effect; duplicate `GQF-0111` evidence |
| GQ1-CHUNK-0125 | [x] DONE | source | critical | test-source | `android/app/src/test/java/com/dxxredux/app/ModManagerMissionZipTest.kt` | L1-L600 | `GQD-0018`, `GQC-0138`; added `GQF-0155`; estimated -26 inherited lines/-2 hunks |
| GQ1-CHUNK-0126 | [x] DONE | source | critical | test-source | `android/app/src/test/java/com/dxxredux/app/ModManagerMissionZipTest.kt` | L601-L972 | `GQD-0019`, `GQC-0139`; no inherited effect; duplicate `GQF-0111` evidence |
| GQ1-CHUNK-0127 | [x] DONE | source | critical | test-source | `4 related paths` | 150 review lines under android/game_scripts | `GQD-0020`, `GQC-0140`; added `GQF-0156`; estimated -144 inherited lines |
| GQ1-CHUNK-0128 | [x] DONE | source | critical | test-source | `2 related paths` | 295 review lines under android/tests | `GQD-0021`, `GQC-0141`; 559 inherited lines already removed externally; added `GQF-0157`, `GQF-0158` |
| GQ1-CHUNK-0129 | [x] DONE | source | critical | test-source | `2 related paths` | 383 review lines under android/tests | `GQD-0022`, `GQC-0142`; no inherited effect; existing-owner evidence only |
| GQ1-CHUNK-0130 | [x] DONE | source | critical | test-source | `3 related paths` | 597 review lines under android/tests | `GQD-0023`, `GQC-0143`; no inherited effect; existing-owner evidence only |
| GQ1-CHUNK-0131 | [x] DONE | source | critical | test-source | `3 related paths` | 268 review lines under android/tests | `GQD-0024`, `GQC-0144`; duplicate PhysFS candidate; added `GQF-0159` |
| GQ1-CHUNK-0132 | [x] DONE | source | critical | test-source | `3 related paths` | 585 review lines under android/tests | `GQD-0025`, `GQC-0145`; duplicate accessor candidate; added `GQF-0160`, `GQF-0161` |
| GQ1-CHUNK-0133 | [x] DONE | source | critical | test-source | `4 related paths` | 488 review lines under android/tests | `GQD-0026`, `GQC-0146`; no inherited effect; extended `GQF-0153`, `GQF-0137` |
| GQ1-CHUNK-0134 | [x] DONE | source | critical | test-source | `5 related paths` | 542 review lines under android/tests | `GQD-0027`, `GQC-0147`; no inherited effect; added `GQF-0162` |
| GQ1-CHUNK-0135 | [x] DONE | source | critical | test-source | `android/tests/test_extract.ps1` | L1-L600 | `GQD-0028`, `GQC-0148`; duplicate accessor candidate; added `GQF-0163` |
| GQ1-CHUNK-0136 | [x] DONE | source | critical | test-source | `android/tests/test_extract.ps1` | L601-L1200 | `GQD-0029`, `GQC-0149`; no inherited effect; existing-owner evidence only |
| GQ1-CHUNK-0137 | [x] DONE | source | critical | test-source | `android/tests/test_saf_archiver.ps1` | L1-L600 | `GQD-0030`, `GQC-0150`; duplicate PhysFS candidate; added `GQF-0164` |
| GQ1-CHUNK-0138 | [x] DONE | source | critical | test-source | `game_data/mods/d2x-xl/test_wav_parser.ps1` | L1-L79 | `GQD-0031`, `GQC-0151`; no inherited effect; added `GQF-0165` |
| GQ1-CHUNK-0139 | [x] DONE | source | high | authored-config | `d1/CMakePresets.json` | diff hunks 1-1, new L20-L20 | `GQD-0032`, `GQC-0152`; retained paired preset seam; duplicate `BR-0610` |
| GQ1-CHUNK-0140 | [x] DONE | source | high | authored-config | `d2/CMakePresets.json` | diff hunks 1-1, new L20-L20 | `GQD-0033`, `GQC-0153`; retained paired preset seam; duplicate `BR-0610` |
| GQ1-CHUNK-0141 | [x] DONE | source | high | authored-config | `3 related paths` | 171 review lines under server | `GQD-0034`, `GQC-0154`; no inherited effect; added `GQF-0166` |
| GQ1-CHUNK-0142 | [x] DONE | source | high | authored-source | `2 related paths` | 102 review lines under android/app/src/main/cpp | `GQD-0035`, `GQC-0155`; no inherited effect; clean |
| GQ1-CHUNK-0143 | [x] DONE | source | high | authored-source | `2 related paths` | 318 review lines under android/app/src/main/cpp | `GQD-0036`, `GQC-0156`; retained 15-line paired surface/headless seam; clean |
| GQ1-CHUNK-0144 | [x] DONE | source | high | authored-source | `2 related paths` | 545 review lines under android/app/src/main/cpp | `GQD-0037`, `GQC-0157`; retained 14-line paired rewind API seam; clean |
| GQ1-CHUNK-0145 | [x] DONE | source | high | authored-source | `2 related paths` | 111 review lines under android/app/src/main/cpp | `GQD-0038`, `GQC-0158`; no inherited effect; clean |
| GQ1-CHUNK-0146 | [x] DONE | source | high | authored-source | `2 related paths` | 60 review lines under android/app/src/main/cpp | `GQD-0039`, `GQC-0159`; retained 21-path co-op hook surface after prior 166-line extraction; clean |
| GQ1-CHUNK-0147 | [x] DONE | source | high | authored-source | `2 related paths` | 458 review lines under android/app/src/main/cpp | `GQD-0042`, `GQC-0162`; retained 4-path GLES shim seam; existing-owner evidence only |
| GQ1-CHUNK-0148 | [x] DONE | source | high | authored-source | `2 related paths` | 181 review lines under android/app/src/main/cpp | `GQD-0040`, `GQC-0160`; retained paired game-control seam; clean |
| GQ1-CHUNK-0149 | [x] DONE | source | high | authored-source | `2 related paths` | 88 review lines under android/app/src/main/cpp | `GQD-0041`, `GQC-0161`; retained 6-path demo-start seam; duplicate `BR-0300` |
| GQ1-CHUNK-0150 | [x] DONE | source | high | authored-source | `2 related paths` | 123 review lines under android/app/src/main/cpp | `GQD-0043`, `GQC-0163`; retained 22-path recorder observation surface; duplicate `BR-0222` |
| GQ1-CHUNK-0151 | [x] DONE | source | high | authored-source | `2 related paths` | 629 review lines under android/app/src/main/cpp | `GQD-0044`, `GQC-0164`; retained 12-path audio/effect seam; added `GQF-0167` |
| GQ1-CHUNK-0152 | [x] DONE | source | high | authored-source | `2 related paths` | 174 review lines under android/app/src/main/cpp | `GQD-0045`, `GQC-0165`; retained 32-path replay seam; added `GQF-0168` |
| GQ1-CHUNK-0153 | [x] DONE | source | high | authored-source | `2 related paths` | 492 review lines under android/app/src/main/cpp | `GQD-0047`, `GQC-0167`; added `GQF-0169`; estimated -22 inherited lines/-2 hunks; extended `GQF-0167` |
| GQ1-CHUNK-0154 | [x] DONE | source | high | authored-source | `2 related paths` | 200 review lines under android/app/src/main/cpp | `GQD-0046`, `GQC-0166`; retained six-path automation hook seam; extended `BR-0224` |
| GQ1-CHUNK-0155 | [x] DONE | source | high | authored-source | `3 related paths` | 750 review lines under android/app/src/main/cpp | `GQD-0048`, `GQC-0168`; added `GQF-0170`; estimated -24 inherited lines/-4 hunks |
| GQ1-CHUNK-0156 | [x] DONE | source | high | authored-source | `3 related paths` | 425 review lines under android/app/src/main/cpp | `GQD-0050`, `GQC-0170`; retained 23-path direct-command/debug seam; extended `GQF-0168` |
| GQ1-CHUNK-0157 | [x] DONE | source | high | authored-source | `3 related paths` | 301 review lines under android/app/src/main/cpp | `GQD-0049`, `GQC-0169`; added `GQF-0171`; estimated -4 inherited lines |
| GQ1-CHUNK-0158 | [x] DONE | source | high | authored-source | `3 related paths` | 424 review lines under android/app/src/main/cpp | `GQD-0051`, `GQC-0171`; duplicate existing `DMR1-CHUNK-003`, estimated -64 inherited lines/-2 body hunks |
| GQ1-CHUNK-0159 | [x] DONE | source | high | authored-source | `3 related paths` | 686 review lines under android/app/src/main/cpp | `GQD-0052`, `GQC-0172`; added `GQF-0172`, `GQF-0173`; estimated -48 inherited lines |
| GQ1-CHUNK-0160 | [x] DONE | source | high | authored-source | `3 related paths` | 224 review lines under android/app/src/main/cpp | `GQD-0053`, `GQC-0173`; retained four-path route build seam; existing-owner evidence |
| GQ1-CHUNK-0161 | [x] DONE | source | high | authored-source | `3 related paths` | 734 review lines under android/app/src/main/cpp | `GQD-0054`, `GQC-0174`; retained four-path lifecycle/loading seam; existing `BR-0254`, `BR-0078` evidence |
| GQ1-CHUNK-0162 | [x] DONE | source | high | authored-source | `3 related paths` | 566 review lines under android/app/src/main/cpp | `GQD-0055`, `GQC-0175`; retained four-path preview/lifecycle seam; clean |
| GQ1-CHUNK-0163 | [x] DONE | source | high | authored-source | `3 related paths` | 425 review lines under android/app/src/main/cpp | `GQD-0056`, `GQC-0176`; retained shared route build seam; duplicate `BR-0229`, `BR-0396`, `BR-0331` |
| GQ1-CHUNK-0164 | [x] DONE | source | high | authored-source | `3 related paths` | 695 review lines under android/app/src/main/cpp | `GQD-0057`, `GQC-0177`; added `GQF-0174`; estimated -40 inherited lines/-2 hunks |
| GQ1-CHUNK-0165 | [x] DONE | source | high | authored-source | `3 related paths` | 569 review lines under android/app/src/main/cpp | `GQD-0058`, `GQC-0178`; added `GQF-0175`, `GQF-0176`; estimated -169 to -177 inherited lines/-2 hunks |
| GQ1-CHUNK-0166 | [x] DONE | source | high | authored-source | `3 related paths` | 310 review lines under android/app/src/main/cpp | `GQD-0059`, `GQC-0179`; retained four-path introspection seam; extended `BR-0244`, `BR-0588` |
| GQ1-CHUNK-0167 | [x] DONE | source | high | authored-source | `3 related paths` | 412 review lines under android/app/src/main/cpp | `GQD-0060`, `GQC-0180`; no additional inherited effect; existing `GQF-0174` unchanged |
| GQ1-CHUNK-0168 | [ ] TODO | source | high | authored-source | `3 related paths` | 570 review lines under android/app/src/main/cpp | - |
| GQ1-CHUNK-0169 | [ ] TODO | source | high | authored-source | `4 related paths` | 453 review lines under android/app/src/main/cpp | - |
| GQ1-CHUNK-0170 | [ ] TODO | source | high | authored-source | `4 related paths` | 609 review lines under android/app/src/main/cpp | - |
| GQ1-CHUNK-0171 | [ ] TODO | source | high | authored-source | `4 related paths` | 648 review lines under android/app/src/main/cpp | - |
| GQ1-CHUNK-0172 | [ ] TODO | source | high | authored-source | `4 related paths` | 750 review lines under android/app/src/main/cpp | - |
| GQ1-CHUNK-0173 | [ ] TODO | source | high | authored-source | `4 related paths` | 694 review lines under android/app/src/main/cpp | - |
| GQ1-CHUNK-0174 | [ ] TODO | source | high | authored-source | `4 related paths` | 749 review lines under android/app/src/main/cpp | - |
| GQ1-CHUNK-0175 | [ ] TODO | source | high | authored-source | `4 related paths` | 234 review lines under android/app/src/main/cpp | - |
| GQ1-CHUNK-0176 | [ ] TODO | source | high | authored-source | `4 related paths` | 529 review lines under android/app/src/main/cpp | - |
| GQ1-CHUNK-0177 | [ ] TODO | source | high | authored-source | `4 related paths` | 417 review lines under android/app/src/main/cpp | - |
| GQ1-CHUNK-0178 | [ ] TODO | source | high | authored-source | `4 related paths` | 647 review lines under android/app/src/main/cpp | - |
| GQ1-CHUNK-0179 | [ ] TODO | source | high | authored-source | `4 related paths` | 424 review lines under android/app/src/main/cpp | - |
| GQ1-CHUNK-0180 | [ ] TODO | source | high | authored-source | `4 related paths` | 278 review lines under android/app/src/main/cpp | - |
| GQ1-CHUNK-0181 | [ ] TODO | source | high | authored-source | `4 related paths` | 676 review lines under android/app/src/main/cpp | - |
| GQ1-CHUNK-0182 | [ ] TODO | source | high | authored-source | `4 related paths` | 694 review lines under android/app/src/main/cpp | - |
| GQ1-CHUNK-0183 | [ ] TODO | source | high | authored-source | `4 related paths` | 743 review lines under android/app/src/main/cpp | - |
| GQ1-CHUNK-0184 | [ ] TODO | source | high | authored-source | `4 related paths` | 315 review lines under android/app/src/main/cpp | - |
| GQ1-CHUNK-0185 | [ ] TODO | source | high | authored-source | `4 related paths` | 716 review lines under android/app/src/main/cpp | - |
| GQ1-CHUNK-0186 | [ ] TODO | source | high | authored-source | `4 related paths` | 529 review lines under android/app/src/main/cpp | - |
| GQ1-CHUNK-0187 | [ ] TODO | source | high | authored-source | `4 related paths` | 595 review lines under android/app/src/main/cpp | - |
| GQ1-CHUNK-0188 | [ ] TODO | source | high | authored-source | `4 related paths` | 584 review lines under android/app/src/main/cpp | - |
| GQ1-CHUNK-0189 | [ ] TODO | source | high | authored-source | `5 related paths` | 542 review lines under android/app/src/main/cpp | - |
| GQ1-CHUNK-0190 | [ ] TODO | source | high | authored-source | `5 related paths` | 637 review lines under android/app/src/main/cpp | - |
| GQ1-CHUNK-0191 | [ ] TODO | source | high | authored-source | `5 related paths` | 659 review lines under android/app/src/main/cpp | - |
| GQ1-CHUNK-0192 | [ ] TODO | source | high | authored-source | `5 related paths` | 716 review lines under android/app/src/main/cpp | - |
| GQ1-CHUNK-0193 | [ ] TODO | source | high | authored-source | `6 related paths` | 695 review lines under android/app/src/main/cpp | - |
| GQ1-CHUNK-0194 | [ ] TODO | source | high | authored-source | `6 related paths` | 484 review lines under android/app/src/main/cpp | - |
| GQ1-CHUNK-0195 | [ ] TODO | source | high | authored-source | `6 related paths` | 686 review lines under android/app/src/main/cpp | - |
| GQ1-CHUNK-0196 | [ ] TODO | source | high | authored-source | `6 related paths` | 616 review lines under android/app/src/main/cpp | - |
| GQ1-CHUNK-0197 | [ ] TODO | source | high | authored-source | `6 related paths` | 570 review lines under android/app/src/main/cpp | - |
| GQ1-CHUNK-0198 | [ ] TODO | source | high | authored-source | `7 related paths` | 746 review lines under android/app/src/main/cpp | - |
| GQ1-CHUNK-0199 | [ ] TODO | source | high | authored-source | `7 related paths` | 446 review lines under android/app/src/main/cpp | - |
| GQ1-CHUNK-0200 | [ ] TODO | source | high | authored-source | `8 related paths` | 540 review lines under android/app/src/main/cpp | - |
| GQ1-CHUNK-0201 | [ ] TODO | source | high | authored-source | `8 related paths` | 585 review lines under android/app/src/main/cpp | - |
| GQ1-CHUNK-0202 | [ ] TODO | source | high | authored-source | `9 related paths` | 676 review lines under android/app/src/main/cpp | - |
| GQ1-CHUNK-0203 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/android_autoselect.cpp` | L1-L454 | - |
| GQ1-CHUNK-0204 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/android_gamepad_config.cpp` | L1-L455 | - |
| GQ1-CHUNK-0205 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/android_input.c` | L1-L750 | - |
| GQ1-CHUNK-0206 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/android_input.c` | L751-L1500 | - |
| GQ1-CHUNK-0207 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/android_input.c` | L1501-L2088 | - |
| GQ1-CHUNK-0208 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/android_pilot_prefs.cpp` | L1-L571 | - |
| GQ1-CHUNK-0209 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/headless/headless_metadata_dump_main.cpp` | L1-L750 | - |
| GQ1-CHUNK-0210 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/android_menu_scale.c` | L1-L600 | - |
| GQ1-CHUNK-0211 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/android_profile.c` | L1-L750 | - |
| GQ1-CHUNK-0212 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/android_profile.c` | L751-L1433 | - |
| GQ1-CHUNK-0213 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/automap_metadata_overlay.c` | L1-L750 | - |
| GQ1-CHUNK-0214 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/cd_preview.c` | L1-L750 | - |
| GQ1-CHUNK-0215 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/coop_indicator_lines.c` | L1-L606 | - |
| GQ1-CHUNK-0216 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/coop/coop_save.c` | L1-L750 | - |
| GQ1-CHUNK-0217 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/coop/coop_save.c` | L751-L1475 | - |
| GQ1-CHUNK-0218 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/digi_tsf_music.c` | L1-L750 | - |
| GQ1-CHUNK-0219 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/game_automate.cpp` | L1-L750 | - |
| GQ1-CHUNK-0220 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/game_automate.cpp` | L751-L1500 | - |
| GQ1-CHUNK-0221 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/game_automate.cpp` | L1501-L2250 | - |
| GQ1-CHUNK-0222 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/game_automate.cpp` | L2251-L3000 | - |
| GQ1-CHUNK-0223 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/game_automate.cpp` | L3001-L3750 | - |
| GQ1-CHUNK-0224 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/game_introspect.cpp` | L1-L750 | - |
| GQ1-CHUNK-0225 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/game_introspect.cpp` | L751-L1500 | - |
| GQ1-CHUNK-0226 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/game_introspect.cpp` | L1501-L2250 | - |
| GQ1-CHUNK-0227 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/gles3_shim.c` | L1-L750 | - |
| GQ1-CHUNK-0228 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/input_demo_controls.cpp` | L1-L750 | - |
| GQ1-CHUNK-0229 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/input_demo_debug_logging.cpp` | L1-L602 | - |
| GQ1-CHUNK-0230 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/input_demo_fixture.cpp` | L1-L750 | - |
| GQ1-CHUNK-0231 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/input_demo_fixture.cpp` | L751-L1324 | - |
| GQ1-CHUNK-0232 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/input_demo_hooks_shared.c` | L1-L750 | - |
| GQ1-CHUNK-0233 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/input_demo_hooks_shared.c` | L751-L1500 | - |
| GQ1-CHUNK-0234 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/input_demo_hooks_shared.c` | L1501-L2243 | - |
| GQ1-CHUNK-0235 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/input_demo_newdemo_shared.c` | L1-L705 | - |
| GQ1-CHUNK-0236 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/input_demo_newdemo_shared.h` | L1-L9 | - |
| GQ1-CHUNK-0237 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/input_demo_recorder.cpp` | L1-L750 | - |
| GQ1-CHUNK-0238 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/input_demo_replay.cpp` | L1-L750 | - |
| GQ1-CHUNK-0239 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/input_demo_result.cpp` | L1-L706 | - |
| GQ1-CHUNK-0240 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/input_demo_rng_trace.h` | L1-L54 | - |
| GQ1-CHUNK-0241 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/input_demo_start_shared.c` | L1-L750 | - |
| GQ1-CHUNK-0242 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/input_demo_state_trace.cpp` | L1-L750 | - |
| GQ1-CHUNK-0243 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/level_metadata_scan.c` | L1-L565 | - |
| GQ1-CHUNK-0244 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/level_metadata_scan.h` | L1-L270 | - |
| GQ1-CHUNK-0245 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/merged_wall_debug.c` | L1-L750 | - |
| GQ1-CHUNK-0246 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/merged_wall_debug.c` | L751-L1500 | - |
| GQ1-CHUNK-0247 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/merged_wall_debug.c` | L1501-L2250 | - |
| GQ1-CHUNK-0248 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/merged_wall_debug.c` | L2251-L3000 | - |
| GQ1-CHUNK-0249 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/merged_wall_debug.c` | L3001-L3750 | - |
| GQ1-CHUNK-0250 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/merged_wall_debug.c` | L3751-L4500 | - |
| GQ1-CHUNK-0251 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/merged_wall_debug.c` | L4501-L5250 | - |
| GQ1-CHUNK-0252 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/merged_wall_debug.c` | L5251-L6000 | - |
| GQ1-CHUNK-0253 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/merged_wall_debug.c` | L6001-L6750 | - |
| GQ1-CHUNK-0254 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/merged_wall_debug.c` | L6751-L7500 | - |
| GQ1-CHUNK-0255 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/merged_wall_debug.c` | L7501-L8109 | - |
| GQ1-CHUNK-0256 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/midi_enumeration.h` | L1-L40 | - |
| GQ1-CHUNK-0257 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/midi_preview.c` | L1-L724 | - |
| GQ1-CHUNK-0258 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/multi_save_transfer.c` | L1-L750 | - |
| GQ1-CHUNK-0259 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/net/net_udp_android.c` | L1-L709 | - |
| GQ1-CHUNK-0260 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/pngfile.c` | L1-L650 | - |
| GQ1-CHUNK-0261 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/rbaudio_bin.c` | L1-L750 | - |
| GQ1-CHUNK-0262 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/rbaudio_bin.c` | L751-L1500 | - |
| GQ1-CHUNK-0263 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/route_planner.cpp` | L1-L750 | - |
| GQ1-CHUNK-0264 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/route_planner.cpp` | L751-L1500 | - |
| GQ1-CHUNK-0265 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/route_planner.cpp` | L1501-L2250 | - |
| GQ1-CHUNK-0266 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/route_planner.cpp` | L2251-L3000 | - |
| GQ1-CHUNK-0267 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/route_snapshot.cpp` | L1-L620 | - |
| GQ1-CHUNK-0268 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/secretarea.c` | L1-L750 | - |
| GQ1-CHUNK-0269 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/secretarea.c` | L751-L1500 | - |
| GQ1-CHUNK-0270 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/secretarea.c` | L1501-L2250 | - |
| GQ1-CHUNK-0271 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/secret_area_scan.c` | L1-L750 | - |
| GQ1-CHUNK-0272 | [ ] TODO | source | high | authored-source | `android/app/src/main/cpp/shared/state_android_shared.c` | L1-L750 | - |
| GQ1-CHUNK-0273 | [ ] TODO | source | high | authored-source | `2 related paths` | 449 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0274 | [ ] TODO | source | high | authored-source | `3 related paths` | 500 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0275 | [ ] TODO | source | high | authored-source | `android/app/src/main/java/com/dxxredux/app/multiplayer/NetworkProtocol.kt` | L1-L666 | - |
| GQ1-CHUNK-0276 | [ ] TODO | source | high | authored-source | `android/app/src/main/java/com/dxxredux/app/StorageFailureDialog.kt` | L1-L29 | - |
| GQ1-CHUNK-0277 | [ ] TODO | source | high | authored-source | `android/app/src/main/res/xml/network_security_config.xml` | L1-L10 | - |
| GQ1-CHUNK-0278 | [ ] TODO | source | high | authored-source | `6 related paths` | 201 review lines under d1/2d | - |
| GQ1-CHUNK-0279 | [ ] TODO | source | high | authored-source | `d1/3d/interp.c` | diff hunks 1-14, new L21-L429 | - |
| GQ1-CHUNK-0280 | [ ] TODO | source | high | authored-source | `5 related paths` | 119 review lines under d1/arch | - |
| GQ1-CHUNK-0281 | [ ] TODO | source | high | authored-source | `5 related paths` | 689 review lines under d1/arch | - |
| GQ1-CHUNK-0282 | [ ] TODO | source | high | authored-source | `8 related paths` | 422 review lines under d1/arch | - |
| GQ1-CHUNK-0283 | [ ] TODO | source | high | authored-source | `d1/arch/ogl/ogl.c` | diff hunks 1-64, new L13-L1619 | - |
| GQ1-CHUNK-0284 | [ ] TODO | source | high | authored-source | `d1/arch/ogl/ogl.c` | diff hunks 65-107, new L1621-L3002 | - |
| GQ1-CHUNK-0285 | [ ] TODO | source | high | authored-source | `16 related paths` | 160 review lines under d1/include | - |
| GQ1-CHUNK-0286 | [ ] TODO | source | high | authored-source | `2 related paths` | 16 review lines under d1/include | - |
| GQ1-CHUNK-0287 | [ ] TODO | source | high | authored-source | `10 related paths` | 310 review lines under d1/main | - |
| GQ1-CHUNK-0288 | [ ] TODO | source | high | authored-source | `11 related paths` | 709 review lines under d1/main | - |
| GQ1-CHUNK-0289 | [ ] TODO | source | high | authored-source | `16 related paths` | 523 review lines under d1/main | - |
| GQ1-CHUNK-0290 | [ ] TODO | source | high | authored-source | `2 related paths` | 173 review lines under d1/main | - |
| GQ1-CHUNK-0291 | [ ] TODO | source | high | authored-source | `3 related paths` | 571 review lines under d1/main | - |
| GQ1-CHUNK-0292 | [ ] TODO | source | high | authored-source | `3 related paths` | 738 review lines under d1/main | - |
| GQ1-CHUNK-0293 | [ ] TODO | source | high | authored-source | `4 related paths` | 656 review lines under d1/main | - |
| GQ1-CHUNK-0294 | [ ] TODO | source | high | authored-source | `4 related paths` | 697 review lines under d1/main | - |
| GQ1-CHUNK-0295 | [ ] TODO | source | high | authored-source | `4 related paths` | 497 review lines under d1/main | - |
| GQ1-CHUNK-0296 | [ ] TODO | source | high | authored-source | `7 related paths` | 648 review lines under d1/main | - |
| GQ1-CHUNK-0297 | [ ] TODO | source | high | authored-source | `7 related paths` | 340 review lines under d1/main | - |
| GQ1-CHUNK-0298 | [ ] TODO | source | high | authored-source | `8 related paths` | 439 review lines under d1/main | - |
| GQ1-CHUNK-0299 | [ ] TODO | source | high | authored-source | `8 related paths` | 732 review lines under d1/main | - |
| GQ1-CHUNK-0300 | [ ] TODO | source | high | authored-source | `d1/main/input_demo_hooks.c` | L1-L750 | - |
| GQ1-CHUNK-0301 | [ ] TODO | source | high | authored-source | `d1/main/net_udp.c` | diff hunks 1-134, new L13-L3802 | - |
| GQ1-CHUNK-0302 | [ ] TODO | source | high | authored-source | `d1/main/net_udp.c` | diff hunks 135-278, new L3805-L7819 | - |
| GQ1-CHUNK-0303 | [ ] TODO | source | high | authored-source | `d1/main/newmenu.c` | diff hunks 1-81, new L50-L1979 | - |
| GQ1-CHUNK-0304 | [ ] TODO | source | high | authored-source | `d1/main/newmenu.c` | diff hunks 82-128, new L1981-L3321 | - |
| GQ1-CHUNK-0305 | [ ] TODO | source | high | authored-source | `d1/main/state.c` | diff hunks 12-12, new L143-L1185 | - |
| GQ1-CHUNK-0306 | [ ] TODO | source | high | authored-source | `d1/main/state.c` | diff hunks 13-93, new L1265-L2919 | - |
| GQ1-CHUNK-0307 | [ ] TODO | source | high | authored-source | `d1/maths/rand.c` | diff hunks 1-11, new L4-L211 | - |
| GQ1-CHUNK-0308 | [ ] TODO | source | high | authored-source | `4 related paths` | 164 review lines under d1/misc | - |
| GQ1-CHUNK-0309 | [ ] TODO | source | high | authored-source | `d1/texmap/texmapl.h` | diff hunks 1-2, new L1-L152 | - |
| GQ1-CHUNK-0310 | [ ] TODO | source | high | authored-source | `4 related paths` | 25 review lines under d1/xmodel | - |
| GQ1-CHUNK-0311 | [ ] TODO | source | high | authored-source | `6 related paths` | 204 review lines under d2/2d | - |
| GQ1-CHUNK-0312 | [ ] TODO | source | high | authored-source | `d2/3d/interp.c` | diff hunks 1-19, new L21-L693 | - |
| GQ1-CHUNK-0313 | [ ] TODO | source | high | authored-source | `10 related paths` | 531 review lines under d2/arch | - |
| GQ1-CHUNK-0314 | [ ] TODO | source | high | authored-source | `3 related paths` | 708 review lines under d2/arch | - |
| GQ1-CHUNK-0315 | [ ] TODO | source | high | authored-source | `8 related paths` | 146 review lines under d2/arch | - |
| GQ1-CHUNK-0316 | [ ] TODO | source | high | authored-source | `d2/arch/ogl/ogl.c` | diff hunks 1-63, new L13-L1629 | - |
| GQ1-CHUNK-0317 | [ ] TODO | source | high | authored-source | `d2/arch/ogl/ogl.c` | diff hunks 64-104, new L1631-L2987 | - |
| GQ1-CHUNK-0318 | [ ] TODO | source | high | authored-source | `16 related paths` | 179 review lines under d2/include | - |
| GQ1-CHUNK-0319 | [ ] TODO | source | high | authored-source | `4 related paths` | 45 review lines under d2/include | - |
| GQ1-CHUNK-0320 | [ ] TODO | source | high | authored-source | `d2/libmve/mveplay.c` | diff hunks 1-16, new L237-L984 | - |
| GQ1-CHUNK-0321 | [ ] TODO | source | high | authored-source | `2 related paths` | 518 review lines under d2/main | - |
| GQ1-CHUNK-0322 | [ ] TODO | source | high | authored-source | `2 related paths` | 639 review lines under d2/main | - |
| GQ1-CHUNK-0323 | [ ] TODO | source | high | authored-source | `2 related paths` | 53 review lines under d2/main | - |
| GQ1-CHUNK-0324 | [ ] TODO | source | high | authored-source | `2 related paths` | 542 review lines under d2/main | - |
| GQ1-CHUNK-0325 | [ ] TODO | source | high | authored-source | `2 related paths` | 722 review lines under d2/main | - |
| GQ1-CHUNK-0326 | [ ] TODO | source | high | authored-source | `3 related paths` | 323 review lines under d2/main | - |
| GQ1-CHUNK-0327 | [ ] TODO | source | high | authored-source | `3 related paths` | 304 review lines under d2/main | - |
| GQ1-CHUNK-0328 | [ ] TODO | source | high | authored-source | `3 related paths` | 740 review lines under d2/main | - |
| GQ1-CHUNK-0329 | [ ] TODO | source | high | authored-source | `3 related paths` | 692 review lines under d2/main | - |
| GQ1-CHUNK-0330 | [ ] TODO | source | high | authored-source | `3 related paths` | 487 review lines under d2/main | - |
| GQ1-CHUNK-0331 | [ ] TODO | source | high | authored-source | `4 related paths` | 709 review lines under d2/main | - |
| GQ1-CHUNK-0332 | [ ] TODO | source | high | authored-source | `4 related paths` | 632 review lines under d2/main | - |
| GQ1-CHUNK-0333 | [ ] TODO | source | high | authored-source | `4 related paths` | 701 review lines under d2/main | - |
| GQ1-CHUNK-0334 | [ ] TODO | source | high | authored-source | `4 related paths` | 178 review lines under d2/main | - |
| GQ1-CHUNK-0335 | [ ] TODO | source | high | authored-source | `5 related paths` | 517 review lines under d2/main | - |
| GQ1-CHUNK-0336 | [ ] TODO | source | high | authored-source | `5 related paths` | 749 review lines under d2/main | - |
| GQ1-CHUNK-0337 | [ ] TODO | source | high | authored-source | `5 related paths` | 668 review lines under d2/main | - |
| GQ1-CHUNK-0338 | [ ] TODO | source | high | authored-source | `6 related paths` | 265 review lines under d2/main | - |
| GQ1-CHUNK-0339 | [ ] TODO | source | high | authored-source | `6 related paths` | 402 review lines under d2/main | - |
| GQ1-CHUNK-0340 | [ ] TODO | source | high | authored-source | `6 related paths` | 265 review lines under d2/main | - |
| GQ1-CHUNK-0341 | [ ] TODO | source | high | authored-source | `7 related paths` | 613 review lines under d2/main | - |
| GQ1-CHUNK-0342 | [ ] TODO | source | high | authored-source | `8 related paths` | 316 review lines under d2/main | - |
| GQ1-CHUNK-0343 | [ ] TODO | source | high | authored-source | `8 related paths` | 690 review lines under d2/main | - |
| GQ1-CHUNK-0344 | [ ] TODO | source | high | authored-source | `8 related paths` | 666 review lines under d2/main | - |
| GQ1-CHUNK-0345 | [ ] TODO | source | high | authored-source | `9 related paths` | 627 review lines under d2/main | - |
| GQ1-CHUNK-0346 | [ ] TODO | source | high | authored-source | `d2/main/ai.c` | diff hunks 1-89, new L53-L2252 | - |
| GQ1-CHUNK-0347 | [ ] TODO | source | high | authored-source | `d2/main/d1_custom.c` | L1-L750 | - |
| GQ1-CHUNK-0348 | [ ] TODO | source | high | authored-source | `d2/main/d1_in_d2.c` | L1-L750 | - |
| GQ1-CHUNK-0349 | [ ] TODO | source | high | authored-source | `d2/main/d1_in_d2.c` | L751-L1500 | - |
| GQ1-CHUNK-0350 | [ ] TODO | source | high | authored-source | `d2/main/d1_in_d2.c` | L1501-L2239 | - |
| GQ1-CHUNK-0351 | [ ] TODO | source | high | authored-source | `d2/main/d1_save_translate.c` | L1-L750 | - |
| GQ1-CHUNK-0352 | [ ] TODO | source | high | authored-source | `d2/main/d1_save_translate.c` | L751-L1500 | - |
| GQ1-CHUNK-0353 | [ ] TODO | source | high | authored-source | `d2/main/dxa_metadata_patch.cpp` | L1-L750 | - |
| GQ1-CHUNK-0354 | [ ] TODO | source | high | authored-source | `d2/main/escort.c` | diff hunks 5-5, new L131-L1438 | - |
| GQ1-CHUNK-0355 | [ ] TODO | source | high | authored-source | `d2/main/escort.c` | diff hunks 6-39, new L1440-L2614 | - |
| GQ1-CHUNK-0356 | [ ] TODO | source | high | authored-source | `d2/main/escort.c` | diff hunks 40-116, new L2627-L4083 | - |
| GQ1-CHUNK-0357 | [ ] TODO | source | high | authored-source | `d2/main/input_demo_energy_trace.h` | L1-L64 | - |
| GQ1-CHUNK-0358 | [ ] TODO | source | high | authored-source | `d2/main/input_demo_hooks.c` | L1-L750 | - |
| GQ1-CHUNK-0359 | [ ] TODO | source | high | authored-source | `d2/main/input_demo_hooks.c` | L751-L1500 | - |
| GQ1-CHUNK-0360 | [ ] TODO | source | high | authored-source | `d2/main/input_demo_hooks.c` | L1501-L2250 | - |
| GQ1-CHUNK-0361 | [ ] TODO | source | high | authored-source | `d2/main/input_demo_hooks.c` | L2251-L3000 | - |
| GQ1-CHUNK-0362 | [ ] TODO | source | high | authored-source | `d2/main/input_demo_hooks.c` | L3001-L3750 | - |
| GQ1-CHUNK-0363 | [ ] TODO | source | high | authored-source | `d2/main/input_demo_hooks.c` | L3751-L4500 | - |
| GQ1-CHUNK-0364 | [ ] TODO | source | high | authored-source | `d2/main/input_demo_hooks.c` | L4501-L5250 | - |
| GQ1-CHUNK-0365 | [ ] TODO | source | high | authored-source | `d2/main/input_demo_hooks.c` | L5251-L6000 | - |
| GQ1-CHUNK-0366 | [ ] TODO | source | high | authored-source | `d2/main/input_demo_hooks.c` | L6001-L6750 | - |
| GQ1-CHUNK-0367 | [ ] TODO | source | high | authored-source | `d2/main/laser.c` | diff hunks 1-89, new L46-L2851 | - |
| GQ1-CHUNK-0368 | [ ] TODO | source | high | authored-source | `d2/main/multi.c` | diff hunks 1-81, new L67-L7642 | - |
| GQ1-CHUNK-0369 | [ ] TODO | source | high | authored-source | `d2/main/net_udp.c` | diff hunks 1-134, new L17-L3767 | - |
| GQ1-CHUNK-0370 | [ ] TODO | source | high | authored-source | `d2/main/net_udp.c` | diff hunks 135-290, new L3775-L7529 | - |
| GQ1-CHUNK-0371 | [ ] TODO | source | high | authored-source | `d2/main/newdemo.c` | diff hunks 1-60, new L28-L3938 | - |
| GQ1-CHUNK-0372 | [ ] TODO | source | high | authored-source | `d2/main/newmenu.c` | diff hunks 1-81, new L51-L1967 | - |
| GQ1-CHUNK-0373 | [ ] TODO | source | high | authored-source | `d2/main/newmenu.c` | diff hunks 82-128, new L1969-L3287 | - |
| GQ1-CHUNK-0374 | [ ] TODO | source | high | authored-source | `d2/main/object.c` | diff hunks 1-75, new L70-L3121 | - |
| GQ1-CHUNK-0375 | [ ] TODO | source | high | authored-source | `d2/main/state.c` | diff hunks 13-13, new L158-L1272 | - |
| GQ1-CHUNK-0376 | [ ] TODO | source | high | authored-source | `d2/main/state.c` | diff hunks 14-107, new L1280-L3182 | - |
| GQ1-CHUNK-0377 | [ ] TODO | source | high | authored-source | `d2/maths/rand.c` | diff hunks 1-14, new L8-L242 | - |
| GQ1-CHUNK-0378 | [ ] TODO | source | high | authored-source | `4 related paths` | 174 review lines under d2/misc | - |
| GQ1-CHUNK-0379 | [ ] TODO | source | high | authored-source | `d2/texmap/texmapl.h` | diff hunks 1-2, new L1-L93 | - |
| GQ1-CHUNK-0380 | [ ] TODO | source | high | authored-source | `4 related paths` | 25 review lines under d2/xmodel | - |
| GQ1-CHUNK-0381 | [ ] TODO | source | high | authored-source | `server/Cargo.toml` | L1-L38 | - |
| GQ1-CHUNK-0382 | [ ] TODO | source | high | build-script | `android/app/src/main/cpp/CMakeLists.txt` | L1-L750 | - |
| GQ1-CHUNK-0383 | [ ] TODO | source | high | build-script | `android/app/src/main/cpp/CMakeLists.txt` | L751-L897 | - |
| GQ1-CHUNK-0384 | [ ] TODO | source | high | build-script | `d1/CMakeLists.txt` | diff hunks 1-13, new L1-L151 | - |
| GQ1-CHUNK-0385 | [ ] TODO | source | high | build-script | `d1/2d/CMakeLists.txt` | diff hunks 1-3, new L1-L26 | - |
| GQ1-CHUNK-0386 | [ ] TODO | source | high | build-script | `d1/3d/CMakeLists.txt` | diff hunks 1-3, new L1-L22 | - |
| GQ1-CHUNK-0387 | [ ] TODO | source | high | build-script | `5 related paths` | 43 review lines under d1/arch | - |
| GQ1-CHUNK-0388 | [ ] TODO | source | high | build-script | `d1/editor/CMakeLists.txt` | diff hunks 1-2, new L1-L42 | - |
| GQ1-CHUNK-0389 | [ ] TODO | source | high | build-script | `d1/iff/CMakeLists.txt` | diff hunks 1-2, new L1-L9 | - |
| GQ1-CHUNK-0390 | [ ] TODO | source | high | build-script | `d1/main/CMakeLists.txt` | diff hunks 1-16, new L1-L339 | - |
| GQ1-CHUNK-0391 | [ ] TODO | source | high | build-script | `d1/maths/CMakeLists.txt` | diff hunks 1-4, new L1-L278 | - |
| GQ1-CHUNK-0392 | [ ] TODO | source | high | build-script | `d1/mem/CMakeLists.txt` | diff hunks 1-2, new L1-L9 | - |
| GQ1-CHUNK-0393 | [ ] TODO | source | high | build-script | `d1/misc/CMakeLists.txt` | diff hunks 1-3, new L1-L31 | - |
| GQ1-CHUNK-0394 | [ ] TODO | source | high | build-script | `d1/texmap/CMakeLists.txt` | diff hunks 1-6, new L1-L30 | - |
| GQ1-CHUNK-0395 | [ ] TODO | source | high | build-script | `d1/ui/CMakeLists.txt` | diff hunks 1-2, new L1-L28 | - |
| GQ1-CHUNK-0396 | [ ] TODO | source | high | build-script | `d1/xmodel/CMakeLists.txt` | diff hunks 1-2, new L1-L12 | - |
| GQ1-CHUNK-0397 | [ ] TODO | source | high | build-script | `d2/CMakeLists.txt` | diff hunks 1-13, new L6-L156 | - |
| GQ1-CHUNK-0398 | [ ] TODO | source | high | build-script | `d2/2d/CMakeLists.txt` | diff hunks 1-3, new L1-L26 | - |
| GQ1-CHUNK-0399 | [ ] TODO | source | high | build-script | `d2/3d/CMakeLists.txt` | diff hunks 1-3, new L1-L22 | - |
| GQ1-CHUNK-0400 | [ ] TODO | source | high | build-script | `5 related paths` | 39 review lines under d2/arch | - |
| GQ1-CHUNK-0401 | [ ] TODO | source | high | build-script | `d2/editor/CMakeLists.txt` | diff hunks 1-2, new L1-L42 | - |
| GQ1-CHUNK-0402 | [ ] TODO | source | high | build-script | `d2/iff/CMakeLists.txt` | diff hunks 1-2, new L1-L9 | - |
| GQ1-CHUNK-0403 | [ ] TODO | source | high | build-script | `d2/libmve/CMakeLists.txt` | diff hunks 1-5, new L1-L27 | - |
| GQ1-CHUNK-0404 | [ ] TODO | source | high | build-script | `d2/main/CMakeLists.txt` | diff hunks 1-19, new L1-L414 | - |
| GQ1-CHUNK-0405 | [ ] TODO | source | high | build-script | `d2/maths/CMakeLists.txt` | diff hunks 1-4, new L1-L313 | - |
| GQ1-CHUNK-0406 | [ ] TODO | source | high | build-script | `d2/mem/CMakeLists.txt` | diff hunks 1-2, new L1-L9 | - |
| GQ1-CHUNK-0407 | [ ] TODO | source | high | build-script | `d2/misc/CMakeLists.txt` | diff hunks 1-3, new L1-L33 | - |
| GQ1-CHUNK-0408 | [ ] TODO | source | high | build-script | `d2/texmap/CMakeLists.txt` | diff hunks 1-6, new L1-L30 | - |
| GQ1-CHUNK-0409 | [ ] TODO | source | high | build-script | `d2/ui/CMakeLists.txt` | diff hunks 1-2, new L1-L28 | - |
| GQ1-CHUNK-0410 | [ ] TODO | source | high | build-script | `d2/xmodel/CMakeLists.txt` | diff hunks 1-2, new L1-L12 | - |
| GQ1-CHUNK-0411 | [ ] TODO | source | high | build-script | `9 related paths` | 507 review lines under server | - |
| GQ1-CHUNK-0412 | [ ] TODO | source | high | documentation | `d1/INSTALL.txt` | diff hunks 1-1, new L93-L93 | - |
| GQ1-CHUNK-0413 | [ ] TODO | source | high | documentation | `d2/INSTALL.txt` | diff hunks 1-1, new L107-L107 | - |
| GQ1-CHUNK-0414 | [ ] TODO | source | high | documentation | `server/rust_version.txt` | L1-L7 | - |
| GQ1-CHUNK-0415 | [ ] TODO | source | high | test-source | `6 related paths` | 597 review lines under android/app/src/main/cpp | - |
| GQ1-CHUNK-0416 | [ ] TODO | source | high | test-source | `9 related paths` | 745 review lines under android/app/src/main/cpp | - |
| GQ1-CHUNK-0417 | [ ] TODO | source | high | test-source | `android/app/src/main/cpp/shared/test_secret_area_scan_budget.c` | L1-L279 | - |
| GQ1-CHUNK-0418 | [ ] TODO | source | high | test-source | `4 related paths` | 278 review lines under android/app/src/test/java | - |
| GQ1-CHUNK-0419 | [ ] TODO | source | high | test-source | `4 related paths` | 416 review lines under android/tests | - |
| GQ1-CHUNK-0420 | [ ] TODO | source | high | test-source | `server/tests/integration.rs` | L1-L750 | - |
| GQ1-CHUNK-0421 | [ ] TODO | source | high | test-source | `server/tests/integration.rs` | L751-L1500 | - |
| GQ1-CHUNK-0422 | [ ] TODO | source | high | test-source | `server/tests/integration.rs` | L1501-L2250 | - |
| GQ1-CHUNK-0423 | [ ] TODO | source | high | test-source | `server/tests/integration.rs` | L2251-L2901 | - |
| GQ1-CHUNK-0424 | [ ] TODO | source | high | test-source | `server/tests/nat_sim_tests.rs` | L1-L502 | - |
| GQ1-CHUNK-0425 | [ ] TODO | source | medium | authored-config | `3 related paths` | 124 review lines under .vscode | - |
| GQ1-CHUNK-0426 | [ ] TODO | source | medium | authored-config | `2 related paths` | 33 review lines under android | - |
| GQ1-CHUNK-0427 | [ ] TODO | source | medium | authored-config | `6 related paths` | 619 review lines under android/app/src/main/assets | - |
| GQ1-CHUNK-0428 | [ ] TODO | source | medium | authored-config | `android/app/src/main/assets/known_albums.json5` | L1-L750 | - |
| GQ1-CHUNK-0429 | [ ] TODO | source | medium | authored-config | `android/app/src/main/assets/known_albums.json5` | L751-L796 | - |
| GQ1-CHUNK-0430 | [ ] TODO | source | medium | authored-config | `android/app/src/main/assets/known_discs.json5` | L1-L602 | - |
| GQ1-CHUNK-0431 | [ ] TODO | source | medium | authored-config | `android/app/src/main/assets/known_versions.json5` | L1-L452 | - |
| GQ1-CHUNK-0432 | [ ] TODO | source | medium | authored-config | `android/get_deps/tool_versions.conf` | L1-L196 | - |
| GQ1-CHUNK-0433 | [ ] TODO | source | medium | authored-config | `10 related paths` | 687 review lines under game_data/CD images | - |
| GQ1-CHUNK-0434 | [ ] TODO | source | medium | authored-config | `16 related paths` | 519 review lines under game_data/CD images | - |
| GQ1-CHUNK-0435 | [ ] TODO | source | medium | authored-config | `8 related paths` | 725 review lines under game_data/CD images | - |
| GQ1-CHUNK-0436 | [ ] TODO | source | medium | authored-config | `game_data/combined launches/Descent II plus Vertigo (USA)/combined_launch.json5` | L1-L14 | - |
| GQ1-CHUNK-0437 | [ ] TODO | source | medium | authored-config | `game_data/demo installers/mac_stuffit_oracles.json` | L1-L85 | - |
| GQ1-CHUNK-0438 | [ ] TODO | source | medium | authored-config | `4 related paths` | 114 review lines under game_data/gog installers | - |
| GQ1-CHUNK-0439 | [ ] TODO | source | medium | authored-config | `game_data/mission_files/cd_level_metadata_sources.json5` | L1-L61 | - |
| GQ1-CHUNK-0440 | [ ] TODO | source | medium | authored-config | `16 related paths` | 430 review lines under game_data/music | - |
| GQ1-CHUNK-0441 | [ ] TODO | source | medium | authored-config | `7 related paths` | 129 review lines under game_data/music | - |
| GQ1-CHUNK-0442 | [ ] TODO | source | medium | authored-source | `android/gradle.properties` | L1-L7 | - |
| GQ1-CHUNK-0443 | [ ] TODO | source | medium | authored-source | `android/app/src/debug/AndroidManifest.xml` | L1-L9 | - |
| GQ1-CHUNK-0444 | [ ] TODO | source | medium | authored-source | `android/app/src/debug/java/com/dxxredux/app/SafTestProvider.kt` | L1-L111 | - |
| GQ1-CHUNK-0445 | [ ] TODO | source | medium | authored-source | `android/app/src/main/AndroidManifest.xml` | L1-L113 | - |
| GQ1-CHUNK-0446 | [ ] TODO | source | medium | authored-source | `2 related paths` | 224 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0447 | [ ] TODO | source | medium | authored-source | `2 related paths` | 623 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0448 | [ ] TODO | source | medium | authored-source | `2 related paths` | 249 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0449 | [ ] TODO | source | medium | authored-source | `2 related paths` | 586 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0450 | [ ] TODO | source | medium | authored-source | `2 related paths` | 735 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0451 | [ ] TODO | source | medium | authored-source | `2 related paths` | 544 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0452 | [ ] TODO | source | medium | authored-source | `2 related paths` | 600 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0453 | [ ] TODO | source | medium | authored-source | `2 related paths` | 461 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0454 | [ ] TODO | source | medium | authored-source | `2 related paths` | 633 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0455 | [ ] TODO | source | medium | authored-source | `2 related paths` | 617 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0456 | [ ] TODO | source | medium | authored-source | `2 related paths` | 665 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0457 | [ ] TODO | source | medium | authored-source | `3 related paths` | 550 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0458 | [ ] TODO | source | medium | authored-source | `3 related paths` | 389 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0459 | [ ] TODO | source | medium | authored-source | `3 related paths` | 553 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0460 | [ ] TODO | source | medium | authored-source | `3 related paths` | 582 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0461 | [ ] TODO | source | medium | authored-source | `3 related paths` | 562 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0462 | [ ] TODO | source | medium | authored-source | `3 related paths` | 366 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0463 | [ ] TODO | source | medium | authored-source | `3 related paths` | 684 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0464 | [ ] TODO | source | medium | authored-source | `3 related paths` | 163 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0465 | [ ] TODO | source | medium | authored-source | `3 related paths` | 590 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0466 | [ ] TODO | source | medium | authored-source | `3 related paths` | 671 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0467 | [ ] TODO | source | medium | authored-source | `3 related paths` | 541 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0468 | [ ] TODO | source | medium | authored-source | `3 related paths` | 479 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0469 | [ ] TODO | source | medium | authored-source | `3 related paths` | 264 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0470 | [ ] TODO | source | medium | authored-source | `3 related paths` | 707 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0471 | [ ] TODO | source | medium | authored-source | `4 related paths` | 681 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0472 | [ ] TODO | source | medium | authored-source | `4 related paths` | 727 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0473 | [ ] TODO | source | medium | authored-source | `4 related paths` | 731 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0474 | [ ] TODO | source | medium | authored-source | `4 related paths` | 327 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0475 | [ ] TODO | source | medium | authored-source | `4 related paths` | 674 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0476 | [ ] TODO | source | medium | authored-source | `4 related paths` | 655 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0477 | [ ] TODO | source | medium | authored-source | `4 related paths` | 701 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0478 | [ ] TODO | source | medium | authored-source | `4 related paths` | 646 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0479 | [ ] TODO | source | medium | authored-source | `4 related paths` | 671 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0480 | [ ] TODO | source | medium | authored-source | `4 related paths` | 420 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0481 | [ ] TODO | source | medium | authored-source | `5 related paths` | 628 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0482 | [ ] TODO | source | medium | authored-source | `5 related paths` | 718 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0483 | [ ] TODO | source | medium | authored-source | `5 related paths` | 697 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0484 | [ ] TODO | source | medium | authored-source | `6 related paths` | 736 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0485 | [ ] TODO | source | medium | authored-source | `8 related paths` | 731 review lines under android/app/src/main/java | - |
| GQ1-CHUNK-0486 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/AdvancedSettingsPage.kt` | L1-L750 | - |
| GQ1-CHUNK-0487 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/AdvancedSettingsPage.kt` | L751-L1500 | - |
| GQ1-CHUNK-0488 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/AdvancedSettingsPage.kt` | L1501-L2250 | - |
| GQ1-CHUNK-0489 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/AudioSourceManager.kt` | L1-L643 | - |
| GQ1-CHUNK-0490 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/AutomapTouchPolicy.kt` | L1-L129 | - |
| GQ1-CHUNK-0491 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/AutoselectEditorPage.kt` | L1-L750 | - |
| GQ1-CHUNK-0492 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/ConfigImportExport.kt` | L1-L741 | - |
| GQ1-CHUNK-0493 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/ControllerConfigModel.kt` | L1-L320 | - |
| GQ1-CHUNK-0494 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt` | L1-L750 | - |
| GQ1-CHUNK-0495 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt` | L751-L1500 | - |
| GQ1-CHUNK-0496 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt` | L1501-L2250 | - |
| GQ1-CHUNK-0497 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/ControllerConfigPage.kt` | L2251-L2821 | - |
| GQ1-CHUNK-0498 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/EnginePreferencesPage.kt` | L1-L673 | - |
| GQ1-CHUNK-0499 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/FileSetManager.kt` | L1-L616 | - |
| GQ1-CHUNK-0500 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/GameFileFormats.kt` | L1-L587 | - |
| GQ1-CHUNK-0501 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/GameFileMetadata.kt` | L1-L603 | - |
| GQ1-CHUNK-0502 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/GraphicsSettingsPage.kt` | L1-L685 | - |
| GQ1-CHUNK-0503 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/GyroInputManager.kt` | L1-L184 | - |
| GQ1-CHUNK-0504 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/HumanReadableConfig.kt` | L1-L750 | - |
| GQ1-CHUNK-0505 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/LauncherScriptExecutor.kt` | L1-L750 | - |
| GQ1-CHUNK-0506 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/LauncherScriptExecutor.kt` | L751-L1235 | - |
| GQ1-CHUNK-0507 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/LevelMetadata.kt` | L1-L750 | - |
| GQ1-CHUNK-0508 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/LevelMetadata.kt` | L751-L1500 | - |
| GQ1-CHUNK-0509 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/LevelMetadata.kt` | L1501-L1889 | - |
| GQ1-CHUNK-0510 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/lobby/LobbyService.kt` | L1-L750 | - |
| GQ1-CHUNK-0511 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/lobby/LobbyService.kt` | L751-L1500 | - |
| GQ1-CHUNK-0512 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/lobby/LobbyService.kt` | L1501-L1547 | - |
| GQ1-CHUNK-0513 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/MainActivity.kt` | L1-L750 | - |
| GQ1-CHUNK-0514 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/MainActivity.kt` | L751-L1500 | - |
| GQ1-CHUNK-0515 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/MainActivity.kt` | L1501-L2250 | - |
| GQ1-CHUNK-0516 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/MainActivity.kt` | L2251-L3000 | - |
| GQ1-CHUNK-0517 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/MainActivity.kt` | L3001-L3750 | - |
| GQ1-CHUNK-0518 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/MissionAssetCatalog.kt` | L1-L135 | - |
| GQ1-CHUNK-0519 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/ModManager.kt` | L1-L750 | - |
| GQ1-CHUNK-0520 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/ModManager.kt` | L751-L1500 | - |
| GQ1-CHUNK-0521 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/multiplayer/CreateGameDialog.kt` | L1-L590 | - |
| GQ1-CHUNK-0522 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/multiplayer/LanDiscoveryTab.kt` | L1-L750 | - |
| GQ1-CHUNK-0523 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/multiplayer/LocalhostProxy.kt` | L1-L559 | - |
| GQ1-CHUNK-0524 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/multiplayer/MatchmakingService.kt` | L1-L750 | - |
| GQ1-CHUNK-0525 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/multiplayer/MatchmakingService.kt` | L751-L1293 | - |
| GQ1-CHUNK-0526 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/multiplayer/MultiplayerResumePrefs.kt` | L1-L407 | - |
| GQ1-CHUNK-0527 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/multiplayer/MultiplayerScreen.kt` | L1-L750 | - |
| GQ1-CHUNK-0528 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/multiplayer/MultiplayerScreen.kt` | L751-L1500 | - |
| GQ1-CHUNK-0529 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/multiplayer/MultiplayerScreen.kt` | L1501-L1501 | - |
| GQ1-CHUNK-0530 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/multiplayer/MultiplayerStatsOverlay.kt` | L1-L626 | - |
| GQ1-CHUNK-0531 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/MusicControlPanel.kt` | L1-L750 | - |
| GQ1-CHUNK-0532 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/MusicPickerPage.kt` | L1-L750 | - |
| GQ1-CHUNK-0533 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/MusicPickerPage.kt` | L751-L1500 | - |
| GQ1-CHUNK-0534 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/ScrollStripMath.kt` | L1-L149 | - |
| GQ1-CHUNK-0535 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt` | L1-L750 | - |
| GQ1-CHUNK-0536 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt` | L751-L1500 | - |
| GQ1-CHUNK-0537 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt` | L1501-L2250 | - |
| GQ1-CHUNK-0538 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt` | L2251-L3000 | - |
| GQ1-CHUNK-0539 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt` | L3001-L3750 | - |
| GQ1-CHUNK-0540 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt` | L3751-L4500 | - |
| GQ1-CHUNK-0541 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt` | L4501-L5215 | - |
| GQ1-CHUNK-0542 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupAutomationApi.kt` | L1-L750 | - |
| GQ1-CHUNK-0543 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupDialogs.kt` | L1-L750 | - |
| GQ1-CHUNK-0544 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupDialogs.kt` | L751-L1500 | - |
| GQ1-CHUNK-0545 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupDialogs.kt` | L1501-L1924 | - |
| GQ1-CHUNK-0546 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupDiscImport.kt` | L1-L750 | - |
| GQ1-CHUNK-0547 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupDiscImport.kt` | L751-L1076 | - |
| GQ1-CHUNK-0548 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupFileImport.kt` | L1-L750 | - |
| GQ1-CHUNK-0549 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupResumePanel.kt` | L1-L457 | - |
| GQ1-CHUNK-0550 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupSaveExplorer.kt` | L1-L750 | - |
| GQ1-CHUNK-0551 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupSaveExplorer.kt` | L751-L1176 | - |
| GQ1-CHUNK-0552 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupSections.kt` | L1-L750 | - |
| GQ1-CHUNK-0553 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupSections.kt` | L751-L1500 | - |
| GQ1-CHUNK-0554 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupSections.kt` | L1501-L2250 | - |
| GQ1-CHUNK-0555 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupSections.kt` | L2251-L3000 | - |
| GQ1-CHUNK-0556 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/SetupSections.kt` | L3001-L3634 | - |
| GQ1-CHUNK-0557 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/TouchBindings.kt` | L1-L498 | - |
| GQ1-CHUNK-0558 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/TouchControl.kt` | L1-L750 | - |
| GQ1-CHUNK-0559 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/TouchControl.kt` | L751-L1259 | - |
| GQ1-CHUNK-0560 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/TouchEditorPage.kt` | L1-L750 | - |
| GQ1-CHUNK-0561 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/TouchEditorPage.kt` | L751-L1500 | - |
| GQ1-CHUNK-0562 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/TouchEditorPage.kt` | L1501-L2250 | - |
| GQ1-CHUNK-0563 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/TouchEditorPage.kt` | L2251-L3000 | - |
| GQ1-CHUNK-0564 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/TouchEditorPage.kt` | L3001-L3750 | - |
| GQ1-CHUNK-0565 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt` | L1-L750 | - |
| GQ1-CHUNK-0566 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt` | L751-L1500 | - |
| GQ1-CHUNK-0567 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt` | L1501-L2250 | - |
| GQ1-CHUNK-0568 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt` | L2251-L3000 | - |
| GQ1-CHUNK-0569 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt` | L3001-L3750 | - |
| GQ1-CHUNK-0570 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt` | L3751-L4500 | - |
| GQ1-CHUNK-0571 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt` | L4501-L5250 | - |
| GQ1-CHUNK-0572 | [ ] TODO | source | medium | authored-source | `android/app/src/main/java/com/dxxredux/app/VideoInfoOverlay.kt` | L1-L750 | - |
| GQ1-CHUNK-0573 | [ ] TODO | source | medium | authored-source | `2 related paths` | 13 review lines under android/app/src/main/res | - |
| GQ1-CHUNK-0574 | [ ] TODO | source | medium | authored-source | `2 related paths` | 489 review lines under android/tools | - |
| GQ1-CHUNK-0575 | [ ] TODO | source | medium | build-script | `6 related paths` | 469 review lines under [root] | - |
| GQ1-CHUNK-0576 | [ ] TODO | source | medium | build-script | `run-windows-build.ps1` | L1-L502 | - |
| GQ1-CHUNK-0577 | [ ] TODO | source | medium | build-script | `3 related paths` | 367 review lines under android | - |
| GQ1-CHUNK-0578 | [ ] TODO | source | medium | build-script | `3 related paths` | 528 review lines under android | - |
| GQ1-CHUNK-0579 | [ ] TODO | source | medium | build-script | `5 related paths` | 655 review lines under android | - |
| GQ1-CHUNK-0580 | [ ] TODO | source | medium | build-script | `android/run_all_tests.ps1` | L1-L750 | - |
| GQ1-CHUNK-0581 | [ ] TODO | source | medium | build-script | `android/run_all_tests.ps1` | L751-L1500 | - |
| GQ1-CHUNK-0582 | [ ] TODO | source | medium | build-script | `android/run_all_tests.ps1` | L1501-L1948 | - |
| GQ1-CHUNK-0583 | [ ] TODO | source | medium | build-script | `android/run_quick_tests.ps1` | L1-L466 | - |
| GQ1-CHUNK-0584 | [ ] TODO | source | medium | build-script | `android/run-code-quality.ps1` | L1-L503 | - |
| GQ1-CHUNK-0585 | [ ] TODO | source | medium | build-script | `android/app/build.gradle` | L1-L476 | - |
| GQ1-CHUNK-0586 | [ ] TODO | source | medium | build-script | `2 related paths` | 210 review lines under android/docker | - |
| GQ1-CHUNK-0587 | [ ] TODO | source | medium | build-script | `3 related paths` | 624 review lines under android/get_deps | - |
| GQ1-CHUNK-0588 | [ ] TODO | source | medium | build-script | `7 related paths` | 689 review lines under android/get_deps | - |
| GQ1-CHUNK-0589 | [ ] TODO | source | medium | build-script | `8 related paths` | 636 review lines under android/get_deps | - |
| GQ1-CHUNK-0590 | [ ] TODO | source | medium | build-script | `8 related paths` | 692 review lines under android/get_deps | - |
| GQ1-CHUNK-0591 | [ ] TODO | source | medium | build-script | `9 related paths` | 745 review lines under android/get_deps | - |
| GQ1-CHUNK-0592 | [ ] TODO | source | medium | build-script | `android/get_deps/check-updates.ps1` | L1-L750 | - |
| GQ1-CHUNK-0593 | [ ] TODO | source | medium | build-script | `android/get_deps/check-updates.ps1` | L751-L1500 | - |
| GQ1-CHUNK-0594 | [ ] TODO | source | medium | build-script | `2 related paths` | 284 review lines under android/helpers | - |
| GQ1-CHUNK-0595 | [ ] TODO | source | medium | build-script | `2 related paths` | 530 review lines under android/helpers | - |
| GQ1-CHUNK-0596 | [ ] TODO | source | medium | build-script | `3 related paths` | 533 review lines under android/helpers | - |
| GQ1-CHUNK-0597 | [ ] TODO | source | medium | build-script | `4 related paths` | 561 review lines under android/helpers | - |
| GQ1-CHUNK-0598 | [ ] TODO | source | medium | build-script | `5 related paths` | 720 review lines under android/helpers | - |
| GQ1-CHUNK-0599 | [ ] TODO | source | medium | build-script | `6 related paths` | 708 review lines under android/helpers | - |
| GQ1-CHUNK-0600 | [ ] TODO | source | medium | build-script | `6 related paths` | 746 review lines under android/helpers | - |
| GQ1-CHUNK-0601 | [ ] TODO | source | medium | build-script | `6 related paths` | 722 review lines under android/helpers | - |
| GQ1-CHUNK-0602 | [ ] TODO | source | medium | build-script | `7 related paths` | 733 review lines under android/helpers | - |
| GQ1-CHUNK-0603 | [ ] TODO | source | medium | build-script | `android/helpers/new_adversarial_review_ledger.ps1` | L1-L633 | - |
| GQ1-CHUNK-0604 | [ ] TODO | source | medium | build-script | `android/helpers/regenerate_all_mission_metadata_host.ps1` | L1-L750 | - |
| GQ1-CHUNK-0605 | [ ] TODO | source | medium | build-script | `android/regression_demos/.gitignore` | L1-L4 | - |
| GQ1-CHUNK-0606 | [ ] TODO | source | medium | build-script | `2 related paths` | 468 review lines under android/tools | - |
| GQ1-CHUNK-0607 | [ ] TODO | source | medium | build-script | `android/tools/decode_object_packets.ps1` | L1-L415 | - |
| GQ1-CHUNK-0608 | [ ] TODO | source | medium | build-script | `5 related paths` | 290 review lines under cmake | - |
| GQ1-CHUNK-0609 | [ ] TODO | source | medium | build-script | `2 related paths` | 737 review lines under game_data | - |
| GQ1-CHUNK-0610 | [ ] TODO | source | medium | build-script | `2 related paths` | 637 review lines under game_data | - |
| GQ1-CHUNK-0611 | [ ] TODO | source | medium | build-script | `2 related paths` | 618 review lines under game_data | - |
| GQ1-CHUNK-0612 | [ ] TODO | source | medium | build-script | `2 related paths` | 560 review lines under game_data | - |
| GQ1-CHUNK-0613 | [ ] TODO | source | medium | build-script | `3 related paths` | 571 review lines under game_data | - |
| GQ1-CHUNK-0614 | [ ] TODO | source | medium | build-script | `game_data/fingerprint_disc_tracks.ps1` | L1-L365 | - |
| GQ1-CHUNK-0615 | [ ] TODO | source | medium | build-script | `2 related paths` | 601 review lines under game_data/mods | - |
| GQ1-CHUNK-0616 | [ ] TODO | source | medium | build-script | `2 related paths` | 260 review lines under game_data/mods | - |
| GQ1-CHUNK-0617 | [ ] TODO | source | medium | build-script | `3 related paths` | 414 review lines under game_data/mods | - |
| GQ1-CHUNK-0618 | [ ] TODO | source | medium | build-script | `5 related paths` | 664 review lines under game_data/mods | - |
| GQ1-CHUNK-0619 | [ ] TODO | source | medium | build-script | `game_data/mods/d2x-xl/convert_d2xxl_sounds.ps1` | L1-L352 | - |
| GQ1-CHUNK-0620 | [ ] TODO | source | medium | build-script | `game_data/mods/d2x-xl/convert_d2xxl_textures.ps1` | L1-L750 | - |
| GQ1-CHUNK-0621 | [ ] TODO | source | medium | build-script | `game_data/mods/xfing/convert-xfing-minimal-dxa.ps1` | L1-L716 | - |
| GQ1-CHUNK-0622 | [ ] TODO | source | medium | build-script | `game_data/mods/xfing/d2x_sp/uud2sp_ham_patch_lib.ps1` | L1-L590 | - |
| GQ1-CHUNK-0623 | [ ] TODO | source | medium | build-script | `game_data/mods/xfing/verify-xfing-minimal-dxa.ps1` | L1-L271 | - |
| GQ1-CHUNK-0624 | [ ] TODO | source | medium | build-script | `game_data/mods/xfing/xfing_minimal_dxa_lib.ps1` | L1-L750 | - |
| GQ1-CHUNK-0625 | [ ] TODO | source | medium | build-script | `game_data/mods/xfing/xfing_minimal_dxa_lib.ps1` | L751-L1500 | - |
| GQ1-CHUNK-0626 | [ ] TODO | source | medium | build-script | `game_data/mods/xfing/xfing_minimal_dxa_lib.ps1` | L1501-L1522 | - |
| GQ1-CHUNK-0627 | [ ] TODO | source | low | documentation | `.github/copilot-instructions.md` | L1-L236 | - |
| GQ1-CHUNK-0628 | [ ] TODO | source | low | documentation | `3 related paths` | 27 review lines under [root] | - |
| GQ1-CHUNK-0629 | [ ] TODO | source | low | documentation | `AGENTS.md` | L1-L3 | - |
| GQ1-CHUNK-0630 | [ ] TODO | source | low | documentation | `d1_d2_ogl_diff.txt` | L1-L900 | - |
| GQ1-CHUNK-0631 | [ ] TODO | source | low | documentation | `3 related paths` | 275 review lines under android | - |
| GQ1-CHUNK-0632 | [ ] TODO | source | low | documentation | `android/app/src/main/assets/DISC_HASHING.md` | L1-L61 | - |
| GQ1-CHUNK-0633 | [ ] TODO | source | low | documentation | `android/get_deps/README-ubuntu.md` | L1-L25 | - |
| GQ1-CHUNK-0634 | [ ] TODO | source | low | documentation | `android/regression_demos/README.md` | L1-L6 | - |
| GQ1-CHUNK-0635 | [ ] TODO | source | low | documentation | `android/test_fixtures/SECRET_AREA_BASELINE.md` | L1-L9 | - |
| GQ1-CHUNK-0636 | [ ] TODO | source | low | documentation | `4 related paths` | 182 review lines under android/tests | - |
| GQ1-CHUNK-0637 | [ ] TODO | source | low | documentation | `2 related paths` | 265 review lines under game_data | - |
| GQ1-CHUNK-0638 | [ ] TODO | source | low | documentation | `game_data_to_copy_to_emulator/README.md` | L1-L22 | - |
| GQ1-CHUNK-0639 | [ ] TODO | source | low | documentation | `game_data/demo installers/README.md` | L1-L119 | - |
| GQ1-CHUNK-0640 | [ ] TODO | source | low | documentation | `3 related paths` | 66 review lines under game_data/mods | - |
| GQ1-CHUNK-0641 | [ ] TODO | source | low | test-source | `android/0_upload_to_test.ps1` | L1-L158 | - |
| GQ1-CHUNK-0642 | [ ] TODO | source | low | test-source | `11 related paths` | 771 review lines under android/app/src/test/java | - |
| GQ1-CHUNK-0643 | [ ] TODO | source | low | test-source | `4 related paths` | 761 review lines under android/app/src/test/java | - |
| GQ1-CHUNK-0644 | [ ] TODO | source | low | test-source | `5 related paths` | 872 review lines under android/app/src/test/java | - |
| GQ1-CHUNK-0645 | [ ] TODO | source | low | test-source | `5 related paths` | 581 review lines under android/app/src/test/java | - |
| GQ1-CHUNK-0646 | [ ] TODO | source | low | test-source | `5 related paths` | 842 review lines under android/app/src/test/java | - |
| GQ1-CHUNK-0647 | [ ] TODO | source | low | test-source | `6 related paths` | 778 review lines under android/app/src/test/java | - |
| GQ1-CHUNK-0648 | [ ] TODO | source | low | test-source | `6 related paths` | 504 review lines under android/app/src/test/java | - |
| GQ1-CHUNK-0649 | [ ] TODO | source | low | test-source | `7 related paths` | 865 review lines under android/app/src/test/java | - |
| GQ1-CHUNK-0650 | [ ] TODO | source | low | test-source | `7 related paths` | 774 review lines under android/app/src/test/java | - |
| GQ1-CHUNK-0651 | [ ] TODO | source | low | test-source | `7 related paths` | 852 review lines under android/app/src/test/java | - |
| GQ1-CHUNK-0652 | [ ] TODO | source | low | test-source | `7 related paths` | 820 review lines under android/app/src/test/java | - |
| GQ1-CHUNK-0653 | [ ] TODO | source | low | test-source | `7 related paths` | 831 review lines under android/app/src/test/java | - |
| GQ1-CHUNK-0654 | [ ] TODO | source | low | test-source | `8 related paths` | 855 review lines under android/app/src/test/java | - |
| GQ1-CHUNK-0655 | [ ] TODO | source | low | test-source | `9 related paths` | 855 review lines under android/app/src/test/java | - |
| GQ1-CHUNK-0656 | [ ] TODO | source | low | test-source | `9 related paths` | 857 review lines under android/app/src/test/java | - |
| GQ1-CHUNK-0657 | [ ] TODO | source | low | test-source | `android/app/src/test/java/com/dxxredux/app/WeaponWheelLabelTest.kt` | L1-L228 | - |
| GQ1-CHUNK-0658 | [ ] TODO | source | low | test-source | `16 related paths` | 854 review lines under android/game_scripts | - |
| GQ1-CHUNK-0659 | [ ] TODO | source | low | test-source | `4 related paths` | 792 review lines under android/game_scripts | - |
| GQ1-CHUNK-0660 | [ ] TODO | source | low | test-source | `5 related paths` | 561 review lines under android/game_scripts | - |
| GQ1-CHUNK-0661 | [ ] TODO | source | low | test-source | `6 related paths` | 533 review lines under android/game_scripts | - |
| GQ1-CHUNK-0662 | [ ] TODO | source | low | test-source | `7 related paths` | 776 review lines under android/game_scripts | - |
| GQ1-CHUNK-0663 | [ ] TODO | source | low | test-source | `7 related paths` | 875 review lines under android/game_scripts | - |
| GQ1-CHUNK-0664 | [ ] TODO | source | low | test-source | `8 related paths` | 874 review lines under android/game_scripts | - |
| GQ1-CHUNK-0665 | [ ] TODO | source | low | test-source | `2 related paths` | 529 review lines under android/helpers | - |
| GQ1-CHUNK-0666 | [ ] TODO | source | low | test-source | `2 related paths` | 610 review lines under android/helpers | - |
| GQ1-CHUNK-0667 | [ ] TODO | source | low | test-source | `android/helpers/test_helpers.ps1` | L1-L900 | - |
| GQ1-CHUNK-0668 | [ ] TODO | source | low | test-source | `android/helpers/test_helpers.ps1` | L901-L1800 | - |
| GQ1-CHUNK-0669 | [ ] TODO | source | low | test-source | `android/helpers/test_helpers.ps1` | L1801-L2518 | - |
| GQ1-CHUNK-0670 | [ ] TODO | source | low | test-source | `2 related paths` | 355 review lines under android/tests | - |
| GQ1-CHUNK-0671 | [ ] TODO | source | low | test-source | `2 related paths` | 735 review lines under android/tests | - |
| GQ1-CHUNK-0672 | [ ] TODO | source | low | test-source | `2 related paths` | 834 review lines under android/tests | - |
| GQ1-CHUNK-0673 | [ ] TODO | source | low | test-source | `2 related paths` | 572 review lines under android/tests | - |
| GQ1-CHUNK-0674 | [ ] TODO | source | low | test-source | `3 related paths` | 727 review lines under android/tests | - |
| GQ1-CHUNK-0675 | [ ] TODO | source | low | test-source | `3 related paths` | 860 review lines under android/tests | - |
| GQ1-CHUNK-0676 | [ ] TODO | source | low | test-source | `3 related paths` | 801 review lines under android/tests | - |
| GQ1-CHUNK-0677 | [ ] TODO | source | low | test-source | `3 related paths` | 194 review lines under android/tests | - |
| GQ1-CHUNK-0678 | [ ] TODO | source | low | test-source | `4 related paths` | 697 review lines under android/tests | - |
| GQ1-CHUNK-0679 | [ ] TODO | source | low | test-source | `4 related paths` | 746 review lines under android/tests | - |
| GQ1-CHUNK-0680 | [ ] TODO | source | low | test-source | `5 related paths` | 877 review lines under android/tests | - |
| GQ1-CHUNK-0681 | [ ] TODO | source | low | test-source | `5 related paths` | 646 review lines under android/tests | - |
| GQ1-CHUNK-0682 | [ ] TODO | source | low | test-source | `5 related paths` | 867 review lines under android/tests | - |
| GQ1-CHUNK-0683 | [ ] TODO | source | low | test-source | `5 related paths` | 797 review lines under android/tests | - |
| GQ1-CHUNK-0684 | [ ] TODO | source | low | test-source | `5 related paths` | 674 review lines under android/tests | - |
| GQ1-CHUNK-0685 | [ ] TODO | source | low | test-source | `5 related paths` | 576 review lines under android/tests | - |
| GQ1-CHUNK-0686 | [ ] TODO | source | low | test-source | `5 related paths` | 554 review lines under android/tests | - |
| GQ1-CHUNK-0687 | [ ] TODO | source | low | test-source | `6 related paths` | 851 review lines under android/tests | - |
| GQ1-CHUNK-0688 | [ ] TODO | source | low | test-source | `7 related paths` | 788 review lines under android/tests | - |
| GQ1-CHUNK-0689 | [ ] TODO | source | low | test-source | `7 related paths` | 891 review lines under android/tests | - |
| GQ1-CHUNK-0690 | [ ] TODO | source | low | test-source | `7 related paths` | 548 review lines under android/tests | - |
| GQ1-CHUNK-0691 | [ ] TODO | source | low | test-source | `7 related paths` | 890 review lines under android/tests | - |
| GQ1-CHUNK-0692 | [ ] TODO | source | low | test-source | `8 related paths` | 783 review lines under android/tests | - |
| GQ1-CHUNK-0693 | [ ] TODO | source | low | test-source | `9 related paths` | 695 review lines under android/tests | - |
| GQ1-CHUNK-0694 | [ ] TODO | source | low | test-source | `9 related paths` | 882 review lines under android/tests | - |
| GQ1-CHUNK-0695 | [ ] TODO | source | low | test-source | `9 related paths` | 850 review lines under android/tests | - |
| GQ1-CHUNK-0696 | [ ] TODO | source | low | test-source | `android/tests/compare_input_demo_rng_trace.ps1` | L1-L355 | - |
| GQ1-CHUNK-0697 | [ ] TODO | source | low | test-source | `android/tests/run_input_demo_regressions.ps1` | L1-L265 | - |
| GQ1-CHUNK-0698 | [ ] TODO | source | low | test-source | `android/tests/run_input_demo_replay.ps1` | L1-L900 | - |
| GQ1-CHUNK-0699 | [ ] TODO | source | low | test-source | `android/tests/run_input_demo_replay.ps1` | L901-L1742 | - |
| GQ1-CHUNK-0700 | [ ] TODO | source | low | test-source | `android/tests/test_input_demo_recorder.cpp` | L1-L900 | - |
| GQ1-CHUNK-0701 | [ ] TODO | source | low | test-source | `android/tests/test_input_demo_replay.cpp` | L1-L900 | - |
| GQ1-CHUNK-0702 | [ ] TODO | source | low | test-source | `android/tests/test_lan.ps1` | L1-L900 | - |
| GQ1-CHUNK-0703 | [ ] TODO | source | low | test-source | `android/tests/test_level_metadata_scan.c` | L1-L900 | - |
| GQ1-CHUNK-0704 | [ ] TODO | source | low | test-source | `android/tests/test_level_metadata_scan.c` | L901-L1657 | - |
| GQ1-CHUNK-0705 | [ ] TODO | source | low | test-source | `android/tests/test_mp.ps1` | L1-L811 | - |
| GQ1-CHUNK-0706 | [ ] TODO | source | low | test-source | `android/tests/test_route_analysis_cache.c` | L1-L112 | - |
| GQ1-CHUNK-0707 | [ ] TODO | source | low | test-source | `android/tests/test_route_snapshot.cpp` | L1-L900 | - |
| GQ1-CHUNK-0708 | [ ] TODO | source | low | test-source | `android/tools/etc2tool/test_etc2_limits.cpp` | L1-L44 | - |
| GQ1-CHUNK-0709 | [ ] TODO | mechanical | mechanical | artifact | `22 paths` | batch 1 | - |
| GQ1-CHUNK-0710 | [ ] TODO | mechanical | mechanical | dependency-lock | `1 paths` | batch 1 | - |
| GQ1-CHUNK-0711 | [ ] TODO | mechanical | mechanical | generated-fixture | `40 paths` | batch 1 | - |
| GQ1-CHUNK-0712 | [ ] TODO | mechanical | mechanical | generated-fixture | `40 paths` | batch 2 | - |
| GQ1-CHUNK-0713 | [ ] TODO | mechanical | mechanical | generated-fixture | `40 paths` | batch 3 | - |
| GQ1-CHUNK-0714 | [ ] TODO | mechanical | mechanical | generated-fixture | `5 paths` | batch 4 | - |
| GQ1-CHUNK-0715 | [ ] TODO | mechanical | mechanical | historical-plan | `1 paths` | batch 35 | - |
| GQ1-CHUNK-0716 | [ ] TODO | mechanical | mechanical | historical-plan | `40 paths` | batch 1 | - |
| GQ1-CHUNK-0717 | [ ] TODO | mechanical | mechanical | historical-plan | `40 paths` | batch 2 | - |
| GQ1-CHUNK-0718 | [ ] TODO | mechanical | mechanical | historical-plan | `40 paths` | batch 3 | - |
| GQ1-CHUNK-0719 | [ ] TODO | mechanical | mechanical | historical-plan | `40 paths` | batch 4 | - |
| GQ1-CHUNK-0720 | [ ] TODO | mechanical | mechanical | historical-plan | `40 paths` | batch 5 | - |
| GQ1-CHUNK-0721 | [ ] TODO | mechanical | mechanical | historical-plan | `40 paths` | batch 6 | - |
| GQ1-CHUNK-0722 | [ ] TODO | mechanical | mechanical | historical-plan | `40 paths` | batch 7 | - |
| GQ1-CHUNK-0723 | [ ] TODO | mechanical | mechanical | historical-plan | `40 paths` | batch 8 | - |
| GQ1-CHUNK-0724 | [ ] TODO | mechanical | mechanical | historical-plan | `40 paths` | batch 9 | - |
| GQ1-CHUNK-0725 | [ ] TODO | mechanical | mechanical | historical-plan | `40 paths` | batch 10 | - |
| GQ1-CHUNK-0726 | [ ] TODO | mechanical | mechanical | historical-plan | `40 paths` | batch 11 | - |
| GQ1-CHUNK-0727 | [ ] TODO | mechanical | mechanical | historical-plan | `40 paths` | batch 12 | - |
| GQ1-CHUNK-0728 | [ ] TODO | mechanical | mechanical | historical-plan | `40 paths` | batch 13 | - |
| GQ1-CHUNK-0729 | [ ] TODO | mechanical | mechanical | historical-plan | `40 paths` | batch 14 | - |
| GQ1-CHUNK-0730 | [ ] TODO | mechanical | mechanical | historical-plan | `40 paths` | batch 15 | - |
| GQ1-CHUNK-0731 | [ ] TODO | mechanical | mechanical | historical-plan | `40 paths` | batch 16 | - |
| GQ1-CHUNK-0732 | [ ] TODO | mechanical | mechanical | historical-plan | `40 paths` | batch 17 | - |
| GQ1-CHUNK-0733 | [ ] TODO | mechanical | mechanical | historical-plan | `40 paths` | batch 18 | - |
| GQ1-CHUNK-0734 | [ ] TODO | mechanical | mechanical | historical-plan | `40 paths` | batch 19 | - |
| GQ1-CHUNK-0735 | [ ] TODO | mechanical | mechanical | historical-plan | `40 paths` | batch 20 | - |
| GQ1-CHUNK-0736 | [ ] TODO | mechanical | mechanical | historical-plan | `40 paths` | batch 21 | - |
| GQ1-CHUNK-0737 | [ ] TODO | mechanical | mechanical | historical-plan | `40 paths` | batch 22 | - |
| GQ1-CHUNK-0738 | [ ] TODO | mechanical | mechanical | historical-plan | `40 paths` | batch 23 | - |
| GQ1-CHUNK-0739 | [ ] TODO | mechanical | mechanical | historical-plan | `40 paths` | batch 24 | - |
| GQ1-CHUNK-0740 | [ ] TODO | mechanical | mechanical | historical-plan | `40 paths` | batch 25 | - |
| GQ1-CHUNK-0741 | [ ] TODO | mechanical | mechanical | historical-plan | `40 paths` | batch 26 | - |
| GQ1-CHUNK-0742 | [ ] TODO | mechanical | mechanical | historical-plan | `40 paths` | batch 27 | - |
| GQ1-CHUNK-0743 | [ ] TODO | mechanical | mechanical | historical-plan | `40 paths` | batch 28 | - |
| GQ1-CHUNK-0744 | [ ] TODO | mechanical | mechanical | historical-plan | `40 paths` | batch 29 | - |
| GQ1-CHUNK-0745 | [ ] TODO | mechanical | mechanical | historical-plan | `40 paths` | batch 30 | - |
| GQ1-CHUNK-0746 | [ ] TODO | mechanical | mechanical | historical-plan | `40 paths` | batch 31 | - |
| GQ1-CHUNK-0747 | [ ] TODO | mechanical | mechanical | historical-plan | `40 paths` | batch 32 | - |
| GQ1-CHUNK-0748 | [ ] TODO | mechanical | mechanical | historical-plan | `40 paths` | batch 33 | - |
| GQ1-CHUNK-0749 | [ ] TODO | mechanical | mechanical | historical-plan | `40 paths` | batch 34 | - |
| GQ1-CHUNK-0750 | [ ] TODO | mechanical | mechanical | other-data | `22 paths` | batch 1 | - |
| GQ1-RECHECK-0001 | [ ] TODO | recheck | high | correctness | `SetupActivity.kt:commandReceiver` | Revalidate BR-0016 preview command serialization and stale completion ownership | - |
| GQ1-RECHECK-0002 | [ ] TODO | recheck | high | correctness | `jni_gog_import.c`, `jni_disc_import.c`, `iso9660_reader.c` | Revalidate BR-0021 cancellation propagation across extraction layers | - |
| GQ1-RECHECK-0003 | [ ] TODO | recheck | critical | concurrency | `jni_main.c`, `android_music_control.c` | Revalidate BR-0029 engine-thread ownership for overlay and music access | - |
| GQ1-RECHECK-0004 | [ ] TODO | recheck | critical | resource-lifetime | `shared/android_jni_overlay.c` | Revalidate BR-0044 local-reference and pending-exception handling | - |
| GQ1-RECHECK-0005 | [ ] TODO | recheck | high | correctness | `jni_level_metadata.cpp` | Revalidate BR-0077 level-load failure propagation | - |
| GQ1-RECHECK-0006 | [ ] TODO | recheck | critical | lifecycle | `jni_main.c` engine start and exit paths | Revalidate BR-0078 process-lifetime engine admission | - |
| GQ1-RECHECK-0007 | [ ] TODO | recheck | critical | bounds | `jni_main.c:nativeGetTeammateStatus` | Revalidate BR-0080 native player and weapon index admission | - |
| GQ1-RECHECK-0008 | [ ] TODO | recheck | high | data-format | `jni_resume_save.cpp`, paired state readers | Revalidate BR-0082 complete save-body admission before Resume | - |
| GQ1-RECHECK-0009 | [ ] TODO | recheck | medium | maintainability | `shared/physfs_archiver_saf.c` | Revalidate BR-0084 dormant duplicate SAF archive ownership | - |
| GQ1-RECHECK-0010 | [ ] TODO | recheck | high | compatibility | `multiplayer/PlayGamesAuth.kt` | Revalidate BR-0090 keypair fallback without Play Games | - |
| GQ1-RECHECK-0011 | [ ] TODO | recheck | high | correctness | `MissionZipAudioFingerprintCache.kt` | Revalidate BR-0099 database-aware cache invalidation | - |
| GQ1-RECHECK-0012 | [ ] TODO | recheck | medium | performance | `MissionZipAudioFingerprintCache.kt` | Revalidate BR-0101 whole-file cache bounds and pruning | - |
| GQ1-RECHECK-0013 | [ ] TODO | recheck | high | resource-lifetime | `MissionZipExtractionStore.kt` | Revalidate BR-0103 recoverable tree and manifest publication | - |
| GQ1-RECHECK-0014 | [ ] TODO | recheck | critical | security | `server/src/config.rs` | Revalidate BR-0106 fail-closed authentication configuration | - |
| GQ1-RECHECK-0015 | [ ] TODO | recheck | high | resource-lifetime | `server/src/nat_sim.rs` | Revalidate BR-0109 NAT task ownership after later edits | - |
| GQ1-RECHECK-0016 | [ ] TODO | recheck | high | correctness | `server/src/nat_sim.rs` mapping table | Revalidate BR-0110 internal-client mapping identity | - |
| GQ1-RECHECK-0017 | [ ] TODO | recheck | high | security | `server/src/nat_sim.rs:stun_query_through_nat` | Revalidate BR-0111 complete STUN attribute bounds | - |
| GQ1-RECHECK-0018 | [ ] TODO | recheck | high | correctness | `server/src/nat_sim.rs` port allocator | Revalidate BR-0112 port wrap, reservation, and collisions | - |
| GQ1-RECHECK-0019 | [ ] TODO | recheck | high | security | `server/src/pow.rs` | Revalidate BR-0113 canonical decoded-key identity | - |
| GQ1-RECHECK-0020 | [ ] TODO | recheck | critical | security | `server/src/rate_limit.rs` | Revalidate BR-0114 trusted-proxy identity before rate limiting | - |
| GQ1-RECHECK-0021 | [ ] TODO | recheck | high | security | `server/src/rate_limit.rs` auth failures | Revalidate BR-0117 authentication failure limiting | - |
| GQ1-RECHECK-0022 | [ ] TODO | recheck | high | concurrency | `server/src/rate_limit.rs:cleanup_map` | Revalidate BR-0118 cleanup versus refreshed entries | - |
| GQ1-RECHECK-0023 | [ ] TODO | recheck | high | compatibility | `server/src/protocol.rs` | Revalidate BR-0119 future protocol version rejection | - |
| GQ1-RECHECK-0024 | [ ] TODO | recheck | critical | security | `server/src/friends.rs`, `ws_handler.rs` | Revalidate BR-0121 atomic friend and block authorization | - |
| GQ1-RECHECK-0025 | [ ] TODO | recheck | critical | security | `server/src/http_api.rs` ban endpoint | Revalidate BR-0123 active-session revocation | - |
| GQ1-RECHECK-0026 | [ ] TODO | recheck | high | correctness | `server/src/http_api.rs` ban duration | Revalidate BR-0124 duration conversion and time bounds | - |
| GQ1-RECHECK-0027 | [ ] TODO | recheck | critical | resource-lifetime | `server/src/identity.rs` | Revalidate BR-0125 verification concurrency, time, and size budgets | - |
| GQ1-RECHECK-0028 | [ ] TODO | recheck | critical | resource-lifetime | `server/src/http_api.rs` public status | Revalidate BR-0126 request concurrency and response-cost limits | - |
| GQ1-RECHECK-0029 | [ ] TODO | recheck | critical | concurrency | `server/src/lib.rs` session registry | Revalidate BR-0127 generation-safe reconnect ownership | - |
| GQ1-RECHECK-0030 | [ ] TODO | recheck | critical | build-release | `server/src/main.rs` listeners | Revalidate BR-0128 atomic required-listener supervision | - |
| GQ1-RECHECK-0031 | [ ] TODO | recheck | high | resource-lifetime | `server/src/main.rs` tracing appender | Revalidate BR-0131 bounded log retention | - |
| GQ1-RECHECK-0032 | [ ] TODO | recheck | critical | security | `server/src/relay.rs` authorization | Revalidate BR-0132 endpoint authentication and lobby slots | - |
| GQ1-RECHECK-0033 | [ ] TODO | recheck | critical | resource-lifetime | `server/src/relay.rs` capacity | Revalidate BR-0133 capacity and lifetime ownership by game | - |
| GQ1-RECHECK-0034 | [ ] TODO | recheck | critical | security | `server/src/db.rs` ban query | Revalidate BR-0137 fail-closed ban enforcement | - |
| GQ1-RECHECK-0035 | [ ] TODO | recheck | critical | performance | `server/src/db.rs` SQLite access | Revalidate BR-0139 blocking work on async executors | - |
| GQ1-RECHECK-0036 | [ ] TODO | recheck | critical | resource-lifetime | `server/src/db.rs` append-only tables | Revalidate BR-0140 attacker-driven growth limits | - |
| GQ1-RECHECK-0037 | [ ] TODO | recheck | critical | security | `server/src/ws_handler.rs` known-key auth | Revalidate BR-0143 fresh challenge and audience binding | - |
| GQ1-RECHECK-0038 | [ ] TODO | recheck | critical | concurrency | `server/src/ws_handler.rs` writer ownership | Revalidate BR-0144 outbound failure connection teardown | - |
| GQ1-RECHECK-0039 | [ ] TODO | recheck | critical | concurrency | `server/src/ws_handler.rs` late join | Revalidate BR-0153 guard release before nested transitions | - |
| GQ1-RECHECK-0040 | [ ] TODO | recheck | high | test-gap | `extract/CMakeLists.txt` | Revalidate BR-0158 explicit fixture skip or failure | - |
| GQ1-RECHECK-0041 | [ ] TODO | recheck | high | build-release | `get_deps/helpers/get_7zip.ps1` | Revalidate BR-0159 verified atomic tool publication | - |
| GQ1-RECHECK-0042 | [ ] TODO | recheck | high | test-gap | `helpers/run_cue_iso_tests.sh` | Revalidate BR-0160 per-test timeout and cleanup isolation | - |
| GQ1-RECHECK-0043 | [ ] TODO | recheck | high | test-gap | `helpers/run_mission_zip_batch.ps1` | Revalidate BR-0162 zero-archive batch failure | - |
| GQ1-RECHECK-0044 | [ ] TODO | recheck | high | resource-lifetime | `tests/test_lan.ps1` | Revalidate BR-0163 child-process cleanup on every exit | - |
| GQ1-RECHECK-0045 | [ ] TODO | recheck | high | security | `helpers/udp_relay.ps1` | Revalidate BR-0164 peer authorization for forwarding | - |
| GQ1-RECHECK-0046 | [ ] TODO | recheck | high | correctness | `helpers/run_mission_zip_batch.ps1` artifacts | Revalidate BR-0167 collision-free run and archive identity | - |
| GQ1-RECHECK-0047 | [ ] TODO | recheck | high | resource-lifetime | `helpers/run_mission_zip_batch.ps1:Push-AppPrivateFile` | Revalidate BR-0168 bounded staging and cleanup | - |
| GQ1-RECHECK-0048 | [ ] TODO | recheck | high | correctness | `game_data/extract_all_cds.ps1`, `extract_all_gog.ps1` | Revalidate BR-0169 aggregate failure propagation | - |
| GQ1-RECHECK-0049 | [ ] TODO | recheck | high | test-gap | `tests/run_input_demo_replay.ps1` | Revalidate BR-0663 child-exit handling after publication | - |
| GQ1-RECHECK-0050 | [ ] TODO | recheck | high | correctness | `tests/test_mission_route_corpus.ps1` | Revalidate BR-0668 duplicate keys and declared/status counts | - |
| GQ1-SWEEP-001 | [ ] TODO | sweep | high | d1-d2 | `d1/ and d2/` | Parity, minimal upstream edits, platform guards, and shared-new-code boundaries | - |
| GQ1-SWEEP-002 | [ ] TODO | sweep | critical | native-boundary | `Android JNI and native code` | Ownership, lifetimes, thread attachment, references, bounds, and exception paths | - |
| GQ1-SWEEP-003 | [ ] TODO | sweep | high | android-lifecycle | `Android Kotlin and Java` | Activity lifecycle, state restoration, cancellation, permissions, backgrounding, and touch-only operation | - |
| GQ1-SWEEP-004 | [ ] TODO | sweep | critical | files-data | `Import, archive, storage, config, and save paths` | Trust boundaries, traversal, size limits, transactions, schema ownership, and C source of truth | - |
| GQ1-SWEEP-005 | [ ] TODO | sweep | critical | server-network | `server/ and multiplayer clients` | Protocol validation, abuse cases, rate and size limits, timeouts, cleanup, deadlocks, and compatibility | - |
| GQ1-SWEEP-006 | [ ] TODO | sweep | high | concurrency-resources | `complete diff` | Threads, locks, cancellation, handles, memory, GL resources, and lifecycle cleanup | - |
| GQ1-SWEEP-007 | [ ] TODO | sweep | high | build-portability | `Build, packaging, dependency, and release files` | Pinned inputs, host preservation, ABI matrix, paths with spaces, exit codes, and reproducibility | - |
| GQ1-SWEEP-008 | [ ] TODO | sweep | high | tests | `Tests, automation, fixtures, and runners` | Meaningful assertions, false passes, cleanup, timeouts, determinism, and missing integration coverage | - |
| GQ1-SWEEP-009 | [ ] TODO | sweep | high | determinism | `Simulation, save, replay, and metadata code` | RNG ownership, floating point, serialization completeness, state restore, and demo transparency | - |
| GQ1-SWEEP-010 | [ ] TODO | sweep | medium | performance | `Hot paths and large-data workflows` | Per-frame work, allocations, blocking I/O, repeated parsing, caching, and unbounded growth | - |
| GQ1-SWEEP-011 | [ ] TODO | sweep | high | errors-logging | `Complete diff` | Fail-safe behavior, cleanup on error, actionable diagnostics, privacy, and Android debug-log routing | - |
| GQ1-SWEEP-012 | [ ] TODO | sweep | high | interfaces | `Cross-language and cross-process interfaces` | API, ABI, protocol, schema, duplicated constants, versioning, and compatibility | - |
| GQ1-SWEEP-013 | [ ] TODO | sweep | medium | maintainability | `Complete diff` | Confusing names, unnecessary layers, duplication, dead code, stale comments, and simpler alternatives | - |
| GQ1-SWEEP-014 | [ ] TODO | sweep | high | pr-hygiene | `Complete diff` | Unexpected artifacts, generated files, historical plans, private data, licenses, and reviewability | - |
| GQ1-SWEEP-015 | [ ] TODO | sweep | high | cleanup | `Complete diff` | Branch-owned warnings, diagnostics, dead code, stale references, script duplication, test runtime, and inherited-file minimization | - |
| GQ1-CLOSE-001 | [ ] TODO | closure | critical | coverage | `Ledger and live branch` | Reconcile every path and chunk, validate findings, inspect head delta, and produce closure summary | - |

## Mechanical batch path lists

### GQ1-CHUNK-0001

- `android/auth_config.json5.template`: L1-L26
- `android/play-store-credentials.sample.json`: L1-L19

### GQ1-CHUNK-0002

- `game_data/CD images/Descent - Test Flight (USA)/extract_regression.json5`: L1-L36
- `game_data/CD images/Descent (Europe) (Alt)/extract_regression.json5`: L1-L36
- `game_data/CD images/Descent (Europe)/extract_regression.json5`: L1-L36
- `game_data/CD images/Descent (USA)/extract_regression.json5`: L1-L36
- `game_data/CD images/Descent Anniversary (ISO)/extract_regression.json5`: L1-L36
- `game_data/CD images/Descent Anniversary (ISO)/track_fingerprints.json`: L1-L7
- `game_data/CD images/Descent I and II - The Definitive Collection (Europe) (Disc 1)/extract_regression.json5`: L1-L36
- `game_data/CD images/Descent I and II - The Definitive Collection (Europe) (Disc 2)/extract_regression.json5`: L1-L71
- `game_data/CD images/Descent I and II - The Definitive Collection (Europe) (Disc 3)/extract_regression.json5`: L1-L66
- `game_data/CD images/Descent I and II - The Definitive Collection (USA) (Disc 1)/extract_regression.json5`: L1-L36
- `game_data/CD images/Descent I and II - The Definitive Collection (USA) (Disc 2)/extract_regression.json5`: L1-L71
- `game_data/CD images/Descent I and II - The Definitive Collection (USA) (Disc 3)/extract_regression.json5`: L1-L66

### GQ1-CHUNK-0003

- `game_data/CD images/Descent-II-Destination-Quartzon_Win_EN_ISO-Version/track_fingerprints.json`: L1-L115
- `game_data/CD images/Dimensions for Descent (USA)/extract_regression.json5`: L1-L122

### GQ1-CHUNK-0004

- `game_data/CD images/Descent II - Destination Quartzon (Europe)/extract_regression.json5`: L1-L88
- `game_data/CD images/Descent II - Destination Quartzon (USA) (Diamond OEM)/extract_regression.json5`: L1-L88
- `game_data/CD images/Descent II - Destination Quartzon (USA) (Logitech OEM)/extract_regression.json5`: L1-L88
- `game_data/CD images/Descent II - Destination Quartzon (USA)/extract_regression.json5`: L1-L88
- `game_data/CD images/Descent II - Destination Quartzon 3D (Europe)/extract_regression.json5`: L1-L60
- `game_data/CD images/Descent II - The Vertigo Series (USA)/extract_regression.json5`: L1-L66
- `game_data/CD images/Descent II (Europe) (v1.1)/extract_regression.json5`: L1-L71

### GQ1-CHUNK-0005

- `game_data/CD images/Descent II (Europe)/extract_regression.json5`: L1-L87
- `game_data/CD images/Descent II (USA) (3-Level Interactive Preview)/extract_regression.json5`: L1-L37
- `game_data/CD images/Descent II (USA) (Alt)/extract_regression.json5`: L1-L87
- `game_data/CD images/Descent II (USA) (Rerelease)/extract_regression.json5`: L1-L71
- `game_data/CD images/Descent II (USA) (v1.1)/extract_regression.json5`: L1-L71
- `game_data/CD images/Descent II (USA)/extract_regression.json5`: L1-L87
- `game_data/CD images/Descent II Infinite Abyss/extract_regression.json5`: L1-L71
- `game_data/CD images/Descent-II-Destination-Quartzon_Win_EN_ISO-Version/extract_regression.json5`: L1-L40

### GQ1-CHUNK-0006

- `game_data/CD images/d1 mac 2nd bin+cue/extract_regression.json5`: L1-L36
- `game_data/CD images/d1 mac 2nd bin+cue/track_fingerprints.json`: L1-L124
- `game_data/CD images/d1 mac 2nd bin+cue/track_hashes.json`: L1-L16
- `game_data/CD images/d2 mac/extract_regression.json5`: L1-L39
- `game_data/CD images/Descent - Anniversary Edition (Brazil) (Covermount)/extract_regression.json5`: L1-L36
- `game_data/CD images/Descent - Anniversary Edition (USA)/extract_regression.json5`: L1-L36
- `game_data/CD images/Descent - Destination Saturn (USA)/extract_regression.json5`: L1-L36
- `game_data/CD images/Descent - Levels of the World (USA)/extract_regression.json5`: L1-L225
- `game_data/CD images/Descent - Mac macplay/extract_regression.json5`: L1-L36

### GQ1-CHUNK-0008

- `game_data/music/Mission ZIP - castaway_redux/chromaprint_info.json5`: L1-L21
- `game_data/music/Mission ZIP - cererian_1.3/chromaprint_info.json5`: L1-L18
- `game_data/music/Mission ZIP - ewithin-versions/chromaprint_info.json5`: L1-L77
- `game_data/music/Mission ZIP - KCXF2RMv11/chromaprint_info.json5`: L1-L19
- `game_data/music/Mission ZIP - nefarious/chromaprint_info.json5`: L1-L12
- `game_data/music/Mission ZIP - Trine1/chromaprint_info.json5`: L1-L25
- `game_data/music/Mission ZIP - trine2/chromaprint_info.json5`: L1-L25
- `game_data/music/Mission ZIP - U3AAH/chromaprint_info.json5`: L1-L13
- `game_data/music/Mission ZIP - ulterior_v1.0.6b/chromaprint_info.json5`: L1-L49
- `game_data/music/Mission ZIP - Uneasy4/chromaprint_info.json5`: L1-L12

### GQ1-CHUNK-0009

- `android/app/src/main/cpp/shared/saf_manifest_parser.c`: L1-L285
- `android/app/src/main/cpp/shared/saf_manifest_parser.h`: L1-L22

### GQ1-CHUNK-0010

- `android/app/src/main/cpp/extract/mac_hfs_extract.c`: L1-L316
- `android/app/src/main/cpp/extract/mac_hfs_extract.h`: L1-L24

### GQ1-CHUNK-0011

- `android/app/src/main/cpp/extract/sow_extract.c`: L601-L1051
- `android/app/src/main/cpp/extract/sow_extract.h`: L1-L77

### GQ1-CHUNK-0012

- `android/app/src/main/cpp/extract/pkg_reader.c`: L601-L999
- `android/app/src/main/cpp/extract/pkg_reader.h`: L1-L91

### GQ1-CHUNK-0013

- `android/app/src/main/cpp/extract/iso9660_reader.c`: L601-L996
- `android/app/src/main/cpp/extract/iso9660_reader.h`: L1-L152

### GQ1-CHUNK-0014

- `android/app/src/main/cpp/extract/hfs_reader.c`: L1201-L1408
- `android/app/src/main/cpp/extract/hfs_reader.h`: L1-L113

### GQ1-CHUNK-0015

- `android/app/src/main/cpp/jni_main.c`: L1201-L1496
- `android/app/src/main/cpp/jni_midi_preview.c`: L1-L124

### GQ1-CHUNK-0016

- `android/app/src/main/cpp/extract/sti2_extract.c`: L2401-L2404
- `android/app/src/main/cpp/extract/sti2_extract.h`: L1-L74

### GQ1-CHUNK-0017

- `android/app/src/main/cpp/extract/jni_gog_import.c`: L1-L455
- `android/app/src/main/cpp/extract/json_writer.c`: L1-L78
- `android/app/src/main/cpp/extract/json_writer.h`: L1-L21

### GQ1-CHUNK-0018

- `android/app/src/main/cpp/shared/net/net_udp_reconnect_auth.c`: L1-L141
- `android/app/src/main/cpp/shared/net/net_udp_reconnect_auth.h`: L1-L53
- `android/app/src/main/cpp/shared/net/net_udp_reconnect_jni.h`: L1-L21

### GQ1-CHUNK-0019

- `android/app/src/main/cpp/extract/stuffit_extract.h`: L1-L23
- `android/app/src/main/cpp/jni_cd_preview.c`: L1-L162
- `android/app/src/main/cpp/jni_fingerprint.c`: L1-L332

### GQ1-CHUNK-0020

- `android/app/src/main/cpp/extract/fingerprint_match.c`: L1-L270
- `android/app/src/main/cpp/extract/game_file_extensions.c`: L1-L71
- `android/app/src/main/cpp/extract/game_file_extensions.h`: L1-L28

### GQ1-CHUNK-0021

- `android/app/src/main/cpp/extract/extract_cd.c`: L601-L704
- `android/app/src/main/cpp/extract/extract_gog.c`: L1-L223
- `android/app/src/main/cpp/extract/extract_limits.h`: L1-L129

### GQ1-CHUNK-0022

- `android/app/src/main/cpp/extract/cd_read_contract.c`: L1-L65
- `android/app/src/main/cpp/extract/cd_read_contract.h`: L1-L15
- `android/app/src/main/cpp/extract/cue_parser.c`: L1-L250
- `android/app/src/main/cpp/extract/cue_parser.h`: L1-L88

### GQ1-CHUNK-0023

- `android/app/src/main/cpp/jni_saf.c`: L1-L92
- `android/app/src/main/cpp/shared/net/net_udp_reconnect_jni.c`: L1-L269
- `android/app/src/main/cpp/shared/android_jni_overlay.c`: L1-L61
- `android/app/src/main/cpp/shared/android_jni_overlay.h`: L1-L18
- `android/app/src/main/cpp/shared/jni_string.c`: L1-L89
- `android/app/src/main/cpp/shared/jni_string.h`: L1-L19

### GQ1-CHUNK-0054

- `android/app/src/main/java/com/dxxredux/app/ArchiveInputStreams.kt`: L1-L237
- `android/app/src/main/java/com/dxxredux/app/ExtractionLimits.kt`: L1-L117

### GQ1-CHUNK-0055

- `android/app/src/main/java/com/dxxredux/app/MissionZipMusic.kt`: L601-L698
- `android/app/src/main/java/com/dxxredux/app/MissionZipMusicNames.kt`: L1-L123

### GQ1-CHUNK-0065

- `server/src/bin/nat_sim.rs`: L1-L51
- `server/src/config.rs`: L1-L220

### GQ1-CHUNK-0066

- `server/src/stun.rs`: L1-L107
- `server/src/tls.rs`: L1-L39

### GQ1-CHUNK-0067

- `server/src/pow.rs`: L1-L139
- `server/src/protocol.rs`: L1-L452

### GQ1-CHUNK-0068

- `server/src/friends.rs`: L1-L121
- `server/src/http_api.rs`: L1-L313
- `server/src/identity.rs`: L1-L121

### GQ1-CHUNK-0069

- `server/src/lib.rs`: L1-L47
- `server/src/lobby.rs`: L1-L231
- `server/src/main.rs`: L1-L167

### GQ1-CHUNK-0070

- `server/src/rate_limit.rs`: L1-L143
- `server/src/relay.rs`: L1-L167
- `server/src/stats.rs`: L1-L185

### GQ1-CHUNK-0082

- `android/helpers/run_mission_zip_batch.ps1`: L601-L916
- `android/helpers/udp_relay.ps1`: L1-L82

### GQ1-CHUNK-0083

- `android/helpers/bounded_extraction.ps1`: L1-L468
- `android/helpers/extract_hfs_machfs.py`: L1-L96
- `android/helpers/mission_zip_batch_recovery.ps1`: L1-L33

### GQ1-CHUNK-0084

- `android/helpers/playstore-auth.ps1`: L1-L151
- `android/helpers/run_bounded_extractor.py`: L1-L153
- `android/helpers/run_cue_iso_tests.sh`: L1-L32

### GQ1-CHUNK-0086

- `game_data/extract_all_cds.ps1`: L1-L225
- `game_data/extract_all_gog.ps1`: L1-L237

### GQ1-CHUNK-0093

- `android/app/src/main/cpp/extract/test_stuffit_demo_oracles.cmake`: L1-L107
- `android/app/src/main/cpp/extract/test_stuffit_direct.c`: L1-L25
- `android/app/src/main/cpp/extract/test_stuffit_malformed.c`: L1-L135
- `android/app/src/main/cpp/extract/test_utf8_codec.c`: L1-L99
- `android/app/src/main/cpp/extract/test/data/stuffit_manifests/testfile.stuffit45_dlx.mac9.sit.json`: L1-L21
- `android/app/src/main/cpp/extract/test/data/stuffit_manifests/testfile.stuffit651_dlx.mac9.sit.json`: L1-L21
- `android/app/src/main/cpp/extract/test/data/stuffit_manifests/testfile.stuffit651_dlx.macx1.sit.json`: L1-L21
- `android/app/src/main/cpp/extract/test/data/stuffit_manifests/testfile.stuffit7_dlx.mac9.sit.json`: L1-L21
- `android/app/src/main/cpp/extract/test/data/stuffit_manifests/testfile.stuffit7_dlx.macx1.sit.json`: L1-L21
- `android/app/src/main/cpp/extract/test/data/stuffit_manifests/testfile.stuffit7.win.sit.json`: L1-L16
- `android/app/src/main/cpp/shared/test_saf_manifest_parser.c`: L1-L85

### GQ1-CHUNK-0094

- `android/app/src/main/cpp/extract/test_gog_fd.c`: L1201-L1467
- `android/app/src/main/cpp/extract/test_graphics_config_transaction.c`: L1-L181

### GQ1-CHUNK-0095

- `android/app/src/main/cpp/extract/test_cue_iso.c`: L3001-L3426
- `android/app/src/main/cpp/extract/test_extract_limits.c`: L1-L51

### GQ1-CHUNK-0096

- `android/app/src/main/cpp/extract/test_pkg_toc_bounds.c`: L601-L665
- `android/app/src/main/cpp/extract/test_sow_direct.c`: L1-L23
- `android/app/src/main/cpp/extract/test_sow_huffman.c`: L1-L72

### GQ1-CHUNK-0097

- `android/app/src/main/cpp/extract/test_hmp_android_shared.c`: L1-L244
- `android/app/src/main/cpp/extract/test_hmp_stubs/hmp.h`: L1-L32
- `android/app/src/main/cpp/extract/test_hmp_stubs/u_mem.h`: L1-L16
- `android/app/src/main/cpp/extract/test_json_writer.cpp`: L1-L75
- `android/app/src/main/cpp/extract/test_pilot_pref_transaction.cpp`: L1-L69

### GQ1-CHUNK-0098

- `android/app/src/main/cpp/extract/test_android_stubs/android/log.h`: L1-L17
- `android/app/src/main/cpp/extract/test_cd_incomplete_read.cmake`: L1-L25
- `android/app/src/main/cpp/extract/test_cd_read_contract.c`: L1-L46
- `android/app/src/main/cpp/extract/test_chromaprint_db_concurrency.cpp`: L1-L105
- `android/app/src/main/cpp/extract/test_chromaprint_db_config.c`: L1-L135

### GQ1-CHUNK-0117

- `android/app/src/test/java/com/dxxredux/app/MissionZipMusicNamesTest.kt`: L1-L179
- `android/app/src/test/java/com/dxxredux/app/MissionZipMusicProgressTest.kt`: L1-L33

### GQ1-CHUNK-0118

- `android/app/src/test/java/com/dxxredux/app/MissionZipExtractionStoreTest.kt`: L1-L231
- `android/app/src/test/java/com/dxxredux/app/MissionZipMusicAnalysisSkipTest.kt`: L1-L63
- `android/app/src/test/java/com/dxxredux/app/MissionZipMusicDisplayTest.kt`: L1-L76
- `android/app/src/test/java/com/dxxredux/app/MissionZipMusicExtractedPreviewTest.kt`: L1-L179

### GQ1-CHUNK-0119

- `android/app/src/test/java/com/dxxredux/app/ArchiveCompressedSizeTest.kt`: L1-L13
- `android/app/src/test/java/com/dxxredux/app/ArchiveInputStreamsTest.kt`: L1-L93
- `android/app/src/test/java/com/dxxredux/app/CueDataTrackExtractionTest.kt`: L1-L288
- `android/app/src/test/java/com/dxxredux/app/ExtractionLimitsTest.kt`: L1-L45
- `android/app/src/test/java/com/dxxredux/app/MissionArchiveProgressFormatTest.kt`: L1-L22

### GQ1-CHUNK-0127

- `android/game_scripts/test_extract_regression_template.json5`: L1-L57
- `android/game_scripts/test_level_metadata_launcher_zip_reusable.json5`: L1-L37
- `android/game_scripts/test_mission_zip_batch_import_metadata_launch.json5`: L1-L38
- `android/game_scripts/test_mission_zip_batch_import_metadata.json5`: L1-L18

### GQ1-CHUNK-0128

- `android/tests/test_net_udp_reconnect_auth.c`: L1-L205
- `android/tests/test_run_bounded_extractor.py`: L1-L90

### GQ1-CHUNK-0129

- `android/tests/extract_regression_recovery.ps1`: L1-L12
- `android/tests/extract_regression_spec_helpers.ps1`: L1-L371

### GQ1-CHUNK-0130

- `android/tests/test_all_extracts.ps1`: L1-L354
- `android/tests/test_bounded_extraction.ps1`: L1-L205
- `android/tests/test_cue_iso.ps1`: L1-L38

### GQ1-CHUNK-0131

- `android/tests/test_saf_archiver.ps1`: L601-L731
- `android/tests/test_validate_extract_regression_specs.ps1`: L1-L10
- `android/tests/validate_extract_regression_specs.ps1`: L1-L127

### GQ1-CHUNK-0132

- `android/tests/test_extract.ps1`: L1201-L1620
- `android/tests/test_extraction_cache_provenance.ps1`: L1-L123
- `android/tests/test_extraction_publication.ps1`: L1-L42

### GQ1-CHUNK-0133

- `android/tests/test_fingerprint_mission_zip_budgets.ps1`: L1-L149
- `android/tests/test_mac_extract_saf.ps1`: L1-L240
- `android/tests/test_mission_zip_batch_recovery.ps1`: L1-L81
- `android/tests/test_mission_zip_batch.ps1`: L1-L18

### GQ1-CHUNK-0134

- `android/tests/test_extract_all_cds_batch.ps1`: L1-L131
- `android/tests/test_extract_all_gog_batch.ps1`: L1-L54
- `android/tests/test_extract_hfs_machfs.py`: L1-L79
- `android/tests/test_extract_regression_workflow.ps1`: L1-L198
- `android/tests/test_extract_suite_device_preflight.ps1`: L1-L80

### GQ1-CHUNK-0141

- `server/config.json5.default`: L1-L53
- `server/nginx-dxx-matchmaking.conf`: L1-L63
- `server/server_config.json5.template`: L1-L55

### GQ1-CHUNK-0142

- `android/app/src/main/cpp/shared/input_demo_hooks_shared.h`: L1-L57
- `android/app/src/main/cpp/shared/input_demo_limits.h`: L1-L45

### GQ1-CHUNK-0143

- `android/app/src/main/cpp/android_surface.c`: L1-L297
- `android/app/src/main/cpp/headless/d2_headless_runtime.c`: L1-L21

### GQ1-CHUNK-0144

- `android/app/src/main/cpp/shared/android_rewind.c`: L1-L496
- `android/app/src/main/cpp/shared/android_rewind.h`: L1-L49

### GQ1-CHUNK-0145

- `android/app/src/main/cpp/shared/utf8_codec.c`: L1-L98
- `android/app/src/main/cpp/shared/utf8_codec.h`: L1-L13

### GQ1-CHUNK-0146

- `android/app/src/main/cpp/shared/coop/coop_powerup_duplication.h`: L1-L42
- `android/app/src/main/cpp/shared/coop/coop_restore_remap.h`: L1-L18

### GQ1-CHUNK-0147

- `android/app/src/main/cpp/shared/gles3_shim.c`: L751-L1025
- `android/app/src/main/cpp/shared/gles3_shim.h`: L1-L183

### GQ1-CHUNK-0148

- `android/app/src/main/cpp/shared/input_demo_controls.cpp`: L751-L764
- `android/app/src/main/cpp/shared/input_demo_controls.h`: L1-L167

### GQ1-CHUNK-0149

- `android/app/src/main/cpp/shared/input_demo_start_shared.c`: L751-L774
- `android/app/src/main/cpp/shared/input_demo_start_shared.h`: L1-L64

### GQ1-CHUNK-0150

- `android/app/src/main/cpp/shared/input_demo_recorder.cpp`: L751-L775
- `android/app/src/main/cpp/shared/input_demo_recorder.h`: L1-L98

### GQ1-CHUNK-0151

- `android/app/src/main/cpp/shared/digi_tsf_music.c`: L751-L1288
- `android/app/src/main/cpp/shared/effect_runtime_shared.c`: L1-L91

### GQ1-CHUNK-0152

- `android/app/src/main/cpp/shared/input_demo_replay.cpp`: L751-L817
- `android/app/src/main/cpp/shared/input_demo_replay.h`: L1-L107

### GQ1-CHUNK-0153

- `android/app/src/main/cpp/shared/rbaudio_bin.c`: L1501-L1960
- `android/app/src/main/cpp/shared/rewind_file_compat.h`: L1-L32

### GQ1-CHUNK-0154

- `android/app/src/main/cpp/shared/game_automate.cpp`: L3751-L3844
- `android/app/src/main/cpp/shared/game_automate.h`: L1-L106

### GQ1-CHUNK-0155

- `android/app/src/main/cpp/shared/graphics_config_transaction.c`: L1-L425
- `android/app/src/main/cpp/shared/graphics_config_transaction.h`: L1-L51
- `android/app/src/main/cpp/shared/hmp_android_shared.c`: L1-L274

### GQ1-CHUNK-0156

- `android/app/src/main/cpp/shared/input_demo_debug_logging.h`: L1-L182
- `android/app/src/main/cpp/shared/input_demo_direct_command_policy.c`: L1-L174
- `android/app/src/main/cpp/shared/input_demo_direct_command_policy.h`: L1-L69

### GQ1-CHUNK-0157

- `android/app/src/main/cpp/shared/input_demo_fixture.h`: L1-L211
- `android/app/src/main/cpp/shared/input_demo_fp_env.c`: L1-L73
- `android/app/src/main/cpp/shared/input_demo_fp_env.h`: L1-L17

### GQ1-CHUNK-0158

- `android/app/src/main/cpp/shared/debug_tex_overlay.h`: L1-L293
- `android/app/src/main/cpp/shared/deterministic_math.h`: L1-L13
- `android/app/src/main/cpp/shared/difficulty_runtime_shared.c`: L1-L118

### GQ1-CHUNK-0159

- `android/app/src/main/cpp/SDL_androidaudio.c`: L1-L555
- `android/app/src/main/cpp/SDL_androidaudio.h`: L1-L44
- `android/app/src/main/cpp/SDL_config_android.h`: L1-L87

### GQ1-CHUNK-0160

- `android/app/src/main/cpp/shared/route_snapshot.h`: L1-L208
- `android/app/src/main/cpp/shared/saf_io_contract.c`: L1-L8
- `android/app/src/main/cpp/shared/saf_io_contract.h`: L1-L8

### GQ1-CHUNK-0161

- `android/app/src/main/cpp/shared/android_lifecycle_diagnostics.c`: L1-L390
- `android/app/src/main/cpp/shared/android_lifecycle_diagnostics.h`: L1-L115
- `android/app/src/main/cpp/shared/android_loading_progress.c`: L1-L229

### GQ1-CHUNK-0162

- `android/app/src/main/cpp/shared/android_level_preview.cpp`: L1-L533
- `android/app/src/main/cpp/shared/android_level_preview.h`: L1-L19
- `android/app/src/main/cpp/shared/android_lifecycle_actions.h`: L1-L14

### GQ1-CHUNK-0163

- `android/app/src/main/cpp/shared/route_edge.cpp`: L1-L300
- `android/app/src/main/cpp/shared/route_edge.h`: L1-L78
- `android/app/src/main/cpp/shared/route_planner_c.h`: L1-L47

### GQ1-CHUNK-0164

- `android/app/src/main/cpp/shared/secret_area_scan.c`: L751-L1190
- `android/app/src/main/cpp/shared/secret_area_scan.h`: L1-L131
- `android/app/src/main/cpp/shared/software_renderer_debug.c`: L1-L124

### GQ1-CHUNK-0165

- `android/app/src/main/cpp/headless/headless_metadata_dump_main.cpp`: L751-L1057
- `android/app/src/main/cpp/headless/input_demo_headless_main.cpp`: L1-L246
- `android/app/src/main/cpp/native-lib.cpp`: L1-L16

### GQ1-CHUNK-0166

- `android/app/src/main/cpp/shared/game_introspect.cpp`: L2251-L2435
- `android/app/src/main/cpp/shared/game_introspect.h`: L1-L84
- `android/app/src/main/cpp/shared/gles3_shim_array_sources.h`: L1-L41

### GQ1-CHUNK-0167

- `android/app/src/main/cpp/shared/secretarea.c`: L2251-L2563
- `android/app/src/main/cpp/shared/secret_area_item_names.c`: L1-L85
- `android/app/src/main/cpp/shared/secret_area_item_names.h`: L1-L14

### GQ1-CHUNK-0168

- `android/app/src/main/cpp/shared/route_planner.cpp`: L3001-L3201
- `android/app/src/main/cpp/shared/route_planner.h`: L1-L317
- `android/app/src/main/cpp/shared/route_snapshot_c.h`: L1-L52

### GQ1-CHUNK-0169

- `android/app/src/main/cpp/shared/songs_android_shared.c`: L1-L335
- `android/app/src/main/cpp/shared/songs_android_shared.h`: L1-L21
- `android/app/src/main/cpp/shared/startup_resume_shared.c`: L1-L89
- `android/app/src/main/cpp/shared/startup_resume_shared.h`: L1-L8

### GQ1-CHUNK-0170

- `android/app/src/main/cpp/shared/ogl_texture_android.c`: L1-L407
- `android/app/src/main/cpp/shared/ogl_texture_android.h`: L1-L77
- `android/app/src/main/cpp/shared/ogl_viewport_android.c`: L1-L101
- `android/app/src/main/cpp/shared/ogl_viewport_android.h`: L1-L24

### GQ1-CHUNK-0171

- `android/app/src/main/cpp/shared/rewind_file.h`: L1-L409
- `android/app/src/main/cpp/shared/rgba8888.h`: L1-L15
- `android/app/src/main/cpp/shared/route_analysis_cache.c`: L1-L159
- `android/app/src/main/cpp/shared/route_analysis_cache.h`: L1-L65

### GQ1-CHUNK-0172

- `android/app/src/main/cpp/shared/input_demo_result.h`: L1-L124
- `android/app/src/main/cpp/shared/input_demo_rng_mode.c`: L1-L181
- `android/app/src/main/cpp/shared/input_demo_rng_mode.h`: L1-L23
- `android/app/src/main/cpp/shared/input_demo_rng_trace.c`: L1-L422

### GQ1-CHUNK-0173

- `android/app/src/main/cpp/shared/playsave_android_shared.c`: L1-L472
- `android/app/src/main/cpp/shared/playsave_android_shared.h`: L1-L34
- `android/app/src/main/cpp/shared/playsave_layout.c`: L1-L161
- `android/app/src/main/cpp/shared/playsave_layout.h`: L1-L27

### GQ1-CHUNK-0174

- `android/app/src/main/cpp/shared/merged_wall_debug.h`: L1-L307
- `android/app/src/main/cpp/shared/merged_wall_geometry_hit.h`: L1-L53
- `android/app/src/main/cpp/shared/messagebox.c`: L1-L18
- `android/app/src/main/cpp/shared/midi_enumeration.c`: L1-L371

### GQ1-CHUNK-0175

- `android/app/src/main/cpp/shared/midi_preview.h`: L1-L63
- `android/app/src/main/cpp/shared/midi_seek_timeline.c`: L1-L120
- `android/app/src/main/cpp/shared/midi_seek_timeline.h`: L1-L34
- `android/app/src/main/cpp/shared/multi_save_transfer_policy.h`: L1-L17

### GQ1-CHUNK-0176

- `android/app/src/main/cpp/shared/net/auto_net.c`: L1-L210
- `android/app/src/main/cpp/shared/net/auto_net.h`: L1-L78
- `android/app/src/main/cpp/shared/net/net_udp_android_autonet_shared.c`: L1-L230
- `android/app/src/main/cpp/shared/net/net_udp_android_autonet_shared.h`: L1-L11

### GQ1-CHUNK-0177

- `android/app/src/main/cpp/shared/ogl_msaa_android.c`: L1-L287
- `android/app/src/main/cpp/shared/ogl_msaa_android.h`: L1-L64
- `android/app/src/main/cpp/shared/ogl_shader_runtime.c`: L1-L54
- `android/app/src/main/cpp/shared/ogl_shader_runtime.h`: L1-L12

### GQ1-CHUNK-0178

- `android/app/src/main/cpp/shared/overlay_ringbuf.cpp`: L1-L154
- `android/app/src/main/cpp/shared/overlay_ringbuf.h`: L1-L48
- `android/app/src/main/cpp/shared/pcm_decoders.c`: L1-L385
- `android/app/src/main/cpp/shared/pcm_decoders.h`: L1-L60

### GQ1-CHUNK-0179

- `android/app/src/main/cpp/shared/playsave_text.c`: L1-L230
- `android/app/src/main/cpp/shared/playsave_text.h`: L1-L15
- `android/app/src/main/cpp/shared/playsave_transaction.c`: L1-L144
- `android/app/src/main/cpp/shared/playsave_transaction.h`: L1-L35

### GQ1-CHUNK-0180

- `android/app/src/main/cpp/shared/classic_demo_wall_validation.h`: L1-L15
- `android/app/src/main/cpp/shared/console_ringbuf.cpp`: L1-L157
- `android/app/src/main/cpp/shared/console_ringbuf.h`: L1-L36
- `android/app/src/main/cpp/shared/coop_indicator_lines_math.h`: L1-L70

### GQ1-CHUNK-0181

- `android/app/src/main/cpp/shared/android_font_scale.c`: L1-L148
- `android/app/src/main/cpp/shared/android_font_scale.h`: L1-L7
- `android/app/src/main/cpp/shared/android_graphics_options.c`: L1-L482
- `android/app/src/main/cpp/shared/android_graphics_options.h`: L1-L39

### GQ1-CHUNK-0182

- `android/app/src/main/cpp/shared/android_menu_scale.h`: L1-L83
- `android/app/src/main/cpp/shared/android_menu_touch_log.c`: L1-L55
- `android/app/src/main/cpp/shared/android_menu_touch_log.h`: L1-L56
- `android/app/src/main/cpp/shared/android_meta_actions.c`: L1-L500

### GQ1-CHUNK-0183

- `android/app/src/main/cpp/shared/classic_demo_json_d2_snapshot.c`: L1-L171
- `android/app/src/main/cpp/shared/classic_demo_json_d2_snapshot.h`: L1-L20
- `android/app/src/main/cpp/shared/classic_demo_json.c`: L1-L357
- `android/app/src/main/cpp/shared/classic_demo_json.h`: L1-L195

### GQ1-CHUNK-0184

- `android/app/src/main/cpp/shared/android_loading_progress.h`: L1-L18
- `android/app/src/main/cpp/shared/android_log.c`: L1-L155
- `android/app/src/main/cpp/shared/android_log.h`: L1-L61
- `android/app/src/main/cpp/shared/android_menu_reorder.h`: L1-L81

### GQ1-CHUNK-0185

- `android/app/src/main/cpp/shared/coop/coop_multi_status.c`: L1-L193
- `android/app/src/main/cpp/shared/coop/coop_multi_status.h`: L1-L37
- `android/app/src/main/cpp/shared/coop/coop_player_session.h`: L1-L38
- `android/app/src/main/cpp/shared/coop/coop_powerup_duplication.c`: L1-L448

### GQ1-CHUNK-0186

- `android/app/src/main/cpp/shared/coop/coop_save.h`: L1-L213
- `android/app/src/main/cpp/shared/coop/coop_warp.c`: L1-L236
- `android/app/src/main/cpp/shared/coop/coop_warp.h`: L1-L59
- `android/app/src/main/cpp/shared/debug_log_categories.h`: L1-L21

### GQ1-CHUNK-0187

- `android/app/src/main/cpp/shared/input_demo_state_trace.cpp`: L751-L791
- `android/app/src/main/cpp/shared/input_demo_state_trace.h`: L1-L422
- `android/app/src/main/cpp/shared/kconfig_android_shared.c`: L1-L91
- `android/app/src/main/cpp/shared/kconfig_android_shared.h`: L1-L41

### GQ1-CHUNK-0188

- `android/app/src/main/cpp/shared/cd_preview.c`: L751-L884
- `android/app/src/main/cpp/shared/cd_preview.h`: L1-L64
- `android/app/src/main/cpp/shared/chromaprint_db.c`: L1-L309
- `android/app/src/main/cpp/shared/chromaprint_db.h`: L1-L77

### GQ1-CHUNK-0189

- `android/app/src/main/cpp/shared/net/net_udp_android.h`: L1-L123
- `android/app/src/main/cpp/shared/net/net_udp_p2p_proxy_shared.c`: L1-L302
- `android/app/src/main/cpp/shared/net/net_udp_p2p_proxy_shared.h`: L1-L16
- `android/app/src/main/cpp/shared/ogl_gpu_timer_android.c`: L1-L79
- `android/app/src/main/cpp/shared/ogl_gpu_timer_android.h`: L1-L22

### GQ1-CHUNK-0190

- `android/app/src/main/cpp/shared/android_audio_diagnostics.c`: L1-L122
- `android/app/src/main/cpp/shared/android_audio_diagnostics.h`: L1-L16
- `android/app/src/main/cpp/shared/android_audio_format.h`: L1-L76
- `android/app/src/main/cpp/shared/android_axis_mailbox.cpp`: L1-L332
- `android/app/src/main/cpp/shared/android_axis_mailbox.h`: L1-L91

### GQ1-CHUNK-0191

- `android/app/src/main/cpp/shared/android_save_meta.c`: L1-L309
- `android/app/src/main/cpp/shared/android_save_meta.h`: L1-L121
- `android/app/src/main/cpp/shared/android_save_set.c`: L1-L158
- `android/app/src/main/cpp/shared/android_save_set.h`: L1-L32
- `android/app/src/main/cpp/shared/android_screen_advance.h`: L1-L39

### GQ1-CHUNK-0192

- `android/app/src/main/cpp/shared/state_android_shared.c`: L751-L1057
- `android/app/src/main/cpp/shared/state_android_shared.h`: L1-L50
- `android/app/src/main/cpp/shared/track_names.c`: L1-L291
- `android/app/src/main/cpp/shared/track_names.h`: L1-L57
- `android/app/src/main/cpp/shared/tsf_impl.c`: L1-L11

### GQ1-CHUNK-0193

- `android/app/src/main/cpp/shared/android_slowdown_detector.c`: L1-L297
- `android/app/src/main/cpp/shared/android_slowdown_detector.h`: L1-L125
- `android/app/src/main/cpp/shared/android_surface_lifecycle.h`: L1-L21
- `android/app/src/main/cpp/shared/android_texture_debug.c`: L1-L196
- `android/app/src/main/cpp/shared/android_texture_debug.h`: L1-L41
- `android/app/src/main/cpp/shared/android_visual_policy.h`: L1-L15

### GQ1-CHUNK-0194

- `android/app/src/main/cpp/shared/physfsx_android_setup.c`: L1-L301
- `android/app/src/main/cpp/shared/physfsx_android_setup.h`: L1-L30
- `android/app/src/main/cpp/shared/physfsx_android_shared.c`: L1-L30
- `android/app/src/main/cpp/shared/physfsx_android_shared.h`: L1-L11
- `android/app/src/main/cpp/shared/pilot_pref_transaction.cpp`: L1-L82
- `android/app/src/main/cpp/shared/pilot_pref_transaction.h`: L1-L30

### GQ1-CHUNK-0195

- `android/app/src/main/cpp/shared/etc2_decode.c`: L1-L357
- `android/app/src/main/cpp/shared/etc2_decode.h`: L1-L17
- `android/app/src/main/cpp/shared/fingerprint_duration.h`: L1-L19
- `android/app/src/main/cpp/shared/fingerprint_gen.c`: L1-L209
- `android/app/src/main/cpp/shared/fingerprint_gen.h`: L1-L69
- `android/app/src/main/cpp/shared/font_control_shared.h`: L1-L15

### GQ1-CHUNK-0196

- `android/app/src/main/cpp/shared/android_meta_actions.h`: L1-L125
- `android/app/src/main/cpp/shared/android_music_control.h`: L1-L10
- `android/app/src/main/cpp/shared/android_newmenu_text_wrap.c`: L1-L149
- `android/app/src/main/cpp/shared/android_newmenu_text_wrap.h`: L1-L21
- `android/app/src/main/cpp/shared/android_pilot_listbox_hold.c`: L1-L266
- `android/app/src/main/cpp/shared/android_pilot_listbox_hold.h`: L1-L45

### GQ1-CHUNK-0197

- `android/app/src/main/cpp/shared/multi_save_transfer.c`: L751-L981
- `android/app/src/main/cpp/shared/music_decode_limits.h`: L1-L39
- `android/app/src/main/cpp/shared/music_name_table.cpp`: L1-L164
- `android/app/src/main/cpp/shared/music_name_table.h`: L1-L21
- `android/app/src/main/cpp/shared/music_track_json.c`: L1-L97
- `android/app/src/main/cpp/shared/music_track_json.h`: L1-L18

### GQ1-CHUNK-0198

- `android/app/src/main/cpp/shared/android_crash_handler.c`: L1-L284
- `android/app/src/main/cpp/shared/android_crash_handler.h`: L1-L41
- `android/app/src/main/cpp/shared/android_dxxerror.h`: L1-L14
- `android/app/src/main/cpp/shared/android_egl_surface.c`: L1-L266
- `android/app/src/main/cpp/shared/android_egl_surface.h`: L1-L36
- `android/app/src/main/cpp/shared/android_file_pair_transaction.c`: L1-L80
- `android/app/src/main/cpp/shared/android_file_pair_transaction.h`: L1-L25

### GQ1-CHUNK-0199

- `android/app/src/main/cpp/shared/automap_metadata_overlay.c`: L751-L797
- `android/app/src/main/cpp/shared/automap_metadata_overlay.h`: L1-L29
- `android/app/src/main/cpp/shared/boss_hud.c`: L1-L202
- `android/app/src/main/cpp/shared/boss_hud.h`: L1-L56
- `android/app/src/main/cpp/shared/bounded_music_read.h`: L1-L27
- `android/app/src/main/cpp/shared/bounded_rle.c`: L1-L68
- `android/app/src/main/cpp/shared/bounded_rle.h`: L1-L17

### GQ1-CHUNK-0200

- `android/app/src/main/cpp/shared/android_profile.h`: L1-L110
- `android/app/src/main/cpp/shared/android_render_fov.c`: L1-L45
- `android/app/src/main/cpp/shared/android_render_fov.h`: L1-L12
- `android/app/src/main/cpp/shared/android_render_resolution.h`: L1-L18
- `android/app/src/main/cpp/shared/android_resume_pilot.c`: L1-L165
- `android/app/src/main/cpp/shared/android_resume_pilot.h`: L1-L15
- `android/app/src/main/cpp/shared/android_rewind_policy.c`: L1-L106
- `android/app/src/main/cpp/shared/android_rewind_policy.h`: L1-L69

### GQ1-CHUNK-0201

- `android/app/src/main/cpp/shared/hmp_android_shared.h`: L1-L15
- `android/app/src/main/cpp/shared/hog_midi_catalog.h`: L1-L209
- `android/app/src/main/cpp/shared/homing_compat.h`: L1-L52
- `android/app/src/main/cpp/shared/hud_counts_shared.c`: L1-L172
- `android/app/src/main/cpp/shared/hud_counts_shared.h`: L1-L26
- `android/app/src/main/cpp/shared/hud_layout_shared.h`: L1-L20
- `android/app/src/main/cpp/shared/input_demo_codec.cpp`: L1-L67
- `android/app/src/main/cpp/shared/input_demo_codec.h`: L1-L24

### GQ1-CHUNK-0202

- `android/app/src/main/cpp/shared/coop_indicator_lines.h`: L1-L19
- `android/app/src/main/cpp/shared/coop_start_positions.c`: L1-L79
- `android/app/src/main/cpp/shared/coop_start_positions.h`: L1-L9
- `android/app/src/main/cpp/shared/coop/coop_host_migration_policy.c`: L1-L37
- `android/app/src/main/cpp/shared/coop/coop_host_migration_policy.h`: L1-L28
- `android/app/src/main/cpp/shared/coop/coop_host_migration.c`: L1-L98
- `android/app/src/main/cpp/shared/coop/coop_host_migration.h`: L1-L7
- `android/app/src/main/cpp/shared/coop/coop_level_restart.c`: L1-L373
- `android/app/src/main/cpp/shared/coop/coop_level_restart.h`: L1-L26

### GQ1-CHUNK-0273

- `android/app/src/main/java/com/dxxredux/app/multiplayer/NetworkEventsOverlay.kt`: L1-L281
- `android/app/src/main/java/com/dxxredux/app/multiplayer/NetworkEventsPanel.kt`: L1-L168

### GQ1-CHUNK-0274

- `android/app/src/main/java/com/dxxredux/app/ImportStorageGuard.kt`: L1-L114
- `android/app/src/main/java/com/dxxredux/app/lobby/LobbyProtocol.kt`: L1-L334
- `android/app/src/main/java/com/dxxredux/app/multiplayer/NetworkConstants.kt`: L1-L52

### GQ1-CHUNK-0278

- `d1/2d/bitblt.c`: diff hunks 1-8, new L387-L412
- `d1/2d/bitmap.c`: diff hunks 1-4, new L34-L272
- `d1/2d/clip.h`: diff hunks 1-2, new L1-L146
- `d1/2d/font.c`: diff hunks 1-13, new L39-L1017
- `d1/2d/palette.c`: diff hunks 1-3, new L87-L122
- `d1/2d/rect.c`: diff hunks 1-3, new L31-L41

### GQ1-CHUNK-0280

- `d1/arch/include/digi_mixer_music.h`: diff hunks 1-1, new L18-L18
- `d1/arch/include/joy.h`: diff hunks 1-5, new L20-L43
- `d1/arch/include/jukebox.h`: diff hunks 1-1, new L15-L18
- `d1/arch/include/window.h`: diff hunks 1-1, new L36-L41
- `d1/arch/ogl/gr.c`: diff hunks 1-25, new L58-L932

### GQ1-CHUNK-0281

- `d1/arch/ogl/ogl.c`: diff hunks 108-132, new L3010-L3744
- `d1/arch/ogl/oglprog.c`: diff hunks 1-24, new L3-L216
- `d1/arch/ogl/oglprog.h`: diff hunks 1-2, new L1-L18
- `d1/arch/sdl/digi_mixer.c`: diff hunks 1-18, new L19-L254
- `d1/arch/sdl/digi.c`: diff hunks 1-1, new L118-L125

### GQ1-CHUNK-0282

- `d1/arch/sdl/event.c`: diff hunks 1-12, new L17-L279
- `d1/arch/sdl/gr.c`: diff hunks 1-9, new L11-L311
- `d1/arch/sdl/joy.c`: diff hunks 1-26, new L5-L516
- `d1/arch/sdl/jukebox.c`: diff hunks 1-8, new L16-L317
- `d1/arch/sdl/mouse.c`: diff hunks 1-4, new L134-L181
- `d1/arch/sdl/timer.c`: diff hunks 1-2, new L47-L59
- `d1/arch/sdl/window.c`: diff hunks 1-3, new L96-L234
- `d1/arch/win32/include/resource.h`: diff hunks 1-2, new L1-L22

### GQ1-CHUNK-0285

- `d1/include/3d.h`: diff hunks 1-1, new L219-L225
- `d1/include/args.h`: diff hunks 1-2, new L30-L56
- `d1/include/console.h`: diff hunks 1-2, new L35-L37
- `d1/include/dxxerror.h`: diff hunks 1-2, new L40-L58
- `d1/include/editor/kdefs.h`: diff hunks 1-2, new L1-L332
- `d1/include/fix.h`: diff hunks 1-2, new L1-L7
- `d1/include/gr.h`: diff hunks 1-1, new L330-L330
- `d1/include/grdef.h`: diff hunks 1-2, new L1-L120
- `d1/include/ignorecase.h`: diff hunks 1-2, new L1-L80
- `d1/include/internal.h`: diff hunks 1-2, new L37-L44
- `d1/include/loadgl.h`: diff hunks 1-3, new L392-L403
- `d1/include/maths.h`: diff hunks 1-3, new L12-L56
- `d1/include/ogl_init.h`: diff hunks 1-10, new L14-L129
- `d1/include/physfsx.h`: diff hunks 1-5, new L179-L221
- `d1/include/pngfile.h`: diff hunks 1-1, new L22-L38
- `d1/include/pstypes.h`: diff hunks 1-1, new L24-L24

### GQ1-CHUNK-0286

- `d1/include/rbaudio.h`: diff hunks 1-1, new L62-L72
- `d1/include/xmodel.h`: diff hunks 1-2, new L1-L28

### GQ1-CHUNK-0287

- `d1/main/state.h`: diff hunks 1-2, new L25-L46
- `d1/main/switch.c`: diff hunks 1-2, new L41-L151
- `d1/main/switch.h`: diff hunks 1-4, new L26-L98
- `d1/main/texmerge.c`: diff hunks 1-9, new L31-L214
- `d1/main/text.h`: diff hunks 1-5, new L442-L699
- `d1/main/titles.c`: diff hunks 1-23, new L55-L1294
- `d1/main/wall.c`: diff hunks 1-5, new L37-L1005
- `d1/main/wall.h`: diff hunks 1-11, new L27-L279
- `d1/main/weapon.c`: diff hunks 1-4, new L48-L726
- `d1/main/weapon.h`: diff hunks 1-1, new L139-L142

### GQ1-CHUNK-0288

- `d1/main/input_demo_start.h`: L1-L17
- `d1/main/kconfig.c`: diff hunks 1-34, new L56-L2084
- `d1/main/kconfig.h`: diff hunks 1-1, new L67-L73
- `d1/main/kmatrix.c`: diff hunks 1-7, new L50-L429
- `d1/main/laser.c`: diff hunks 1-28, new L45-L1654
- `d1/main/laser.h`: diff hunks 1-3, new L71-L110
- `d1/main/menu.c`: diff hunks 1-41, new L59-L2284
- `d1/main/menu.h`: diff hunks 1-1, new L25-L25
- `d1/main/mglobal.c`: diff hunks 1-1, new L95-L97
- `d1/main/mission.c`: diff hunks 1-2, new L116-L337
- `d1/main/mission.h`: diff hunks 1-3, new L44-L72

### GQ1-CHUNK-0289

- `d1/main/collide.c`: diff hunks 1-33, new L54-L1763
- `d1/main/collide.h`: diff hunks 1-1, new L45-L46
- `d1/main/config.c`: diff hunks 1-20, new L36-L401
- `d1/main/config.h`: diff hunks 1-3, new L47-L59
- `d1/main/console.c`: diff hunks 1-10, new L23-L342
- `d1/main/coop_save.h`: L1-L7
- `d1/main/coop_warp.h`: L1-L7
- `d1/main/digi.h`: diff hunks 1-1, new L97-L97
- `d1/main/effects.c`: diff hunks 1-2, new L57-L99
- `d1/main/effects.h`: diff hunks 1-1, new L66-L74
- `d1/main/endlevel.c`: diff hunks 1-22, new L60-L964
- `d1/main/fireball.c`: diff hunks 1-26, new L53-L1464
- `d1/main/fuelcen.c`: diff hunks 1-3, new L37-L501
- `d1/main/fuelcen.h`: diff hunks 1-5, new L27-L162
- `d1/main/fvi.c`: diff hunks 1-5, new L35-L941
- `d1/main/fvi.h`: diff hunks 1-3, new L109-L143

### GQ1-CHUNK-0290

- `d1/main/inferno.c`: diff hunks 1-19, new L58-L543
- `d1/main/input_demo_control_info.h`: L1-L65

### GQ1-CHUNK-0291

- `d1/main/multi.c`: diff hunks 1-54, new L62-L5727
- `d1/main/multi.h`: diff hunks 1-15, new L30-L742
- `d1/main/multibot.c`: diff hunks 1-7, new L41-L1173

### GQ1-CHUNK-0292

- `d1/main/input_demo_hooks.c`: L751-L1326
- `d1/main/input_demo_hooks.h`: L1-L90
- `d1/main/input_demo_start.c`: L1-L72

### GQ1-CHUNK-0293

- `d1/main/gameseq.c`: diff hunks 1-43, new L29-L1505
- `d1/main/gauges.c`: diff hunks 1-54, new L26-L4418
- `d1/main/hud.c`: diff hunks 1-21, new L34-L306
- `d1/main/hudmsg.h`: diff hunks 1-3, new L16-L22

### GQ1-CHUNK-0294

- `d1/main/playsave.c`: diff hunks 1-26, new L25-L2100
- `d1/main/playsave.h`: diff hunks 1-3, new L139-L211
- `d1/main/polyobj.c`: diff hunks 1-4, new L50-L536
- `d1/main/powerup.c`: diff hunks 1-11, new L189-L493

### GQ1-CHUNK-0295

- `d1/main/net_udp.c`: diff hunks 279-305, new L7859-L8181
- `d1/main/net_udp.h`: diff hunks 1-10, new L1-L227
- `d1/main/newdemo.c`: diff hunks 1-14, new L28-L3423
- `d1/main/newdemo.h`: diff hunks 1-1, new L109-L110

### GQ1-CHUNK-0296

- `d1/main/game.c`: diff hunks 1-50, new L32-L1683
- `d1/main/game.h`: diff hunks 1-5, new L35-L181
- `d1/main/gamecntl.c`: diff hunks 1-18, new L74-L1628
- `d1/main/gamefont.c`: diff hunks 1-1, new L118-L122
- `d1/main/gamerend.c`: diff hunks 1-12, new L59-L716
- `d1/main/gamesave.c`: diff hunks 1-16, new L24-L1433
- `d1/main/gameseg.c`: diff hunks 1-1, new L1808-L1808

### GQ1-CHUNK-0297

- `d1/main/newmenu.c`: diff hunks 129-130, new L3325-L3370
- `d1/main/newmenu.h`: diff hunks 1-2, new L115-L155
- `d1/main/object.c`: diff hunks 1-43, new L41-L2452
- `d1/main/object.h`: diff hunks 1-5, new L105-L555
- `d1/main/physics.c`: diff hunks 1-13, new L17-L969
- `d1/main/piggy.c`: diff hunks 1-10, new L48-L1142
- `d1/main/piggy.h`: diff hunks 1-1, new L139-L139

### GQ1-CHUNK-0298

- `d1/main/render.c`: diff hunks 1-38, new L57-L2140
- `d1/main/render.h`: diff hunks 1-1, new L46-L52
- `d1/main/scores.c`: diff hunks 1-2, new L47-L221
- `d1/main/screens.h`: diff hunks 1-1, new L121-L122
- `d1/main/secretarea.h`: L1-L42
- `d1/main/songs.c`: diff hunks 1-23, new L26-L501
- `d1/main/songs.h`: diff hunks 1-1, new L29-L29
- `d1/main/state.c`: diff hunks 1-11, new L37-L136

### GQ1-CHUNK-0299

- `d1/main/ai.c`: diff hunks 1-63, new L56-L3541
- `d1/main/ai.h`: diff hunks 1-3, new L78-L113
- `d1/main/aipath.c`: diff hunks 1-7, new L42-L1250
- `d1/main/automap.c`: diff hunks 1-35, new L63-L1319
- `d1/main/automap.h`: diff hunks 1-1, new L26-L58
- `d1/main/bm.c`: diff hunks 1-2, new L145-L163
- `d1/main/cntrlcen.c`: diff hunks 1-17, new L34-L510
- `d1/main/cntrlcen.h`: diff hunks 1-6, new L28-L100

### GQ1-CHUNK-0308

- `d1/misc/args.c`: diff hunks 1-19, new L123-L319
- `d1/misc/error.c`: diff hunks 1-3, new L23-L85
- `d1/misc/hmp.c`: diff hunks 1-2, new L16-L791
- `d1/misc/physfsx.c`: diff hunks 1-5, new L22-L292

### GQ1-CHUNK-0310

- `d1/xmodel/strfunc.h`: diff hunks 1-2, new L1-L93
- `d1/xmodel/tga.cpp`: diff hunks 1-1, new L742-L742
- `d1/xmodel/xmodel.cpp`: diff hunks 1-5, new L68-L218
- `d1/xmodel/xmodelnames.h`: diff hunks 1-2, new L1-L157

### GQ1-CHUNK-0311

- `d2/2d/bitblt.c`: diff hunks 1-9, new L30-L412
- `d2/2d/bitmap.c`: diff hunks 1-4, new L34-L276
- `d2/2d/clip.h`: diff hunks 1-2, new L1-L145
- `d2/2d/font.c`: diff hunks 1-13, new L39-L1017
- `d2/2d/palette.c`: diff hunks 1-3, new L40-L162
- `d2/2d/rect.c`: diff hunks 1-3, new L31-L41

### GQ1-CHUNK-0313

- `d2/arch/sdl/digi_mixer.c`: diff hunks 1-17, new L19-L251
- `d2/arch/sdl/digi.c`: diff hunks 1-1, new L121-L128
- `d2/arch/sdl/event.c`: diff hunks 1-12, new L17-L279
- `d2/arch/sdl/gr.c`: diff hunks 1-6, new L21-L312
- `d2/arch/sdl/joy.c`: diff hunks 1-28, new L5-L515
- `d2/arch/sdl/jukebox.c`: diff hunks 1-8, new L15-L315
- `d2/arch/sdl/mouse.c`: diff hunks 1-5, new L134-L198
- `d2/arch/sdl/timer.c`: diff hunks 1-2, new L47-L59
- `d2/arch/sdl/window.c`: diff hunks 1-3, new L96-L234
- `d2/arch/win32/include/resource.h`: diff hunks 1-2, new L1-L22

### GQ1-CHUNK-0314

- `d2/arch/ogl/ogl.c`: diff hunks 105-128, new L2991-L3851
- `d2/arch/ogl/oglprog.c`: diff hunks 1-24, new L3-L216
- `d2/arch/ogl/oglprog.h`: diff hunks 1-2, new L1-L18

### GQ1-CHUNK-0315

- `d2/arch/cocoa/SDLMain.h`: diff hunks 1-2, new L1-L16
- `d2/arch/include/android_surface.h`: L1-L20
- `d2/arch/include/digi_mixer_music.h`: diff hunks 1-1, new L18-L18
- `d2/arch/include/joy.h`: diff hunks 1-5, new L20-L43
- `d2/arch/include/jukebox.h`: diff hunks 1-1, new L15-L18
- `d2/arch/include/mouse.h`: diff hunks 1-1, new L44-L44
- `d2/arch/include/window.h`: diff hunks 1-1, new L36-L41
- `d2/arch/ogl/gr.c`: diff hunks 1-25, new L58-L939

### GQ1-CHUNK-0318

- `d2/include/3d.h`: diff hunks 1-1, new L193-L199
- `d2/include/args.h`: diff hunks 1-2, new L27-L54
- `d2/include/console.h`: diff hunks 1-2, new L35-L37
- `d2/include/dxxerror.h`: diff hunks 1-4, new L27-L60
- `d2/include/editor/kdefs.h`: diff hunks 1-2, new L1-L341
- `d2/include/fix.h`: diff hunks 1-2, new L1-L7
- `d2/include/gr.h`: diff hunks 1-1, new L317-L317
- `d2/include/grdef.h`: diff hunks 1-2, new L1-L63
- `d2/include/ignorecase.h`: diff hunks 1-2, new L1-L80
- `d2/include/internal.h`: diff hunks 1-2, new L37-L44
- `d2/include/interp.h`: diff hunks 1-1, new L20-L23
- `d2/include/loadgl.h`: diff hunks 1-8, new L462-L495
- `d2/include/maths.h`: diff hunks 1-3, new L12-L56
- `d2/include/ogl_init.h`: diff hunks 1-10, new L14-L129
- `d2/include/physfsx.h`: diff hunks 1-5, new L179-L221
- `d2/include/pngfile.h`: diff hunks 1-1, new L22-L38

### GQ1-CHUNK-0319

- `d2/include/pstypes.h`: diff hunks 1-1, new L24-L24
- `d2/include/rbaudio.h`: diff hunks 1-1, new L62-L72
- `d2/include/replay_debug_overlay.h`: L1-L27
- `d2/include/xmodel.h`: diff hunks 1-2, new L1-L28

### GQ1-CHUNK-0321

- `d2/main/newdemo.c`: diff hunks 61-61, new L4021-L4514
- `d2/main/newdemo.h`: diff hunks 1-3, new L104-L140

### GQ1-CHUNK-0322

- `d2/main/escort.c`: diff hunks 117-149, new L4087-L4681
- `d2/main/escort.h`: diff hunks 1-3, new L9-L126

### GQ1-CHUNK-0323

- `d2/main/newmenu.c`: diff hunks 129-132, new L3294-L3359
- `d2/main/newmenu.h`: diff hunks 1-5, new L115-L181

### GQ1-CHUNK-0324

- `d2/main/net_udp.c`: diff hunks 291-336, new L7535-L8393
- `d2/main/net_udp.h`: diff hunks 1-10, new L1-L227

### GQ1-CHUNK-0325

- `d2/main/input_demo_hooks.c`: L6751-L7138
- `d2/main/input_demo_hooks.h`: L1-L334

### GQ1-CHUNK-0326

- `d2/main/d1_in_d2.h`: L1-L66
- `d2/main/d1_pig_validation.c`: L1-L239
- `d2/main/d1_pig_validation.h`: L1-L18

### GQ1-CHUNK-0327

- `d2/main/multi.h`: diff hunks 1-21, new L63-L809
- `d2/main/multibot.c`: diff hunks 1-29, new L40-L1403
- `d2/main/multibot.h`: diff hunks 1-3, new L25-L56

### GQ1-CHUNK-0328

- `d2/main/game.c`: diff hunks 1-73, new L31-L2215
- `d2/main/game.h`: diff hunks 1-5, new L35-L195
- `d2/main/gamecntl.c`: diff hunks 1-34, new L46-L2328

### GQ1-CHUNK-0329

- `d2/main/ai.c`: diff hunks 90-105, new L2258-L2547
- `d2/main/ai.h`: diff hunks 1-5, new L106-L319
- `d2/main/ai2.c`: diff hunks 1-94, new L58-L2596

### GQ1-CHUNK-0330

- `d2/main/d1_save_translate.c`: L1501-L1882
- `d2/main/d1_save_translate.h`: L1-L104
- `d2/main/digi.h`: diff hunks 1-1, new L99-L99

### GQ1-CHUNK-0331

- `d2/main/object.h`: diff hunks 1-7, new L108-L589
- `d2/main/physics.c`: diff hunks 1-34, new L13-L1160
- `d2/main/piggy.c`: diff hunks 1-62, new L54-L2406
- `d2/main/piggy.h`: diff hunks 1-6, new L47-L132

### GQ1-CHUNK-0332

- `d2/main/cntrlcen.c`: diff hunks 1-18, new L47-L610
- `d2/main/cntrlcen.h`: diff hunks 1-7, new L28-L119
- `d2/main/collide.c`: diff hunks 1-107, new L55-L3321
- `d2/main/collide.h`: diff hunks 1-1, new L46-L48

### GQ1-CHUNK-0333

- `d2/main/aipath.c`: diff hunks 1-44, new L42-L1650
- `d2/main/automap.c`: diff hunks 1-80, new L63-L1840
- `d2/main/automap.h`: diff hunks 1-2, new L27-L63
- `d2/main/bm.c`: diff hunks 1-5, new L56-L243

### GQ1-CHUNK-0334

- `d2/main/wall.c`: diff hunks 1-16, new L35-L1485
- `d2/main/wall.h`: diff hunks 1-13, new L27-L312
- `d2/main/weapon.c`: diff hunks 1-14, new L69-L1552
- `d2/main/weapon.h`: diff hunks 1-1, new L182-L185

### GQ1-CHUNK-0335

- `d2/main/input_demo_start.c`: L1-L81
- `d2/main/input_demo_start.h`: L1-L21
- `d2/main/kconfig.c`: diff hunks 1-37, new L56-L2136
- `d2/main/kconfig.h`: diff hunks 1-1, new L71-L92
- `d2/main/kmatrix.c`: diff hunks 1-7, new L53-L474

### GQ1-CHUNK-0336

- `d2/main/gauges.c`: diff hunks 1-61, new L39-L5040
- `d2/main/hud.c`: diff hunks 1-22, new L34-L332
- `d2/main/hudmsg.h`: diff hunks 1-3, new L16-L22
- `d2/main/inferno.c`: diff hunks 1-36, new L40-L635
- `d2/main/input_demo_control_info.h`: L1-L73

### GQ1-CHUNK-0337

- `d2/main/playsave.c`: diff hunks 1-27, new L54-L1839
- `d2/main/playsave.h`: diff hunks 1-3, new L132-L204
- `d2/main/polyobj.c`: diff hunks 1-6, new L43-L552
- `d2/main/polyobj.h`: diff hunks 1-1, new L114-L116
- `d2/main/powerup.c`: diff hunks 1-35, new L50-L731

### GQ1-CHUNK-0338

- `d2/main/config.c`: diff hunks 1-20, new L36-L424
- `d2/main/config.h`: diff hunks 1-3, new L48-L61
- `d2/main/console.c`: diff hunks 1-10, new L23-L343
- `d2/main/controls.c`: diff hunks 1-10, new L40-L279
- `d2/main/coop_save.h`: L1-L7
- `d2/main/coop_warp.h`: L1-L7

### GQ1-CHUNK-0339

- `d2/main/fireball.c`: diff hunks 1-50, new L46-L1799
- `d2/main/fireball.h`: diff hunks 1-2, new L24-L77
- `d2/main/fuelcen.c`: diff hunks 1-3, new L34-L523
- `d2/main/fuelcen.h`: diff hunks 1-6, new L26-L174
- `d2/main/fvi.c`: diff hunks 1-10, new L37-L1262
- `d2/main/fvi.h`: diff hunks 1-3, new L55-L89

### GQ1-CHUNK-0340

- `d2/main/d1_custom.c`: L751-L894
- `d2/main/d1_custom.h`: L1-L26
- `d2/main/d1_in_d2_input_demo.c`: L1-L22
- `d2/main/d1_in_d2_input_demo.h`: L1-L15
- `d2/main/d1_in_d2_semantics.c`: L1-L40
- `d2/main/d1_in_d2_semantics.h`: L1-L18

### GQ1-CHUNK-0341

- `d2/main/render.c`: diff hunks 1-48, new L54-L2568
- `d2/main/render.h`: diff hunks 1-1, new L48-L55
- `d2/main/scores.c`: diff hunks 1-3, new L47-L249
- `d2/main/secretarea.h`: L1-L42
- `d2/main/songs.c`: diff hunks 1-30, new L26-L529
- `d2/main/songs.h`: diff hunks 1-1, new L29-L29
- `d2/main/state.c`: diff hunks 1-12, new L37-L155

### GQ1-CHUNK-0342

- `d2/main/laser.h`: diff hunks 1-3, new L102-L155
- `d2/main/lighting.c`: diff hunks 1-3, new L44-L537
- `d2/main/menu.c`: diff hunks 1-44, new L59-L2327
- `d2/main/menu.h`: diff hunks 1-1, new L25-L25
- `d2/main/mglobal.c`: diff hunks 1-1, new L96-L98
- `d2/main/mission.c`: diff hunks 1-12, new L39-L1148
- `d2/main/mission.h`: diff hunks 1-3, new L44-L99
- `d2/main/movie.c`: diff hunks 1-20, new L37-L531

### GQ1-CHUNK-0343

- `d2/main/gamefont.c`: diff hunks 1-1, new L117-L121
- `d2/main/gamemine.c`: diff hunks 1-2, new L46-L50
- `d2/main/gamepal.c`: diff hunks 1-2, new L63-L103
- `d2/main/gamepal.h`: diff hunks 1-1, new L26-L26
- `d2/main/gamerend.c`: diff hunks 1-25, new L53-L1212
- `d2/main/gamesave.c`: diff hunks 1-20, new L24-L1636
- `d2/main/gameseg.c`: diff hunks 1-1, new L1890-L1890
- `d2/main/gameseq.c`: diff hunks 1-59, new L29-L2131

### GQ1-CHUNK-0344

- `d2/main/state.c`: diff hunks 108-127, new L3184-L3749
- `d2/main/state.h`: diff hunks 1-2, new L24-L47
- `d2/main/switch.c`: diff hunks 1-15, new L47-L662
- `d2/main/switch.h`: diff hunks 1-6, new L26-L138
- `d2/main/texmerge.c`: diff hunks 1-9, new L30-L213
- `d2/main/text.h`: diff hunks 1-4, new L466-L746
- `d2/main/thief_network_policy.h`: L1-L19
- `d2/main/titles.c`: diff hunks 1-30, new L55-L1621

### GQ1-CHUNK-0345

- `d2/main/dxa_metadata_patch.cpp`: L751-L1060
- `d2/main/dxa_metadata_patch.h`: L1-L7
- `d2/main/effects.c`: diff hunks 1-2, new L57-L99
- `d2/main/effects.h`: diff hunks 1-1, new L66-L74
- `d2/main/endlevel.c`: diff hunks 1-26, new L48-L1109
- `d2/main/escort_exit_policy.h`: L1-L15
- `d2/main/escort_owner_policy.c`: L1-L102
- `d2/main/escort_owner_policy.h`: L1-L40
- `d2/main/escort.c`: diff hunks 1-4, new L59-L127

### GQ1-CHUNK-0378

- `d2/misc/args.c`: diff hunks 1-19, new L124-L335
- `d2/misc/error.c`: diff hunks 1-3, new L23-L85
- `d2/misc/hmp.c`: diff hunks 1-2, new L16-L790
- `d2/misc/physfsx.c`: diff hunks 1-5, new L22-L297

### GQ1-CHUNK-0380

- `d2/xmodel/strfunc.h`: diff hunks 1-2, new L1-L93
- `d2/xmodel/tga.cpp`: diff hunks 1-1, new L742-L742
- `d2/xmodel/xmodel.cpp`: diff hunks 1-5, new L60-L218
- `d2/xmodel/xmodelnames.h`: diff hunks 1-2, new L1-L317

### GQ1-CHUNK-0387

- `d1/arch/cocoa/CMakeLists.txt`: diff hunks 1-1, new L1-L1
- `d1/arch/ogl/CMakeLists.txt`: diff hunks 1-4, new L1-L21
- `d1/arch/sdl/CMakeLists.txt`: diff hunks 1-7, new L1-L40
- `d1/arch/win32/CMakeLists.txt`: diff hunks 1-3, new L1-L8
- `d1/arch/x11/CMakeLists.txt`: diff hunks 1-3, new L1-L8

### GQ1-CHUNK-0400

- `d2/arch/cocoa/CMakeLists.txt`: diff hunks 1-1, new L1-L1
- `d2/arch/ogl/CMakeLists.txt`: diff hunks 1-4, new L1-L21
- `d2/arch/sdl/CMakeLists.txt`: diff hunks 1-7, new L1-L40
- `d2/arch/win32/CMakeLists.txt`: diff hunks 1-2, new L1-L8
- `d2/arch/x11/CMakeLists.txt`: diff hunks 1-2, new L1-L8

### GQ1-CHUNK-0411

- `server/.gitignore`: L1-L13
- `server/deploy_build.sh`: L1-L281
- `server/deploy_service.sh`: L1-L66
- `server/generate_lan_cert.sh`: L1-L48
- `server/run_nat_tests.ps1`: L1-L17
- `server/rust_dependency_lint.sh`: L1-L22
- `server/rust_lint.sh`: L1-L19
- `server/rust_upgrade.sh`: L1-L24
- `server/update_all.sh`: L1-L17

### GQ1-CHUNK-0415

- `android/app/src/main/cpp/shared/test_music_name_table.cpp`: L1-L77
- `android/app/src/main/cpp/shared/test_music_track_json.c`: L1-L54
- `android/app/src/main/cpp/shared/test_physfsx_android_setup.c`: L1-L260
- `android/app/src/main/cpp/shared/test_rewind_file.c`: L1-L132
- `android/app/src/main/cpp/shared/test_rgba8888.c`: L1-L49
- `android/app/src/main/cpp/shared/test_saf_io_contract.c`: L1-L25

### GQ1-CHUNK-0416

- `android/app/src/main/cpp/shared/test_android_audio_format.c`: L1-L58
- `android/app/src/main/cpp/shared/test_bounded_music_read.c`: L1-L62
- `android/app/src/main/cpp/shared/test_classic_demo_wall_validation.c`: L1-L34
- `android/app/src/main/cpp/shared/test_gles3_shim_array_sources.c`: L1-L77
- `android/app/src/main/cpp/shared/test_hog_midi_catalog.c`: L1-L116
- `android/app/src/main/cpp/shared/test_input_demo_limits.c`: L1-L43
- `android/app/src/main/cpp/shared/test_merged_wall_geometry_hit.c`: L1-L54
- `android/app/src/main/cpp/shared/test_midi_seek_timeline.c`: L1-L265
- `android/app/src/main/cpp/shared/test_music_decode_limits.c`: L1-L36

### GQ1-CHUNK-0418

- `android/app/src/test/java/com/dxxredux/app/CustomAudioImportStorageTest.kt`: L1-L83
- `android/app/src/test/java/com/dxxredux/app/DiscDatabaseContractTest.kt`: L1-L105
- `android/app/src/test/java/com/dxxredux/app/ImportStorageGuardTest.kt`: L1-L31
- `android/app/src/test/java/com/dxxredux/app/LobbyProtocolStartOptionsTest.kt`: L1-L59

### GQ1-CHUNK-0419

- `android/tests/test_playsave_layout.c`: L1-L131
- `android/tests/test_playsave_text.c`: L1-L113
- `android/tests/test_playsave_transaction.c`: L1-L107
- `android/tests/test_thief_network_policy.c`: L1-L65

### GQ1-CHUNK-0425

- `.vscode/c_cpp_properties.json`: L1-L63
- `.vscode/extensions.json`: L1-L18
- `.vscode/settings.json`: L1-L43

### GQ1-CHUNK-0426

- `android/acoustid_config.json5.example`: L1-L18
- `android/keystore.properties.example`: L1-L15

### GQ1-CHUNK-0427

- `android/app/src/main/assets/configs/controller/default.json`: L1-L33
- `android/app/src/main/assets/configs/touch/advanced.json`: L1-L241
- `android/app/src/main/assets/configs/touch/claw.json`: L1-L205
- `android/app/src/main/assets/configs/touch/controller_menus.json`: L1-L41
- `android/app/src/main/assets/configs/touch/simple.json`: L1-L84
- `android/app/src/main/assets/fingerprint_config.json5`: L1-L15

### GQ1-CHUNK-0433

- `game_data/CD images/Descent II (Europe) (v1.1)/track_fingerprints.json`: L1-L79
- `game_data/CD images/Descent II (Europe)/track_fingerprints.json`: L1-L115
- `game_data/CD images/Descent II (USA) (3-Level Interactive Preview)/track_fingerprints.json`: L1-L7
- `game_data/CD images/Descent II (USA) (Alt)/track_fingerprints.json`: L1-L115
- `game_data/CD images/Descent II (USA) (Rerelease)/track_fingerprints.json`: L1-L79
- `game_data/CD images/Descent II (USA) (v1.1)/track_fingerprints.json`: L1-L79
- `game_data/CD images/Descent II (USA)/track_fingerprints.json`: L1-L115
- `game_data/CD images/Descent II Infinite Abyss/track_fingerprints.json`: L1-L79
- `game_data/CD images/Descent II Infinite Abyss/track_hashes.json`: L1-L12
- `game_data/CD images/Dimensions for Descent (USA)/track_fingerprints.json`: L1-L7

### GQ1-CHUNK-0434

- `game_data/CD images/d2 mac/track_fingerprints.json`: L1-L142
- `game_data/CD images/d2 mac/track_hashes.json`: L1-L18
- `game_data/CD images/Descent - Anniversary Edition (Brazil) (Covermount)/track_fingerprints.json`: L1-L7
- `game_data/CD images/Descent - Anniversary Edition (USA)/track_fingerprints.json`: L1-L7
- `game_data/CD images/Descent - Destination Saturn (USA)/track_fingerprints.json`: L1-L7
- `game_data/CD images/Descent - Levels of the World (USA)/track_fingerprints.json`: L1-L7
- `game_data/CD images/Descent - Mac macplay/track_fingerprints.json`: L1-L124
- `game_data/CD images/Descent - Mac macplay/track_hashes.json`: L1-L16
- `game_data/CD images/Descent - Test Flight (USA)/track_fingerprints.json`: L1-L7
- `game_data/CD images/Descent (Europe) (Alt)/track_fingerprints.json`: L1-L7
- `game_data/CD images/Descent (Europe)/track_fingerprints.json`: L1-L7
- `game_data/CD images/Descent (USA)/track_fingerprints.json`: L1-L7
- `game_data/CD images/Descent I and II - The Definitive Collection (Europe) (Disc 1)/track_fingerprints.json`: L1-L7
- `game_data/CD images/Descent I and II - The Definitive Collection (Europe) (Disc 2)/track_fingerprints.json`: L1-L79
- `game_data/CD images/Descent I and II - The Definitive Collection (Europe) (Disc 3)/track_fingerprints.json`: L1-L70
- `game_data/CD images/Descent I and II - The Definitive Collection (USA) (Disc 1)/track_fingerprints.json`: L1-L7

### GQ1-CHUNK-0435

- `game_data/CD images/Descent I and II - The Definitive Collection (USA) (Disc 2)/track_fingerprints.json`: L1-L79
- `game_data/CD images/Descent I and II - The Definitive Collection (USA) (Disc 3)/track_fingerprints.json`: L1-L70
- `game_data/CD images/Descent II - Destination Quartzon (Europe)/track_fingerprints.json`: L1-L115
- `game_data/CD images/Descent II - Destination Quartzon (USA) (Diamond OEM)/track_fingerprints.json`: L1-L115
- `game_data/CD images/Descent II - Destination Quartzon (USA) (Logitech OEM)/track_fingerprints.json`: L1-L115
- `game_data/CD images/Descent II - Destination Quartzon (USA)/track_fingerprints.json`: L1-L115
- `game_data/CD images/Descent II - Destination Quartzon 3D (Europe)/track_fingerprints.json`: L1-L46
- `game_data/CD images/Descent II - The Vertigo Series (USA)/track_fingerprints.json`: L1-L70

### GQ1-CHUNK-0438

- `game_data/gog installers/descent_2_enUS_1_0_51877_regression.json5`: L1-L30
- `game_data/gog installers/descent_enUS_1_0_35122_regression.json5`: L1-L27
- `game_data/gog installers/setup_descent_1.4a_(16596)_regression.json5`: L1-L27
- `game_data/gog installers/setup_descent_2_1.1_(16596)_regression.json5`: L1-L30

### GQ1-CHUNK-0440

- `game_data/music/D1 macplay mp3/chromaprint_info.json5`: L1-L20
- `game_data/music/D1 MIDI mp3 ARACHNO/chromaprint_info.json5`: L1-L34
- `game_data/music/D1 MIDI mp3 MU80/chromaprint_info.json5`: L1-L34
- `game_data/music/D1 MIDI mp3 opl3/chromaprint_info.json5`: L1-L38
- `game_data/music/D1 MIDI mp3 sc55/chromaprint_info.json5`: L1-L60
- `game_data/music/D1 MIDI mp3 SC88/chromaprint_info.json5`: L1-L34
- `game_data/music/D1 MIDI mp3 SC88Pro/chromaprint_info.json5`: L1-L34
- `game_data/music/D1 MIDI mp3/chromaprint_info.json5`: L1-L34
- `game_data/music/D1 playstation mp3/chromaprint_info.json5`: L1-L24
- `game_data/music/D1 sunspire remix/chromaprint_info.json5`: L1-L9
- `game_data/music/D2 infinite abyss redbook mp3/chromaprint_info.json5`: L1-L15
- `game_data/music/D2 macplay mp3/chromaprint_info.json5`: L1-L22
- `game_data/music/D2 MIDI mp3 ARACHNO/chromaprint_info.json5`: L1-L16
- `game_data/music/D2 MIDI mp3 MU80/chromaprint_info.json5`: L1-L16
- `game_data/music/D2 midi mp3 opl3/chromaprint_info.json5`: L1-L14
- `game_data/music/D2 midi mp3 sc55/chromaprint_info.json5`: L1-L26

### GQ1-CHUNK-0441

- `game_data/music/D2 MIDI mp3 SC88/chromaprint_info.json5`: L1-L16
- `game_data/music/D2 MIDI mp3 SC88Pro/chromaprint_info.json5`: L1-L16
- `game_data/music/D2 mp3/chromaprint_info.json5`: L1-L30
- `game_data/music/D2 redbook mp3 rips/chromaprint_info.json5`: L1-L21
- `game_data/music/D2 vampyro mp3/chromaprint_info.json5`: L1-L10
- `game_data/music/D2 vertigo mp3/chromaprint_info.json5`: L1-L14
- `game_data/music/Descent Maximum (ps1) mp3/chromaprint_info.json5`: L1-L22

### GQ1-CHUNK-0446

- `android/app/src/main/java/com/dxxredux/app/LoadingProgressOverlayView.kt`: L1-L166
- `android/app/src/main/java/com/dxxredux/app/lobby/LobbyDiagnostics.kt`: L1-L58

### GQ1-CHUNK-0447

- `android/app/src/main/java/com/dxxredux/app/WeaponState.kt`: L1-L77
- `android/app/src/main/java/com/dxxredux/app/WeaponWheelLabels.kt`: L1-L546

### GQ1-CHUNK-0448

- `android/app/src/main/java/com/dxxredux/app/ExitButtonView.kt`: L1-L101
- `android/app/src/main/java/com/dxxredux/app/FileProviderGrantStore.kt`: L1-L148

### GQ1-CHUNK-0449

- `android/app/src/main/java/com/dxxredux/app/FingerprintBridge.kt`: L1-L510
- `android/app/src/main/java/com/dxxredux/app/GameActivityState.kt`: L1-L76

### GQ1-CHUNK-0450

- `android/app/src/main/java/com/dxxredux/app/ControllerConfigSlotRepository.kt`: L1-L201
- `android/app/src/main/java/com/dxxredux/app/ControllerConfigStore.kt`: L1-L534

### GQ1-CHUNK-0451

- `android/app/src/main/java/com/dxxredux/app/multiplayer/FriendsTab.kt`: L1-L291
- `android/app/src/main/java/com/dxxredux/app/multiplayer/IceProgressPanel.kt`: L1-L253

### GQ1-CHUNK-0452

- `android/app/src/main/java/com/dxxredux/app/CustomAudioSetManager.kt`: L1-L375
- `android/app/src/main/java/com/dxxredux/app/D2HamPatchSchema.kt`: L1-L225

### GQ1-CHUNK-0453

- `android/app/src/main/java/com/dxxredux/app/SetupFileImport.kt`: L751-L760
- `android/app/src/main/java/com/dxxredux/app/SetupGameFiles.kt`: L1-L451

### GQ1-CHUNK-0454

- `android/app/src/main/java/com/dxxredux/app/SetupAutomationApi.kt`: L751-L1086
- `android/app/src/main/java/com/dxxredux/app/SetupConfigFiles.kt`: L1-L297

### GQ1-CHUNK-0455

- `android/app/src/main/java/com/dxxredux/app/multiplayer/LanDiscoveryTab.kt`: L751-L1101
- `android/app/src/main/java/com/dxxredux/app/multiplayer/LobbyScreen.kt`: L1-L266

### GQ1-CHUNK-0456

- `android/app/src/main/java/com/dxxredux/app/TouchEditorPage.kt`: L3751-L4042
- `android/app/src/main/java/com/dxxredux/app/TouchLayoutRepository.kt`: L1-L373

### GQ1-CHUNK-0457

- `android/app/src/main/java/com/dxxredux/app/DebugLog.kt`: L1-L405
- `android/app/src/main/java/com/dxxredux/app/DebugLogCategory.kt`: L1-L33
- `android/app/src/main/java/com/dxxredux/app/DemoInstallerPackages.kt`: L1-L112

### GQ1-CHUNK-0458

- `android/app/src/main/java/com/dxxredux/app/SkipButtonView.kt`: L1-L282
- `android/app/src/main/java/com/dxxredux/app/SlowdownCapturePrefs.kt`: L1-L3
- `android/app/src/main/java/com/dxxredux/app/StartGameButtonView.kt`: L1-L104

### GQ1-CHUNK-0459

- `android/app/src/main/java/com/dxxredux/app/multiplayer/TvButtons.kt`: L1-L115
- `android/app/src/main/java/com/dxxredux/app/multiplayer/UdpReconnectIdentity.kt`: L1-L97
- `android/app/src/main/java/com/dxxredux/app/multiplayer/UpnpClient.kt`: L1-L341

### GQ1-CHUNK-0460

- `android/app/src/main/java/com/dxxredux/app/multiplayer/MultiplayerCallsigns.kt`: L1-L265
- `android/app/src/main/java/com/dxxredux/app/multiplayer/MultiplayerForegroundService.kt`: L1-L250
- `android/app/src/main/java/com/dxxredux/app/multiplayer/MultiplayerResumeOffer.kt`: L1-L67

### GQ1-CHUNK-0461

- `android/app/src/main/java/com/dxxredux/app/multiplayer/MatchmakingState.kt`: L1-L239
- `android/app/src/main/java/com/dxxredux/app/multiplayer/MissionPicker.kt`: L1-L292
- `android/app/src/main/java/com/dxxredux/app/multiplayer/MultiplayerBackgroundDeadline.kt`: L1-L31

### GQ1-CHUNK-0462

- `android/app/src/main/java/com/dxxredux/app/multiplayer/ConnectivityChecker.kt`: L1-L168
- `android/app/src/main/java/com/dxxredux/app/multiplayer/ControllerFocus.kt`: L1-L188
- `android/app/src/main/java/com/dxxredux/app/multiplayer/CoopDesyncLog.kt`: L1-L10

### GQ1-CHUNK-0463

- `android/app/src/main/java/com/dxxredux/app/LevelPreviewActivity.kt`: L1-L519
- `android/app/src/main/java/com/dxxredux/app/LevelPreviewRequestStore.kt`: L1-L147
- `android/app/src/main/java/com/dxxredux/app/LevelPreviewReturnRefreshGate.kt`: L1-L18

### GQ1-CHUNK-0464

- `android/app/src/main/java/com/dxxredux/app/GamepadButtonEdgeTracker.kt`: L1-L19
- `android/app/src/main/java/com/dxxredux/app/GogImportBridge.kt`: L1-L141
- `android/app/src/main/java/com/dxxredux/app/GraphicsDebugPrefs.kt`: L1-L3

### GQ1-CHUNK-0465

- `android/app/src/main/java/com/dxxredux/app/DpadFocusUtils.kt`: L1-L231
- `android/app/src/main/java/com/dxxredux/app/DxaTextureScanner.kt`: L1-L297
- `android/app/src/main/java/com/dxxredux/app/DxxReduxApp.kt`: L1-L62

### GQ1-CHUNK-0466

- `android/app/src/main/java/com/dxxredux/app/DiscIdentifier.kt`: L1-L258
- `android/app/src/main/java/com/dxxredux/app/DiscImportBridge.kt`: L1-L390
- `android/app/src/main/java/com/dxxredux/app/DormancyDiagnostics.kt`: L1-L23

### GQ1-CHUNK-0467

- `android/app/src/main/java/com/dxxredux/app/ConfigImportLoader.kt`: L1-L186
- `android/app/src/main/java/com/dxxredux/app/ConfigSlotDialog.kt`: L1-L326
- `android/app/src/main/java/com/dxxredux/app/ConfigSlotModel.kt`: L1-L29

### GQ1-CHUNK-0468

- `android/app/src/main/java/com/dxxredux/app/AtomicFilePublication.kt`: L1-L201
- `android/app/src/main/java/com/dxxredux/app/AudioFilePreviewDialog.kt`: L1-L241
- `android/app/src/main/java/com/dxxredux/app/AudioPreviewResourceOwner.kt`: L1-L37

### GQ1-CHUNK-0469

- `android/app/src/main/java/com/dxxredux/app/TouchLayoutSlotRepository.kt`: L1-L198
- `android/app/src/main/java/com/dxxredux/app/TouchMouseAcceleration.kt`: L1-L54
- `android/app/src/main/java/com/dxxredux/app/TouchOverlayColors.kt`: L1-L12

### GQ1-CHUNK-0470

- `android/app/src/main/java/com/dxxredux/app/AdvancedSettingsPage.kt`: L2251-L2679
- `android/app/src/main/java/com/dxxredux/app/AndroidGameFileExtensions.kt`: L1-L15
- `android/app/src/main/java/com/dxxredux/app/AssetManifest.kt`: L1-L263

### GQ1-CHUNK-0471

- `android/app/src/main/java/com/dxxredux/app/NativePilotPatcher.kt`: L1-L141
- `android/app/src/main/java/com/dxxredux/app/NativePilotPreferences.kt`: L1-L433
- `android/app/src/main/java/com/dxxredux/app/NativeTextureLookupCache.kt`: L1-L35
- `android/app/src/main/java/com/dxxredux/app/OwnedCacheDirectories.kt`: L1-L72

### GQ1-CHUNK-0472

- `android/app/src/main/java/com/dxxredux/app/ControllerDisplayDevice.kt`: L1-L43
- `android/app/src/main/java/com/dxxredux/app/ControllerLongPressDetector.kt`: L1-L245
- `android/app/src/main/java/com/dxxredux/app/ControllerOverlayNavigation.kt`: L1-L64
- `android/app/src/main/java/com/dxxredux/app/CrashLog.kt`: L1-L375

### GQ1-CHUNK-0473

- `android/app/src/main/java/com/dxxredux/app/multiplayer/NetLog.kt`: L1-L73
- `android/app/src/main/java/com/dxxredux/app/multiplayer/RecentAddressPrefs.kt`: L1-L74
- `android/app/src/main/java/com/dxxredux/app/multiplayer/RuntimeGameStateBridge.kt`: L1-L333
- `android/app/src/main/java/com/dxxredux/app/multiplayer/StunClient.kt`: L1-L251

### GQ1-CHUNK-0474

- `android/app/src/main/java/com/dxxredux/app/MusicControlPanel.kt`: L751-L918
- `android/app/src/main/java/com/dxxredux/app/MusicDiagnosticGeometry.kt`: L1-L46
- `android/app/src/main/java/com/dxxredux/app/MusicNameSidecar.kt`: L1-L65
- `android/app/src/main/java/com/dxxredux/app/MusicOverlaySourceOption.kt`: L1-L48

### GQ1-CHUNK-0475

- `android/app/src/main/java/com/dxxredux/app/AutoselectEditorPage.kt`: L751-L924
- `android/app/src/main/java/com/dxxredux/app/BinHexDecoder.kt`: L1-L182
- `android/app/src/main/java/com/dxxredux/app/CdAudioSourceNormalization.kt`: L1-L185
- `android/app/src/main/java/com/dxxredux/app/CdPreviewBridge.kt`: L1-L133

### GQ1-CHUNK-0476

- `android/app/src/main/java/com/dxxredux/app/HumanReadableConfig.kt`: L751-L763
- `android/app/src/main/java/com/dxxredux/app/ImportChooserConfig.kt`: L1-L19
- `android/app/src/main/java/com/dxxredux/app/ImportLocationManager.kt`: L1-L428
- `android/app/src/main/java/com/dxxredux/app/ImportTreeScanner.kt`: L1-L195

### GQ1-CHUNK-0477

- `android/app/src/main/java/com/dxxredux/app/VideoInfoOverlay.kt`: L751-L1004
- `android/app/src/main/java/com/dxxredux/app/VisualReplacementPolicy.kt`: L1-L123
- `android/app/src/main/java/com/dxxredux/app/WarpButtonOverlay.kt`: L1-L197
- `android/app/src/main/java/com/dxxredux/app/WeaponAmmoStatus.kt`: L1-L127

### GQ1-CHUNK-0478

- `android/app/src/main/java/com/dxxredux/app/ModManager.kt`: L1501-L1920
- `android/app/src/main/java/com/dxxredux/app/multiplayer/ActiveGamesTab.kt`: L1-L89
- `android/app/src/main/java/com/dxxredux/app/multiplayer/ChatArea.kt`: L1-L99
- `android/app/src/main/java/com/dxxredux/app/multiplayer/ClientIdentity.kt`: L1-L38

### GQ1-CHUNK-0479

- `android/app/src/main/java/com/dxxredux/app/MusicPickerPage.kt`: L1501-L1966
- `android/app/src/main/java/com/dxxredux/app/NativeAutoselectPatcher.kt`: L1-L144
- `android/app/src/main/java/com/dxxredux/app/NativeFatalErrorStore.kt`: L1-L27
- `android/app/src/main/java/com/dxxredux/app/NativeMetaActions.kt`: L1-L34

### GQ1-CHUNK-0480

- `android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt`: L5251-L5405
- `android/app/src/main/java/com/dxxredux/app/TvButtons.kt`: L1-L114
- `android/app/src/main/java/com/dxxredux/app/TvDetection.kt`: L1-L16
- `android/app/src/main/java/com/dxxredux/app/UpdateChecker.kt`: L1-L135

### GQ1-CHUNK-0481

- `android/app/src/main/java/com/dxxredux/app/SafManifest.kt`: L1-L177
- `android/app/src/main/java/com/dxxredux/app/SafUriPermissions.kt`: L1-L130
- `android/app/src/main/java/com/dxxredux/app/SaveExplorerBridge.kt`: L1-L178
- `android/app/src/main/java/com/dxxredux/app/SaveMetadataLabels.kt`: L1-L28
- `android/app/src/main/java/com/dxxredux/app/ScrollIndicators.kt`: L1-L115

### GQ1-CHUNK-0482

- `android/app/src/main/java/com/dxxredux/app/PendingResumeLaunch.kt`: L1-L162
- `android/app/src/main/java/com/dxxredux/app/PilotPreferenceTransaction.kt`: L1-L51
- `android/app/src/main/java/com/dxxredux/app/RemainingTouchActions.kt`: L1-L215
- `android/app/src/main/java/com/dxxredux/app/ResumeSaveBridge.kt`: L1-L176
- `android/app/src/main/java/com/dxxredux/app/SafDescriptorStager.kt`: L1-L114

### GQ1-CHUNK-0483

- `android/app/src/main/java/com/dxxredux/app/MainActivity.kt`: L3751-L3982
- `android/app/src/main/java/com/dxxredux/app/MetadataLoadProgress.kt`: L1-L99
- `android/app/src/main/java/com/dxxredux/app/MidiBytesPreviewDialog.kt`: L1-L202
- `android/app/src/main/java/com/dxxredux/app/MidiEnumerationBridge.kt`: L1-L64
- `android/app/src/main/java/com/dxxredux/app/MidiPreviewBridge.kt`: L1-L100

### GQ1-CHUNK-0484

- `android/app/src/main/java/com/dxxredux/app/AcceptJoinButtonView.kt`: L1-L112
- `android/app/src/main/java/com/dxxredux/app/AcoustIdClient.kt`: L1-L235
- `android/app/src/main/java/com/dxxredux/app/AcoustIdConfiguration.kt`: L1-L23
- `android/app/src/main/java/com/dxxredux/app/AcoustIdLabelPolicy.kt`: L1-L86
- `android/app/src/main/java/com/dxxredux/app/AcoustIdPrefs.kt`: L1-L3
- `android/app/src/main/java/com/dxxredux/app/AdminTrayPolicy.kt`: L1-L277

### GQ1-CHUNK-0485

- `android/app/src/main/java/com/dxxredux/app/InputDemoManager.kt`: L1-L286
- `android/app/src/main/java/com/dxxredux/app/InputDemoPrefs.kt`: L1-L3
- `android/app/src/main/java/com/dxxredux/app/InputMixer.kt`: L1-L157
- `android/app/src/main/java/com/dxxredux/app/Json5.kt`: L1-L26
- `android/app/src/main/java/com/dxxredux/app/KnownVersions.kt`: L1-L85
- `android/app/src/main/java/com/dxxredux/app/LauncherDebugLog.kt`: L1-L17
- `android/app/src/main/java/com/dxxredux/app/LauncherFileCopy.kt`: L1-L138
- `android/app/src/main/java/com/dxxredux/app/LauncherFileLabels.kt`: L1-L19

### GQ1-CHUNK-0573

- `android/app/src/main/res/xml/backup_rules.xml`: L1-L4
- `android/app/src/main/res/xml/file_paths.xml`: L1-L9

### GQ1-CHUNK-0574

- `android/tools/etc2tool/etc2_limits.h`: L1-L105
- `android/tools/etc2tool/etc2tool.cpp`: L1-L384

### GQ1-CHUNK-0575

- `.clang-format`: L1-L46
- `.cmake-format.yaml`: L1-L43
- `.editorconfig`: L1-L43
- `.gitattributes`: L1-L2
- `.gitignore`: diff hunks 1-2, new L61-L132
- `run-linux-build.sh`: L1-L265

### GQ1-CHUNK-0577

- `android/install-aab.ps1`: L1-L223
- `android/PSScriptAnalyzerSettings.psd1`: L1-L63
- `android/regenerate_all_regression_data.ps1`: L1-L81

### GQ1-CHUNK-0578

- `android/Run-Emulator.ps1`: L1-L314
- `android/Run-TestMenu.ps1`: L1-L178
- `android/settings.gradle`: L1-L36

### GQ1-CHUNK-0579

- `android/.gitignore`: L1-L1
- `android/1_build-aab.ps1`: L1-L160
- `android/2_deploy-playstore.ps1`: L1-L420
- `android/build.gradle`: L1-L5
- `android/gradlew.bat`: L1-L69

### GQ1-CHUNK-0586

- `android/docker/nat-testbed/docker-compose.yml`: L1-L43
- `android/docker/nat-testbed/nat_proxy.py`: L1-L167

### GQ1-CHUNK-0587

- `android/get_deps/check-updates.ps1`: L1501-L1867
- `android/get_deps/get_all.sh`: L1-L144
- `android/get_deps/helpers/create_avd.sh`: L1-L113

### GQ1-CHUNK-0588

- `android/get_deps/helpers/get_unar.sh`: L1-L65
- `android/get_deps/helpers/Get-DepPlatform.ps1`: L1-L177
- `android/get_deps/helpers/platform.sh`: L1-L159
- `android/get_deps/helpers/resolve_dep_base.sh`: L1-L58
- `android/get_deps/helpers/safe_conf_value.ps1`: L1-L40
- `android/get_deps/helpers/verify_sha256.sh`: L1-L31
- `android/get_deps/update-powershell.ps1`: L1-L159

### GQ1-CHUNK-0589

- `android/get_deps/helpers/get_imagemagick.ps1`: L1-L73
- `android/get_deps/helpers/get_jdk.sh`: L1-L151
- `android/get_deps/helpers/get_ktlint.sh`: L1-L44
- `android/get_deps/helpers/get_linux_android_prereqs.sh`: L1-L6
- `android/get_deps/helpers/get_linux_build_prereqs.sh`: L1-L228
- `android/get_deps/helpers/get_mac_oracle_tools.ps1`: L1-L79
- `android/get_deps/helpers/get_ndk.sh`: L1-L49
- `android/get_deps/helpers/get_powershell_ubuntu.sh`: L1-L6

### GQ1-CHUNK-0590

- `android/get_deps/helpers/get_powershell.ps1`: L1-L131
- `android/get_deps/helpers/get_powershell.sh`: L1-L210
- `android/get_deps/helpers/get_rust.sh`: L1-L68
- `android/get_deps/helpers/get_sdk.sh`: L1-L86
- `android/get_deps/helpers/get_shellcheck.sh`: L1-L61
- `android/get_deps/helpers/get_shfmt.sh`: L1-L48
- `android/get_deps/helpers/get_soundfont.sh`: L1-L43
- `android/get_deps/helpers/get_stuffit.sh`: L1-L45

### GQ1-CHUNK-0591

- `android/get_deps/helpers/create_light_avds.ps1`: L1-L149
- `android/get_deps/helpers/finalize.sh`: L1-L70
- `android/get_deps/helpers/get_clang_format.sh`: L1-L93
- `android/get_deps/helpers/get_cmake_format.sh`: L1-L111
- `android/get_deps/helpers/get_cmake.sh`: L1-L62
- `android/get_deps/helpers/get_dosbox.sh`: L1-L79
- `android/get_deps/helpers/get_emulator.sh`: L1-L56
- `android/get_deps/helpers/get_fpcalc.ps1`: L1-L69
- `android/get_deps/helpers/get_gradle_wrapper.sh`: L1-L56

### GQ1-CHUNK-0594

- `android/helpers/json5.ps1`: L1-L122
- `android/helpers/kill-stale-emulators.ps1`: L1-L162

### GQ1-CHUNK-0595

- `android/helpers/normalize_json.py`: L1-L171
- `android/helpers/push_game_data.sh`: L1-L359

### GQ1-CHUNK-0596

- `android/helpers/summarize-profiling-log.ps1`: L1-L389
- `android/helpers/teardown_docker_nat.ps1`: L1-L44
- `android/helpers/verified_dependencies.ps1`: L1-L100

### GQ1-CHUNK-0597

- `android/helpers/setup_docker_nat.ps1`: L1-L165
- `android/helpers/standard_game_data.ps1`: L1-L56
- `android/helpers/stop-stale-formatters.ps1`: L1-L151
- `android/helpers/strip_input_demo_frame_state.ps1`: L1-L189

### GQ1-CHUNK-0598

- `android/helpers/gather-warnings.ps1`: L1-L92
- `android/helpers/generate-keystore.ps1`: L1-L48
- `android/helpers/generate-stuffit-corpus-manifests.ps1`: L1-L354
- `android/helpers/get-test-report-runtimes.ps1`: L1-L78
- `android/helpers/introspect.sh`: L1-L148

### GQ1-CHUNK-0599

- `android/helpers/diff_vs_upstream.ps1`: L1-L87
- `android/helpers/emu_health.ps1`: L1-L420
- `android/helpers/fingerprint_audio_results.ps1`: L1-L40
- `android/helpers/fingerprint_config.ps1`: L1-L36
- `android/helpers/fingerprint_source_identity.ps1`: L1-L41
- `android/helpers/gather-warnings-msvc.ps1`: L1-L84

### GQ1-CHUNK-0600

- `android/helpers/acoustid_title_match.ps1`: L1-L75
- `android/helpers/atomic_text_file.ps1`: L1-L23
- `android/helpers/build.sh`: L1-L23
- `android/helpers/cd_level_metadata_sources.ps1`: L1-L113
- `android/helpers/clean-old-artifacts.ps1`: L1-L369
- `android/helpers/collect_crash.ps1`: L1-L143

### GQ1-CHUNK-0601

- `android/helpers/run-cmake-lint.ps1`: L1-L109
- `android/helpers/run-ktlint.ps1`: L1-L128
- `android/helpers/run-psscriptanalyzer.ps1`: L1-L181
- `android/helpers/run-shellcheck.ps1`: L1-L120
- `android/helpers/run-shfmt.ps1`: L1-L130
- `android/helpers/set_vars.sh`: L1-L54

### GQ1-CHUNK-0602

- `android/helpers/regenerate_all_mission_metadata_host.ps1`: L751-L868
- `android/helpers/regenerate_all_mission_metadata.ps1`: L1-L66
- `android/helpers/retain-recent-artifacts.ps1`: L1-L43
- `android/helpers/run_automation.sh`: L1-L222
- `android/helpers/run_verified_python.py`: L1-L18
- `android/helpers/run-clang-format.ps1`: L1-L140
- `android/helpers/run-cmake-format.ps1`: L1-L126

### GQ1-CHUNK-0606

- `android/tools/decode_object_packets.py`: L1-L385
- `android/tools/etc2tool/CMakeLists.txt`: L1-L83

### GQ1-CHUNK-0608

- `cmake/input-demo-build-metadata.cmake`: L1-L53
- `cmake/input-demo-codec-deps.cmake`: L1-L32
- `cmake/ndk-auto.cmake`: L1-L98
- `cmake/platform-math.cmake`: L1-L5
- `cmake/vcpkg-auto.cmake`: L1-L102

### GQ1-CHUNK-0609

- `game_data/fingerprint_music_packs.ps1`: L1-L468
- `game_data/generate_game_data_index.ps1`: L1-L269

### GQ1-CHUNK-0610

- `game_data/hash_assets.ps1`: L1-L378
- `game_data/hash_disc_tracks.ps1`: L1-L259

### GQ1-CHUNK-0611

- `game_data/update_known_discs_albums.ps1`: L1-L477
- `game_data/update_known_discs_fingerprints.ps1`: L1-L141

### GQ1-CHUNK-0612

- `game_data/generate_regression_specs.ps1`: L1-L501
- `game_data/generate_source_manifest.ps1`: L1-L59

### GQ1-CHUNK-0613

- `game_data/manage_data.ps1`: L1-L356
- `game_data/run_all_cd_regressions.ps1`: L1-L124
- `game_data/update_all_fingerprints.ps1`: L1-L91

### GQ1-CHUNK-0615

- `game_data/mods/xfing/d2x_sp/verify-uud2sp-ham-patch-dxa.ps1`: L1-L195
- `game_data/mods/xfing/d2x_sp/verify-uud2sp-uud2tp-ham-composition.ps1`: L1-L406

### GQ1-CHUNK-0616

- `game_data/mods/xfing/d2x_sp/.gitignore`: L1-L4
- `game_data/mods/xfing/d2x_sp/convert-uud2sp-ham-patch-dxa.ps1`: L1-L256

### GQ1-CHUNK-0617

- `game_data/mods/.gitignore`: L1-L6
- `game_data/mods/d2x-xl/.gitignore`: L1-L4
- `game_data/mods/d2x-xl/convert_all.ps1`: L1-L404

### GQ1-CHUNK-0618

- `game_data/mods/d2x-xl/convert_d2xxl_textures.ps1`: L751-L1004
- `game_data/mods/d2x-xl/convert_progress_helpers.ps1`: L1-L72
- `game_data/mods/d2x-xl/d2xxl_pack_lib.ps1`: L1-L135
- `game_data/mods/d2x-xl/repack_d2xxl_docs_and_layout.ps1`: L1-L195
- `game_data/mods/xfing/.gitignore`: L1-L8

### GQ1-CHUNK-0628

- `d1_d2_ogl_diff.txt`: L901-L922
- `README.md`: diff hunks 1-1, new L61-L64
- `test.txt`: L1-L1

### GQ1-CHUNK-0631

- `android/outstanding_bugs.md`: L1-L239
- `android/privacy_policy.md`: L1-L6
- `android/README.md`: L1-L30

### GQ1-CHUNK-0636

- `android/tests/HOST_INPUT_DEMO_REPLAY.md`: L1-L52
- `android/tests/INPUT_DEMO_DESYNC_ANALYSIS.md`: L1-L123
- `android/tests/input_demo_determinism_fixtures.txt`: L1-L4
- `android/tests/input_demo_graphics_canaries.txt`: L1-L3

### GQ1-CHUNK-0637

- `game_data/game_data_index.txt`: L1-L25
- `game_data/SOURCE_FILES.txt`: L1-L240

### GQ1-CHUNK-0640

- `game_data/mods/d2x-xl/README.md`: L1-L21
- `game_data/mods/README.md`: L1-L26
- `game_data/mods/xfing/README.md`: L1-L19

### GQ1-CHUNK-0642

- `android/app/src/test/java/com/dxxredux/app/ControllerLongPressDetectorTest.kt`: L1-L139
- `android/app/src/test/java/com/dxxredux/app/ControllerMenuCycleTest.kt`: L1-L151
- `android/app/src/test/java/com/dxxredux/app/ControllerMenuTouchOverlayDefaultTest.kt`: L1-L65
- `android/app/src/test/java/com/dxxredux/app/CustomAudioSetManagerTest.kt`: L1-L46
- `android/app/src/test/java/com/dxxredux/app/D2HamPatchSchemaTest.kt`: L1-L34
- `android/app/src/test/java/com/dxxredux/app/DebugLogCategoryTest.kt`: L1-L12
- `android/app/src/test/java/com/dxxredux/app/DebugLogFileFingerprintTest.kt`: L1-L25
- `android/app/src/test/java/com/dxxredux/app/DemoInstallerOfferTest.kt`: L1-L62
- `android/app/src/test/java/com/dxxredux/app/DemoInstallerPackagesTest.kt`: L1-L64
- `android/app/src/test/java/com/dxxredux/app/DiscIdentifierHashTest.kt`: L1-L63
- `android/app/src/test/java/com/dxxredux/app/DiscImportHoistTest.kt`: L1-L110

### GQ1-CHUNK-0643

- `android/app/src/test/java/com/dxxredux/app/GuidebotLockedWheelTest.kt`: L1-L188
- `android/app/src/test/java/com/dxxredux/app/GyroToggleConfigTest.kt`: L1-L334
- `android/app/src/test/java/com/dxxredux/app/ImportChooserConfigTest.kt`: L1-L28
- `android/app/src/test/java/com/dxxredux/app/ImportLocationMigrateTest.kt`: L1-L211

### GQ1-CHUNK-0644

- `android/app/src/test/java/com/dxxredux/app/SaveExplorerTest.kt`: L1-L301
- `android/app/src/test/java/com/dxxredux/app/ScrollStripTest.kt`: L1-L154
- `android/app/src/test/java/com/dxxredux/app/SettingsChildOverlayControllerTest.kt`: L1-L105
- `android/app/src/test/java/com/dxxredux/app/SetupAutomationScrollTest.kt`: L1-L44
- `android/app/src/test/java/com/dxxredux/app/SetupLaunchReadinessTest.kt`: L1-L268

### GQ1-CHUNK-0645

- `android/app/src/test/java/com/dxxredux/app/DxaTextureScannerTest.kt`: L1-L199
- `android/app/src/test/java/com/dxxredux/app/FileProviderGrantStoreTest.kt`: L1-L99
- `android/app/src/test/java/com/dxxredux/app/FileSetMigrationTest.kt`: L1-L97
- `android/app/src/test/java/com/dxxredux/app/FingerprintMatchingConfigTest.kt`: L1-L55
- `android/app/src/test/java/com/dxxredux/app/GameFileFormatsTest.kt`: L1-L131

### GQ1-CHUNK-0646

- `android/app/src/test/java/com/dxxredux/app/RemainingKeyTouchActionsTest.kt`: L1-L487
- `android/app/src/test/java/com/dxxredux/app/ResumeSavePanelTest.kt`: L1-L166
- `android/app/src/test/java/com/dxxredux/app/RewindPreferencePolicyTest.kt`: L1-L18
- `android/app/src/test/java/com/dxxredux/app/SafManifestTest.kt`: L1-L95
- `android/app/src/test/java/com/dxxredux/app/SafUriPermissionsTest.kt`: L1-L76

### GQ1-CHUNK-0647

- `android/app/src/test/java/com/dxxredux/app/GameFileMetadataTest.kt`: L1-L350
- `android/app/src/test/java/com/dxxredux/app/GamepadButtonDispatchPolicyTest.kt`: L1-L76
- `android/app/src/test/java/com/dxxredux/app/GamepadButtonEdgeTrackerTest.kt`: L1-L34
- `android/app/src/test/java/com/dxxredux/app/GogAudioSourceRegistrationTest.kt`: L1-L59
- `android/app/src/test/java/com/dxxredux/app/GogImportBridgeParsingTest.kt`: L1-L32
- `android/app/src/test/java/com/dxxredux/app/GraphicsConfigHelpersTest.kt`: L1-L227

### GQ1-CHUNK-0648

- `android/app/src/test/java/com/dxxredux/app/MusicLaunchPolicyTest.kt`: L1-L105
- `android/app/src/test/java/com/dxxredux/app/MusicNameSidecarTest.kt`: L1-L66
- `android/app/src/test/java/com/dxxredux/app/MusicOverlaySourcesTest.kt`: L1-L82
- `android/app/src/test/java/com/dxxredux/app/NativeFatalErrorStoreTest.kt`: L1-L27
- `android/app/src/test/java/com/dxxredux/app/OverlayVisibilityPolicyTest.kt`: L1-L167
- `android/app/src/test/java/com/dxxredux/app/OwnedCacheDirectoriesTest.kt`: L1-L57

### GQ1-CHUNK-0649

- `android/app/src/test/java/com/dxxredux/app/AcoustIdClientTest.kt`: L1-L170
- `android/app/src/test/java/com/dxxredux/app/AcoustIdConfigurationTest.kt`: L1-L34
- `android/app/src/test/java/com/dxxredux/app/AcoustIdLabelPolicyTest.kt`: L1-L117
- `android/app/src/test/java/com/dxxredux/app/AdminTrayUiTest.kt`: L1-L323
- `android/app/src/test/java/com/dxxredux/app/AdvancedPageLoadProgressTest.kt`: L1-L30
- `android/app/src/test/java/com/dxxredux/app/AndroidGameFileExtensionsTest.kt`: L1-L90
- `android/app/src/test/java/com/dxxredux/app/AssetManifestTest.kt`: L1-L101

### GQ1-CHUNK-0650

- `android/app/src/test/java/com/dxxredux/app/CdAudioSourceVisibilityTest.kt`: L1-L172
- `android/app/src/test/java/com/dxxredux/app/ConfigImportExportPreferenceTest.kt`: L1-L119
- `android/app/src/test/java/com/dxxredux/app/ConfigImportLoaderTest.kt`: L1-L142
- `android/app/src/test/java/com/dxxredux/app/ConfigSlotRepositoryTest.kt`: L1-L169
- `android/app/src/test/java/com/dxxredux/app/ControllerAxisExponentTest.kt`: L1-L54
- `android/app/src/test/java/com/dxxredux/app/ControllerConfigSerializationTest.kt`: L1-L80
- `android/app/src/test/java/com/dxxredux/app/ControllerDeviceSelectionTest.kt`: L1-L38

### GQ1-CHUNK-0651

- `android/app/src/test/java/com/dxxredux/app/MetadataLoadProgressTest.kt`: L1-L46
- `android/app/src/test/java/com/dxxredux/app/MissionDescriptorEncodingTest.kt`: L1-L173
- `android/app/src/test/java/com/dxxredux/app/MissionDescriptorFileDetailsTest.kt`: L1-L61
- `android/app/src/test/java/com/dxxredux/app/ModManagerDetailsTest.kt`: L1-L387
- `android/app/src/test/java/com/dxxredux/app/MouseModeTuningTest.kt`: L1-L91
- `android/app/src/test/java/com/dxxredux/app/multiplayer/MissionLevelAdmissionTest.kt`: L1-L53
- `android/app/src/test/java/com/dxxredux/app/multiplayer/MultiplayerBackgroundDeadlineTest.kt`: L1-L41

### GQ1-CHUNK-0652

- `android/app/src/test/java/com/dxxredux/app/multiplayer/MultiplayerCallsignsTest.kt`: L1-L58
- `android/app/src/test/java/com/dxxredux/app/multiplayer/MultiplayerControllerFocusPolicyTest.kt`: L1-L62
- `android/app/src/test/java/com/dxxredux/app/multiplayer/MultiplayerResumePrefsTest.kt`: L1-L496
- `android/app/src/test/java/com/dxxredux/app/multiplayer/MultiplayerStatsOverlayTest.kt`: L1-L32
- `android/app/src/test/java/com/dxxredux/app/multiplayer/RuntimeGameStateBridgeTest.kt`: L1-L78
- `android/app/src/test/java/com/dxxredux/app/multiplayer/UdpReconnectIdentityTest.kt`: L1-L68
- `android/app/src/test/java/com/dxxredux/app/MusicDiagnosticGeometryTest.kt`: L1-L26

### GQ1-CHUNK-0653

- `android/app/src/test/java/com/dxxredux/app/TouchAxisRegionTravelTest.kt`: L1-L30
- `android/app/src/test/java/com/dxxredux/app/TouchCheatInjectionTest.kt`: L1-L112
- `android/app/src/test/java/com/dxxredux/app/TouchEditorZoneEdgeTest.kt`: L1-L293
- `android/app/src/test/java/com/dxxredux/app/TouchOverlayDragZonePolicyTest.kt`: L1-L127
- `android/app/src/test/java/com/dxxredux/app/TouchStickExtremeActionTest.kt`: L1-L161
- `android/app/src/test/java/com/dxxredux/app/VideoInfoOverlayLayoutTest.kt`: L1-L76
- `android/app/src/test/java/com/dxxredux/app/WeaponStateTest.kt`: L1-L32

### GQ1-CHUNK-0654

- `android/app/src/test/java/com/dxxredux/app/AudioPlaylistCapacityTest.kt`: L1-L41
- `android/app/src/test/java/com/dxxredux/app/AudioPreviewResourceOwnerTest.kt`: L1-L40
- `android/app/src/test/java/com/dxxredux/app/AudioSourceManagerArtifactPathsTest.kt`: L1-L216
- `android/app/src/test/java/com/dxxredux/app/AudioSourceManagerPersistenceTest.kt`: L1-L98
- `android/app/src/test/java/com/dxxredux/app/AutomapTouchPolicyTest.kt`: L1-L116
- `android/app/src/test/java/com/dxxredux/app/AutoselectReorderScrollTest.kt`: L1-L84
- `android/app/src/test/java/com/dxxredux/app/BinHexDecoderTest.kt`: L1-L128
- `android/app/src/test/java/com/dxxredux/app/CdAudioSourceNormalizationTest.kt`: L1-L132

### GQ1-CHUNK-0655

- `android/app/src/test/java/com/dxxredux/app/ImportTreeScannerTest.kt`: L1-L196
- `android/app/src/test/java/com/dxxredux/app/InputDemoManagerTest.kt`: L1-L107
- `android/app/src/test/java/com/dxxredux/app/InputMixerTest.kt`: L1-L83
- `android/app/src/test/java/com/dxxredux/app/LauncherFileCopyTest.kt`: L1-L168
- `android/app/src/test/java/com/dxxredux/app/LauncherFileLabelsTest.kt`: L1-L51
- `android/app/src/test/java/com/dxxredux/app/LauncherFocusPolicyTest.kt`: L1-L73
- `android/app/src/test/java/com/dxxredux/app/LevelMetadataAnalysisSingleFlightTest.kt`: L1-L32
- `android/app/src/test/java/com/dxxredux/app/LevelMetadataCheckpointProgressTest.kt`: L1-L74
- `android/app/src/test/java/com/dxxredux/app/LevelMetadataProgressDeadlineTest.kt`: L1-L71

### GQ1-CHUNK-0656

- `android/app/src/test/java/com/dxxredux/app/LevelMetadataResultTest.kt`: L1-L58
- `android/app/src/test/java/com/dxxredux/app/LevelMetadataRouteNumberingTest.kt`: L1-L78
- `android/app/src/test/java/com/dxxredux/app/LevelMetadataTargetsTest.kt`: L1-L259
- `android/app/src/test/java/com/dxxredux/app/LevelMetadataWorkerIdentityTest.kt`: L1-L119
- `android/app/src/test/java/com/dxxredux/app/LevelPreviewRequestStoreTest.kt`: L1-L92
- `android/app/src/test/java/com/dxxredux/app/LevelPreviewReturnRefreshGateTest.kt`: L1-L35
- `android/app/src/test/java/com/dxxredux/app/LoadingProgressOverlayLayoutTest.kt`: L1-L38
- `android/app/src/test/java/com/dxxredux/app/LobbyDiagnosticsTest.kt`: L1-L93
- `android/app/src/test/java/com/dxxredux/app/MainActivityInputTypeTest.kt`: L1-L85

### GQ1-CHUNK-0658

- `android/game_scripts/test_controller_compare_unified.json5`: L1-L96
- `android/game_scripts/test_controls_readability_d2.json5`: L1-L135
- `android/game_scripts/test_coop_guidebot_observer_host.json5`: L1-L32
- `android/game_scripts/test_coop_guidebot_observer_joiner.json5`: L1-L30
- `android/game_scripts/test_coop_guidebot_owner_host.json5`: L1-L59
- `android/game_scripts/test_coop_guidebot_owner_joiner.json5`: L1-L61
- `android/game_scripts/test_coop_guidebot_restore_remap_host.json5`: L1-L22
- `android/game_scripts/test_coop_guidebot_restore_remap_joiner.json5`: L1-L21
- `android/game_scripts/test_coop_guidebot_restore_save_host.json5`: L1-L40
- `android/game_scripts/test_coop_guidebot_restore_save_joiner.json5`: L1-L24
- `android/game_scripts/test_coop_host_migration_prepare_host.json5`: L1-L28
- `android/game_scripts/test_coop_host_migration_prepare_joiner.json5`: L1-L25
- `android/game_scripts/test_d2_level7_reactor_water_profile.json5`: L1-L55
- `android/game_scripts/test_death.json5`: L1-L108
- `android/game_scripts/test_debug_log_refresh_button.json5`: L1-L12
- `android/game_scripts/test_engine_prefs_unified.json5`: L1-L106

### GQ1-CHUNK-0659

- `android/game_scripts/test_kcxf2_guidebot_route_next.json5`: L1-L460
- `android/game_scripts/test_keyboard_manual.json5`: L1-L66
- `android/game_scripts/test_launch_to_automap.json5`: L1-L248
- `android/game_scripts/test_level_metadata_request_mount_scope.json5`: L1-L18

### GQ1-CHUNK-0660

- `android/game_scripts/test_gles3_shim_vbo_arrays.json5`: L1-L27
- `android/game_scripts/test_gog_installer_d1_unified.json5`: L1-L135
- `android/game_scripts/test_gog_installer_redbook_unified.json5`: L1-L231
- `android/game_scripts/test_guidebot_unexplored_goal.json5`: L1-L125
- `android/game_scripts/test_intro_skip_inputs_unified.json5`: L1-L43

### GQ1-CHUNK-0661

- `android/game_scripts/test_saf_basic.json5`: L1-L67
- `android/game_scripts/test_saf_redbook.json5`: L1-L209
- `android/game_scripts/test_secret_reveal_automap_d2.json5`: L1-L67
- `android/game_scripts/test_skip_every_launch_button_manual_unified.json5`: L1-L38
- `android/game_scripts/test_title_music_skip_pref_unified.json5`: L1-L59
- `android/game_scripts/test_trine2_d1_in_d2_custom_textures.json5`: L1-L93

### GQ1-CHUNK-0662

- `android/game_scripts/test_levelcomplete_touch_skip.json5`: L1-L151
- `android/game_scripts/test_lunar_series_revamped_metadata_only.json5`: L1-L15
- `android/game_scripts/test_merged_wall_snapshot_regression.json5`: L1-L128
- `android/game_scripts/test_merged_wall_two_pass_probe.json5`: L1-L110
- `android/game_scripts/test_mod_loading.json5`: L1-L89
- `android/game_scripts/test_music_track_controls_unified.json5`: L1-L103
- `android/game_scripts/test_newmenu_render_paths_unified.json5`: L1-L180

### GQ1-CHUNK-0663

- `android/game_scripts/test_obsidian_level1_objective_markers.json5`: L1-L400
- `android/game_scripts/test_ogl_runtime_texture_options_unified.json5`: L1-L150
- `android/game_scripts/test_pilot_long_hold_delete_unified.json5`: L1-L63
- `android/game_scripts/test_quick_record_classic_sidecar.json5`: L1-L87
- `android/game_scripts/test_random_level_preview.json5`: L1-L19
- `android/game_scripts/test_readable_tiny_help_d2.json5`: L1-L61
- `android/game_scripts/test_resolution_unified.json5`: L1-L95

### GQ1-CHUNK-0664

- `android/game_scripts/.gitignore`: L1-L2
- `android/game_scripts/profile_uneasy4_level_preview.json5`: L1-L16
- `android/game_scripts/test_abort_game_to_main_menu_d2.json5`: L1-L82
- `android/game_scripts/test_android_saveload_dispatch_unified.json5`: L1-L90
- `android/game_scripts/test_autosave_resume_missing_pilot_unified.json5`: L1-L111
- `android/game_scripts/test_autoselect_crash_unified.json5`: L1-L167
- `android/game_scripts/test_axis_mapping.json5`: L1-L320
- `android/game_scripts/test_boss_health_bar.json5`: L1-L86

### GQ1-CHUNK-0665

- `android/helpers/run_test.ps1`: L1-L424
- `android/helpers/test_env.ps1`: L1-L105

### GQ1-CHUNK-0666

- `android/helpers/test_host_platform.ps1`: L1-L581
- `android/helpers/test_suite_progress.ps1`: L1-L29

### GQ1-CHUNK-0670

- `android/tests/test_xfing_asset_validation.ps1`: L1-L316
- `android/tests/update_secret_area_baseline.ps1`: L1-L39

### GQ1-CHUNK-0671

- `android/tests/test_input_demo_controls.cpp`: L1-L443
- `android/tests/test_input_demo_determinism_matrix.ps1`: L1-L292

### GQ1-CHUNK-0672

- `android/tests/compare_input_demo_state_trace.ps1`: L1-L677
- `android/tests/export_input_demo_state_trace.ps1`: L1-L157

### GQ1-CHUNK-0673

- `android/tests/test_route_snapshot.cpp`: L901-L1385
- `android/tests/test_saf_redbook.ps1`: L1-L87

### GQ1-CHUNK-0674

- `android/tests/test_lan_broadcast.ps1`: L1-L328
- `android/tests/test_lan_discovery.ps1`: L1-L160
- `android/tests/test_lan_lobby_discovery.ps1`: L1-L239

### GQ1-CHUNK-0675

- `android/tests/test_dual_emu_setup.ps1`: L1-L343
- `android/tests/test_dual_emu.ps1`: L1-L433
- `android/tests/test_dxa_animation_validation.py`: L1-L84

### GQ1-CHUNK-0676

- `android/tests/test_lan.ps1`: L901-L1397
- `android/tests/test_launcher_dpad.ps1`: L1-L281
- `android/tests/test_legacy_save_header_loadability.py`: L1-L23

### GQ1-CHUNK-0677

- `android/tests/test_input_demo_recorder.cpp`: L901-L991
- `android/tests/test_input_demo_regressions_graphics.ps1`: L1-L39
- `android/tests/test_input_demo_regressions.ps1`: L1-L64

### GQ1-CHUNK-0678

- `android/tests/test_input_demo_fixture.cpp`: L1-L613
- `android/tests/test_input_demo_host_build_guard.ps1`: L1-L62
- `android/tests/test_input_demo_policy_pack1.c`: L1-L11
- `android/tests/test_input_demo_policy_pack16.c`: L1-L11

### GQ1-CHUNK-0679

- `android/tests/test_input_demo_replay.cpp`: L901-L1059
- `android/tests/test_input_demo_result.cpp`: L1-L414
- `android/tests/test_input_demo_rng_mode.c`: L1-L111
- `android/tests/test_input_demo_rng_trace_compare.ps1`: L1-L62

### GQ1-CHUNK-0680

- `android/tests/test_android_slowdown_detector.c`: L1-L238
- `android/tests/test_args_defaults.c`: L1-L192
- `android/tests/test_autoselect_order_validation.py`: L1-L46
- `android/tests/test_base_mission_route_status.ps1`: L1-L53
- `android/tests/test_bot_client.ps1`: L1-L348

### GQ1-CHUNK-0681

- `android/tests/test_bounded_rle.c`: L1-L63
- `android/tests/test_cd_level_metadata_sources.ps1`: L1-L71
- `android/tests/test_cd_preview_sync.py`: L1-L236
- `android/tests/test_cd_regression_runner.ps1`: L1-L111
- `android/tests/test_classic_demo_dump_transaction.py`: L1-L165

### GQ1-CHUNK-0682

- `android/tests/input_demo_game_data.ps1`: L1-L256
- `android/tests/input_demo_graphics_canary_helpers.ps1`: L1-L67
- `android/tests/input_demo_host_build_guard.ps1`: L1-L273
- `android/tests/probe_autoselect_plx.ps1`: L1-L174
- `android/tests/run_input_demo_headless.ps1`: L1-L97

### GQ1-CHUNK-0683

- `android/tests/test_fingerprint_threshold.ps1`: L1-L96
- `android/tests/test_fpcalc_and_acoustid.ps1`: L1-L281
- `android/tests/test_game_data_asset_manifest_writer.ps1`: L1-L174
- `android/tests/test_generate_regression_specs.ps1`: L1-L170
- `android/tests/test_get_deps_runtime_updates.ps1`: L1-L76

### GQ1-CHUNK-0684

- `android/tests/test_android_render_fov.c`: L1-L53
- `android/tests/test_android_renderer_contracts.py`: L1-L123
- `android/tests/test_android_rewind_policy.c`: L1-L213
- `android/tests/test_android_save_meta.c`: L1-L199
- `android/tests/test_android_save_set.c`: L1-L86

### GQ1-CHUNK-0685

- `android/tests/test_input_demo_runtime_smoke.ps1`: L1-L279
- `android/tests/test_input_demo_start_difficulty_validation.py`: L1-L30
- `android/tests/test_input_demo_state_trace_compare.ps1`: L1-L87
- `android/tests/test_json5_and_tracklist_parsing.ps1`: L1-L105
- `android/tests/test_kconfig_android_shared.c`: L1-L75

### GQ1-CHUNK-0686

- `android/tests/test_manual_lan_coop.ps1`: L1-L24
- `android/tests/test_midi_preview_sync.py`: L1-L267
- `android/tests/test_mission_metadata_json_normalization.ps1`: L1-L48
- `android/tests/test_mission_metadata_travel_times.ps1`: L1-L39
- `android/tests/test_mission_route_corpus.ps1`: L1-L176

### GQ1-CHUNK-0687

- `android/tests/test_save_runtime_validation.py`: L1-L339
- `android/tests/test_secret_area_baseline_diff.ps1`: L1-L34
- `android/tests/test_secret_area_baseline.ps1`: L1-L238
- `android/tests/test_server_integration.ps1`: L1-L46
- `android/tests/test_standard_game_data_resolution.ps1`: L1-L54
- `android/tests/test_state_persistence_contracts.py`: L1-L140

### GQ1-CHUNK-0688

- `android/tests/test_test_helpers_process_wait.ps1`: L1-L175
- `android/tests/test_test_report_runtimes.ps1`: L1-L70
- `android/tests/test_test_suite_progress.ps1`: L1-L30
- `android/tests/test_thief_checkpoint_index_validation.py`: L1-L36
- `android/tests/test_tsf_render_thread_tuning.py`: L1-L177
- `android/tests/test_validate_automation_catalog.ps1`: L1-L172
- `android/tests/test_xcrash_native_report.ps1`: L1-L128

### GQ1-CHUNK-0689

- `android/tests/secret_area_baseline_helpers.ps1`: L1-L146
- `android/tests/test_acoustid_config_packaging.ps1`: L1-L63
- `android/tests/test_acoustid_regeneration.ps1`: L1-L134
- `android/tests/test_active_game_data_reset.ps1`: L1-L81
- `android/tests/test_android_audio_lifecycle.py`: L1-L131
- `android/tests/test_android_axis_mailbox.cpp`: L1-L167
- `android/tests/test_android_file_pair_transaction.c`: L1-L169

### GQ1-CHUNK-0690

- `android/tests/test_gog_installer_d1_unified.ps1`: L1-L134
- `android/tests/test_gog_installer_redbook_unified.ps1`: L1-L138
- `android/tests/test_gradle_unit_tests.ps1`: L1-L17
- `android/tests/test_hash_assets_force_completeness.ps1`: L1-L61
- `android/tests/test_homing_compat.c`: L1-L40
- `android/tests/test_hud_layout.c`: L1-L109
- `android/tests/test_inno_capability_docs.py`: L1-L49

### GQ1-CHUNK-0691

- `android/tests/test_d2xxl_tga_layout.ps1`: L1-L325
- `android/tests/test_dependency_temp_files.sh`: L1-L72
- `android/tests/test_deployment_script_contracts.py`: L1-L84
- `android/tests/test_deterministic_math.c`: L1-L33
- `android/tests/test_double_launch.ps1`: L1-L192
- `android/tests/test_download_verification.ps1`: L1-L120
- `android/tests/test_dpog_d1_bitmap_mapping.py`: L1-L64

### GQ1-CHUNK-0692

- `android/tests/test_classic_demo_json.c`: L1-L269
- `android/tests/test_clean_old_artifacts.ps1`: L1-L140
- `android/tests/test_client_identity_backup.ps1`: L1-L49
- `android/tests/test_cockpit_mode_validation.py`: L1-L43
- `android/tests/test_control_center_trigger_validation.py`: L1-L43
- `android/tests/test_coop_host_migration_policy.c`: L1-L111
- `android/tests/test_coop_indicator_lines_math.c`: L1-L60
- `android/tests/test_coop_player_session.c`: L1-L68

### GQ1-CHUNK-0693

- `android/tests/test_coop_powerup_duplication.c`: L1-L149
- `android/tests/test_coop_start_fanout_mapset.ps1`: L1-L119
- `android/tests/test_coop_warp_policy.c`: L1-L29
- `android/tests/test_counterstrike_level2_trigger21_route.ps1`: L1-L119
- `android/tests/test_d_tick_state_validation.py`: L1-L47
- `android/tests/test_d1_custom_rle_staging.py`: L1-L54
- `android/tests/test_d1_pig_validation.c`: L1-L87
- `android/tests/test_d1_save_translate_weapon_validation.py`: L1-L37
- `android/tests/test_d2xxl_sound_format.ps1`: L1-L54

### GQ1-CHUNK-0694

- `android/tests/test_multi_save_transfer_policy.c`: L1-L22
- `android/tests/test_multiplayer_source_contracts.py`: L1-L46
- `android/tests/test_native_host_unit_tests.ps1`: L1-L55
- `android/tests/test_obsidian_level2_key_preference.ps1`: L1-L58
- `android/tests/test_random_level_preview.ps1`: L1-L320
- `android/tests/test_regenerate_all_regression_data.ps1`: L1-L79
- `android/tests/test_regression_tool_contracts.py`: L1-L103
- `android/tests/test_replacement_texture_limits.py`: L1-L67
- `android/tests/test_rng_seed_resume.c`: L1-L132

### GQ1-CHUNK-0695

- `android/tests/test_dxa_ham_patch_transaction.py`: L1-L77
- `android/tests/test_dxa_robot_weapon_validation.py`: L1-L49
- `android/tests/test_escort_exit_policy.c`: L1-L14
- `android/tests/test_escort_owner_policy.c`: L1-L140
- `android/tests/test_fingerprint_audio_enumeration.ps1`: L1-L144
- `android/tests/test_fingerprint_manifest_publication.ps1`: L1-L220
- `android/tests/test_fingerprint_match_strict.py`: L1-L73
- `android/tests/test_fingerprint_music_pack_build_guard.ps1`: L1-L41
- `android/tests/test_fingerprint_source_identity.ps1`: L1-L92

### GQ1-CHUNK-0709

- `.tmp`
- `android/app_images/icon_512.png`
- `android/build_output.txt`
- `android/test_fixtures/allext_out/data.bin`
- `android/test_fixtures/allext_test.bin`
- `android/test_fixtures/bad_pvd.bin`
- `android/test_fixtures/extracted_iso_image/image.hog`
- `android/test_fixtures/extracted/test.hog`
- `android/test_fixtures/filter_test.bin`
- `android/test_fixtures/name_test1.bin`
- `android/test_fixtures/name_test2.bin`
- `android/test_fixtures/name_test3.bin`
- `android/test_fixtures/roundtrip_out/descent2.hog`
- `android/test_fixtures/roundtrip.bin`
- `android/test_fixtures/test_extract.bin`
- `android/test_fixtures/test_iso.bin`
- `android/test_output.txt`
- `android/tests/test_redbook_data/test_disc.bin`
- `dxx_matchmaking.db`
- `dxx_matchmaking.db-shm`
- `dxx_matchmaking.db-wal`
- `game_data_to_copy_to_emulator/debuglog_20260520_211753.txt`

### GQ1-CHUNK-0710

- `server/Cargo.lock`

### GQ1-CHUNK-0711

- `android/test_fixtures/secret_area_base_game_baseline.json`
- `android/tests/fixtures/mission_route_baseline.json`
- `game_data/mission_files/-MOON-.json`
- `game_data/mission_files/af_d1_beta.json`
- `game_data/mission_files/af-d2x.json`
- `game_data/mission_files/anachron.json`
- `game_data/mission_files/ascent.json`
- `game_data/mission_files/Bahagad.json`
- `game_data/mission_files/BelialSystemXL.json`
- `game_data/mission_files/bratmaze.json`
- `game_data/mission_files/castaway_redux.json`
- `game_data/mission_files/castaway_redux.tracklist.json`
- `game_data/mission_files/CD - Descent - Anniversary Edition (USA) extras.json`
- `game_data/mission_files/CD - Descent - Destination Saturn (USA).json`
- `game_data/mission_files/CD - Descent - Levels of the World (USA).json`
- `game_data/mission_files/CD - Descent II - The Vertigo Series (USA).json`
- `game_data/mission_files/CD - Dimensions for Descent (USA).json`
- `game_data/mission_files/cererian_1.3.json`
- `game_data/mission_files/cererian_1.3.tracklist.json`
- `game_data/mission_files/Chasm.json`
- `game_data/mission_files/chromium.json`
- `game_data/mission_files/chron10b.json`
- `game_data/mission_files/Colossus.json`
- `game_data/mission_files/Countd2.json`
- `game_data/mission_files/Counterstrike.json`
- `game_data/mission_files/D1Lost.json`
- `game_data/mission_files/d1mercen.json`
- `game_data/mission_files/d1secret.json`
- `game_data/mission_files/D2Crossfire.json`
- `game_data/mission_files/dd1lvls1.json`
- `game_data/mission_files/dd2lvls1.json`
- `game_data/mission_files/Descend Again.json`
- `game_data/mission_files/Descent 1 to Descent 2 Conversion.json`
- `game_data/mission_files/descent_maximum_fixed.json`
- `game_data/mission_files/Descent- Invertaus 1.2.json`
- `game_data/mission_files/Descent.json`
- `game_data/mission_files/diehard.json`
- `game_data/mission_files/Disint_Beta_3.json`
- `game_data/mission_files/dontpnic.json`
- `game_data/mission_files/dozen.json`

### GQ1-CHUNK-0712

- `game_data/mission_files/driller1.json`
- `game_data/mission_files/driller2.json`
- `game_data/mission_files/drmsaga.json`
- `game_data/mission_files/DVLVLS1.json`
- `game_data/mission_files/EAF.json`
- `game_data/mission_files/EAF2.json`
- `game_data/mission_files/entropy_v.1.0.json`
- `game_data/mission_files/Entropy.json`
- `game_data/mission_files/Entropy2.json`
- `game_data/mission_files/eq-set.json`
- `game_data/mission_files/erisreb.json`
- `game_data/mission_files/ex0core.json`
- `game_data/mission_files/Extra_Missions.json`
- `game_data/mission_files/fimbul.json`
- `game_data/mission_files/freelan.json`
- `game_data/mission_files/GALAXY ASTEROIDS ALL.json`
- `game_data/mission_files/galmond2.json`
- `game_data/mission_files/gigalo.json`
- `game_data/mission_files/gnomes.json`
- `game_data/mission_files/grad3d.json`
- `game_data/mission_files/HDPACK1.json`
- `game_data/mission_files/HDVBETA2.json`
- `game_data/mission_files/Hydro.json`
- `game_data/mission_files/icerealm.json`
- `game_data/mission_files/Imds.json`
- `game_data/mission_files/INSANE MISSION PACK ULTRA.json`
- `game_data/mission_files/ironblade.json`
- `game_data/mission_files/ironstar.json`
- `game_data/mission_files/k_sos.json`
- `game_data/mission_files/KAK.json`
- `game_data/mission_files/kcxf2.json`
- `game_data/mission_files/KCXF2RMv11.json`
- `game_data/mission_files/KCXF2RMv11.tracklist.json`
- `game_data/mission_files/KKR.json`
- `game_data/mission_files/lagrange.json`
- `game_data/mission_files/legacy.json`
- `game_data/mission_files/levigen.json`
- `game_data/mission_files/Lostlvls.json`
- `game_data/mission_files/Lunar Series Revamped.json`
- `game_data/mission_files/magma.json`

### GQ1-CHUNK-0713

- `game_data/mission_files/Mandrill.json`
- `game_data/mission_files/megalo.json`
- `game_data/mission_files/mna.json`
- `game_data/mission_files/mustfind.json`
- `game_data/mission_files/nefarious.json`
- `game_data/mission_files/nefarious.tracklist.json`
- `game_data/mission_files/Obsidian.json`
- `game_data/mission_files/odyssee.json`
- `game_data/mission_files/ORION-D2.json`
- `game_data/mission_files/Orion.json`
- `game_data/mission_files/outerrch11.json`
- `game_data/mission_files/phenomia.json`
- `game_data/mission_files/Phobos.json`
- `game_data/mission_files/plutonia.json`
- `game_data/mission_files/plutonionOutbreak.json`
- `game_data/mission_files/prophecy.json`
- `game_data/mission_files/quotienc_1.1.1.json`
- `game_data/mission_files/revdrav2.json`
- `game_data/mission_files/revodrav.json`
- `game_data/mission_files/ROGUE.json`
- `game_data/mission_files/sanitati2.json`
- `game_data/mission_files/sens9.json`
- `game_data/mission_files/sirius.json`
- `game_data/mission_files/sirius2.json`
- `game_data/mission_files/TEW.json`
- `game_data/mission_files/THEHIVE.json`
- `game_data/mission_files/TheOmicronProject.json`
- `game_data/mission_files/trainng.json`
- `game_data/mission_files/Trine1.json`
- `game_data/mission_files/Trine1.tracklist.json`
- `game_data/mission_files/trine2.json`
- `game_data/mission_files/trine2.tracklist.json`
- `game_data/mission_files/TRINITY.json`
- `game_data/mission_files/tu.json`
- `game_data/mission_files/Tyrsis.json`
- `game_data/mission_files/U3AAH.json`
- `game_data/mission_files/ulterior_v1.0.6b.json`
- `game_data/mission_files/ulterior_v1.0.6b.tracklist.json`
- `game_data/mission_files/Uneasy4.json`
- `game_data/mission_files/Vela1.json`

### GQ1-CHUNK-0714

- `game_data/mission_files/Vertigo Missions.json`
- `game_data/mission_files/Vesta.json`
- `game_data/mission_files/vignett2.json`
- `game_data/mission_files/Vignettes.json`
- `game_data/test_data_manifest.json`

### GQ1-CHUNK-0715

- `android/ai tool plans/ui/plan_scroll_strip_edge_packing.md`

### GQ1-CHUNK-0716

- `android/ai tool plans/asset management/ASSET_SYSTEM_PLAN.md`
- `android/ai tool plans/asset management/dxa-hash-integration.md`
- `android/ai tool plans/asset management/export_regression_demos_20260531.md`
- `android/ai tool plans/asset management/file_import_generality.md`
- `android/ai tool plans/asset management/MOD_SYSTEM_PLAN.md`
- `android/ai tool plans/asset management/oversized_import_warning_investigation.md`
- `android/ai tool plans/asset management/plan_7z_mission_import_20260619.md`
- `android/ai tool plans/asset management/plan_briefing_single_tap_section_advance_20260609.md`
- `android/ai tool plans/asset management/plan_d1_d2_overlay_mod_compatibility_20260614.md`
- `android/ai tool plans/asset management/plan_d1_in_d2_full_support_design_20260613.md`
- `android/ai tool plans/asset management/plan_d1_trine2_blown_texture_mismatch_20260614.md`
- `android/ai tool plans/asset management/plan_d1_trine2_texture_precedence_20260614.md`
- `android/ai tool plans/asset management/plan_d2x_xl_hires_mods.md`
- `android/ai tool plans/asset management/plan_descent_max_two_pack_zip_view_20260611.md`
- `android/ai tool plans/asset management/plan_descent_maximum_briefing_screens_20260609.md`
- `android/ai tool plans/asset management/plan_descent2workshop_ham_library_review_20260518.md`
- `android/ai tool plans/asset management/plan_descriptor_hog_metadata_pairing_20260608.md`
- `android/ai tool plans/asset management/plan_dxa_all_in_one_and_texture_test.md`
- `android/ai tool plans/asset management/plan_dxa_download_suffix_import_20260518.md`
- `android/ai tool plans/asset management/plan_dxa_error_reporting_and_fix.md`
- `android/ai tool plans/asset management/plan_dxa_multi_mod_assessment_20260518.md`
- `android/ai tool plans/asset management/plan_dxa_pack_docs_and_layout_20260518.md`
- `android/ai tool plans/asset management/plan_dxa_parallel_speed_survey.md`
- `android/ai tool plans/asset management/plan_dxa_patch_overlap_detection_20260518.md`
- `android/ai tool plans/asset management/plan_enemy_within_15th_zip_support_20260606.md`
- `android/ai tool plans/asset management/plan_etc2tool_logging_and_dxa_rebuild.md`
- `android/ai tool plans/asset management/plan_extract_d1_in_d2_code_20260614.md`
- `android/ai tool plans/asset management/plan_extracted_mission_music_preview_20260619.md`
- `android/ai tool plans/asset management/plan_fix_mod_loading_0pct.md`
- `android/ai tool plans/asset management/plan_game_file_format_registry_consolidation_20260605.md`
- `android/ai tool plans/asset management/plan_game_file_metadata_support_20260605.md`
- `android/ai tool plans/asset management/plan_generalize_d1_effect_destinations_20260614.md`
- `android/ai tool plans/asset management/plan_generic_dxa_texture_paths_20260518.md`
- `android/ai tool plans/asset management/plan_import_storage_feedback_20260425.md`
- `android/ai tool plans/asset management/plan_include_large_ewithin_zip_analysis_20260611.md`
- `android/ai tool plans/asset management/plan_infinite_abyss_game_ready_import_20260521.md`
- `android/ai tool plans/asset management/plan_large_mission_archive_extraction_design_20260619.md`
- `android/ai tool plans/asset management/plan_large_mission_archive_extraction_feedback_20260619.md`
- `android/ai tool plans/asset management/plan_large_mission_archive_extraction_impl_20260619.md`
- `android/ai tool plans/asset management/plan_large_mission_zip_extraction_20260611.md`

### GQ1-CHUNK-0717

- `android/ai tool plans/asset management/plan_large_zip_metadata_resume_rescan_20260611.md`
- `android/ai tool plans/asset management/plan_lunar_series_metadata_failure_json_20260611.md`
- `android/ai tool plans/asset management/plan_metadata_introspection_tranche_20260605.md`
- `android/ai tool plans/asset management/plan_metadata_tool_idea_review_20260605.md`
- `android/ai tool plans/asset management/plan_mission_archive_import_progress_20260619.md`
- `android/ai tool plans/asset management/plan_mission_zip_batch_failures_20260611.md`
- `android/ai tool plans/asset management/plan_mission_zip_batch_order_debug_20260612.md`
- `android/ai tool plans/asset management/plan_mission_zip_batch_ordering_20260611.md`
- `android/ai tool plans/asset management/plan_mission_zip_external_readmes_20260610.md`
- `android/ai tool plans/asset management/plan_mission_zip_metadata_music_and_advanced_lazy_20260619.md`
- `android/ai tool plans/asset management/plan_mission_zip_term_cleanup_20260605.md`
- `android/ai tool plans/asset management/plan_mod_detail_path_and_hashes_20260518.md`
- `android/ai tool plans/asset management/plan_mod_details_dialog_20260518.md`
- `android/ai tool plans/asset management/plan_mod_details_visibility_and_hashes_20260518.md`
- `android/ai tool plans/asset management/plan_mod_manager.md`
- `android/ai tool plans/asset management/plan_mod_pack_tooling_relocation_20260518.md`
- `android/ai tool plans/asset management/plan_mod_preview_scroll_clipping_20260606.md`
- `android/ai tool plans/asset management/plan_multi_mission_zip_metadata_picker_20260608.md`
- `android/ai tool plans/asset management/plan_music_track_chromaprint_progress_20260611.md`
- `android/ai tool plans/asset management/plan_obsidian_zip_feature_support_20260606.md`
- `android/ai tool plans/asset management/plan_outerrch11_mission_name_truncation_20260611.md`
- `android/ai tool plans/asset management/plan_plain_texture_dxa_base_version_preflight_20260518.md`
- `android/ai tool plans/asset management/plan_plutonia_metadata_crash_20260609.md`
- `android/ai tool plans/asset management/plan_rar_import_support_20260619.md`
- `android/ai tool plans/asset management/plan_relocate_imported_files_storage.md`
- `android/ai tool plans/asset management/plan_remove_mod_detail_md5_20260518.md`
- `android/ai tool plans/asset management/plan_run_all_mission_zip_sample_20260612.md`
- `android/ai tool plans/asset management/plan_single_mission_zip_metadata_button_20260608.md`
- `android/ai tool plans/asset management/plan_uud2sp_ham_patch_dxa_20260518.md`
- `android/ai tool plans/asset management/plan_vertigo_import_mn2_20260608.md`
- `android/ai tool plans/asset management/plan_vertigo_secret_level_metadata_20260608.md`
- `android/ai tool plans/asset management/plan_xfing_engine_texture_patch_support_20260518.md`
- `android/ai tool plans/asset management/plan_xfing_minimal_dxa_conversion_20260518.md`
- `android/ai tool plans/asset management/plan_xfing_plain_texture_dxa_conversion_20260518.md`
- `android/ai tool plans/asset management/plan_xfing_texture_pack_dxa_study_20260517.md`
- `android/ai tool plans/asset management/plan-precompressed-etc2-dxa.md`
- `android/ai tool plans/asset management/stuffit-manifest-relocation-plan.md`
- `android/ai tool plans/audio/plan_br_0013_track_capacity_20260727.md`
- `android/ai tool plans/audio/plan_br_0028_verify_midi_seek_serialization_20260727.md`
- `android/ai tool plans/audio/plan_br_0030_midi_audio_initialization_20260727.md`

### GQ1-CHUNK-0718

- `android/ai tool plans/audio/plan_br_0198_hmp_event_bounds_20260726.md`
- `android/ai tool plans/build, release, packaging/build-1025-music-gyro-touch.md`
- `android/ai tool plans/build, release, packaging/check_updates_compile_sdk_dependency_bump_20260608.md`
- `android/ai tool plans/build, release, packaging/check_updates_windows_powershell_20260608.md`
- `android/ai tool plans/build, release, packaging/plan_build_95x_fixes.md`
- `android/ai tool plans/build, release, packaging/plan_build_malformed_unicode_properties.md`
- `android/ai tool plans/build, release, packaging/plan_d2tp_sp_combined_patch_alignment_20260518.md`
- `android/ai tool plans/build, release, packaging/plan_d2x_sp_gitignore_20260518.md`
- `android/ai tool plans/build, release, packaging/plan_extension_list_dedup_android.md`
- `android/ai tool plans/build, release, packaging/plan_fix_android_net_udp_build_interruption.md`
- `android/ai tool plans/build, release, packaging/plan_fix_android_upload_collide_probe_decls_20260502.md`
- `android/ai tool plans/build, release, packaging/plan_fix_android_upload_physics_probe_build.md`
- `android/ai tool plans/build, release, packaging/plan_imagemagick_128_downscale_readmes.md`
- `android/ai tool plans/build, release, packaging/plan_temp_artifact_retention_research_20260719.md`
- `android/ai tool plans/build, release, packaging/server-deploy-scripts.md`
- `android/ai tool plans/build, release, packaging/version-code-rev-suffix.md`
- `android/ai tool plans/build, release, packaging/warning_lint_cleanup_20260714.md`
- `android/ai tool plans/CD audio/cd-image-acoustid-lookup.md`
- `android/ai tool plans/CD audio/fix_br_0408_acoustid_asset_config.md`
- `android/ai tool plans/CD audio/fix_br_0413_validate_acoustid_labels.md`
- `android/ai tool plans/CD audio/plan_android_sfx_latency_survey_20260605.md`
- `android/ai tool plans/CD audio/plan_build_1030_music_fixes.md`
- `android/ai tool plans/CD audio/plan_cd_audio_resume_programming_20260522.md`
- `android/ai tool plans/CD audio/plan_cd_audio_saf_multibin_followup_20260508.md`
- `android/ai tool plans/CD audio/plan_cd_audio_saf_tree_permission_crash_20260507.md`
- `android/ai tool plans/CD audio/plan_cd_audio_synthetic_storage_cleanup_20260507.md`
- `android/ai tool plans/CD audio/plan_chromaprint_song_recognition.md`
- `android/ai tool plans/CD audio/plan_d2_redbook_mp3_manifest_20260525.md`
- `android/ai tool plans/CD audio/plan_fix_acoustid_lookups.md`
- `android/ai tool plans/CD audio/plan_fix_acoustid_regeneration.md`
- `android/ai tool plans/CD audio/plan_fix_chromaprint_stereo_and_tests.md`
- `android/ai tool plans/CD audio/plan_general_ui_audio_fixes.md`
- `android/ai tool plans/CD audio/plan_gog_dialog_audio_import_saf.md`
- `android/ai tool plans/CD audio/plan_gog_inst_music_visibility_20260525.md`
- `android/ai tool plans/CD audio/plan_gog_tv_cd_audio_debug_20260507.md`
- `android/ai tool plans/CD audio/plan_infinite_abyss_static_cd_audio_20260511.md`
- `android/ai tool plans/CD audio/plan_mission_tracklist_sweep_20260619.md`
- `android/ai tool plans/CD audio/plan_mission_zip_tracklist_fallback_20260619.md`
- `android/ai tool plans/CD audio/plan_multi_cd_menu_radial_jukebox.md`
- `android/ai tool plans/CD audio/plan_music_fixes.md`

### GQ1-CHUNK-0719

- `android/ai tool plans/CD audio/plan_music_import_and_midi.md`
- `android/ai tool plans/CD audio/plan_music_playback_verification.md`
- `android/ai tool plans/CD audio/plan_music_ui_improvements.md`
- `android/ai tool plans/CD audio/plan_reuse_hmp2mid.md`
- `android/ai tool plans/CD audio/plan_saf_redbook_test_triage_20260516.md`
- `android/ai tool plans/CD audio/plan_title_music_skip_all_20260425.md`
- `android/ai tool plans/CD audio/plan_ui_audio_music_picker.md`
- `android/ai tool plans/CD audio/plan_unbound_actions_and_multibin_track_investigation_20260522.md`
- `android/ai tool plans/CD audio/plan_unified_music_controls.md`
- `android/ai tool plans/CD, installer parsing/BINCUE_PLAN.md`
- `android/ai tool plans/CD, installer parsing/extract-reorganization-plan.md`
- `android/ai tool plans/CD, installer parsing/fix_gog_import_issues.md`
- `android/ai tool plans/CD, installer parsing/GOG_AUDIO_OPTIONAL_EXTRACT.md`
- `android/ai tool plans/CD, installer parsing/GOG_EXTRACTION_PLAN.md`
- `android/ai tool plans/CD, installer parsing/INNOSETUP_FORMAT_SPEC.md`
- `android/ai tool plans/CD, installer parsing/integrate_mac_macplay_cd.md`
- `android/ai tool plans/CD, installer parsing/iso-data-loading-plan.md`
- `android/ai tool plans/CD, installer parsing/mac_cd_desktop_script_consolidation.md`
- `android/ai tool plans/CD, installer parsing/mac_extraction_pipeline.md`
- `android/ai tool plans/CD, installer parsing/on_device_mac_extract.md`
- `android/ai tool plans/CD, installer parsing/PKG_PARSER_PLAN.md`
- `android/ai tool plans/CD, installer parsing/plan_android_tv_saf_multiselect_bin_cue.md`
- `android/ai tool plans/CD, installer parsing/plan_br_0026_hfs_catalog_bounds_verification_20260726.md`
- `android/ai tool plans/CD, installer parsing/plan_br_0050_hfs_allocated_leaf_traversal_20260726.md`
- `android/ai tool plans/CD, installer parsing/plan_br_0051_hfs_partition_extent_bounds_20260726.md`
- `android/ai tool plans/CD, installer parsing/plan_br_0052_pe_resource_bounded_views_20260726.md`
- `android/ai tool plans/CD, installer parsing/plan_br_0053_inno_fixed_version_field_20260726.md`
- `android/ai tool plans/CD, installer parsing/plan_br_0054_inno_unicode_destination_paths_20260726.md`
- `android/ai tool plans/CD, installer parsing/plan_br_0055_inno_complete_file_catalog_20260726.md`
- `android/ai tool plans/CD, installer parsing/plan_br_0056_inno_unsigned_counts_indices_20260726.md`
- `android/ai tool plans/CD, installer parsing/plan_br_0058_inno_encrypted_chunks_20260726.md`
- `android/ai tool plans/CD, installer parsing/plan_br_0059_inno_buffered_decoder_failures_20260726.md`
- `android/ai tool plans/CD, installer parsing/plan_br_0060_iso_complete_directory_records_20260726.md`
- `android/ai tool plans/CD, installer parsing/plan_br_0061_iso_name_containment_20260726.md`
- `android/ai tool plans/CD, installer parsing/plan_br_0062_iso_cycle_safe_traversal_20260726.md`
- `android/ai tool plans/CD, installer parsing/plan_br_0063_iso_multi_extent_files_20260726.md`
- `android/ai tool plans/CD, installer parsing/plan_br_0064_android_all_cue_data_tracks_20260726.md`
- `android/ai tool plans/CD, installer parsing/plan_br_0071_arj_basic_header_validation_20260726.md`
- `android/ai tool plans/CD, installer parsing/plan_chromaprint_import_recognition_research_20260620.md`
- `android/ai tool plans/CD, installer parsing/plan_d1_gog_installer_regression.md`

### GQ1-CHUNK-0720

- `android/ai tool plans/CD, installer parsing/plan_demo_installer_extraction_20260524.md`
- `android/ai tool plans/CD, installer parsing/plan_expanded_cd_image_support_20260521.md`
- `android/ai tool plans/CD, installer parsing/plan_gog_installer_redbook_test_hardening_20260722.md`
- `android/ai tool plans/CD, installer parsing/plan_gog_installer_variant_coverage.md`
- `android/ai tool plans/CD, installer parsing/plan_mission_archive_count_progress_20260621.md`
- `android/ai tool plans/CD, installer parsing/plan_mission_zip_chromaprint_import_implementation_20260620.md`
- `android/ai tool plans/CD, installer parsing/plan_mission_zip_metadata_decoded_names_20260621.md`
- `android/ai tool plans/CD, installer parsing/plan_mission_zip_metadata_skip_cached_chromaprint_20260621.md`
- `android/ai tool plans/CD, installer parsing/plan_pc_android_cd_extract_audit_20260521.md`
- `android/ai tool plans/CD, installer parsing/plan_pc_android_extract_parity_audit_20260524.md`
- `android/ai tool plans/CD, installer parsing/plan_pc_side_native_extract_utilities.md`
- `android/ai tool plans/CD, installer parsing/plan_test_all_extracts_preview_disc_20260522.md`
- `android/ai tool plans/code management/adversarial_review_worker_orchestration.md`
- `android/ai tool plans/code management/audit_cd_audio_regression_diffs_20260722.md`
- `android/ai tool plans/code management/br_0018_extraction_budgets_20260721.md`
- `android/ai tool plans/code management/br_0019_hfs_output_containment_20260721.md`
- `android/ai tool plans/code management/br_0022_sow_huffman_length_validation_20260721.md`
- `android/ai tool plans/code management/br_0023_sow_arj_crc_validation_20260722.md`
- `android/ai tool plans/code management/br_0024_sow_ctest_coverage_20260722.md`
- `android/ai tool plans/code management/br_0025_sow_header_documentation_20260722.md`
- `android/ai tool plans/code management/br_0026_hfs_catalog_bounds_20260722.md`
- `android/ai tool plans/code management/br_0027_hfs_heap_catalog_20260722.md`
- `android/ai tool plans/code management/br_0028_midi_seek_render_serialization_20260721.md`
- `android/ai tool plans/code management/br_0031_sti2_encryption_rejection_20260721.md`
- `android/ai tool plans/code management/br_0032_sti2_archive_order_20260721.md`
- `android/ai tool plans/code management/br_0033_sti2_integrity_20260721.md`
- `android/ai tool plans/code management/br_0034_inno_file_checksums_20260721.md`
- `android/ai tool plans/code management/br_0035_inno_chunk_ranges_20260721.md`
- `android/ai tool plans/code management/br_0036_inno_md5_layout_20260721.md`
- `android/ai tool plans/code management/br_0037_inno_capability_documentation_20260721.md`
- `android/ai tool plans/code management/br_0042_native_json_strings_20260721.md`
- `android/ai tool plans/code management/br_0076_scope_level_metadata_physfs_mounts_20260803.md`
- `android/ai tool plans/code management/br_0085_stage_nonseekable_saf_descriptors_20260803.md`
- `android/ai tool plans/code management/br_0086_reject_incomplete_saf_manifests_publish_atomically_20260803.md`
- `android/ai tool plans/code management/br_0087_release_saf_manifest_io_and_close_prior_remediations_20260803.md`
- `android/ai tool plans/code management/br_0105_archive_output_collisions_20260721.md`
- `android/ai tool plans/code management/br_0178_sti2_method14_code_lengths_20260721.md`
- `android/ai tool plans/code management/br_0252_enforce_mono_stereo_pcm_contract_20260803.md`
- `android/ai tool plans/code management/br_0253_fail_android_startup_when_selected_physfs_content_cannot_mount_20260803.md`
- `android/ai tool plans/code management/br_0410_separate_album_fingerprints_from_physical_disc_parser_20260803.md`

### GQ1-CHUNK-0721

- `android/ai tool plans/code management/br_0444_move_storage_provider_probes_off_compose_thread_20260803.md`
- `android/ai tool plans/code management/br_0456_publish_imported_game_files_after_exact_copy_20260803.md`
- `android/ai tool plans/code management/br_0459_preserve_shared_audio_tree_grants_20260803.md`
- `android/ai tool plans/code management/br_0505_preserve_cd_track_identity_20260802.md`
- `android/ai tool plans/code management/br_0512_bound_provider_directory_traversal_20260802.md`
- `android/ai tool plans/code management/br_0513_canonical_save_metadata_labels_20260803.md`
- `android/ai tool plans/code management/br_0515_immutable_fileprovider_uris_20260803.md`
- `android/ai tool plans/code management/branch_adversarial_review_framework_20260718.md`
- `android/ai tool plans/code management/branch_adversarial_review_ledger.done.md`
- `android/ai tool plans/code management/branch_adversarial_review_ledger.md`
- `android/ai tool plans/code management/branch_adversarial_review_process.md`
- `android/ai tool plans/code management/build-919-fixes.md`
- `android/ai tool plans/code management/cleanup_metl154_rename_finalize.md`
- `android/ai tool plans/code management/cleanup_tranche_20260529_survey.md`
- `android/ai tool plans/code management/close_br_0437_br_0466.md`
- `android/ai tool plans/code management/CODE_QUALITY_TOOLING_PLAN.md`
- `android/ai tool plans/code management/controller-config-model-store-split-20260524.md`
- `android/ai tool plans/code management/D1_D2_COMPILATION_FIXES.md`
- `android/ai tool plans/code management/D1_INTEGRATION_PLAN.md`
- `android/ai tool plans/code management/d1d2_diff_candidate_catalog_20260711.md`
- `android/ai tool plans/code management/d1d2_diff_minimization_ledger_20260811.md`
- `android/ai tool plans/code management/d1d2_diff_minimization_worker_process.md`
- `android/ai tool plans/code management/d1d2_diff_shrink_study.md`
- `android/ai tool plans/code management/d1d2_shrink_phase2_remaining_and_phase3_candidates.md`
- `android/ai tool plans/code management/d1d2_shrink_phase3_execution_plan.md`
- `android/ai tool plans/code management/dual-game-test-markers.md`
- `android/ai tool plans/code management/kotlin-refactor-survey-20260524.md`
- `android/ai tool plans/code management/luna_max_audit_trial.md`
- `android/ai tool plans/code management/msaa_fbo_helper_extraction_20260530.md`
- `android/ai tool plans/code management/ogl_overwrite_and_stale_formatter_defenses.md`
- `android/ai tool plans/code management/ogl_phase24_31_recovery_after_checkout.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase1.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase10.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase11.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase12.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase13.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase14.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase15.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase16.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase17.md`

### GQ1-CHUNK-0722

- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase18.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase19.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase2.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase20.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase21.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase22.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase23.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase24.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase25.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase26.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase27.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase28.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase29.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase3.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase30.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase31.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase32.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase33.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase34.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase35.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase36.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase4.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase5.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase6.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase7.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase8.md`
- `android/ai tool plans/code management/ogl_shared_helper_extraction_phase9.md`
- `android/ai tool plans/code management/PATH_B_OGLES_NOTES.md`
- `android/ai tool plans/code management/plan_add_pwsh_shebangs_20260524.md`
- `android/ai tool plans/code management/plan_android_build_failures_20260508.md`
- `android/ai tool plans/code management/plan_android_lint_cleanup_20260523.md`
- `android/ai tool plans/code management/plan_android_test_gitignore_audit_20260527.md`
- `android/ai tool plans/code management/plan_base_loop_render_side_effect_cleanup_20260503.md`
- `android/ai tool plans/code management/plan_br_0004_runtime_game_state_ipc_20260727.md`
- `android/ai tool plans/code management/plan_br_0015_remove_duplicate_disc_jni_20260727.md`
- `android/ai tool plans/code management/plan_br_0038_canonical_fingerprint_threshold_20260727.md`
- `android/ai tool plans/code management/plan_br_0041_extension_policy_owner_20260727.md`
- `android/ai tool plans/code management/plan_br_0043_fingerprint_enumeration_failure_20260727.md`
- `android/ai tool plans/code management/plan_br_0065_jni_string_boundaries_20260727.md`
- `android/ai tool plans/code management/plan_br_0067_xar_toc_bounds_20260727.md`

### GQ1-CHUNK-0723

- `android/ai tool plans/code management/plan_br_0068_pkg_scripts_stream_20260727.md`
- `android/ai tool plans/code management/plan_br_0079_tsf_render_thread_tuning_20260727.md`
- `android/ai tool plans/code management/plan_br_0157_download_verification_20260727.md`
- `android/ai tool plans/code management/plan_br_0172_dos_demo_archive_containment_20260727.md`
- `android/ai tool plans/code management/plan_br_0314.md`
- `android/ai tool plans/code management/plan_br_0319.md`
- `android/ai tool plans/code management/plan_br_0412.md`
- `android/ai tool plans/code management/plan_br_0414.md`
- `android/ai tool plans/code management/plan_br_0416.md`
- `android/ai tool plans/code management/plan_br_0616.md`
- `android/ai tool plans/code management/plan_br_0638.md`
- `android/ai tool plans/code management/plan_br_0639.md`
- `android/ai tool plans/code management/plan_br_0640.md`
- `android/ai tool plans/code management/plan_br_0646.md`
- `android/ai tool plans/code management/plan_br_0647.md`
- `android/ai tool plans/code management/plan_br_0648.md`
- `android/ai tool plans/code management/plan_centralize_debug_logging.md`
- `android/ai tool plans/code management/plan_check_updates_first_pass_install_20260703.md`
- `android/ai tool plans/code management/plan_cmake_format_lint.md`
- `android/ai tool plans/code management/plan_code_cleanup_and_test_cleanup_20260514.md`
- `android/ai tool plans/code management/plan_crlf_lf_normalization.md`
- `android/ai tool plans/code management/plan_d1d2_diff_android_egl_surface_extract_20260712.md`
- `android/ai tool plans/code management/plan_d1d2_diff_android_fatal_bridge_20260712.md`
- `android/ai tool plans/code management/plan_d1d2_diff_android_scaled_font_extract_20260712.md`
- `android/ai tool plans/code management/plan_d1d2_diff_automap_metadata_overlay_extract_20260712.md`
- `android/ai tool plans/code management/plan_d1d2_diff_coop_multi_status_extract_20260711.md`
- `android/ai tool plans/code management/plan_d1d2_diff_coop_restore_remap_extract_20260712.md`
- `android/ai tool plans/code management/plan_d1d2_diff_coop_start_fanout_extract_20260712.md`
- `android/ai tool plans/code management/plan_d1d2_diff_effect_runtime_extract_20260712.md`
- `android/ai tool plans/code management/plan_d1d2_diff_expansive_resurvey_20260710.md`
- `android/ai tool plans/code management/plan_d1d2_diff_gamecntl_saveload_extract_20260711.md`
- `android/ai tool plans/code management/plan_d1d2_diff_hmp_mem_extract_20260711.md`
- `android/ai tool plans/code management/plan_d1d2_diff_hud_counts_extract_20260712.md`
- `android/ai tool plans/code management/plan_d1d2_diff_input_demo_probe_centralization_20260712.md`
- `android/ai tool plans/code management/plan_d1d2_diff_jukebox_names_extract_20260712.md`
- `android/ai tool plans/code management/plan_d1d2_diff_kconfig_scaled_render_extract_20260712.md`
- `android/ai tool plans/code management/plan_d1d2_diff_live_difficulty_extract_20260712.md`
- `android/ai tool plans/code management/plan_d1d2_diff_masked_bitmap_scale_extract_20260711.md`
- `android/ai tool plans/code management/plan_d1d2_diff_minimization_campaign_20260711.md`
- `android/ai tool plans/code management/plan_d1d2_diff_minimization_chunked_round_20260811.md`

### GQ1-CHUNK-0724

- `android/ai tool plans/code management/plan_d1d2_diff_msaa_frame_extract_20260712.md`
- `android/ai tool plans/code management/plan_d1d2_diff_ogl_dxa_mask_reuse_20260711.md`
- `android/ai tool plans/code management/plan_d1d2_diff_ogl_runtime_texture_controls_20260711.md`
- `android/ai tool plans/code management/plan_d1d2_diff_ogl_viewport_extract_20260712.md`
- `android/ai tool plans/code management/plan_d1d2_diff_physfs_upstream_sync_20260711.md`
- `android/ai tool plans/code management/plan_d1d2_diff_physfsx_extract_20260711.md`
- `android/ai tool plans/code management/plan_d1d2_diff_refresh_biggest_changes_20260519.md`
- `android/ai tool plans/code management/plan_d1d2_diff_restore_netgame_count_extract_20260712.md`
- `android/ai tool plans/code management/plan_d1d2_diff_sdl_mixer_diagnostics_extract_20260712.md`
- `android/ai tool plans/code management/plan_d1d2_diff_shader_runtime_extract_20260712.md`
- `android/ai tool plans/code management/plan_d1d2_diff_shrink_next_20260519.md`
- `android/ai tool plans/code management/plan_d1d2_diff_shrink_refresh_20260710.md`
- `android/ai tool plans/code management/plan_d1d2_diff_songs_extract_20260710.md`
- `android/ai tool plans/code management/plan_d1d2_diff_startup_resume_extract_20260712.md`
- `android/ai tool plans/code management/plan_d1d2_diff_texmerge_owner_extract_20260712.md`
- `android/ai tool plans/code management/plan_d1d2_high_coupling_cleanup_campaign_20260712.md`
- `android/ai tool plans/code management/plan_d2_classic_demo_serialization_boundary_20260712.md`
- `android/ai tool plans/code management/plan_file_encoding_repair.md`
- `android/ai tool plans/code management/plan_finish_review_sweeps_and_fix_format_findings_20260809.md`
- `android/ai tool plans/code management/plan_fix_android_upload_build_20260428.md`
- `android/ai tool plans/code management/plan_fix_android_upload_build_20260501.md`
- `android/ai tool plans/code management/plan_fix_br_0177_20260730.md`
- `android/ai tool plans/code management/plan_fix_br_0274_serialize_cd_preview_seek_20260809.md`
- `android/ai tool plans/code management/plan_fix_br_0324_skip_redbook_data_tracks_20260809.md`
- `android/ai tool plans/code management/plan_fix_br_0325_cue_size_budget_20260809.md`
- `android/ai tool plans/code management/plan_fix_br_0326_audio_playlist_capacity_20260809.md`
- `android/ai tool plans/code management/plan_fix_br_0327_redbook_io_completion_20260809.md`
- `android/ai tool plans/code management/plan_fix_br_0328_redbook_stopped_paused_20260809.md`
- `android/ai tool plans/code management/plan_fix_br_0350_0353_0360_0367_save_validation_20260809.md`
- `android/ai tool plans/code management/plan_fix_br_0355_atomic_object_topology_20260809.md`
- `android/ai tool plans/code management/plan_fix_br_0365_br_0366_d1_pig_safety_20260809.md`
- `android/ai tool plans/code management/plan_fix_br_0370_classic_demo_alias_20260809.md`
- `android/ai tool plans/code management/plan_fix_br_0373_atomic_ham_patch_validation_20260809.md`
- `android/ai tool plans/code management/plan_fix_br_0375_dpog_d1_bitmap_mapping_20260809.md`
- `android/ai tool plans/code management/plan_fix_br_0376_custom_rle_validation_20260809.md`
- `android/ai tool plans/code management/plan_fix_br_0380_dxa_animation_progress_20260809.md`
- `android/ai tool plans/code management/plan_fix_br_0445_d1_pig_metadata_offsets_20260809.md`
- `android/ai tool plans/code management/plan_fix_br_0453_confine_cd_cleanup_20260809.md`
- `android/ai tool plans/code management/plan_fix_branch_touched_warnings_20260428.md`
- `android/ai tool plans/code management/plan_game_data_manifest_20260524.md`

### GQ1-CHUNK-0725

- `android/ai tool plans/code management/plan_header_source_survey_20260519.md`
- `android/ai tool plans/code management/plan_headless_dump_tool_split_20260611.md`
- `android/ai tool plans/code management/plan_host_migration_disconnect_extraction_20260712.md`
- `android/ai tool plans/code management/plan_input_demo_direct_command_policy_20260712.md`
- `android/ai tool plans/code management/plan_integrate_luna_sol_trial_20260730.md`
- `android/ai tool plans/code management/plan_known_albums_ambiguity_audit.md`
- `android/ai tool plans/code management/plan_lint_cleanup_and_vscode_exclusions.md`
- `android/ai tool plans/code management/plan_linux_dependency_bootstrap_20260525.md`
- `android/ai tool plans/code management/plan_luna_max_audit_experiment_20260730.md`
- `android/ai tool plans/code management/plan_luna_vs_sol_audit_trial_20260730.md`
- `android/ai tool plans/code management/plan_main_vs_cmake_diff_shrink_20260504.md`
- `android/ai tool plans/code management/plan_mission_music_variant_audit.md`
- `android/ai tool plans/code management/plan_next_30_local_correctness_fixes_20260811.md`
- `android/ai tool plans/code management/plan_next_30_parser_correctness_fixes_20260811.md`
- `android/ai tool plans/code management/plan_powershell_msi_system_update_20260624.md`
- `android/ai tool plans/code management/plan_powershell_stable_only_20260624.md`
- `android/ai tool plans/code management/plan_powershell_update_helper_20260624.md`
- `android/ai tool plans/code management/plan_private_newmenu_rendering_state_20260712.md`
- `android/ai tool plans/code management/plan_rank_cd_import_file_format_findings_20260809.md`
- `android/ai tool plans/code management/plan_regenerate_known_albums_and_continue_review_20260808.md`
- `android/ai tool plans/code management/plan_repo_root_cleanup_20260524.md`
- `android/ai tool plans/code management/plan_sdk_cmdline_tools_install_sync_20260624.md`
- `android/ai tool plans/code management/plan_targeted_code_quality_guidance_20260606.md`
- `android/ai tool plans/code management/plan_task_mixup_audit_20260611.md`
- `android/ai tool plans/code management/plan_windows_build_autofind.md`
- `android/ai tool plans/code management/plan_write_side_cleanup_20260514.md`
- `android/ai tool plans/code management/plan-reliable-debug-output.md`
- `android/ai tool plans/code management/script_cleanup_20260526.md`
- `android/ai tool plans/code management/script_duplicate_cleanup_20260527.md`
- `android/ai tool plans/code management/setup-activity-automation-api-split-20260524.md`
- `android/ai tool plans/code management/setup-activity-config-disc-split-20260524.md`
- `android/ai tool plans/code management/setup-activity-dialogs-split-20260524.md`
- `android/ai tool plans/code management/setup-activity-file-import-split-20260524.md`
- `android/ai tool plans/code management/setup-activity-game-files-split-20260524.md`
- `android/ai tool plans/code management/setup-activity-resume-panel-split-20260524.md`
- `android/ai tool plans/code management/setup-activity-sections-split-20260524.md`
- `android/ai tool plans/code management/sit5_entry_header_bounds_br_0017_20260721.md`
- `android/ai tool plans/code management/touch-overlay-helper-split-20260524.md`
- `android/ai tool plans/control mapping/axis-fix-and-parameterized-tests.md`
- `android/ai tool plans/control mapping/axis-test-completion.md`

### GQ1-CHUNK-0726

- `android/ai tool plans/control mapping/binding_limit_enforcement.md`
- `android/ai tool plans/control mapping/control-mapping-root-cause-fixes.md`
- `android/ai tool plans/control mapping/controller-bugs-audio-menu-buttons.md`
- `android/ai tool plans/control mapping/controller-defaults-and-guidebot-fix.md`
- `android/ai tool plans/control mapping/double-tap-fix-and-cleanup.md`
- `android/ai tool plans/control mapping/emulator-gyro-axisregion.md`
- `android/ai tool plans/control mapping/etc2-reenable-launch-button.md`
- `android/ai tool plans/control mapping/exit-button-disk-rebuild.md`
- `android/ai tool plans/control mapping/EXTRA_CONTROLS_PLAN.md`
- `android/ai tool plans/control mapping/fix-logging-and-exit-button.md`
- `android/ai tool plans/control mapping/fix-start-exit-buttons-and-jni-log-bridge.md`
- `android/ai tool plans/control mapping/gyro-overhaul-touch-drag-gl-error.md`
- `android/ai tool plans/control mapping/half-axis-trigger-ui.md`
- `android/ai tool plans/control mapping/multiple-controller-touch-config-slots.md`
- `android/ai tool plans/control mapping/overlay-controller-admin-fixes.md`
- `android/ai tool plans/control mapping/phases-b-d-touch-mouse-missions.md`
- `android/ai tool plans/control mapping/plan_android_tv_controller_support.md`
- `android/ai tool plans/control mapping/plan_automap_marker_quick_menu_20260606.md`
- `android/ai tool plans/control mapping/plan_config_stale_cleanup.md`
- `android/ai tool plans/control mapping/plan_controller_deadzone_and_live_view.md`
- `android/ai tool plans/control mapping/plan_controller_mapping_and_exit_button_fixes.md`
- `android/ai tool plans/control mapping/plan_controller_menu_and_slider_fixes.md`
- `android/ai tool plans/control mapping/plan_controller_menu_cycle_and_overlay_focus_20260520.md`
- `android/ai tool plans/control mapping/plan_controller_touch_editor_updates_20260507.md`
- `android/ai tool plans/control mapping/plan_game_menu_guidebot_controller_20260522.md`
- `android/ai tool plans/control mapping/plan_game_menu_touch_wheel_labels_20260522.md`
- `android/ai tool plans/control mapping/plan_general_scroll_repeat_20260507.md`
- `android/ai tool plans/control mapping/plan_input_mixer.md`
- `android/ai tool plans/control mapping/plan_launcher_controller_focus_outline_20260601.md`
- `android/ai tool plans/control mapping/plan_launcher_dpad_focus_fix.md`
- `android/ai tool plans/control mapping/plan_level_select_numeric_keyboard_fix_20260504.md`
- `android/ai tool plans/control mapping/plan_menu_checkbox_controller_activate.md`
- `android/ai tool plans/control mapping/plan_mp_controller_fixes.md`
- `android/ai tool plans/control mapping/plan_multiplayer_lazy_dpad_focus_20260601.md`
- `android/ai tool plans/control mapping/plan_shield_controller_focus_fixes.md`
- `android/ai tool plans/control mapping/plan_skippable_screen_input_centralization_research_20260806.md`
- `android/ai tool plans/control mapping/plan_slide_up_button_col2.md`
- `android/ai tool plans/control mapping/plan_touch_button_dynamic_weapon_labels_20260512.md`
- `android/ai tool plans/control mapping/plan_touch_controls_music_radial.md`
- `android/ai tool plans/control mapping/plan_touch_editor_floating_stick_regions_20260507.md`

### GQ1-CHUNK-0727

- `android/ai tool plans/control mapping/plan_touch_menu_and_exit_movie_research_20260525.md`
- `android/ai tool plans/control mapping/plan_touch_more_action_filters_20260512.md`
- `android/ai tool plans/control mapping/plan_touch_more_gyro_light_followup_20260512.md`
- `android/ai tool plans/control mapping/plan_touch_more_overlay_fixes_20260511.md`
- `android/ai tool plans/control mapping/plan_touch_stick_extreme_action_20260613.md`
- `android/ai tool plans/control mapping/plan_touch_stick_initial_position_travel_20260613.md`
- `android/ai tool plans/control mapping/plan_unbound_settings_controller_bindings_20260701.md`
- `android/ai tool plans/control mapping/plan_unbound_settings_weapon_cycle_fallback_20260701.md`
- `android/ai tool plans/control mapping/scroll-gyro-doubletap-chain.md`
- `android/ai tool plans/control mapping/test_mp_button_nav_rewrite.md`
- `android/ai tool plans/control mapping/TOUCH_INTERFACE_PLAN.md`
- `android/ai tool plans/control mapping/touch-controller-fixes-build916.md`
- `android/ai tool plans/control mapping/touch-controls-sensitivity-gyro.md`
- `android/ai tool plans/control mapping/touch-interface-bugs.md`
- `android/ai tool plans/controls/guidebot_unexplored_wheel_slice_20260711.md`
- `android/ai tool plans/crash, logging, diagnostics/br_0226_remove_orphaned_texture_upload_diagnostics.md`
- `android/ai tool plans/crash, logging, diagnostics/coop_crosshair_face_and_palette_probe_20260707.md`
- `android/ai tool plans/crash, logging, diagnostics/coop_door26_palette_logs_20260707.md`
- `android/ai tool plans/crash, logging, diagnostics/coop_door26_still_wrong_after_palette_invalidate_20260707.md`
- `android/ai tool plans/crash, logging, diagnostics/coop_hidden_door_texture_reasoning_20260707.md`
- `android/ai tool plans/crash, logging, diagnostics/coop_level_state_tap_compare_20260707.md`
- `android/ai tool plans/crash, logging, diagnostics/coop_level16_missing_launch_investigation.md`
- `android/ai tool plans/crash, logging, diagnostics/coop_level7_blue_texture_log_review_20260706.md`
- `android/ai tool plans/crash, logging, diagnostics/coop_level7_new_logs_20260707.md`
- `android/ai tool plans/crash, logging, diagnostics/coop_level7_no_visual_change_after_route_unify_20260707.md`
- `android/ai tool plans/crash, logging, diagnostics/coop_level7_no_visual_change_log_review_20260707.md`
- `android/ai tool plans/crash, logging, diagnostics/coop_level7_slowdown_log_review_20260722.md`
- `android/ai tool plans/crash, logging, diagnostics/coop_level7_ten_round_log_review_20260707.md`
- `android/ai tool plans/crash, logging, diagnostics/coop_level8_start_crash_review_20260722.md`
- `android/ai tool plans/crash, logging, diagnostics/coop_level9_thief_texture_slowdown_log_review_20260723.md`
- `android/ai tool plans/crash, logging, diagnostics/coop_rendering_difference_audit_20260707.md`
- `android/ai tool plans/crash, logging, diagnostics/coop_restore_desync_log_study_20260708.md`
- `android/ai tool plans/crash, logging, diagnostics/coop_sp_vs_coop_tap_compare_20260707.md`
- `android/ai tool plans/crash, logging, diagnostics/coop_texture_cleanup_retrospective_20260707.md`
- `android/ai tool plans/crash, logging, diagnostics/coop_texture_desync_logging_20260706.md`
- `android/ai tool plans/crash, logging, diagnostics/coop_texture_ten_round_logging_20260707.md`
- `android/ai tool plans/crash, logging, diagnostics/crash-reporting.md`
- `android/ai tool plans/crash, logging, diagnostics/d1_robot_ownership_save_fix_20260723.md`
- `android/ai tool plans/crash, logging, diagnostics/debug_log_export_visibility_20260719.md`
- `android/ai tool plans/crash, logging, diagnostics/multiplayer_loading_palette_20260708.md`

### GQ1-CHUNK-0728

- `android/ai tool plans/crash, logging, diagnostics/palette_lifetime_audit_20260708.md`
- `android/ai tool plans/crash, logging, diagnostics/plan_crash_handler_and_flip_logging.md`
- `android/ai tool plans/crash, logging, diagnostics/plan_death_animation_border_static_20260610.md`
- `android/ai tool plans/crash, logging, diagnostics/plan_investigate_coop_restore_exit_20260809.md`
- `android/ai tool plans/crash, logging, diagnostics/plan_profiling_log_category_20260521.md`
- `android/ai tool plans/crash, logging, diagnostics/plan_purge_gamelog_txt.md`
- `android/ai tool plans/crash, logging, diagnostics/plan_reactor_slowdown_texture_mismap_log_review_20260725.md`
- `android/ai tool plans/crash, logging, diagnostics/plan_xcrash_integration.md`
- `android/ai tool plans/crash, logging, diagnostics/sanitize_native_source_paths_20260807.md`
- `android/ai tool plans/crash, logging, diagnostics/sigabrt-crash-fix.md`
- `android/ai tool plans/emulator/plan_emulator_recovery_hardening_and_lint_20260518.md`
- `android/ai tool plans/emulator/plan_emulator_start_crash_fallback_20260526.md`
- `android/ai tool plans/emulator/plan_lan_multicast_emulator_365.md`
- `android/ai tool plans/emulator/plan_lightweight_avds_and_separate_process.md`
- `android/ai tool plans/emulator/plan_run_all_tests_auto_stale_emulator_cleanup_20260526.md`
- `android/ai tool plans/emulator/plan_suite_emulator_recovery_20260515.md`
- `android/ai tool plans/emulator/plan.DUAL_EMU_DOCKER_NAT.md`
- `android/ai tool plans/emulator/plan.TWO_PLAYER_TEST.md`
- `android/ai tool plans/emulator/texfilt-text-menu-emulator.md`
- `android/ai tool plans/emulator/two-emu-reliability-phase8.md`
- `android/ai tool plans/emulator/windows_host_build_repair_and_emulator_verify.md`
- `android/ai tool plans/extraction/plan_br_0020_fail_incomplete_archive_import_20260727.md`
- `android/ai tool plans/file import/fix_br_0429_bounded_config_import.md`
- `android/ai tool plans/file import/fix_br_0437_typed_preference_import.md`
- `android/ai tool plans/file import/plan_gradle_unit_tests_hashing_loop_20260619.md`
- `android/ai tool plans/file import/plan_hashing_loop_cleanup_20260619.md`
- `android/ai tool plans/file import/plan_launcher_hashing_loop_introspection_20260619.md`
- `android/ai tool plans/file import/plan_run_all_tests_hashing_loop_20260619.md`
- `android/ai tool plans/floating-point determinism/fp-determinism-tranche-priority-lockdown-20260429.md`
- `android/ai tool plans/floating-point determinism/fp-determinism-tranche-revision-20260429.md`
- `android/ai tool plans/floating-point determinism/fp-startup-hardening-20260429.md`
- `android/ai tool plans/floating-point determinism/plan_fp_environment_shared_helper_20260502.md`
- `android/ai tool plans/gameplay/2026-07-21-coop-guidebot-cage-investigation.md`
- `android/ai tool plans/gameplay/android_dormant_state_power_audit_20260805.md`
- `android/ai tool plans/gameplay/automap_objective_display_modes_20260713.md`
- `android/ai tool plans/gameplay/base_campaign_route_regression_audit_20260711.md`
- `android/ai tool plans/gameplay/base_mission_boss_guidebot_metadata_plan_20260704.md`
- `android/ai tool plans/gameplay/contained_key_robot_objectives_20260713.md`
- `android/ai tool plans/gameplay/coop_duplicate_powerup_energy.md`
- `android/ai tool plans/gameplay/counterstrike_level2_switch_completion_20260716.md`

### GQ1-CHUNK-0729

- `android/ai tool plans/gameplay/descent_level9_blue_key_route_20260705.md`
- `android/ai tool plans/gameplay/duplicate_pickup_behavior_audit.md`
- `android/ai tool plans/gameplay/guidebot_coop_key_goal_20260701.md`
- `android/ai tool plans/gameplay/guidebot_keyed_progression_preference_20260804.md`
- `android/ai tool plans/gameplay/guidebot_metadata_pathing_parity_20260705.md`
- `android/ai tool plans/gameplay/guidebot_metadata_pathing_parity_survey_20260709.md`
- `android/ai tool plans/gameplay/guidebot_metadata_pathing_unification_plan_20260711.md`
- `android/ai tool plans/gameplay/guidebot_mid_level_trigger_routing_plan_20260705.md`
- `android/ai tool plans/gameplay/guidebot_navigation_cpu_audit_20260711.md`
- `android/ai tool plans/gameplay/guidebot_nearest_shoot_guidance_point_20260715.md`
- `android/ai tool plans/gameplay/guidebot_owner_menu_crash_20260702.md`
- `android/ai tool plans/gameplay/guidebot_pathing_modernization_remediation_20260709.md`
- `android/ai tool plans/gameplay/guidebot_phase6_blastable_wall_completion_20260713.md`
- `android/ai tool plans/gameplay/guidebot_phase6_target_completion_20260713.md`
- `android/ai tool plans/gameplay/guidebot_phase7_dynamic_ownership_invalidation_20260713.md`
- `android/ai tool plans/gameplay/guidebot_phase7_event_generations_20260714.md`
- `android/ai tool plans/gameplay/guidebot_phase7_multiplayer_lifecycle_20260714.md`
- `android/ai tool plans/gameplay/guidebot_phase8_superseded_pathing_removal_20260714.md`
- `android/ai tool plans/gameplay/guidebot_placement_metadata_plan_20260705.md`
- `android/ai tool plans/gameplay/guidebot_save_restore_route_goal_reset_20260709.md`
- `android/ai tool plans/gameplay/guidebot_spawn_release_sync_20260701.md`
- `android/ai tool plans/gameplay/guidebot_unexplored_wheel_goal_20260709.md`
- `android/ai tool plans/gameplay/guidebot_unreachable_fallback_explore_goal_20260709.md`
- `android/ai tool plans/gameplay/headlight_not_on_default_qol_20260706.md`
- `android/ai tool plans/gameplay/investigate_mission_travel_time_text_20260711.md`
- `android/ai tool plans/gameplay/kcxf2_automap_scan_and_route_regressions_20260713.md`
- `android/ai tool plans/gameplay/kcxf2_level1_partial_route_regression_20260709.md`
- `android/ai tool plans/gameplay/kcxf2_level2_exit_path_20260704.md`
- `android/ai tool plans/gameplay/kcxf2_level3_shootable_pathing_20260705.md`
- `android/ai tool plans/gameplay/kcxf2_level5_multi_switch_route_20260711.md`
- `android/ai tool plans/gameplay/kcxf2_metadata_status_and_route_wording_20260710.md`
- `android/ai tool plans/gameplay/key_preference_unresolved_regression_20260717.md`
- `android/ai tool plans/gameplay/key_preferred_transparent_shortcuts_20260717.md`
- `android/ai tool plans/gameplay/level_metadata_trigger_route_guidebot_plan_20260704.md`
- `android/ai tool plans/gameplay/mission_route_failure_corpus_audit_20260716.md`
- `android/ai tool plans/gameplay/mission_route_failure_corpus_audit_round2_20260716.md`
- `android/ai tool plans/gameplay/mission_route_trigger_dependency_audit_20260716.md`
- `android/ai tool plans/gameplay/objective_numbering_unification_20260715.md`
- `android/ai tool plans/gameplay/obsidian_level1_next_objectives_20260715.md`
- `android/ai tool plans/gameplay/obsidian_level1_objective_marker_remediation_20260715.md`

### GQ1-CHUNK-0730

- `android/ai tool plans/gameplay/obsidian_level1_post_blue_route_20260715.md`
- `android/ai tool plans/gameplay/obsidian_level2_post_blue_grate_skip_20260811.md`
- `android/ai tool plans/gameplay/obsidian_level2_route_investigation_20260715.md`
- `android/ai tool plans/gameplay/ordinary_shoot_open_door_routes_20260716.md`
- `android/ai tool plans/gameplay/original_homing_behavior_20260714.md`
- `android/ai tool plans/gameplay/plan_coop_duplicate_weapon_pickup_20260725.md`
- `android/ai tool plans/gameplay/plan_coop_energy_shield_per_player_20260725.md`
- `android/ai tool plans/gameplay/plan_coop_rewind_round5_log_analysis_20260729.md`
- `android/ai tool plans/gameplay/plan_coop_warp_distance_only_20260729.md`
- `android/ai tool plans/gameplay/plan_d1_in_d2_gameplay_semantics_detail_20260616.md`
- `android/ai tool plans/gameplay/plan_d1_in_d2_gameplay_semantics_survey_20260614.md`
- `android/ai tool plans/gameplay/plan_d2_hud_counts_pref_20260703.md`
- `android/ai tool plans/gameplay/plan_d2_level10_key_carrier_routing_20260725.md`
- `android/ai tool plans/gameplay/plan_flythrough_exit_metadata_guidebot_20260704.md`
- `android/ai tool plans/gameplay/plan_guidebot_wall_edge_stall_research_20260723.md`
- `android/ai tool plans/gameplay/plan_indicator_line_fade_visibility_20260703.md`
- `android/ai tool plans/gameplay/plan_kcxf2_guidebot_hidden_wall_20260709.md`
- `android/ai tool plans/gameplay/projectile_valid_switch_routing_20260715.md`
- `android/ai tool plans/gameplay/remove_legacy_metadata_travel_calculation_20260710.md`
- `android/ai tool plans/gameplay/route_activation_awareness_20260708.md`
- `android/ai tool plans/gameplay/route_cache_android_audit_20260715.md`
- `android/ai tool plans/gameplay/shoot_through_waypoint_rule_audit_20260715.md`
- `android/ai tool plans/gameplay/transparent_projectile_route_surfaces_20260716.md`
- `android/ai tool plans/gameplay/unexplored_route_endpoint_correction_20260711.md`
- `android/ai tool plans/gameplay/unused_level_key_metadata_20260717.md`
- `android/ai tool plans/graphics/adaptive_slowdown_flight_recorder_20260720.md`
- `android/ai tool plans/graphics/br_0204_native_window_handoff.md`
- `android/ai tool plans/graphics/br_0205_software_rgba_packing.md`
- `android/ai tool plans/graphics/dormancy_resume_profiling_suppression_20260808.md`
- `android/ai tool plans/graphics/merged_wall_experiment_test_compile_20260708.md`
- `android/ai tool plans/graphics/merged_wall_stale_script_cleanup_20260708.md`
- `android/ai tool plans/graphics/normal_renderer_performance_survey_20260719.md`
- `android/ai tool plans/graphics/plan_br_0196_gles_vbo_array_state_20260726.md`
- `android/ai tool plans/graphics/render_gl_gequal_build_fix_20260708.md`
- `android/ai tool plans/graphics/restore_merged_wall_two_pass_tests_20260708.md`
- `android/ai tool plans/guidebot_warp_to_me_20260705.md`
- `android/ai tool plans/input demo, replay, determinism/input-demo-camera-wake-engine-fix-20260512.md`
- `android/ai tool plans/input demo, replay, determinism/input-demo-d1-d2-parity-audit-20260512.md`
- `android/ai tool plans/input demo, replay, determinism/input-demo-hard-coded-probe-cleanup-20260512.md`
- `android/ai tool plans/input demo, replay, determinism/input-demo-level9-death-replay-20260511.md`

### GQ1-CHUNK-0731

- `android/ai tool plans/input demo, replay, determinism/input-demo-level9-headlight-20260512.md`
- `android/ai tool plans/input demo, replay, determinism/input-demo-schema.md`
- `android/ai tool plans/input demo, replay, determinism/plan_awareness_probe_logging_20260501.md`
- `android/ai tool plans/input demo, replay, determinism/plan_awareness_source_instrumentation_20260502.md`
- `android/ai tool plans/input demo, replay, determinism/plan_collision_pose_logging_20260501.md`
- `android/ai tool plans/input demo, replay, determinism/plan_d1_demo_d1_in_d2_replay_20260616.md`
- `android/ai tool plans/input demo, replay, determinism/plan_d1_demo_d1_in_d2_runner_path_20260616.md`
- `android/ai tool plans/input demo, replay, determinism/plan_d1_in_d2_full_demo_pass_20260619.md`
- `android/ai tool plans/input demo, replay, determinism/plan_d1_in_d2_input_demo_centralization_20260617.md`
- `android/ai tool plans/input demo, replay, determinism/plan_d1_level12_demo_replay_20260617.md`
- `android/ai tool plans/input demo, replay, determinism/plan_d1_level13_14_demo_replay_20260617.md`
- `android/ai tool plans/input demo, replay, determinism/plan_d1_level16_17_demo_replay_20260618.md`
- `android/ai tool plans/input demo, replay, determinism/plan_d1_level16_18_demo_replay_20260618.md`
- `android/ai tool plans/input demo, replay, determinism/plan_d1_level4_20260616_121645_replay_20260616.md`
- `android/ai tool plans/input demo, replay, determinism/plan_d1_level6_9_15_demo_replay_20260617.md`
- `android/ai tool plans/input demo, replay, determinism/plan_d1_level9_recreated_demo_instrumentation_20260617.md`
- `android/ai tool plans/input demo, replay, determinism/plan_d2_headless_replay_isolation_hardening_20260722.md`
- `android/ai tool plans/input demo, replay, determinism/plan_dem_to_json_and_desync_analysis_20260429.md`
- `android/ai tool plans/input demo, replay, determinism/plan_demo_134049_rng_regression_20260511.md`
- `android/ai tool plans/input demo, replay, determinism/plan_demo_analysis.md`
- `android/ai tool plans/input demo, replay, determinism/plan_demo_final_robot_kills_fix_20260502.md`
- `android/ai tool plans/input demo, replay, determinism/plan_demo_log_sufficiency_20260510.md`
- `android/ai tool plans/input demo, replay, determinism/plan_demo_pref_key_cleanup_and_object_gate_20260505.md`
- `android/ai tool plans/input demo, replay, determinism/plan_demo_runner_crash_and_cleanup_20260505.md`
- `android/ai tool plans/input demo, replay, determinism/plan_engine_determinism_roadmap_20260510.md`
- `android/ai tool plans/input demo, replay, determinism/plan_enhanced_save_state_for_checkpoint_replay.md`
- `android/ai tool plans/input demo, replay, determinism/plan_export_rng_trace_from_advanced_page.md`
- `android/ai tool plans/input demo, replay, determinism/plan_first_divergent_object_detail_20260510.md`
- `android/ai tool plans/input demo, replay, determinism/plan_fix_headless_checkpoint_crash_20260714.md`
- `android/ai tool plans/input demo, replay, determinism/plan_fresh_demo_desync_analysis_20260429.md`
- `android/ai tool plans/input demo, replay, determinism/plan_full_buddy_checkpoint_state_20260501.md`
- `android/ai tool plans/input demo, replay, determinism/plan_gauss_spawn_probe_20260511.md`
- `android/ai tool plans/input demo, replay, determinism/plan_headless_console_demo_runner_20260430.md`
- `android/ai tool plans/input demo, replay, determinism/plan_headless_demo_regressions_20260714.md`
- `android/ai tool plans/input demo, replay, determinism/plan_headless_runner_ux_and_replay_logging_trim_20260503.md`
- `android/ai tool plans/input demo, replay, determinism/plan_host_replay_sandbox_guardrails.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_actual_result_sidecar_cleanup_20260504.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_base64_sha256_centralization_20260429.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_build_freshness_helper_20260504.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_classic_dem_sidecar_20260429.md`

### GQ1-CHUNK-0732

- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_codec_dependency_swap_20260429.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_device_recording_and_regression.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_frame_events_20260501.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_header_result_and_checkpoint_compression_20260429.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_helper_extraction_survey_20260504.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_live_state_trace_20260430.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_mid_level_start.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_original_homing_20260714.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_per_frame_state_20260430.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_phase2_coalescer.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_phase2_controls_json.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_phase3_fixture_writer.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_phase3_live_recorder.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_phase4_d1_runtime_bootstrap.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_phase4_d2_runtime_bootstrap.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_phase4_replay_session.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_phase5_desktop_runtime_smoke.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_phase5_recorder_rich_baseline.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_phase5_result_compare.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_phase5_result_serializer.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_playercfg_header_subset_20260429.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_replay_rng_trace_20260501.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_replay_state_compare_alignment_20260501.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_single_file_rework.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_temp_game_logs_20260430.md`
- `android/ai tool plans/input demo, replay, determinism/plan_input_demo_unrecorded_action_survey_20260503.md`
- `android/ai tool plans/input demo, replay, determinism/plan_level6_demo_replay_analysis_20260508_123202.md`
- `android/ai tool plans/input demo, replay, determinism/plan_level6_demo_replay_analysis_20260508_193201.md`
- `android/ai tool plans/input demo, replay, determinism/plan_level7_demo_replay_analysis_20260508_233937.md`
- `android/ai tool plans/input demo, replay, determinism/plan_level7_demo_replay_analysis_20260509_140807.md`
- `android/ai tool plans/input demo, replay, determinism/plan_level8_demo_replay_analysis_20260509_173157.md`
- `android/ai tool plans/input demo, replay, determinism/plan_level8_demo_replay_analysis_20260509_202642_202756_202946.md`
- `android/ai tool plans/input demo, replay, determinism/plan_level9_demo_replay_analysis_20260510_102637_102738.md`
- `android/ai tool plans/input demo, replay, determinism/plan_level9_demo_replay_analysis_20260511_091552.md`
- `android/ai tool plans/input demo, replay, determinism/plan_long_demo_desync_d2_level2_20260503_203112.md`
- `android/ai tool plans/input demo, replay, determinism/plan_new_l2_demo_replay_analysis_20260430.md`
- `android/ai tool plans/input demo, replay, determinism/plan_next_demo_recording_logging_20260502.md`
- `android/ai tool plans/input demo, replay, determinism/plan_nonheadless_robot_disappear_20260503.md`
- `android/ai tool plans/input demo, replay, determinism/plan_object_list_order_detection_20260510.md`
- `android/ai tool plans/input demo, replay, determinism/plan_on_device_replay_and_forcefield_sync_20260505.md`

### GQ1-CHUNK-0733

- `android/ai tool plans/input demo, replay, determinism/plan_optional_per_frame_state_tracking.md`
- `android/ai tool plans/input demo, replay, determinism/plan_player_drag_localization_20260501_141150.md`
- `android/ai tool plans/input demo, replay, determinism/plan_player_hit_logging_20260501_141150.md`
- `android/ai tool plans/input demo, replay, determinism/plan_player_motion_probe_20260501_141150.md`
- `android/ai tool plans/input demo, replay, determinism/plan_regression_demos_headless_20260430.md`
- `android/ai tool plans/input demo, replay, determinism/plan_replay_analysis_20260509_level9_225113_level8_224859.md`
- `android/ai tool plans/input demo, replay, determinism/plan_replay_debug_demo_20260428_141502.md`
- `android/ai tool plans/input demo, replay, determinism/plan_replay_debug_with_rngtrace_20260427.md`
- `android/ai tool plans/input demo, replay, determinism/plan_replay_helper_cleanup_and_optional_logging_20260503.md`
- `android/ai tool plans/input demo, replay, determinism/plan_replay_manual_step_controls_20260503.md`
- `android/ai tool plans/input demo, replay, determinism/plan_replay_playercfg_audit_20260429.md`
- `android/ai tool plans/input demo, replay, determinism/plan_replay_probe_d2_20260428_191632.md`
- `android/ai tool plans/input demo, replay, determinism/plan_replay_probe_d2_20260428_195254.md`
- `android/ai tool plans/input demo, replay, determinism/plan_replay_probe_d2_20260428_212524.md`
- `android/ai tool plans/input demo, replay, determinism/plan_replay_probe_d2_20260428_220300.md`
- `android/ai tool plans/input demo, replay, determinism/plan_replay_probe_d2_20260429_074558.md`
- `android/ai tool plans/input demo, replay, determinism/plan_replay_probe_d2_20260429_124801.md`
- `android/ai tool plans/input demo, replay, determinism/plan_replay_root_cause_ai_weapon_order_20260509.md`
- `android/ai tool plans/input demo, replay, determinism/plan_replay_runner_build_guardrails_20260502.md`
- `android/ai tool plans/input demo, replay, determinism/plan_replay_runner_render_profiles_20260502.md`
- `android/ai tool plans/input demo, replay, determinism/plan_replay_script_dedup_20260502.md`
- `android/ai tool plans/input demo, replay, determinism/plan_rewind_support_research_20260516.md`
- `android/ai tool plans/input demo, replay, determinism/plan_rng_code_local_notes_20260511.md`
- `android/ai tool plans/input demo, replay, determinism/plan_rng_discipline_frametime_math_20260510.md`
- `android/ai tool plans/input demo, replay, determinism/plan_rng_discipline_survey_20260510.md`
- `android/ai tool plans/input demo, replay, determinism/plan_rng_full_instrumentation_20260501.md`
- `android/ai tool plans/input demo, replay, determinism/plan_rng_fx_revert_tracking_20260511.md`
- `android/ai tool plans/input demo, replay, determinism/plan_rng_omega_and_fireball_audit_20260511.md`
- `android/ai tool plans/input demo, replay, determinism/plan_rng_origin_compare_detail_20260510.md`
- `android/ai tool plans/input demo, replay, determinism/plan_rng_sim_vs_nonsim_split.md`
- `android/ai tool plans/input demo, replay, determinism/plan_rng_trace_sidecar_for_input_demos.md`
- `android/ai tool plans/input demo, replay, determinism/plan_robot_ai_state_logging_20260501.md`
- `android/ai tool plans/input demo, replay, determinism/plan_robot_spawn_state_audit_20260502.md`
- `android/ai tool plans/input demo, replay, determinism/plan_sim_determinism_render_decoupling_20260502.md`
- `android/ai tool plans/input demo, replay, determinism/plan_spreadfire_demo_playback_ogl_audit_20260429.md`
- `android/ai tool plans/input demo, replay, determinism/plan_strip_input_demo_frame_state_20260504.md`
- `android/ai tool plans/input demo, replay, determinism/plan_temp_game_logs_homing_desync_20260505.md`
- `android/ai tool plans/input demo, replay, determinism/plan_temp_game_logs_replay_analysis_20260501_103312.md`
- `android/ai tool plans/input demo, replay, determinism/plan_temp_game_logs_replay_analysis_20260501_125538.md`
- `android/ai tool plans/input demo, replay, determinism/plan_temp_game_logs_replay_analysis_20260501_133242.md`

### GQ1-CHUNK-0734

- `android/ai tool plans/input demo, replay, determinism/plan_temp_game_logs_replay_analysis_20260501_141150.md`
- `android/ai tool plans/input demo, replay, determinism/plan_temp_game_logs_replay_analysis_20260501_154249.md`
- `android/ai tool plans/input demo, replay, determinism/plan_temp_game_logs_replay_analysis_20260501.md`
- `android/ai tool plans/input demo, replay, determinism/plan_temp_game_logs_replay_analysis_20260502_223549.md`
- `android/ai tool plans/input demo, replay, determinism/plan_temp_game_logs_replay_analysis_20260502_231831_headless_rng_audit.md`
- `android/ai tool plans/input demo, replay, determinism/plan_temp_game_logs_replay_analysis_20260503_184215_rng_gap.md`
- `android/ai tool plans/input demo, replay, determinism/plan_temp_game_logs_replay_analysis_20260504_fresh_desyncs.md`
- `android/ai tool plans/input demo, replay, determinism/plan_temp_game_logs_replay_analysis_20260506_135119.md`
- `android/ai tool plans/input demo, replay, determinism/plan_temp_game_logs_replay_analysis_20260506_223956.md`
- `android/ai tool plans/input demo, replay, determinism/plan_temp_game_logs_replay_analysis_20260507_210511.md`
- `android/ai tool plans/input demo, replay, determinism/plan_terminal_exit_marker_20260506.md`
- `android/ai tool plans/input demo, replay, determinism/plan_thief_checkpoint_state_audit_20260503.md`
- `android/ai tool plans/input demo, replay, determinism/plan_thief_checkpoint_state_impl_20260503.md`
- `android/ai tool plans/input demo, replay, determinism/tap_now_pose_logging_and_pose_warp.md`
- `android/ai tool plans/launcher admin/CONFIG_IMPORT_EXPORT_PLAN.md`
- `android/ai tool plans/launcher admin/fix-home-button-crash.md`
- `android/ai tool plans/launcher admin/fix-stuck-launcher-and-regressions.md`
- `android/ai tool plans/launcher admin/in-progress-games-config-sharing-bugfixes.md`
- `android/ai tool plans/launcher admin/pilot-file-management-and-advanced-tab.md`
- `android/ai tool plans/launcher admin/plan_abort_autosave_slot_20260602.md`
- `android/ai tool plans/launcher admin/plan_about_store_page_link.md`
- `android/ai tool plans/launcher admin/plan_add_launcher_stale_file_debug_logging.md`
- `android/ai tool plans/launcher admin/plan_advanced_tab_open_progress_20260621.md`
- `android/ai tool plans/launcher admin/plan_android_autosave_resume_20260512.md`
- `android/ai tool plans/launcher admin/plan_autosave_postlaunch_timeout_20260515.md`
- `android/ai tool plans/launcher admin/plan_check_updates_cmake_sdk_managed.md`
- `android/ai tool plans/launcher admin/plan_check_updates_dependency_coverage.md`
- `android/ai tool plans/launcher admin/plan_check_updates_github_fallback_and_pwsh.md`
- `android/ai tool plans/launcher admin/plan_check_updates_hard_lock_guard.md`
- `android/ai tool plans/launcher admin/plan_check_updates_installed_versions.md`
- `android/ai tool plans/launcher admin/plan_check_updates_loading_and_jdk_sync_20260523.md`
- `android/ai tool plans/launcher admin/plan_check_updates_optional_tool_cleanup.md`
- `android/ai tool plans/launcher admin/plan_check_updates_powershell_host_drift.md`
- `android/ai tool plans/launcher admin/plan_check_updates_reload_conf_failure.md`
- `android/ai tool plans/launcher admin/plan_check_updates_stable_versions.md`
- `android/ai tool plans/launcher admin/plan_check_updates_unknowns_and_powershell.md`
- `android/ai tool plans/launcher admin/plan_check_updates_unknowns_and_pwsh.md`
- `android/ai tool plans/launcher admin/plan_check_updates_unrar_sort_and_gradle_tracking.md`
- `android/ai tool plans/launcher admin/plan_clear_all_game_data_20260521.md`
- `android/ai tool plans/launcher admin/plan_demo_installer_launch_offers_20260525.md`

### GQ1-CHUNK-0735

- `android/ai tool plans/launcher admin/plan_engine_prefs_touch_launcher_qol.md`
- `android/ai tool plans/launcher admin/plan_fix_launcher_gpgs_app_id_manifest_type.md`
- `android/ai tool plans/launcher admin/plan_forget_selected_files_confirm_20260602.md`
- `android/ai tool plans/launcher admin/plan_game_preferences_launcher_section_20260613.md`
- `android/ai tool plans/launcher admin/plan_gamepad_admin_tray_consolidation.md`
- `android/ai tool plans/launcher admin/plan_get_deps_linux_compatibility_20260524.md`
- `android/ai tool plans/launcher admin/plan_launcher_icons_20260507.md`
- `android/ai tool plans/launcher admin/plan_launcher_icons_tv_banner_fix_20260507.md`
- `android/ai tool plans/launcher admin/plan_launcher_touch_highlight_flash_20260526.md`
- `android/ai tool plans/launcher admin/plan_resume_recent_save_button_swap_20260526.md`
- `android/ai tool plans/launcher admin/plan_resume_save_thumbnail_fullscreen_20260526.md`
- `android/ai tool plans/launcher admin/plan_save_preview_thumbnail_2x_20260527.md`
- `android/ai tool plans/launcher admin/plan_skip_every_launch_ui_and_pref_sync.md`
- `android/ai tool plans/launcher admin/play-store-update-check.md`
- `android/ai tool plans/launcher admin/TOUCH_EDITOR_FIXES_PLAN.md`
- `android/ai tool plans/launcher admin/unified_launcher_game_scripting.md`
- `android/ai tool plans/launcher automation, editor, scripting/autoselect_refactor_plan.md`
- `android/ai tool plans/launcher automation, editor, scripting/button_based_launcher_automation.md`
- `android/ai tool plans/launcher automation, editor, scripting/feature_c_autoselect_editor.md`
- `android/ai tool plans/launcher automation, editor, scripting/plan_autoselect_newest_mismatch_20260604.md`
- `android/ai tool plans/launcher automation, editor, scripting/plan_autoselect_save_condition_from_shown_order_20260604.md`
- `android/ai tool plans/launcher automation, editor, scripting/plan_autoselect_save_only_mismatch_20260604.md`
- `android/ai tool plans/launcher automation, editor, scripting/plan_tv_perf_test_autosetup_20260520.md`
- `android/ai tool plans/launcher/af_d1_beta_boss_reactor_investigation_20260718.md`
- `android/ai tool plans/launcher/callsign_picker_create_foldin_20260708.md`
- `android/ai tool plans/launcher/fix_br_0431_quote_config_replacements.md`
- `android/ai tool plans/launcher/large_level_metadata_progress_timeout_20260718.md`
- `android/ai tool plans/launcher/metadata_dual_progress_bars_20260719.md`
- `android/ai tool plans/launcher/metadata_level_preview_loading_progress_20260719.md`
- `android/ai tool plans/launcher/metadata_route_step_layout_20260708.md`
- `android/ai tool plans/launcher/plan_level_automap_preview_study_20260717.md`
- `android/ai tool plans/launcher/plan_menu_reload_progress_20260703.md`
- `android/ai tool plans/launcher/plan_metadata_browser_progress_20260704.md`
- `android/ai tool plans/launcher/plan_multiplayer_callsign_selection_20260703.md`
- `android/ai tool plans/launcher/plan_player_file_selectability_followup_20260703.md`
- `android/ai tool plans/launcher/plan_report_20260620_launcher_dpad.md`
- `android/ai tool plans/launcher/plan_report_20260620_next_failure.md`
- `android/ai tool plans/launcher/plan_report_20260620_one_failure.md`
- `android/ai tool plans/launcher/plan_save_explorer_coop_message_20260703.md`
- `android/ai tool plans/launcher/save_explorer_centered_tabs_indicators_20260708.md`

### GQ1-CHUNK-0736

- `android/ai tool plans/launcher/save_explorer_choose_tab_20260708.md`
- `android/ai tool plans/launcher/save_explorer_compact_swipe_tabs_20260709.md`
- `android/ai tool plans/launcher/save_explorer_live_pager_tabs_20260708.md`
- `android/ai tool plans/launcher/save_explorer_pop_open_20260708.md`
- `android/ai tool plans/launcher/save_explorer_readable_scrolling_tabs_20260709.md`
- `android/ai tool plans/launcher/view_saves_collapsed_label_20260708.md`
- `android/ai tool plans/lunar_series_revamped_zip_parse_20260612.md`
- `android/ai tool plans/metadata_crash_context_20260705.md`
- `android/ai tool plans/metadata_d2_level7_gold_key_20260705.md`
- `android/ai tool plans/metadata_resume_invalid_station_type_20260705.md`
- `android/ai tool plans/metadata/br_0215_bounded_metadata_analysis.md`
- `android/ai tool plans/metadata/obsidian_sng_track_list_view_20260811.md`
- `android/ai tool plans/mission_json_name_regression_20260612.md`
- `android/ai tool plans/mission_json_stale_level_name_diff_20260612.md`
- `android/ai tool plans/mission_zip_batch_used_old_code_20260612.md`
- `android/ai tool plans/mixed batch, umbrella/build-info-admin-api-cancel-overlay-clog.md`
- `android/ai tool plans/mixed batch, umbrella/code_quality_and_test_lan_fix.md`
- `android/ai tool plans/mixed batch, umbrella/coop_indicator_and_controls_diag.md`
- `android/ai tool plans/mixed batch, umbrella/coop_reactor_stall_level_transition_crash_20260719.md`
- `android/ai tool plans/mixed batch, umbrella/FIVE_FIXES_PLAN.md`
- `android/ai tool plans/mixed batch, umbrella/gog-installer-redbook-regression.md`
- `android/ai tool plans/mixed batch, umbrella/outstanding_bugs_triage_20260612.md`
- `android/ai tool plans/mixed batch, umbrella/outstanding_bugs_workplan.md`
- `android/ai tool plans/mixed batch, umbrella/plan_11_items_batch.md`
- `android/ai tool plans/mixed batch, umbrella/plan_7_items_round2.md`
- `android/ai tool plans/mixed batch, umbrella/plan_audio_import_advanced_tab_lan_discovery.md`
- `android/ai tool plans/mixed batch, umbrella/plan_automap_translation_speed_20260604.md`
- `android/ai tool plans/mixed batch, umbrella/plan_axis_music_touch.md`
- `android/ai tool plans/mixed batch, umbrella/plan_blown02_settings_rename_readme.md`
- `android/ai tool plans/mixed batch, umbrella/plan_controls_overlay_graphics_followups_20260522.md`
- `android/ai tool plans/mixed batch, umbrella/plan_d1d2_diff_shrink_net_udp_next_20260519.md`
- `android/ai tool plans/mixed batch, umbrella/plan_d1d2_diff_shrink_net_udp_observer_followup_20260519.md`
- `android/ai tool plans/mixed batch, umbrella/plan_energy_center_count_20260608.md`
- `android/ai tool plans/mixed batch, umbrella/plan_four_bugs_study.md`
- `android/ai tool plans/mixed batch, umbrella/plan_game_menu_guidebot_followups_20260522b.md`
- `android/ai tool plans/mixed batch, umbrella/plan_header_guards_and_net_udp_cleanup_20260519.md`
- `android/ai tool plans/mixed batch, umbrella/plan_inf_abyss_import_refresh_texture_audit_20260522.md`
- `android/ai tool plans/mixed batch, umbrella/plan_linux_root_build_script_20260524.md`
- `android/ai tool plans/mixed batch, umbrella/plan_nice_to_haves.md`
- `android/ai tool plans/mixed batch, umbrella/plan_outstanding_bugs_import_fire_resume_pilot_20260520.md`

### GQ1-CHUNK-0737

- `android/ai tool plans/mixed batch, umbrella/plan_parameterize_mod_loading_tests.md`
- `android/ai tool plans/mixed batch, umbrella/plan_redbook_textures_diagnostics.md`
- `android/ai tool plans/mixed batch, umbrella/plan_shield_tv_launcher_followups.md`
- `android/ai tool plans/mixed batch, umbrella/study_secret_area_autolabel_20260606.md`
- `android/ai tool plans/mixed batch, umbrella/test_failures_20260612.md`
- `android/ai tool plans/music/fix_br_0466_preserve_custom_audio.md`
- `android/ai tool plans/music/plan_trine2_embedded_soundtrack_investigation_20260609.md`
- `android/ai tool plans/music/plan_zip_music_metadata_browser_study_20260609.md`
- `android/ai tool plans/networking/C4a_NAT_TRAVERSAL_TEST_PLAN.md`
- `android/ai tool plans/networking/C5_GPGS_SIGN_IN_PLAN.md`
- `android/ai tool plans/networking/cleanup_coop_files_move.md`
- `android/ai tool plans/networking/cleanup_net_udp_extract.md`
- `android/ai tool plans/networking/CLIENT_NETWORKING_PLAN.md`
- `android/ai tool plans/networking/coop_autosave_restore_existing_protocol_fix_20260708.md`
- `android/ai tool plans/networking/coop_desync_logging_20260704.md`
- `android/ai tool plans/networking/coop_guidebot_rejoin_exit_fixes.md`
- `android/ai tool plans/networking/coop_host_only_restore_fix_20260708.md`
- `android/ai tool plans/networking/coop_indicator_and_save_bugs.md`
- `android/ai tool plans/networking/coop_indicator_lines.md`
- `android/ai tool plans/networking/coop_inventory_preservation.md`
- `android/ai tool plans/networking/coop_level_start_restart_design_20260721.md`
- `android/ai tool plans/networking/coop_next_level_host_start_failure_20260724.md`
- `android/ai tool plans/networking/coop_restore_repeat_fire_fix.md`
- `android/ai tool plans/networking/coop_restore_single_owner_20260802.md`
- `android/ai tool plans/networking/coop_restore_wait_timing_and_banner_20260720.md`
- `android/ai tool plans/networking/coop_sync_investigation.md`
- `android/ai tool plans/networking/coop_thief_drop_desync_log_analysis.md`
- `android/ai tool plans/networking/coop-autosave-lobby.md`
- `android/ai tool plans/networking/coop-guidebot-and-host-migration.md`
- `android/ai tool plans/networking/coop-hmig-phase9-diagnostics.md`
- `android/ai tool plans/networking/coop-inventory-rejoin-research.md`
- `android/ai tool plans/networking/coop-lobby-save-offer.md`
- `android/ai tool plans/networking/coop-multi-slot-autosave.md`
- `android/ai tool plans/networking/coop-peer-status-broadcast.md`
- `android/ai tool plans/networking/coop-qol-features.md`
- `android/ai tool plans/networking/coop-restore-diagnostics-round3.md`
- `android/ai tool plans/networking/disk-dem-lansave.md`
- `android/ai tool plans/networking/fix_host_migration_pdata_loss.md`
- `android/ai tool plans/networking/fix_isyou_address_collision.md`
- `android/ai tool plans/networking/fix_mtu_truncation.md`

### GQ1-CHUNK-0738

- `android/ai tool plans/networking/fix_test_mp_phase8.md`
- `android/ai tool plans/networking/fix-coop-invuln-faking.md`
- `android/ai tool plans/networking/guidebot coop resume desync fix 20260526.md`
- `android/ai tool plans/networking/guidebot coop resume desync investigation 20260526.md`
- `android/ai tool plans/networking/host_migration_and_guidebot_summary.md`
- `android/ai tool plans/networking/host_migration_pdata_conntype_diagnosis.md`
- `android/ai tool plans/networking/host_migration_proxy_plan.md`
- `android/ai tool plans/networking/HOST_MIGRATION_REJOIN_FIX.md`
- `android/ai tool plans/networking/lan_game_result_first_20260811.md`
- `android/ai tool plans/networking/lan_host_options_bottom_20260708.md`
- `android/ai tool plans/networking/lan_only_multiplayer_release_20260708.md`
- `android/ai tool plans/networking/MP_LOGGING_DEBUG_GUIDE.md`
- `android/ai tool plans/networking/multiplayer_lobby_background_ready_hardening_20260720.md`
- `android/ai tool plans/networking/multiplayer_prefs_persistence_20260703.md`
- `android/ai tool plans/networking/multiplayer-quick-resume-plan.md`
- `android/ai tool plans/networking/net_udp_cleanup_candidates.md`
- `android/ai tool plans/networking/NETWORKING_PLAN.md`
- `android/ai tool plans/networking/PHASE_C1_MATCHMAKING_CLIENT_PLAN.md`
- `android/ai tool plans/networking/PHASE_C2_C3_LOBBY_PLAN.md`
- `android/ai tool plans/networking/PHASE_C7_MESSAGING_PLAN.md`
- `android/ai tool plans/networking/PHASE2_3_REMAINING_PLAN.md`
- `android/ai tool plans/networking/phase2.5-server-implementation.md`
- `android/ai tool plans/networking/PHASE3_IDENTITY_ANTIABUSE_PLAN.md`
- `android/ai tool plans/networking/PHASE3_POW_AUTH_PLAN.md`
- `android/ai tool plans/networking/plan_5_mp_bugfixes.md`
- `android/ai tool plans/networking/plan_956_lan_mp_fixes.md`
- `android/ai tool plans/networking/plan_coop_desync_crash_log_analysis_20260617.md`
- `android/ai tool plans/networking/plan_coop_level_2_3_log_analysis_20260619.md`
- `android/ai tool plans/networking/plan_coop_rewind_host_disconnect.md`
- `android/ai tool plans/networking/plan_coop_rewind_save_load_20260701.md`
- `android/ai tool plans/networking/plan_coop_save_death_spew_lifetime_20260811.md`
- `android/ai tool plans/networking/plan_coop_saves_callsign_guidebot.md`
- `android/ai tool plans/networking/plan_coop_start_fanout_clean_room_20260611.md`
- `android/ai tool plans/networking/plan_coop_start_fresh_level_reset_20260706.md`
- `android/ai tool plans/networking/plan_coop_start_fresh_savegame_20260618.md`
- `android/ai tool plans/networking/plan_desync_log_analysis_20260616.md`
- `android/ai tool plans/networking/plan_desync_resume_sync_implementation_20260616.md`
- `android/ai tool plans/networking/plan_fix_pdata_loss_after_host_migration.md`
- `android/ai tool plans/networking/plan_fix_proxy_crash_and_logs.md`
- `android/ai tool plans/networking/plan_guidebot_multiplayer.md`

### GQ1-CHUNK-0739

- `android/ai tool plans/networking/plan_ice_progress_ui.md`
- `android/ai tool plans/networking/plan_lan_directconnect_and_logging.md`
- `android/ai tool plans/networking/plan_lan_discovery_resume_self_heal_20260526.md`
- `android/ai tool plans/networking/plan_lan_games_autoselect_layout_cleanup.md`
- `android/ai tool plans/networking/plan_lan_join_by_ip_row_cleanup_20260526.md`
- `android/ai tool plans/networking/plan_lan_mp_fixes.md`
- `android/ai tool plans/networking/plan_lobby_unification.md`
- `android/ai tool plans/networking/plan_mp_bugfixes_round2.md`
- `android/ai tool plans/networking/plan_mp_bugfixes_round3.md`
- `android/ai tool plans/networking/plan_mp_test_fixes.md`
- `android/ai tool plans/networking/plan_multiplayer_100_spew_rate_20260527.md`
- `android/ai tool plans/networking/plan_multiplayer_controller_focus_20260527.md`
- `android/ai tool plans/networking/plan_multiplayer_resume_texture_palette_20260615.md`
- `android/ai tool plans/networking/plan_net_failure_log_analysis_20260615.md`
- `android/ai tool plans/networking/plan_networking_audit.md`
- `android/ai tool plans/networking/plan_networking_round4.md`
- `android/ai tool plans/networking/plan_player_spew_no_expire_20260616.md`
- `android/ai tool plans/networking/plan_relay_limit_ip_privacy_netlog.md`
- `android/ai tool plans/networking/plan_test_failures_20260619.md`
- `android/ai tool plans/networking/plan_test_mp_phase8_fix.md`
- `android/ai tool plans/networking/plan_thief_bot_multiplayer_drops_20260527.md`
- `android/ai tool plans/networking/plan_thief_bot_multiplayer_movement_20260721.md`
- `android/ai tool plans/networking/plan_thief_loot_spew_vectors_20260722.md`
- `android/ai tool plans/networking/plan_uuid_player_identity.md`
- `android/ai tool plans/networking/PLAN.BACKGROUND_RESUME_TEST.md`
- `android/ai tool plans/networking/plan.C4_GAME_LAUNCH.md`
- `android/ai tool plans/networking/PLAN.D1_D2_TOUCH_BINDINGS_FIXES.md`
- `android/ai tool plans/networking/PLAN.D1_PILOT_SEPARATION_TOUCH_BLACKSCREEN.md`
- `android/ai tool plans/networking/plan.ICE_STRATEGY_REWRITE.md`
- `android/ai tool plans/networking/PLAN.KEYBOARD_VIEWPORT_OFFSET.md`
- `android/ai tool plans/networking/plan.NAT_SIMULATOR.md`
- `android/ai tool plans/networking/plan.NAT_TRAVERSAL_IMPL.md`
- `android/ai tool plans/networking/rejoin-exit-diag-phase2.md`
- `android/ai tool plans/networking/rejoin-sync-diagnostics-v4.md`
- `android/ai tool plans/networking/rejoin-sync-fixes-build1115.md`
- `android/ai tool plans/networking/rejoin-sync-log-analysis-build1112.md`
- `android/ai tool plans/networking/relay-fix-plan.md`
- `android/ai tool plans/networking/relay-removal-and-test-dedup.md`
- `android/ai tool plans/networking/round4-mp-fixes.md`
- `android/ai tool plans/networking/study_guidebot_multiplayer.md`

### GQ1-CHUNK-0740

- `android/ai tool plans/overlay, menu, etc/address-caching-overlay-fixes.md`
- `android/ai tool plans/overlay, menu, etc/admin_tray_pause_overlay_fixes.md`
- `android/ai tool plans/overlay, menu, etc/automap_next_objectives_position_20260715.md`
- `android/ai tool plans/overlay, menu, etc/centralize-touch-active-highlight-bindings.md`
- `android/ai tool plans/overlay, menu, etc/cockpit-hud-resolution-fix.md`
- `android/ai tool plans/overlay, menu, etc/consolidated_custom_button_visibility_audit.md`
- `android/ai tool plans/overlay, menu, etc/crash_investigation_menu_close.md`
- `android/ai tool plans/overlay, menu, etc/darker-touch-active-highlight.md`
- `android/ai tool plans/overlay, menu, etc/debug_overlays.md`
- `android/ai tool plans/overlay, menu, etc/debug-logging-black-textures.md`
- `android/ai tool plans/overlay, menu, etc/demo-recording-active-highlight-state.md`
- `android/ai tool plans/overlay, menu, etc/design_boss_health_bar_20260802.md`
- `android/ai tool plans/overlay, menu, etc/design_touch_multi_selector_20260808.md`
- `android/ai tool plans/overlay, menu, etc/etc2comp_hud_labels.md`
- `android/ai tool plans/overlay, menu, etc/fix-multi-item-menu-keyboard-viewport.md`
- `android/ai tool plans/overlay, menu, etc/fix-overlay-color-and-texture-issues.md`
- `android/ai tool plans/overlay, menu, etc/font-rendering-deep-dive.md`
- `android/ai tool plans/overlay, menu, etc/frame-bar-supertrans-graphics-settings.md`
- `android/ai tool plans/overlay, menu, etc/graphics-page-msaa-af-persist.md`
- `android/ai tool plans/overlay, menu, etc/hires-texture-alpha-fix.md`
- `android/ai tool plans/overlay, menu, etc/hires-texture-gap-analysis.md`
- `android/ai tool plans/overlay, menu, etc/hires-transparency-perf-graphics.md`
- `android/ai tool plans/overlay, menu, etc/implement_touch_overlay_music_menu_refactor_20260612.md`
- `android/ai tool plans/overlay, menu, etc/LEVEL_NAME_OVERLAY_PLAN.md`
- `android/ai tool plans/overlay, menu, etc/menu_scale_ogl_plan_20260513.md`
- `android/ai tool plans/overlay, menu, etc/metl154_hires_premerge_fix.md`
- `android/ai tool plans/overlay, menu, etc/metl154_merge_clip_fix.md`
- `android/ai tool plans/overlay, menu, etc/metl154_plain_alpha_fix.md`
- `android/ai tool plans/overlay, menu, etc/metl154_post_merge_clip_followup.md`
- `android/ai tool plans/overlay, menu, etc/metl154_postfix_cleanup_and_debug_harness.md`
- `android/ai tool plans/overlay, menu, etc/metl154_prior_work.md`
- `android/ai tool plans/overlay, menu, etc/metl154_runtime_diagnostic.md`
- `android/ai tool plans/overlay, menu, etc/metl154_tranche1_rename_and_mode_cleanup.md`
- `android/ai tool plans/overlay, menu, etc/metl154_tranche2_hardcoded_diagnostic_removal.md`
- `android/ai tool plans/overlay, menu, etc/metl154_tranche3_shared_snapshot_and_regression.md`
- `android/ai tool plans/overlay, menu, etc/metl154_tranche4_shader_and_gl_state_audit.md`
- `android/ai tool plans/overlay, menu, etc/metl154_tranche5_cache_and_efficiency.md`
- `android/ai tool plans/overlay, menu, etc/metl154_tranche6_two_pass_cull_cleanup.md`
- `android/ai tool plans/overlay, menu, etc/metl154_tranche7_two_pass_polygon_offset_cleanup.md`
- `android/ai tool plans/overlay, menu, etc/metl154_tranche8_two_pass_debug_depth_cleanup.md`

### GQ1-CHUNK-0741

- `android/ai tool plans/overlay, menu, etc/metl154_tranche9_launcher_debug_controls.md`
- `android/ai tool plans/overlay, menu, etc/metl154_visibility_backoff_and_occlusion_probe.md`
- `android/ai tool plans/overlay, menu, etc/metl154-mipmap-alpha-fix.md`
- `android/ai tool plans/overlay, menu, etc/mission_zip_soundtrack_unified_20260612.md`
- `android/ai tool plans/overlay, menu, etc/music_one_track_checkbox_snap_20260612.md`
- `android/ai tool plans/overlay, menu, etc/music_overlay_midi_to_mission_freeze_20260612.md`
- `android/ai tool plans/overlay, menu, etc/music_overlay_refresh_and_controls_20260612.md`
- `android/ai tool plans/overlay, menu, etc/music_overlay_source_dropdown_20260612.md`
- `android/ai tool plans/overlay, menu, etc/music_overlay_source_semantics_and_castaway_20260612.md`
- `android/ai tool plans/overlay, menu, etc/music_overlay_testing_fixes_20260612.md`
- `android/ai tool plans/overlay, menu, etc/net-stats-overlay-improvements.md`
- `android/ai tool plans/overlay, menu, etc/objective_automap_overlay_plan_20260711.md`
- `android/ai tool plans/overlay, menu, etc/objective_number_connector_and_metadata_info_20260715.md`
- `android/ai tool plans/overlay, menu, etc/ogl_merge_super_transparent.md`
- `android/ai tool plans/overlay, menu, etc/ogl_render_resolution.md`
- `android/ai tool plans/overlay, menu, etc/overlay_save_preview_supertransparency_research.md`
- `android/ai tool plans/overlay, menu, etc/overlay-fixes-and-selective-filtering.md`
- `android/ai tool plans/overlay, menu, etc/overlay-fixes-round2.md`
- `android/ai tool plans/overlay, menu, etc/overlay-launcher-graphics-settings.md`
- `android/ai tool plans/overlay, menu, etc/overlay-rendering-four-cases.md`
- `android/ai tool plans/overlay, menu, etc/phase4_metl154_regression_followup.md`
- `android/ai tool plans/overlay, menu, etc/phase5_metl154_new_angles_diagnosis.md`
- `android/ai tool plans/overlay, menu, etc/phase5-network-events-overlay.md`
- `android/ai tool plans/overlay, menu, etc/plan_afterburner_stick_highlight_fill_20260704.md`
- `android/ai tool plans/overlay, menu, etc/plan_android_original_graphics_options_reuse_study.md`
- `android/ai tool plans/overlay, menu, etc/plan_automap_touch_overlay_rewrite_20260602.md`
- `android/ai tool plans/overlay, menu, etc/plan_autosave_thumbnail_reliability_research_20260525.md`
- `android/ai tool plans/overlay, menu, etc/plan_autoselect_dpad_reorder_20260602.md`
- `android/ai tool plans/overlay, menu, etc/plan_broad_android_border_backing_20260610.md`
- `android/ai tool plans/overlay, menu, etc/plan_controller_only_menu_buttons_overlay_20260703.md`
- `android/ai tool plans/overlay, menu, etc/plan_controller_only_warp_accept_unbound_20260527.md`
- `android/ai tool plans/overlay, menu, etc/plan_controls_editor_device_logging_20260603.md`
- `android/ai tool plans/overlay, menu, etc/plan_controls_editor_device_text_20260603.md`
- `android/ai tool plans/overlay, menu, etc/plan_controls_editor_scroll_20260603.md`
- `android/ai tool plans/overlay, menu, etc/plan_controls_editor_text_readability_20260603.md`
- `android/ai tool plans/overlay, menu, etc/plan_controls_menu_touch_coordinates_20260603.md`
- `android/ai tool plans/overlay, menu, etc/plan_coop_post_level_skip_input_20260806.md`
- `android/ai tool plans/overlay, menu, etc/plan_cutscene_video_borders_20260526.md`
- `android/ai tool plans/overlay, menu, etc/plan_d1_difficulty_menu_tap_debug_20260619.md`
- `android/ai tool plans/overlay, menu, etc/plan_d1_render_crash_alignment.md`

### GQ1-CHUNK-0742

- `android/ai tool plans/overlay, menu, etc/plan_d1_touch_afterburner_bar_20260621.md`
- `android/ai tool plans/overlay, menu, etc/plan_d2_skip_title_screen.md`
- `android/ai tool plans/overlay, menu, etc/plan_death_saying_keyboard_back_20260618.md`
- `android/ai tool plans/overlay, menu, etc/plan_diagnose_black_3d_textures.md`
- `android/ai tool plans/overlay, menu, etc/plan_door45_base_texture_pose_repro.md`
- `android/ai tool plans/overlay, menu, etc/plan_door45_paged_out_buf_stale.md`
- `android/ai tool plans/overlay, menu, etc/plan_door45_single_draw_state_reset.md`
- `android/ai tool plans/overlay, menu, etc/plan_dpad_text_input_nav_audit_20260602.md`
- `android/ai tool plans/overlay, menu, etc/plan_energy_shield_hold_20260610.md`
- `android/ai tool plans/overlay, menu, etc/plan_energy_shield_unbound_message_20260606.md`
- `android/ai tool plans/overlay, menu, etc/plan_etc2_to_ktx2_migration.md`
- `android/ai tool plans/overlay, menu, etc/plan_exit_overlay_button_20260526.md`
- `android/ai tool plans/overlay, menu, etc/plan_extract_reorder_helper_20260604.md`
- `android/ai tool plans/overlay, menu, etc/plan_fix_host_build_blank_thumbnail_20260513.md`
- `android/ai tool plans/overlay, menu, etc/plan_fix_msaa_missile_glitch.md`
- `android/ai tool plans/overlay, menu, etc/plan_general_touch_region_diagnostics_20260605.md`
- `android/ai tool plans/overlay, menu, etc/plan_gles3_etc2_hires_textures.md`
- `android/ai tool plans/overlay, menu, etc/plan_guidebot_d1_vs_d2_texture_feasibility_20260613.md`
- `android/ai tool plans/overlay, menu, etc/plan_guidebot_locked_spawn_20260606.md`
- `android/ai tool plans/overlay, menu, etc/plan_guidebot_nearest_point_navigation_20260611.md`
- `android/ai tool plans/overlay, menu, etc/plan_guidebot_progress_text_investigation_20260714.md`
- `android/ai tool plans/overlay, menu, etc/plan_guidebot_wheel_next_release_20260704.md`
- `android/ai tool plans/overlay, menu, etc/plan_guidebot_wheel_unlock_next_goal_20260703.md`
- `android/ai tool plans/overlay, menu, etc/plan_hidden_cover_focus_audit.md`
- `android/ai tool plans/overlay, menu, etc/plan_hud_pip_rear_view_research_20260613.md`
- `android/ai tool plans/overlay, menu, etc/plan_hud_strips_overlay_button.md`
- `android/ai tool plans/overlay, menu, etc/plan_in_game_difficulty_change_20260604.md`
- `android/ai tool plans/overlay, menu, etc/plan_in_game_fov_slider_20260604.md`
- `android/ai tool plans/overlay, menu, etc/plan_indicator_line_bold_warp_liberal_20260703.md`
- `android/ai tool plans/overlay, menu, etc/plan_indicator_line_fixes.md`
- `android/ai tool plans/overlay, menu, etc/plan_indicator_line_thickness_20260703.md`
- `android/ai tool plans/overlay, menu, etc/plan_ingame_autoselect_long_press_drag_20260604.md`
- `android/ai tool plans/overlay, menu, etc/plan_ingame_load_save_preview_highlight_20260605.md`
- `android/ai tool plans/overlay, menu, etc/plan_ingame_menu_text_readability_20260603.md`
- `android/ai tool plans/overlay, menu, etc/plan_intro_movie_loading_android.md`
- `android/ai tool plans/overlay, menu, etc/plan_intro_skip_input_relaunch_fix.md`
- `android/ai tool plans/overlay, menu, etc/plan_intro_timeout_batch_20260515.md`
- `android/ai tool plans/overlay, menu, etc/plan_landscape_portrait_ui_fixes.md`
- `android/ai tool plans/overlay, menu, etc/plan_launcher_outline_menu_fixes_20260602.md`
- `android/ai tool plans/overlay, menu, etc/plan_level_complete_stats_page_20260515.md`

### GQ1-CHUNK-0743

- `android/ai tool plans/overlay, menu, etc/plan_locked_guide_deploy_offset_20260809.md`
- `android/ai tool plans/overlay, menu, etc/plan_menu_loading_texture_filter_regression_20260508.md`
- `android/ai tool plans/overlay, menu, etc/plan_merge_coop_into_net_stats_20260809.md`
- `android/ai tool plans/overlay, menu, etc/plan_merged_wall_texture_shift_diagnostic.md`
- `android/ai tool plans/overlay, menu, etc/plan_mine_exit_movie_skip.md`
- `android/ai tool plans/overlay, menu, etc/plan_minimize_save_thumbnails_20260514.md`
- `android/ai tool plans/overlay, menu, etc/plan_modal_border_static_survey_20260610.md`
- `android/ai tool plans/overlay, menu, etc/plan_multiplayer_abdicate_guidebot_settings_20260527.md`
- `android/ai tool plans/overlay, menu, etc/plan_multiplayer_join_wait_background_20260526.md`
- `android/ai tool plans/overlay, menu, etc/plan_multiplayer_stats_overlay.md`
- `android/ai tool plans/overlay, menu, etc/plan_overlay_menu_followups_20260522b.md`
- `android/ai tool plans/overlay, menu, etc/plan_oversized_texture_fix.md`
- `android/ai tool plans/overlay, menu, etc/plan_pilot_select_touch_delete_filter_20260605.md`
- `android/ai tool plans/overlay, menu, etc/plan_pilot_touch_log_analysis_20260606.md`
- `android/ai tool plans/overlay, menu, etc/plan_pilot_touch_second_log_analysis_20260606.md`
- `android/ai tool plans/overlay, menu, etc/plan_preexisting_bug_triage_20260601.md`
- `android/ai tool plans/overlay, menu, etc/plan_radiused_corner_hud_text_inset_20260602.md`
- `android/ai tool plans/overlay, menu, etc/plan_relax_hires_texture_limits.md`
- `android/ai tool plans/overlay, menu, etc/plan_robot_hostage_hud_counts_20260601.md`
- `android/ai tool plans/overlay, menu, etc/plan_robot_total_runtime_spawns_20260603.md`
- `android/ai tool plans/overlay, menu, etc/plan_rounded_corner_hud_text_inset_20260602.md`
- `android/ai tool plans/overlay, menu, etc/plan_rounded_corner_hud_text_inset_tristate_20260602.md`
- `android/ai tool plans/overlay, menu, etc/plan_save_game_default_level_name_20260606.md`
- `android/ai tool plans/overlay, menu, etc/plan_score_added_counter_stack_20260602.md`
- `android/ai tool plans/overlay, menu, etc/plan_scroll_selector_card_scale_and_runtime_line_20260809.md`
- `android/ai tool plans/overlay, menu, etc/plan_scroll_selector_row_offset_20260809.md`
- `android/ai tool plans/overlay, menu, etc/plan_scroll_selector_runtime_touch_priority_20260809.md`
- `android/ai tool plans/overlay, menu, etc/plan_selective_android_frame_clear_20260610.md`
- `android/ai tool plans/overlay, menu, etc/plan_shield_gap_brightness_and_perf_probe_20260520.md`
- `android/ai tool plans/overlay, menu, etc/plan_shield_graphics_perf_20260521.md`
- `android/ai tool plans/overlay, menu, etc/plan_shield_keyboard_gap_and_texture_load_20260520.md`
- `android/ai tool plans/overlay, menu, etc/plan_shield_texture_log_analysis_20260520.md`
- `android/ai tool plans/overlay, menu, etc/plan_skippable_intro_movies.md`
- `android/ai tool plans/overlay, menu, etc/plan_start_game_overlay_select_players_20260526.md`
- `android/ai tool plans/overlay, menu, etc/plan_startup_error_static_background_20260607.md`
- `android/ai tool plans/overlay, menu, etc/plan_stats_persistence_20260608.md`
- `android/ai tool plans/overlay, menu, etc/plan_tex_naming_128_readme_mine_exit.md`
- `android/ai tool plans/overlay, menu, etc/plan_texture_loading_diagnostics.md`
- `android/ai tool plans/overlay, menu, etc/plan_texture_scan_netstats_debuglog_shader_warnings.md`
- `android/ai tool plans/overlay, menu, etc/plan_three_autosave_categories_20260525.md`

### GQ1-CHUNK-0744

- `android/ai tool plans/overlay, menu, etc/plan_thumbnail_cleanup_20260514.md`
- `android/ai tool plans/overlay, menu, etc/plan_touch_editor_dpad_navigation_20260602.md`
- `android/ai tool plans/overlay, menu, etc/plan_touch_editor_hit_priority_20260809.md`
- `android/ai tool plans/overlay, menu, etc/plan_touch_multi_selector_implementation_20260808.md`
- `android/ai tool plans/overlay, menu, etc/plan_touch_pip_window_cycle_buttons_20260613.md`
- `android/ai tool plans/overlay, menu, etc/plan_touch_selector_anchor_alignment_20260809.md`
- `android/ai tool plans/overlay, menu, etc/plan_track_selector_shared_render_geometry_20260809.md`
- `android/ai tool plans/overlay, menu, etc/plan_transparency_effects_menu_freeze_20260602.md`
- `android/ai tool plans/overlay, menu, etc/plan_video_overlay_af_msaa_live_apply_research_20260524.md`
- `android/ai tool plans/overlay, menu, etc/plan_video_overlay_brightness_20260523.md`
- `android/ai tool plans/overlay, menu, etc/plan_video_overlay_height_scaling_20260524.md`
- `android/ai tool plans/overlay, menu, etc/plan_weapon_wheel_ammo_status_20260601.md`
- `android/ai tool plans/overlay, menu, etc/plan-cockpit-fix-overlay-consolidation-build-step.md`
- `android/ai tool plans/overlay, menu, etc/plan-cutscene-death-overlays-mouse-mode-autoselect.md`
- `android/ai tool plans/overlay, menu, etc/plan-etc2-black-texture-diagnostics.md`
- `android/ai tool plans/overlay, menu, etc/plan-video-overlay-stats-etc2-hang.md`
- `android/ai tool plans/overlay, menu, etc/save_music_preferences_to_player_file_20260612.md`
- `android/ai tool plans/overlay, menu, etc/SKIP_BUTTON_PLAN.md`
- `android/ai tool plans/overlay, menu, etc/strip-split-and-tmap2-labels.md`
- `android/ai tool plans/overlay, menu, etc/study_cheats_menu_redesign_20260608.md`
- `android/ai tool plans/overlay, menu, etc/study_touch_overlay_music_menu_refactor_20260612.md`
- `android/ai tool plans/overlay, menu, etc/super_transparent_mask_fix.md`
- `android/ai tool plans/overlay, menu, etc/supertransparent_threshold_docs_cleanup.md`
- `android/ai tool plans/overlay, menu, etc/texture_visual_issues_investigation.md`
- `android/ai tool plans/overlay, menu, etc/ui_cleanup_batch_2.md`
- `android/ai tool plans/overlay, menu, etc/ui_cleanup_batch.md`
- `android/ai tool plans/overlay, menu, etc/VIDEO_INFO_OVERLAY_PLAN.md`
- `android/ai tool plans/overlay, menu, etc/video_overlay_admin_tray_race_and_windows_build_helper.md`
- `android/ai tool plans/plan_mission_descriptor_file_details_20260605.md`
- `android/ai tool plans/plan_mission_zip_readme_view_20260606.md`
- `android/ai tool plans/plan_sectorgame_zip_mission_import_20260604.md`
- `android/ai tool plans/plan_test_saf_archiver_report_20260523_232445.md`
- `android/ai tool plans/plan_uneasy_mission_zip_ogg_music_20260606.md`
- `android/ai tool plans/plan_uneasy_zip_hog_level_loading_20260605.md`
- `android/ai tool plans/plan_uneasy_zip_hog_realpath_fix_20260605.md`
- `android/ai tool plans/plan_zip_mod_file_button_clipping_20260605.md`
- `android/ai tool plans/release_upload_build_stamp_20260603.md`
- `android/ai tool plans/reusable/cleanup.md`
- `android/ai tool plans/reusable/demo-file-format-reference.md`
- `android/ai tool plans/reusable/fp-determinism-survey-20260428-report.md`

### GQ1-CHUNK-0745

- `android/ai tool plans/reusable/fp-determinism-survey-20260428.md`
- `android/ai tool plans/reusable/fp-determinism-test-notes-20260429.md`
- `android/ai tool plans/reusable/plan_reusable_cleanup_instructions_20260517.md`
- `android/ai tool plans/reusable/polling-not-timeouts.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_cd_storage_pilot_delete_20260520.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_d1_level14_route_debug_20260608.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_door45_and_saf_archiver_triage_20260516.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_level_metadata_crash_repro_fix_20260608.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_level_metadata_implementation_20260608.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_level_metadata_native_safety_study_20260608.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_level_metadata_route_completion_20260608.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_level_metadata_scroll_ui_20260608.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_level_metadata_view_study_20260608.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_level_metadata_volume_travel_time_20260608.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_masked_save_sets_and_explorer_20260606.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_metadata_start_secret_diff_20260608.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_metadata_volume_display_precision_20260608.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_missing_reactor_travel_note_20260608.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_obsidian_trigger_wall_travel_20260608.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_periodic_android_autosaves_20260606.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_plutonian_shores_unreachable_research_20260610.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_reactor_shootable_route_metadata_20260611.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_resume_recent_scoped_save_crash_20260606.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_resume_save_logging_cleanup_20260606.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_saf_archiver_wrapper_hang_20260516.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_saf_link_removal_and_cd_filter_20260507.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_save_explorer_base_save_visibility_20260606.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_save_explorer_default_recent_sort_20260606.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_save_explorer_detail_popover_20260607.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_save_explorer_most_recent_default_20260607.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_save_explorer_recent_dedup_20260606.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_save_structure_resume_regression_20260608.md`
- `android/ai tool plans/storage, SAF, edge cases/plan_unified_free_space_preflight_20260611.md`
- `android/ai tool plans/storage, SAF, edge cases/study_level_metadata_volume_travel_time_20260608.md`
- `android/ai tool plans/test runner/plan_run_all_tests_ctrl_c_20260619.md`
- `android/ai tool plans/testing/2026-07-21-emulator-contamination-audit.md`
- `android/ai tool plans/testing/2026-07-21-harden-gog-installer-d1-test.md`
- `android/ai tool plans/testing/2026-07-21-harden-gradle-unit-test-state.md`
- `android/ai tool plans/testing/2026-07-21-harden-merged-wall-snapshot-test.md`
- `android/ai tool plans/testing/2026-07-21-latest-report-failure-fix.md`

### GQ1-CHUNK-0746

- `android/ai tool plans/testing/2026-07-21-remaining-report-failures.md`
- `android/ai tool plans/testing/audit_game_data_script_tracking_20260722.md`
- `android/ai tool plans/testing/audit_powershell_cwd_leaks_20260707.md`
- `android/ai tool plans/testing/auto-infra-test-suite.md`
- `android/ai tool plans/testing/cd_regression_adb_daemon_recovery_20260801.md`
- `android/ai tool plans/testing/demo-regression-testing.md`
- `android/ai tool plans/testing/draft_copilot_instructions_additions.md`
- `android/ai tool plans/testing/drmsaga_mission_name_regression_20260802.md`
- `android/ai tool plans/testing/extraction_launcher_handoff_recovery_20260801.md`
- `android/ai tool plans/testing/fix_cd_batch_scalar_and_failure_reporting_20260722.md`
- `android/ai tool plans/testing/fix_cd_regression_library_workflow_20260722.md`
- `android/ai tool plans/testing/fix_extract_suite_setup_health_20260722.md`
- `android/ai tool plans/testing/fix_host_metadata_helper_cwd_20260707.md`
- `android/ai tool plans/testing/fix_test_failures_20260329.md`
- `android/ai tool plans/testing/fix-test-failures-20260328.md`
- `android/ai tool plans/testing/fix-test-failures-20260406.md`
- `android/ai tool plans/testing/fix-test-suite-failures.md`
- `android/ai tool plans/testing/full_mission_metadata_regeneration_20260705.md`
- `android/ai tool plans/testing/full_track_chromaprint_20260801.md`
- `android/ai tool plans/testing/host_metadata_regeneration_implementation_20260705.md`
- `android/ai tool plans/testing/host_metadata_regeneration_repair_20260705.md`
- `android/ai tool plans/testing/host_mission_metadata_regeneration_design_20260705.md`
- `android/ai tool plans/testing/level_metadata_json_normalization_plan_20260705.md`
- `android/ai tool plans/testing/manual_metadata_regeneration_progress_20260705.md`
- `android/ai tool plans/testing/metadata_regeneration_script_docs_20260705.md`
- `android/ai tool plans/testing/mission_metadata_active_set_preflight.md`
- `android/ai tool plans/testing/mission_metadata_numeric_canonicalization_20260718.md`
- `android/ai tool plans/testing/mission_metadata_regen_failures_20260801.md`
- `android/ai tool plans/testing/mission_metadata_slow_host_hardening_20260801.md`
- `android/ai tool plans/testing/mission_zip_launcher_process_exit_20260802.md`
- `android/ai tool plans/testing/music_fingerprint_atomic_replace_20260801.md`
- `android/ai tool plans/testing/plan_added_test_consolidation_20260811.md`
- `android/ai tool plans/testing/plan_all_cd_level_metadata_regressions_20260730.md`
- `android/ai tool plans/testing/plan_autosave_resume_test_triage_20260516b.md`
- `android/ai tool plans/testing/plan_autosave_resume_timeout_20260621.md`
- `android/ai tool plans/testing/plan_cd_level_metadata_regressions_20260730.md`
- `android/ai tool plans/testing/plan_cd_regression_full_run_investigation_20260723.md`
- `android/ai tool plans/testing/plan_cd_regression_repairs_20260723.md`
- `android/ai tool plans/testing/plan_cleanup_report_quibbles_20260628_195417.md`
- `android/ai tool plans/testing/plan_combined_extract_launch_fixture_20260730.md`

### GQ1-CHUNK-0747

- `android/ai tool plans/testing/plan_coop_start_metadata_20260609.md`
- `android/ai tool plans/testing/plan_emulator_test_consolidation_round2_survey_20260723.md`
- `android/ai tool plans/testing/plan_emulator_test_consolidation_survey_20260723.md`
- `android/ai tool plans/testing/plan_engine_prefs_test_triage_20260516.md`
- `android/ai tool plans/testing/plan_failure_fault_tolerance_20260621.md`
- `android/ai tool plans/testing/plan_fault_tolerance_report_20260621_140747.md`
- `android/ai tool plans/testing/plan_final_quick_record_failure_20260804.md`
- `android/ai tool plans/testing/plan_fire_primary_test_probe_20260516.md`
- `android/ai tool plans/testing/plan_fix_test_failures_20260409_round2.md`
- `android/ai tool plans/testing/plan_fix_test_failures_20260409.md`
- `android/ai tool plans/testing/plan_full_suite_rerun_after_demo_cleanup_20260517.md`
- `android/ai tool plans/testing/plan_game_data_index_canonicalization_20260526.md`
- `android/ai tool plans/testing/plan_game_data_index_minimal_test_deps_20260526.md`
- `android/ai tool plans/testing/plan_get_deps_helpers_reorg_20260526.md`
- `android/ai tool plans/testing/plan_gog_regression_launch_timeout_cleanup_20260526.md`
- `android/ai tool plans/testing/plan_intro_test_cleanup.md`
- `android/ai tool plans/testing/plan_json_baseline_pretty_print_20260609.md`
- `android/ai tool plans/testing/plan_json_output_normalization_in_tests_20260609.md`
- `android/ai tool plans/testing/plan_last_two_failures_20260806.md`
- `android/ai tool plans/testing/plan_levelcomplete_touch_skip_triage_20260516.md`
- `android/ai tool plans/testing/plan_linux_data_recovery_helpers_20260526.md`
- `android/ai tool plans/testing/plan_linux_regression_testing_compat_20260525.md`
- `android/ai tool plans/testing/plan_master_regression_data_regeneration.md`
- `android/ai tool plans/testing/plan_merge_conflict_cleanup_20260526.md`
- `android/ai tool plans/testing/plan_merged_wall_next_failure_20260628.md`
- `android/ai tool plans/testing/plan_merged_wall_two_pass_geometry_contract_20260624.md`
- `android/ai tool plans/testing/plan_merged_wall_two_pass_probe_20260624.md`
- `android/ai tool plans/testing/plan_mission_zip_batch_d1_d2_detection_20260609.md`
- `android/ai tool plans/testing/plan_mission_zip_batch_emulator_recovery_and_crashes_20260609.md`
- `android/ai tool plans/testing/plan_mission_zip_batch_manual_run_20260609.md`
- `android/ai tool plans/testing/plan_mission_zip_batch_metadata_launch_20260608.md`
- `android/ai tool plans/testing/plan_mission_zip_regression_gitignore_20260609.md`
- `android/ai tool plans/testing/plan_mission_zip_regression_output_and_assets_20260609.md`
- `android/ai tool plans/testing/plan_next_failing_tests_20260516.md`
- `android/ai tool plans/testing/plan_next_failure_transform_20260628.md`
- `android/ai tool plans/testing/plan_next_remaining_failure_20260628.md`
- `android/ai tool plans/testing/plan_pilot_long_hold_delete_hardening_20260624.md`
- `android/ai tool plans/testing/plan_preexisting_test_fixes.md`
- `android/ai tool plans/testing/plan_quick_record_sidecar_hardening_20260624.md`
- `android/ai tool plans/testing/plan_quick_record_sidecar_install_hardening_20260624.md`

### GQ1-CHUNK-0748

- `android/ai tool plans/testing/plan_quick_tests_historical_seconds_20260701.md`
- `android/ai tool plans/testing/plan_recent_batch_failures_20260806.md`
- `android/ai tool plans/testing/plan_recent_critical_review_failures_20260804.md`
- `android/ai tool plans/testing/plan_recent_test_report_fixes_20260612.md`
- `android/ai tool plans/testing/plan_remaining_test_failures_20260516.md`
- `android/ai tool plans/testing/plan_remaining_test_failures_fresh_report_round_20260517.md`
- `android/ai tool plans/testing/plan_report_20260518_223317_fixes.md`
- `android/ai tool plans/testing/plan_report_20260519_232520_fixes.md`
- `android/ai tool plans/testing/plan_report_20260520_232305_triage.md`
- `android/ai tool plans/testing/plan_report_20260522_232545_followup.md`
- `android/ai tool plans/testing/plan_report_20260526_144107_windows_compat.md`
- `android/ai tool plans/testing/plan_report_20260528_115705_failures.md`
- `android/ai tool plans/testing/plan_report_20260528_153202_axis_mapping.md`
- `android/ai tool plans/testing/plan_report_20260528_201452_failures.md`
- `android/ai tool plans/testing/plan_report_20260620_153831_state_cluster.md`
- `android/ai tool plans/testing/plan_report_20260624_201136_hardening.md`
- `android/ai tool plans/testing/plan_report_20260627_231136_hardening.md`
- `android/ai tool plans/testing/plan_report_20260627_231136_remaining_hardening.md`
- `android/ai tool plans/testing/plan_report_20260628_131121_followup.md`
- `android/ai tool plans/testing/plan_report_20260628_151027_followup.md`
- `android/ai tool plans/testing/plan_report_20260628_195417_audit.md`
- `android/ai tool plans/testing/plan_report_20260628_222248_timeout_failure.md`
- `android/ai tool plans/testing/plan_report_20260725_184825_failures.md`
- `android/ai tool plans/testing/plan_report_20260725_final_failures.md`
- `android/ai tool plans/testing/plan_report_20260725_route_failures.md`
- `android/ai tool plans/testing/plan_report_20260725_route_followup.md`
- `android/ai tool plans/testing/plan_report_20260726_001059_failures.md`
- `android/ai tool plans/testing/plan_report_20260726_161246_failures.md`
- `android/ai tool plans/testing/plan_report_20260727_215931_failures.md`
- `android/ai tool plans/testing/plan_report_triage_20260516.md`
- `android/ai tool plans/testing/plan_resolution_test_triage_20260516.md`
- `android/ai tool plans/testing/plan_reticle_options_test_triage_20260516.md`
- `android/ai tool plans/testing/plan_reusable_level_metadata_zip_repro_20260608.md`
- `android/ai tool plans/testing/plan_run_all_cd_result_divergence_20260724.md`
- `android/ai tool plans/testing/plan_run_all_extract_subset_research_20260526.md`
- `android/ai tool plans/testing/plan_run_all_tests_failure_followup_20260617.md`
- `android/ai tool plans/testing/plan_run_all_tests_input_demo_matrix_20260617.md`
- `android/ai tool plans/testing/plan_run_all_tests_linux_20260525.md`
- `android/ai tool plans/testing/plan_run_all_tests_linux_20260602.md`
- `android/ai tool plans/testing/plan_run_all_tests_prereq_preflight_20260515.md`

### GQ1-CHUNK-0749

- `android/ai tool plans/testing/plan_run_quick_tests_20260519.md`
- `android/ai tool plans/testing/plan_run_testmenu_mission_zip_batch_20260609.md`
- `android/ai tool plans/testing/plan_saf_basic_test_triage_20260516.md`
- `android/ai tool plans/testing/plan_server_and_engine_prefs_failures_20260805.md`
- `android/ai tool plans/testing/plan_test_abort_game_to_main_menu_d2_report_20260523_114346.md`
- `android/ai tool plans/testing/plan_test_centralization_survey_20260525.md`
- `android/ai tool plans/testing/plan_test_dpad_triggers_report_20260522_232545.md`
- `android/ai tool plans/testing/plan_test_dpad_triggers_report_20260523_114346.md`
- `android/ai tool plans/testing/plan_test_failure_pattern_standardization.md`
- `android/ai tool plans/testing/plan_test_failure_robustness_20260628.md`
- `android/ai tool plans/testing/plan_test_input_demo_runtime_smoke_20260523_110600.md`
- `android/ai tool plans/testing/plan_test_parameterization.md`
- `android/ai tool plans/testing/plan_test_progress_estimate.md`
- `android/ai tool plans/testing/plan_test_remaining_runtime_estimator.md`
- `android/ai tool plans/testing/plan_test_report_fixes.md`
- `android/ai tool plans/testing/plan_test_report_rt_pause.md`
- `android/ai tool plans/testing/plan_test_runner_progress_and_combined_failures_20260723.md`
- `android/ai tool plans/testing/plan_test_suite_cleanup_20260620.md`
- `android/ai tool plans/testing/plan_test_suite_dedup_runtime_pass_20260806.md`
- `android/ai tool plans/testing/plan_test_suite_fixes.md`
- `android/ai tool plans/testing/plan_test_suite_runtime_survey_20260620.md`
- `android/ai tool plans/testing/plan_two_batch_failures_20260805.md`
- `android/ai tool plans/testing/plan.TEST_RELIABILITY.md`
- `android/ai tool plans/testing/plan.TEST_REPORT_FIXES_20260328.md`
- `android/ai tool plans/testing/regenerate_all_mission_metadata_20260705.md`
- `android/ai tool plans/testing/regenerated_regression_diff_audit_20260801.md`
- `android/ai tool plans/testing/regeneration_diff_review_20260801.md`
- `android/ai tool plans/testing/regeneration_output_stability_20260801.md`
- `android/ai tool plans/testing/regeneration_wrapper_argument_validation_20260801.md`
- `android/ai tool plans/testing/report_20260612_165248_intro_state_cluster.md`
- `android/ai tool plans/testing/report_20260612_165248_pause_cluster.md`
- `android/ai tool plans/testing/report_20260612_201722_failure_triage.md`
- `android/ai tool plans/testing/report_20260612_220011_autosave_missing_pilot_timeout.md`
- `android/ai tool plans/testing/test_fixes_and_speed_survey_20260328.md`
- `android/ai tool plans/testing/test_suite_rationalization_and_flake_hardening_20260709.md`
- `android/ai tool plans/testing/test_suite_speed_fixed_wait_audit_20260612.md`
- `android/ai tool plans/testing/unified_cd_regression_runner_20260722.md`
- `android/ai tool plans/testing/unify_test_cleanup_and_controller_compare.md`
- `android/ai tool plans/testing/zero_param_metadata_regen_helper_20260705.md`
- `android/ai tool plans/ui/level-metadata-restore-flash.md`

### GQ1-CHUNK-0750

- `android/app/src/main/cpp/extract/test/data/bad_backwards_index.cue`
- `android/app/src/main/cpp/extract/test/data/bad_empty.cue`
- `android/app/src/main/cpp/extract/test/data/bad_garbage_msf.cue`
- `android/app/src/main/cpp/extract/test/data/bad_missing_index.cue`
- `android/app/src/main/cpp/extract/test/data/bad_no_file_directive.cue`
- `android/app/src/main/cpp/extract/test/data/bad_start_past_eof.cue`
- `android/app/src/main/cpp/extract/test/data/bad_unclosed_quote.cue`
- `android/app/src/main/cpp/extract/test/data/valid_data_plus_audio.cue`
- `android/app/src/main/cpp/extract/test/data/valid_extra_whitespace.cue`
- `android/app/src/main/cpp/extract/test/data/valid_many_audio.cue`
- `android/app/src/main/cpp/extract/test/data/valid_mode2.cue`
- `android/app/src/main/cpp/extract/test/data/valid_multi_file.cue`
- `android/app/src/main/cpp/extract/test/data/valid_pregap.cue`
- `android/app/src/main/cpp/extract/test/data/valid_single_data.cue`
- `android/docker/nat-testbed/Dockerfile`
- `android/gradlew`
- `android/tests/test_redbook_data/test_disc.cue`
- `android/tests/udp_test_tool_android.csrc`
- `game_data_to_copy_to_emulator/data/.gitkeep`
- `game_data_to_copy_to_emulator/download/.gitkeep`
- `game_data/CD images/Descent Anniversary (ISO)/descent_anniversary.cue`
- `server/config.json5.lan`

## Chunk completion notes

Append one completion note for every finished queue item using the process template

### GQ1-PREFLIGHT-001 completion

- Worker: fresh `gpt-5.6-sol` worker at medium reasoning effort
- Outcome: `ISSUES`; normalized as `GQC-0011`
- Frozen scope: complete diff `fb555eec75e1ed12c8348805ab335afb4c721b06..7877ad30d05887b8e19869ed4c50075e41e2f88e`; 1,252 commits, 3,136 paths, +810,516/-4,360
- Assigned questions completed: PR composition, provenance, generated artifacts, secrets, licenses, and split strategy
- Normalized observations: artifact evidence extended `GQF-0001` through `GQF-0004`; server-license evidence extended `GQF-0013`; historical roots `BR-0001`, `BR-0002`, `BR-0003`, `BR-0007`, and `BR-0073` were linked without duplicate findings; production dependency integrity was admitted as `GQF-0024`; the full-history secret-scan gap was retained as `GQI-0001`
- Explicit clean dimensions: intentional binary fixtures, Cargo lock integrity, absence of commercial payload formats among classified artifacts, representative generated-data consumers, bounded frozen-head credential patterns, and narrow ignore allowlists
- Limits: no dedicated dependency, license, vulnerability, or secret scanner; no build, package, generator, emulator, or runtime execution; generated fixtures and historical plans retain their assigned mechanical coverage
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-PREFLIGHT-002 completion

- Worker: separate fresh `gpt-5.6-sol` worker at medium reasoning effort
- Outcome: `ISSUES`; normalized as `GQC-0012`
- Frozen scope: complete diff architecture, intended behavior, ownership boundaries, and highest-risk data flows; 3,136 changed paths with deterministic name/status and raw-blob fingerprints
- Assigned questions completed: mapped eight subsystems and eight principal ownership boundaries; traced launcher/file publication, isolated engine processes, paired native ownership, import/extraction, input/diagnostics, replay/automation, multiplayer/server, and build/test flows
- Normalized observations: extended `GQF-0005` through `GQF-0009`, `GQF-0011`, `GQF-0014`, `GQF-0015`, `GQF-0018`, and `GQF-0020` through `GQF-0023`; linked live and archived BR owners; found no distinct new architecture root
- Explicit clean dimensions: isolated non-exported engine/metadata processes, separate D1/D2 build ownership, representative native binary-format ownership, cross-process state bridge without structural `BR-0004` regrowth, ordinary atomic file publication, request-owned metadata containment, durable automation oracles, and visible server module ownership
- Limits: static frozen review only; no exhaustive line review, protocol-schema diff, build, emulator, hostile archive, sanitizer, fuzz, multiplayer, or server execution
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-PREFLIGHT-003 completion

- Worker: separate fresh `gpt-5.6-sol` worker at medium reasoning effort
- Outcome: `ISSUES`; normalized as `GQC-0013`
- Frozen scope: complete 1,252-commit change history, including commit clusters, superseded approaches, partial migrations, and abandoned compatibility paths
- Normalized observations: complete-history and stale-owner evidence extended `BR-0002` and `BR-0006`; admitted `GQF-0025` through `GQF-0028` for one incomplete test consolidation and three unsupported pre-release compatibility residues
- Explicit clean dimensions: complete immediate matchmaking revert, closed JNI source migration with anti-regrowth guard, root-to-helper script relocation, most unified-test migrations, DMR1 ownership moves, and intentional inherited engine/media compatibility
- Limits: static history and reference analysis; no build, quick suite, emulator, migration, crash, or runtime execution; commit subjects are clustering signals rather than semantic proof
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0001 completion

- Worker: separate fresh `gpt-5.6-sol` worker at medium reasoning effort
- Outcome: `ISSUES`; normalized as `GQC-0014`
- Frozen assigned scope: `android/auth_config.json5.template` L1-L26 and `android/play-store-credentials.sample.json` L1-L19, plus required frozen consumers and ownership context
- Normalized observations: the credential sample's PKCS#1 envelope versus PKCS#8 importer mismatch confirms open historical `BR-0007`; one U+2014 cleanup note was attached to that eventual sample edit and rejected as a standalone finding
- Explicit clean dimensions: placeholder-only samples, ignored live credential filenames, public GPGS identifier classification, auth-template schema and defaults, JSON parseability, HTTPS token endpoint, direct consumers, and no duplicate credential schema
- Limits: no live Google account, key, token exchange, Play Games sign-in, AAB, upload, or full-history secret scanner
- Scope fingerprint: `0909a3717ef530e36b6484cfea44adb3feb4ce0c`
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0002 completion

- Worker: fresh `gpt-5.6-sol` worker at medium reasoning effort, read-only and limited to this chunk
- Outcome: `ISSUES`; normalized as `GQC-0015`
- Coverage: all 533 assigned lines across 12 CD extraction/fingerprint records plus frozen generators, validators, runners, database records, sibling manifests, tests, and history
- Normalization: admitted stale Anniversary fingerprint baseline `GQF-0029` as incomplete remediation/regrowth linked to archived `BR-0184`; extended open `BR-0011`, `BR-0008`, `BR-0188`, `BR-0010`, and `BR-0012`
- Clean dimensions: syntax/encoding, hash shape, path hygiene, canonical schema/order, local uniqueness, classification/launch policy, import modes, serialization, privacy/payload boundary, and existing diagnostics
- Limits: omitted proprietary bytes prevented fresh media hashes, extraction, and regeneration; the stale baseline is supported by frozen CUE geometry, corrected hashing code, canonical digest, and history, and must be confirmed from exact media before replacement
- Scope fingerprint: `84f4bfeffe01fcc71c5cb74c274cf8ec9fc7a52c`
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0003 completion

- Worker: separate fresh `gpt-5.6-sol` worker at medium reasoning effort, read-only and limited to this chunk
- Outcome: `ISSUES`; normalized as `GQC-0016`
- Coverage: both assigned records and necessary fingerprint generator, extraction generator/runner, packaged database, complete 34-manifest collision corpus, tests, and history
- Normalization: extended `BR-0008` and `BR-0010`; admitted missing physical-disc fingerprint generation identity as `GQF-0030`; recorded the repaired 88-file Dimensions oracle as positive scope evidence without closing broader `BR-0188`
- Clean dimensions: JSON/JSON5 integrity, ordered contiguous track schema, exact packaged-database agreement, no conflicting repeated SHA-1 metadata, complete Dimensions path oracle, atomic publication, naming, privacy, payload exclusion, and diagnostics
- Limits: proprietary source bytes, decoded PCM, extraction outputs, AcoustID, generators, and emulator execution were unavailable or outside the frozen read-only unit
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0004 completion

- Worker: separate fresh `gpt-5.6-sol` worker at medium reasoning effort, read-only and limited to this chunk
- Outcome: `ISSUES`; normalized as `GQC-0017`
- Coverage: all 549 assigned lines across seven D2 CD regression specs plus frozen generator, validator, runner, setup-introspection, sibling records, database, tests, and histories
- Normalization: admitted unasserted per-release CD audio counts as `GQF-0031`; extended `BR-0008`, `BR-0188`, `BR-0011`, `BR-0010`, `BR-0012`, and `BR-0009`
- Clean dimensions: syntax/encoding, canonical schema, digest shape, source naming, internal count coherence, cross-edition policy, serialization, provenance/privacy, D1/D2 applicability, and shared ownership
- Limits: omitted commercial media prevented fresh extraction/fingerprint checks; no emulator run was required to establish the static false-pass paths
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0005 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0018`
- Coverage: all 551 assigned lines across eight retail/OEM CD specs plus frozen generator, validator, runner, introspection, fingerprints, database, tests, histories, and existing owners
- Normalization: extended `GQF-0031`, `GQF-0030`, `BR-0011`, `BR-0008`, `BR-0188`, `BR-0010`, `BR-0012`, `BR-0009`, and `BR-0189`; admitted no new root
- Clean/limits: canonical schema, source/count coherence, edition policies, serialization, privacy and payload boundaries were clean; proprietary bytes and emulator execution were unavailable
- Scope fingerprint: `31e3ed9e315ba324fddbe1dbb3875d976602eed4`
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0006 completion

- Worker/outcome: separate fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0019`
- Coverage: all 584 assigned lines across nine Mac and D1 release records plus HFS/STi2, fingerprint/hash, database, generator, validator, runner, tests, history, and owners
- Normalization: extended `BR-0008`, `GQF-0031`, `BR-0188`, `BR-0009`, `BR-0010`, `BR-0012`, `GQF-0030`, and `BR-0619`; admitted no new root
- Clean/limits: record syntax, source normalization, exact disc identity, D1 Mac track agreement, repaired HFS result, fork semantics, complete 191-file Levels oracle, cross-platform paths, privacy and payload exclusion were clean; proprietary sources prevented dynamic reproduction
- Scope fingerprint: `44b487f1ec6b1071bf553d55e4579eb68fef5841`
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0007 completion

- Worker/outcome: separate fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0020`
- Coverage: all 35 assigned combined-launch lines plus helper, both component specs, generator, validator, runner, automation template, workflow tests, completion history, and owners
- Normalization: admitted undeclared component collision ownership as `GQF-0032` and missing derived-oracle semantic freshness validation as `GQF-0033`; extended `BR-0187`, `BR-0008`, `BR-0188`, `BR-0010`, `BR-0012`, and `BR-0009`
- Clean/limits: schema/order, exact component-union agreement, launch assertions, mission staging, historical launch evidence, diagnostics, privacy and payload exclusion were clean; omitted component bytes prevent choosing a collision owner, which is part of the finding
- Scope fingerprint: `2d26715012898ab3f3ac300cd7a6c7510a47e68b71704c5f416507a26f5451e9`
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0008 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0021`
- Coverage: all 271 assigned lines across ten mission music sidecars plus producer, packaged-database consumer, source manifest, corpus collision analysis, tracklists, tests, history, and existing owners
- Normalization: admitted matching-identity malformed-cache acceptance as `GQF-0034`, broadened `GQF-0030` to all maintained fingerprint generations, admitted evidence-free AcoustID selection regrowth as `GQF-0035`, and extended `BR-0623`
- Clean/limits: assigned sidecars currently have valid complete roots/tracks, consistent duplicates, packaged mappings, current native generation, atomic publication, no secrets/raw audio; omitted archives and network responses prevented source and remote revalidation
- Scope fingerprint: `bc946093a8ceab0a341294ee4451f76c17af714e`
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0009 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0022`
- Coverage: complete SAF manifest parser/header plus frozen Kotlin/JNI/PhysicsFS producers and consumers, limits, tests, history, and owners
- Normalization: admitted allocation/lookup amplification as `GQF-0036`; admitted `NewStringUTF` encoding mismatch as `GQF-0037`, a generation-specific regrowth of archived `BR-0065`; extended dormant-subsystem owner `BR-0084` for leaf-name policy
- Clean/limits: syntax and integer bounds, string writes, failure ownership, case-fold semantics, fd lifecycle, atomic publication, warnings and current tests were clean; no production `SafFileEntry` producer exists at the frozen head
- Scope fingerprint: `c54c8956ca7001ccebd09e3fddb2a918f0c3e09bfd27d86b88520feeee5d70f7`
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0010 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0023`
- Coverage: complete Mac HFS wrapper/header plus callers, HFS/STi2 readers, limits, staging/publication, tests, history, and owners
- Normalization: admitted unchecked output joins `GQF-0038`, shared scratch ownership `GQF-0039`, and silent flattened-name collision selection `GQF-0040`; extended cancellation owner `BR-0021`
- Clean/limits: checked size arithmetic, descriptor/mapping cleanup, parser bounds, per-output and attempt publication were clean; synthetic races/collisions and proprietary media were not executed in this read-only unit
- Scope fingerprint: `8fc87d80b545908b1da9f16980939859f80cbff422c6b9013b310740d26b662a`
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0011 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0024`
- Coverage: all 528 assigned SOW lines plus earlier parser state, JNI/CLI callers, limits, decoder/output ownership, tests, real-media history and prior owners
- Normalization: extended `GQF-0038`, `GQF-0040`/archived `BR-0072`, and `BR-0021`; admitted append-prefix deletion `GQF-0041`, link-following destination ownership `GQF-0042`, and filtered progress mismatch `GQF-0043`
- Clean/limits: parser span/CRC/ratio/count/budget checks, selected extension policy, memory ownership and ordinary failure cleanup were clean; filesystem race and fault injection were not dynamically executed
- Scope fingerprint: `9e4ec7d64b60cfd01e1a7de0e87a15cb2746c02e0e74c62f702215aa77623a6b`
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0012 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0025`
- Coverage: all 490 assigned PKG lines plus scan state, JNI/CLI callers, manifest/CPIO/gzip ownership, limits, tests and historical owners
- Normalization: admitted optional-audio storage misaccounting `GQF-0044`, resetting per-file UI progress `GQF-0045`, and incomplete public/direct publication rollback `GQF-0046`; extended `BR-0021` and `BR-0070`
- Clean/limits: manifest equality, stream/CRC/ratio/output budgets, selected entry integrity, current-file cleanup and Android outer staging were clean; proprietary packages and injected I/O failures were not run
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0013 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0026`
- Coverage: all 548 assigned ISO9660 lines plus earlier record state, CUE/standalone/JNI/CLI callers, limits, publication, malformed tests and historical owners
- Normalization: admitted physical-containment regrowth `GQF-0047`, normalized-name collision selection `GQF-0048`, unsupported accepted layout semantics `GQF-0049`, and descriptor-sequence rejection `GQF-0050`; extended `BR-0021` and the shared direct-publication root `GQF-0046`
- Clean/limits: source span and size arithmetic, record/recursion/path lexical bounds, multi-extent sequencing, output budgets and current-file failure cleanup were clean; symlink races and uncommon valid ISO layouts were not executed
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0014 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0027`
- Coverage: all 321 assigned HFS reader/public-header lines plus full parser state, Mac wrapper/callers, catalog/fork/name behavior, tests, history, and owners
- Normalization: extended `GQF-0038`, `GQF-0039`, `GQF-0042`, `GQF-0040`, and `BR-0021`; admitted no new root
- Clean/limits: size/extent bounds, bounded transfer memory, entry ownership and descriptor cleanup were clean; uncommon media and filesystem race tests were not executed
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0015 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0028`
- Coverage: all 420 assigned JNI/MIDI lines plus Kotlin declarations, overlay, coop/host-migration, native MIDI/HOG/audio owners, D1/D2 wiring, tests and history
- Normalization: extended `BR-0029`, `BR-0044`, `BR-0078`, `BR-0016`, and `BR-0276`; broadened UTF-8 regrowth `GQF-0037` to MIDI path/JSON bridges; admitted fallible acquisition/array misuse as `GQF-0052`
- Clean/limits: signature parity, ordinary array releases, buffer caps and direct audio format ownership were clean; no CheckJNI, OOM injection or lifecycle race execution occurred
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0016 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0029`
- Coverage: assigned STi2 terminal/header contract plus necessary full implementation, callers, compression/fork/integrity limits, output publication, tests, license history and owners
- Normalization: admitted method-15 peak-memory budget regrowth `GQF-0051`; extended `GQF-0038`, `GQF-0040`, `GQF-0046`, `BR-0021`; duplicated `BR-0073`
- Clean/limits: structural/fork/integrity/ratio/output bounds, member containment and attempt-level Android staging were clean; peak-memory stress and concurrent output behavior were not executed
- Scope fingerprint: `b171ea0717b9322173a64f2b25e5cf09a29df2ac`
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0017 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0030`
- Coverage: all 554 GOG JNI/JSON lines plus Kotlin, Inno/PKG, progress/cancellation, CLI serializers/consumers, tests, history and owners
- Normalization: admitted unbound Inno analysis/extraction generations `GQF-0053` and discarded JSON output failures `GQF-0054`; extended `GQF-0038`, `BR-0021`, `GQF-0052`; duplicated `BR-0070`
- Clean/limits: signatures, repaired strict text helpers, bounded record fields, selected-size arithmetic and ordinary cleanup were clean; provider swaps, OOM/CheckJNI and failing output streams were not executed
- Scope fingerprint: `47cc613146930537bdf18f5cc2146e8c5b75283e`
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0018 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0031`
- Coverage: all 215 reconnect-auth contract lines plus D1/D2 request/challenge/proof state, JNI/Kotlin P-256, host migration, server/proxy routes, tests, history and owners
- Normalization: admitted route-proof enforcement regrowth `GQF-0055`, missing game/generation domain binding `GQF-0056`, and pre-verification EC/JNI flood cost `GQF-0057`
- Clean/limits: transcript bounds/endian encoding, P-256 primitive/key ownership, wire lengths, constant-work equality, normal challenge binding, bounded static state, ordinary JNI cleanup and D1/D2 layout parity were clean; no live adversarial multiplayer or latency load ran
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0019 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0032`
- Coverage: StuffIt public contract, complete CD preview and fingerprint JNI bridges plus Kotlin/native owners, decoders, packaged fingerprint data, tests, history and owners
- Normalization: admitted complete-track work-budget/cancellation regrowth `GQF-0058`; extended `GQF-0037`, `GQF-0052`, and `BR-0016`; StuffIt header yielded no distinct root
- Clean/limits: JNI signature parity, normal resource release, current fingerprint result shape and native preview locking were clean; long-media budget/cancellation, Unicode and forced JNI failures were not executed
- Scope fingerprint: `1da8da98b3f36b397efdd4a6226172b6d42aca3f`
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0020 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0033`
- Coverage: all matcher/extension-policy lines plus database producers/consumers, JNI/tools, configuration, tests, history and owners
- Normalization: admitted lossy score ranking `GQF-0059` and asymmetric exact-collision duration regrowth `GQF-0060`; extended archived `BR-0041` parity-test boundary
- Clean/limits: strict database parsing/bounds, current table values, shared matcher threshold configuration, cleanup and output schemas were clean; crafted near-tie and boundary fingerprints were not executed
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0021 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0034`
- Coverage: all assigned CD/GOG CLI and shared-limit lines plus full control flows, parser/extractor APIs, scripts/tests, publication and owners
- Normalization: admitted reset aggregate extraction budgets `GQF-0061` and drive-relative volume mismatch `GQF-0062`; extended `GQF-0038`, `GQF-0046`, `GQF-0054`, and `BR-0169`
- Clean/limits: per-operation limits, ordinary argument parsing, exit propagation for detected extractor failures, staging in maintained batch wrappers and secret/payload boundaries were clean; multi-volume and bomb fixtures were not run
- Scope fingerprint: `233a6b2ba2f037f3c5011feca4187297379cc68a`
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0022 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0035`
- Coverage: all exact-read/CUE parser/header lines plus later parser logic, all callers, Kotlin ordering/publication, sector consumers, malformed fixtures, history and owners
- Normalization: admitted malformed FILE grammar `GQF-0063`, oversized-line synthetic directives `GQF-0064`, ambiguous track-number identity `GQF-0065`, and embedded-NUL prefix parsing `GQF-0066`
- Clean/limits: exact bounded reads, capacity/MSF/layout/size fixes, ordinary file/track spans and cleanup were clean; compatibility decisions for nonstandard but valid CUEs need fixtures during remediation
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0023 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0036`
- Coverage: all six JNI/shared string paths plus native/Kotlin SAF, reconnect, overlay, Activity/global lifetime, D1/D2 wiring, tests, history and owners
- Normalization: extended `GQF-0037`, `GQF-0052`, `GQF-0057`, `BR-0044`, and `BR-0078`; admitted no new root
- Clean/limits: strict shared codec internals, bounded arrays, ordinary attach/ref cleanup and signature parity were clean; CheckJNI/OOM/race/flood execution remains with owners
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0024 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0037`
- Coverage: all 600 assigned CD extraction lines plus complete tail, ISO/HFS/SOW/CUE/JNI/Kotlin/CLI/script transaction context, tests, history and owners
- Normalization: extended `GQF-0053`, archived `BR-0008`, `GQF-0038`, `BR-0169`, `GQF-0058`, and duplicate/confirmation owners; admitted no new root
- Clean/limits: track span/source bounds, supported fallback decisions, ordinary cleanup and maintained batch staging were clean; source swaps, long paths and raw-track load were not executed
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0025 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0038`
- Coverage: complete audio fingerprint CLI/decoder owner plus JNI/scripts, JSON/database consumers, limits, tests, history and owners
- Normalization: extended `GQF-0058` and `GQF-0054`; admitted non-regular/reparse input objects `GQF-0067`, unregistered enumeration regression `GQF-0068`, and bounded Windows stream-orientation investigation `GQI-0002`
- Clean/limits: enumeration cap/cleanup, filename arithmetic/sorting, Windows wide filesystem access, JSON content, audio arithmetic and normal errors were clean; links/special files, long-media cancellation, failing sinks and exact UCRT diagnostics were not run
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0026 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0039`
- Coverage: complete CD fingerprint CLI plus CUE/parser, generator, publication, database, native test, history and ownership context
- Normalization: admitted mixed BIN generations `GQF-0069` and Windows Unicode path identity `GQF-0070`; extended `GQF-0058`, `GQF-0054`, `GQF-0063`, `GQF-0067`, `GQF-0030`, and `GQF-0065`
- Clean/limits: sector/span arithmetic, exact-read failure, cleanup, JSON shape, maintained atomic publication and ordinary supported-media behavior were clean; path replacement, mutation, Unicode and sink-failure cases were not executed
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0027 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0040`
- Coverage: first 600 HFS reader lines plus complete catalog/tree/extraction, caller, format, test, history and owner context
- Normalization: admitted malformed catalog ancestry `GQF-0071` and HFS metadata/work-budget regrowth `GQF-0072`; retained allocation-state policy as `GQI-0003`; extended archived `BR-0051` and live `BR-0021`
- Clean/limits: numeric partition/extent confinement, basic disk layout, existing leaf-map checks, bounded allocations, fork transfer and ordinary ownership were clean; hostile catalogs, bitmap agreement, work ceilings and cancellation were not executed
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0028 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0041`
- Coverage: second 600 HFS reader lines plus complete parser, wrapper/callers, format contracts, tests, history and owner context
- Normalization: admitted multi-node allocation-map rejection `GQF-0073` and truncated fixed catalog records `GQF-0074`; corroborated `GQI-0003`; extended archived `BR-0050` and `GQF-0040`
- Clean/limits: offset arithmetic, allocation lifetime, bounded traversal, numeric fork bounds, CNID uniqueness and intentional data-fork-only policy were clean; allocation ownership and full root/index reachability remain open
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0029 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0042`
- Coverage: first 600 Inno reader lines plus complete PE resource/offset-table consumer, archive-open path, tests, history and owner context
- Normalization: admitted lost PE resource leaf boundary `GQF-0075` as incomplete-fix/regrowth linked to archived `BR-0052`; deduplicated existing source-generation, publication and capability owners
- Clean/limits: checksum implementations, arithmetic, allocation/descriptor ownership, exclusive leaf staging, wider PE traversal, legacy-integrity purpose and ordinary portability were clean; hostile leaf-size end-to-end fixtures and proprietary installers were not executed
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0030 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0043`
- Coverage: second 600 Inno reader lines plus complete metadata decode, version/layout admission, archive consumer, capability, tests, history and owner context
- Normalization: admitted Inno peak-memory regrowth `GQF-0076`, incomplete metadata LZMA termination `GQF-0077`, and signed version-encoding overflow `GQF-0078`; linked archived `BR-0018`, `BR-0059`, and adjacent `GQF-0051`
- Clean/limits: PE resource arithmetic, bounded traversal, loader layouts, metadata framing/CRCs, individual LZMA ceilings, cleanup, strict ordinary version grammar and portability were clean; allocation instrumentation, malformed metadata seams, UBSan and proprietary installers were not executed
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0031 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0044`
- Coverage: assigned Inno parser range plus Galaxy/extraction consumers, format/capability docs, tests, history and owners
- Normalization: admitted discarded Galaxy multipart assembly `GQF-0079` and incomplete Windows destination identity `GQF-0080`; independently extended signed version root `GQF-0078`
- Clean/limits: bounded record reads, selected layout fields, ordinary path components and single-part parsing were clean; multipart media and Unicode Windows collision fixtures were unavailable
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0032 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0045`
- Coverage: assigned Inno extraction/decode range plus full reader, API, tests, callers, format, history and owners
- Normalization: admitted split-volume descriptor loss `GQF-0081`, solid-chunk prefix/terminal routing `GQF-0082`, and aggregate repeated-decode work `GQF-0083`; extended `GQF-0076` and duplicated `BR-0021`
- Clean/limits: bounded source/output arithmetic, ordinary decoder cleanup and checksum ownership were clean; split archives, high-offset solid files, decoder allocation instrumentation and aggregate stress were not executed
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0033 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0046`
- Coverage: assigned Inno extraction/publication range plus complete decoder, Galaxy, caller, tests, history and owners
- Normalization: corroborated aggregate decode-work `GQF-0083`; admitted Galaxy size validation after publication `GQF-0084`; extended cancellation owner `BR-0021`
- Clean/limits: per-operation source/output/checksum bounds, temporary-file ownership and ordinary cleanup were clean; repeated alias stress, process-death publication and callback cancellation were not executed
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0034 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0047`
- Coverage: Inno reader tail plus complete output-name, progress, Galaxy, decoder, caller, test and owner context
- Normalization: admitted quadratic name preflight `GQF-0085` and solid-chunk progress accounting `GQF-0086`; extended `BR-0021`, `GQF-0084`, `GQF-0080`, and `GQF-0083`
- Clean/limits: bounded extraction arithmetic, leaf transaction ownership and ordinary callbacks were clean; maximum-prefix comparisons and sparse-selection progress were not executed
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0035 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0048`
- Coverage: complete Inno public contract plus implementation, JNI/CLI callers, tests, capability and owner context
- Normalization: extended `BR-0021`, `GQF-0080`, `GQF-0079`, `GQF-0081`, and `GQF-0053`; admitted no new root
- Clean/limits: ABI/linkage, widths, handles/catalog ownership, capability visibility, licensing and test seam were clean; existing behavioral gaps retain their implementation owners
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0036 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0049`
- Coverage: first 600 ISO9660 reader lines plus complete parser/caller/test/history/owner context
- Normalization: admitted generic `zero` directory suppression `GQF-0087` and missing declared-volume containment `GQF-0088`; extended `GQF-0047` through `GQF-0050`
- Clean/limits: sector arithmetic, bounded records, lexical containment and ordinary descriptor cleanup were clean; synthetic declared-volume and cross-media `zero` fixtures were not executed
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0037 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0050`
- Coverage: complete JNI disc-import bridge plus CUE, ISO, SOW, archive, Kotlin, tests, history and owner context
- Normalization: admitted UTF-8 title boundary `GQF-0089`; extended `GQF-0052`, `BR-0021`, `GQF-0063`, and `GQF-0066`; duplicated the archived SOW source-manifest root
- Clean/limits: ABI parity, ownership, exact ordinary reads, geometry, serialization and synchronous callback lifetime were clean; CheckJNI, allocation, callback-exception and multibyte-boundary execution remain open
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0038 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0051`
- Coverage: first 600 PKG reader lines plus complete catalog/extraction, JNI/CLI, tests, history and owner context
- Normalization: admitted unscoped XAR field selection `GQF-0090` and collision-weak source generation `GQF-0091`; extended `GQF-0044` through `GQF-0046`, `BR-0021`, and `BR-0070`
- Clean/limits: fixed headers, TOC inflate, physical containment, CPIO framing, terminal checks, resource ceilings, names and ordinary cleanup were clean; proprietary media, collision fixtures and mutable providers were not executed
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0039 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0052`
- Coverage: first 600 SOW extractor lines plus scanner/extraction/caller/test/history and owner context
- Normalization: extended archived `BR-0072`, `GQF-0067`, `GQF-0070`, and `GQF-0042`; admitted zero-length allocation portability `GQF-0092`
- Clean/limits: bounded record arithmetic, normal decoding and cleanup were clean; link/race, Unicode Windows, output-root and zero-allocation behaviors were not executed
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0040 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0053`
- Coverage: first 600 STi2 lines plus complete parser/decoder/caller/test/license and owner context
- Normalization: extended `GQF-0038` and `GQF-0040`; verified archived `BR-0032` remains closed; duplicated `BR-0073`; admitted no new root
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0041 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0054`
- Coverage: second 600 STi2 lines plus complete method-14 decoder, production wiring, tests, history and provenance owners
- Normalization: admitted missing production method-14 success oracle `GQF-0093` as current test-gap/regrowth linked to archived `BR-0158`; verified `BR-0178` and `BR-0033` remain closed; duplicated `BR-0073`
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0042 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0055`
- Coverage: third 600 STi2 lines plus method-14/method-15 decoder, tests, history and provenance context
- Normalization: duplicated current `GQF-0093` and historical `BR-0073`; verified archived `BR-0178` and `BR-0033` remain closed; admitted no further root
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0043 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0056`
- Coverage: STi2 tail plus complete high-level extraction, nested StuffIt path, decoders, tests, history and owners
- Normalization: admitted fixed entry-list stack footprint `GQF-0094`; extended `GQF-0051`, `GQF-0046`, `GQF-0038`, `GQF-0040`, and `BR-0021`; verified `BR-0033` remains closed
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0044 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0057`
- Coverage: complete StuffIt extractor plus nested STi2, publication, cancellation, corpus, licensing and owner context
- Normalization: extended `GQF-0051`, `GQF-0038`, `GQF-0039`, `GQF-0040`, `GQF-0046`, and `BR-0021`; duplicated `BR-0182`, `BR-0073`; admitted no new root
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0045 completion

- Worker/outcome: fresh read-only `gpt-5.6-sol` medium worker; `ISSUES`, normalized as `GQC-0058`
- Coverage: first 600 level-metadata JNI lines plus retained-service lifecycle, global runtime initialization, callers, tests, history and owners
- Normalization: admitted retry over partial process-global initialization `GQF-0095`; verified archived `BR-0076` and `BR-0065` repairs remain present
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0046 through GQ1-CHUNK-0048 completion

- Workers/outcomes: three fresh read-only `gpt-5.6-sol` medium workers; all `ISSUES`, normalized as `GQC-0059` through `GQC-0061`
- Coverage: remaining level-metadata JNI and first 600 JNI-main lines plus engine/Activity/service/caller/test/history context
- Normalization: chunks 0046 and 0047 extend active `BR-0077`; chunk 0048 extends `BR-0078`, `BR-0029`, and `GQF-0052`; archived UTF-8/init cleanup repairs remain closed; no new root admitted
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0049 through GQ1-CHUNK-0051 completion

- Workers/outcomes: three fresh read-only `gpt-5.6-sol` medium workers; all `ISSUES`, normalized as `GQC-0062` through `GQC-0064`
- Coverage: JNI main tail, complete music control, and first 600 resume-save lines plus engine/UI/filesystem/test/history context
- Normalization: chunks 0049 and 0050 extend existing UI/engine, automation, weapon, hosting and UTF-8 owners; chunk 0051 admits long mission identity `GQF-0096` and repeated main-thread traversal `GQF-0097`, and extends `BR-0082`
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0064 through GQ1-CHUNK-0066 completion

- Workers/outcomes: three fresh read-only `gpt-5.6-sol` medium workers; `CLEAN`, `ISSUES`, `ISSUES`, normalized as `GQC-0077` through `GQC-0079`
- Coverage: Android data-extraction policy and assigned server config/STUN/TLS paths plus consumers, tests, history and owners
- Normalization: chunk 0064 is explicitly clean and verifies `BR-0414`; chunk 0065 extends `GQF-0014` and `BR-0106` through `BR-0108`; chunk 0066 duplicates five existing owners; no new finding or investigation
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0067 through GQ1-CHUNK-0069 completion

- Workers/outcomes: three fresh read-only `gpt-5.6-sol` medium workers; all `ISSUES`, normalized as `GQC-0080` through `GQC-0082`
- Coverage: assigned server PoW/protocol/status/auth/STUN/session paths plus tests, history and owners
- Normalization: all observations deduplicate to existing adversarial/server owners except evidence extensions for `BR-0135` and `BR-0125`; no new finding, investigation, or remediation
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0052 through GQ1-CHUNK-0054 completion

- Workers/outcomes: three fresh read-only `gpt-5.6-sol` medium workers; all `ISSUES`, normalized as `GQC-0065` through `GQC-0067`
- Coverage: resume-save tail, complete SAF archiver, and assigned ZIP import Kotlin paths plus callers/tests/history/owners
- Normalization: admitted conditional-delete generation `GQF-0098`, SAF source generation `GQF-0099`, and ZIP staging/parser/budget/memory roots `GQF-0100` through `GQF-0104`; extended `BR-0417` and `GQF-0036`
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0079 through GQ1-CHUNK-0081 completion

- Workers/outcomes: three fresh read-only `gpt-5.6-sol` medium workers; all `ISSUES`, normalized as `GQC-0092` through `GQC-0094`
- Coverage: complete extraction CMake build/test graph and verified 7-Zip helper plus dependency, caller, test, portability and history context
- Normalization: chunk 0079 admitted TinySoundFont incremental rebuild identity `GQF-0123`, extended `GQF-0024`, `GQF-0068`, `BR-0158`, and duplicated `BR-0182`; chunk 0080 extended `BR-0160` and `BR-0236`; chunk 0081 extended `BR-0174` and recorded remediation evidence for `BR-0159`
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0082 through GQ1-CHUNK-0084 completion

- Workers/outcomes: three fresh read-only `gpt-5.6-sol` medium workers; all `ISSUES`, normalized as `GQC-0095` through `GQC-0097`
- Coverage: mission batch tail/relay, bounded extraction/HFS recovery, Play Store auth, bounded child runner, and POSIX CUE/ISO entry point plus callers, tests and history
- Normalization: admitted diagnostic-finally abort `GQF-0124`, PATH-selected Python identity `GQF-0125`, zero-length HFS omission `GQF-0126`, child-process-tree lifetime `GQF-0127`, link/special-file containment `GQF-0128`, and non-executable documented POSIX entry point `GQF-0129`; extended existing HFS, fixture isolation and historical owners without duplicate IDs
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0085 and GQ1-CHUNK-0086 completion

- Workers/outcomes: two fresh read-only `gpt-5.6-sol` medium workers; both `ISSUES`, normalized as `GQC-0098` and `GQC-0099`
- Coverage: mission batch first 600 lines and bulk CD/GOG extraction scripts plus parser, publication, process, caller, test and history context
- Normalization: chunk 0085 records regrowth of archived `BR-0161` and duplicates `BR-0165`, `BR-0167`, `BR-0168`; chunk 0086 admitted same-destination extraction publication race `GQF-0130`, extended `BR-0169`, and duplicated `GQF-0016`/`BR-0171`
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0087 through GQ1-CHUNK-0090 completion

- Workers/outcomes: four fresh read-only `gpt-5.6-sol` medium workers; all `ISSUES`, normalized as `GQC-0100` through `GQC-0103`
- Coverage: DOS/Mac extraction workflows and mission music fingerprint generator first 600 lines plus parser, tool, publication, cache, test and history context
- Normalization: admitted output semantic admission `GQF-0131`, legacy Mac CUE geometry and INDEX validation `GQF-0132`/`GQF-0133`, bounded-supervisor provenance `GQF-0134`, direct unbounded Mac-demo child phases `GQF-0135`, legacy oracle provenance `GQF-0136`, nested archive depth `GQF-0137`, and conflicting tracklist selector policy `GQF-0138`; remaining observations extend or duplicate current and historical owners
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0091 and GQ1-CHUNK-0092 completion

- Workers/outcomes: two fresh read-only `gpt-5.6-sol` medium workers; both `ISSUES`, normalized as `GQC-0104` and `GQC-0105`
- Coverage: mission fingerprint generator tail and complete Inno capability matrix plus implementation, callers, tests and history
- Normalization: admitted stale prior audio generation `GQF-0139` and unbounded native fingerprint process `GQF-0140`; extended `BR-0180`, `GQF-0130`, `GQF-0078`, and `GQF-0018`; duplicated `BR-0179` and `BR-0174`
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0093 and GQ1-CHUNK-0094 completion

- Workers/outcomes: two fresh read-only `gpt-5.6-sol` medium workers; both `ISSUES`, normalized as `GQC-0106` and `GQC-0107`
- Coverage: StuffIt/UTF-8/SAF manifest test batch, GOG descriptor tail and graphics transaction test plus production/build/history context
- Normalization: chunk 0093 extends existing false-pass, production-seam and UTF-8 test owners; chunk 0094 admitted rollback backup loss `GQF-0141` and fsync failure descriptor leak `GQF-0142`, extended `GQF-0075`, and duplicated `BR-0158`
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0095 and GQ1-CHUNK-0096 completion

- Workers/outcomes: two fresh read-only `gpt-5.6-sol` medium workers; both `ISSUES`, normalized as `GQC-0108` and `GQC-0109`
- Coverage: CUE/ISO tail and extraction-limit tests plus PKG/SOW tests with production/build/history context
- Normalization: chunk 0095 admitted extension-filter false-pass `GQF-0143` and extended `BR-0160`, `GQF-0061`, `GQF-0062`; chunk 0096 extended `GQF-0090`, `GQF-0091`, `GQF-0046` and verified archived `BR-0024` remains closed
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0097 and GQ1-CHUNK-0098 completion

- Workers/outcomes: two fresh read-only `gpt-5.6-sol` medium workers; both `ISSUES`, normalized as `GQC-0110` and `GQC-0111`
- Coverage: HMP/JSON/pilot transaction tests and CD short-read/Chromaprint database tests plus production/build/history context
- Normalization: admitted HMP output-oracle gap `GQF-0144` and invalid-threshold stale database state `GQF-0145`; extended `GQF-0054`, `BR-0236`, `BR-0046`, and `GQF-0069`
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0099 and GQ1-CHUNK-0100 completion

- Workers/outcomes: two fresh read-only `gpt-5.6-sol` medium workers; both `ISSUES`, normalized as `GQC-0112` and `GQC-0113`
- Coverage: first 1,200 CUE/ISO test lines plus production/build/history context
- Normalization: both chunks extend `BR-0160` with fixed-path and unchecked fixture-generation evidence; chunk 0100 admitted unused CUE fixture helpers `GQF-0146`
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0101 through GQ1-CHUNK-0103 completion

- Workers/outcomes: three fresh read-only `gpt-5.6-sol` medium workers; all `ISSUES`, normalized as `GQC-0114` through `GQC-0116`
- Coverage: CUE/ISO test lines 1,201-3,000 plus production/build/history context
- Normalization: admitted Windows reserved-device component admission `GQF-0147` and production-sized test stack catalogs `GQF-0148`; extended `BR-0160`, `GQF-0046`, archived `BR-0020`, and `GQF-0063`
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0104 and GQ1-CHUNK-0105 completion

- Workers/outcomes: two fresh read-only `gpt-5.6-sol` medium workers; both `ISSUES`, normalized as `GQC-0117` and `GQC-0118`
- Coverage: complete fingerprint C test and shared game-extension contract test plus production/build/history context
- Normalization: admitted fingerprint input-byte parity gap `GQF-0149`, raw PCM fixture integrity `GQF-0150`, and reevaluating assertion macro `GQF-0151`; extended `BR-0181`, `BR-0662`, and archived `BR-0041`
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

### GQ1-CHUNK-0106 and GQ1-CHUNK-0107 completion

- Workers/outcomes: two fresh read-only `gpt-5.6-sol` medium workers; both `ISSUES`, normalized as `GQC-0119` and `GQC-0120`
- Coverage: GOG descriptor tests lines 1-1,200 plus production/build/history context
- Normalization: chunk 0106 extends `GQF-0076` through `GQF-0080` and `GQF-0085`; chunk 0107 admitted uninitialized LZMA fixture tail `GQF-0152` and extended archived `BR-0034`, `BR-0058`, and current `GQF-0082`
- Evidence: `general_code_quality_evidence_ledger_20260811.md`

## Diff-minimization decisions

Beginning with Chunk 0108, this table is the primary output of each coverage unit. `CANDIDATE` means a measured extraction, consolidation, or revert is actionable after live overlap review. `RETAIN` means the inherited boundary is already minimal. `NO_INHERITED_EFFECT` means the assigned branch-added unit and its traced owners create no inherited merge pressure. `DEFER` records a plausible reduction with a named prerequisite or payoff/risk gate

| ID | Coverage | Disposition | Inherited merge surface and decision |
|---|---|---|---|
| `GQD-0001` | `GQ1-CHUNK-0108` | `NO_INHERITED_EFFECT` | Measured 0 inherited paths, 0 hunks and +0/-0 lines. The HFS test, reader, wrapper, interfaces, callers and CMake owners are branch-added under `android/`; frozen D1/D2 contain no HFS reference or registration |
| `GQD-0002` | `GQ1-CHUNK-0109` | `NO_INHERITED_EFFECT` | Measured 0 inherited paths, 0 hunks and +0/-0 lines. The HFS test tail traces only to the same branch-added parser, wrapper, callers and build owners, with no inherited hook or paired game change to extract |
| `GQD-0003` | `GQ1-CHUNK-0110` | `NO_INHERITED_EFFECT` | Measured 0 inherited paths, 0 hunks and +0/-0 lines. The PKG test, parser, JNI/Kotlin consumers and extraction CMake wiring are branch-added; no frozen inherited hook, declaration or registration is attributable |
| `GQD-0004` | `GQ1-CHUNK-0111` | `NO_INHERITED_EFFECT` | Assigned SOW integrity test is a branch-added +567-line file; every traced CMake, native, JNI and Kotlin owner is branch-added, for 0 inherited paths, 0 hunks and +0/-0 lines |
| `GQD-0005` | `GQ1-CHUNK-0112` | `NO_INHERITED_EFFECT` | Assigned +77-line CMake driver and its test registration, extractor, host/JNI and build owners are branch-added. Frozen D1/D2 have zero SOW references, so inherited impact is 0 paths, 0 hunks and +0/-0 lines |
| `GQD-0006` | `GQ1-CHUNK-0113` | `NO_INHERITED_EFFECT` | Measured 0 inherited paths, 0 hunks and +0/-0 lines. The STi2 test, production decoder, wrappers, JNI/Kotlin consumers and both Android game build owners are branch-added |
| `GQD-0007` | `GQ1-CHUNK-0114` | `NO_INHERITED_EFFECT` | Measured 0 inherited paths, 0 hunks and +0/-0 lines. The STi2 test tail and complete traced decoder/caller/build boundary remain in branch-added Android paths, with no inherited hook to extract |
| `GQD-0008` | `GQ1-CHUNK-0115` | `NO_INHERITED_EFFECT` | Measured 0 inherited paths, 0 hunks and +0/-0 lines. The StuffIt corpus test, decoder, production parser, JNI/Kotlin consumers and build owners are branch-added Android paths |
| `GQD-0009` | `GQ1-CHUNK-0116` | `NO_INHERITED_EFFECT` | Measured 0 inherited paths, 0 hunks and +0/-0 lines. The corpus tail and all traced test/build/parser/JNI owners remain in branch-added Android paths |
| `GQD-0010` | `GQ1-CHUNK-0117` | `DEFER` | Assigned tests are branch-added and require no inherited registration. A shared paired `songs.c` post-init callback could remove about 16 inherited additions across D1/D2 but no hunks; retain below standalone DMR1 threshold until coherent adjacent songs work raises payoff |
| `GQD-0011` | `GQ1-CHUNK-0118` | `NO_INHERITED_EFFECT` | Four assigned tests are branch-added +549/-0 and all tested production/build/caller owners are branch-added Android; measured inherited effect is 0 paths, 0 hunks and +0/-0 lines |
| `GQD-0012` | `GQ1-CHUNK-0119` | `NO_INHERITED_EFFECT` | Five assigned tests are branch-added +461/-0 and all traced Kotlin, JNI, native and build owners are branch-added; measured inherited effect is 0 paths, 0 hunks and +0/-0 lines |
| `GQD-0013` | `GQ1-CHUNK-0120` | `NO_INHERITED_EFFECT` | Assigned +403-line test has no inherited hook. The full feature chain ends in four paired inherited additions: one include and one lifecycle load call per `songs.c`; retain this natural boundary because a wrapper saves at most two lines and adds indirection |
| `GQD-0014` | `GQ1-CHUNK-0121` | `NO_INHERITED_EFFECT` | Assigned +486-line test and StageManager/catalog/archive/publication/build/UI owners are branch-added; D1/D2 consume only a later sidecar. Measured inherited effect is 0 paths, 0 hunks and +0/-0 lines |
| `GQD-0015` | `GQ1-CHUNK-0122` | `NO_INHERITED_EFFECT` | Assigned +357-line test and directly tested +698-line Kotlin catalog boundary cause 0 inherited paths, hunks or lines. Broader paired `songs.c` marker hunks are orthogonal and remain owned by `GQD-0010`/`GQD-0013` |
| `GQD-0016` | `GQ1-CHUNK-0123` | `NO_INHERITED_EFFECT` | Assigned MissionZip test is one branch-added +706-line hunk; all 16 direct MissionZip production/test references are branch-added and frozen D1/D2 contain zero MissionZip token hits |
| `GQD-0017` | `GQ1-CHUNK-0124` | `NO_INHERITED_EFFECT` | Assigned MissionZip test tail and direct production/test owners are branch-added; measured inherited impact is 0 paths, 0 hunks and +0/-0 lines, with no extraction or revert target |
| `GQD-0018` | `GQ1-CHUNK-0125` | `CANDIDATE` | Paired `d1/misc/physfsx.c` and `d2/misc/physfsx.c` retain 40 branch-added lines across six relevant hunks. Extending existing `physfsx_android_shared.c/.h` with a parameterized init helper should remove about 26 inherited lines and two hunks while preserving compact per-game directory calls and desktop flow |
| `GQD-0019` | `GQ1-CHUNK-0126` | `NO_INHERITED_EFFECT` | Assigned branch-added test tail and traced Kotlin/build/native owners cause 0 inherited paths, 0 hunks and +0/-0 lines. The previously measured PhysFS seam remains solely owned by `GQD-0018` |
| `GQD-0020` | `GQ1-CHUNK-0127` | `CANDIDATE` | Eight paired inherited newmenu/window implementation and header paths contain 154 duplicated accessor/declaration lines. Branch-added type-complete implementation fragments and one shared declaration header should leave about ten include-seam lines, removing an estimated 144 inherited lines without moving private geometry or frame orchestration |
| `GQD-0021` | `GQ1-CHUNK-0128` | `CANDIDATE` | Frozen paired maths CMake files carried 559 branch-caused host-test lines, including 12 reconnect-test registration lines. Post-freeze commit `7f0f7c20` moved the graph to branch-added `android/tests/CMakeLists.txt` and removed all 559 inherited lines; record as completed external minimization with no new implementation chunk |
| `GQD-0022` | `GQ1-CHUNK-0129` | `NO_INHERITED_EFFECT` | Two assigned branch-added scripts and seven direct branch-added consumers trace to 0 inherited paths, 0 hunks and +0/-0 causal lines across extraction, JNI/build and paired game boundaries |
| `GQD-0023` | `GQ1-CHUNK-0130` | `NO_INHERITED_EFFECT` | Three assigned branch-added test paths and all 12 traced runner, helper, build, native-test and workflow owners are branch-added. Measured causal inherited effect is 0 paths, 0 hunks and +0/-0 lines |
| `GQD-0024` | `GQ1-CHUNK-0131` | `CANDIDATE` | Duplicate confirmation of `GQD-0018`: paired `d1/misc/physfsx.c` and `d2/misc/physfsx.c` retain 40 additions across six relevant hunks, with the same estimated 26-line and two-hunk reduction into the existing shared PhysFS owner. No second finding or remediation |
| `GQD-0025` | `GQ1-CHUNK-0132` | `CANDIDATE` | Duplicate confirmation of `GQD-0020`: the extraction launcher consumes the same 154 inherited accessor/declaration lines across paired newmenu/window paths, with about 144 removable. Add exact-level extraction launch to `GQR-0143` validation. Retain the separate compact paired event-loop seam of 22 additions across six hunks |
| `GQD-0026` | `GQ1-CHUNK-0133` | `NO_INHERITED_EFFECT` | Four assigned tests contain 488 branch-added lines across four creation hunks; every traced production, build and launcher owner is also branch-added, and frozen D1/D2 contain no attributable reference or registration. Measured inherited impact is 0 paths, 0 hunks and +0/-0 lines |
| `GQD-0027` | `GQ1-CHUNK-0134` | `NO_INHERITED_EFFECT` | Five assigned branch-added tests contain 542 additions and trace to 17 branch-added production, build, JNI, launcher and game-data owners. Measured attributable inherited impact is 0 paths, 0 hunks and +0/-0 lines |
| `GQD-0028` | `GQ1-CHUNK-0135` | `CANDIDATE` | Duplicate confirmation of `GQD-0020`: this extraction runner consumes the same 154 inherited accessor/declaration lines across eight paired newmenu/window paths, with about 144 removable. Retain the separate 22-line paired event-loop seam across six hunks |
| `GQD-0029` | `GQ1-CHUNK-0136` | `NO_INHERITED_EFFECT` | The assigned branch-added test range and 12 traced branch-added production, runner and workflow owners cause 0 inherited paths, 0 hunks and +0/-0 inherited lines; frozen D1/D2 contain no attributable consumer or registration |
| `GQD-0030` | `GQ1-CHUNK-0137` | `CANDIDATE` | Duplicate confirmation of `GQD-0018`: paired `d1/misc/physfsx.c` and `d2/misc/physfsx.c` retain 40 additions across six relevant hunks, with the same estimated 26-line and two-hunk reduction into `physfsx_android_shared.c/.h`. No second minimization finding or remediation |
| `GQD-0031` | `GQ1-CHUNK-0138` | `NO_INHERITED_EFFECT` | The branch-added WAV test and its branch-added converter/pack owners have no inherited registration or D1/D2 reference. Measured inherited impact and safe reduction are 0 paths, 0 hunks and +0/-0 lines |
| `GQD-0032` | `GQ1-CHUNK-0139` | `RETAIN` | D1 carries one inherited path, one hunk and a one-line substitution; paired aggregate is two inherited paths, two hunks and +2/-2. The direct shared-toolchain preset value is already the minimum stable integration, so zero lines or hunks are safely removable |
| `GQD-0033` | `GQ1-CHUNK-0140` | `RETAIN` | D2 is the paired half of `GQD-0032`: one inherited path, one hunk and +1/-1, with two paths/two hunks/+2/-2 across both games. Preserve direct parity and the branch-added shared discovery owner |
| `GQD-0034` | `GQ1-CHUNK-0141` | `NO_INHERITED_EFFECT` | Three branch-added server configuration/deployment paths contain 171 additions and trace only to branch-added Rust/server owners. Measured inherited impact and removable inherited surface are 0 paths, 0 hunks and +0/-0 lines |
| `GQD-0035` | `GQ1-CHUNK-0142` | `NO_INHERITED_EFFECT` | Both assigned input-demo headers and all direct includers are branch-added or test-owned. Inherited collision consumers are isolated behind branch-added public hook facades; moving declarations would invert ownership while removing 0 inherited paths, hunks or lines |
| `GQD-0036` | `GQ1-CHUNK-0143` | `RETAIN` | Direct original/inherited footprint is 15 additions at seven necessary logical sites across paired renderer surface hooks and three D1/D2 headless registrations. These are already compact lifecycle/build seams; extraction would not remove a path or material hunk without obscuring ordering |
| `GQD-0037` | `GQ1-CHUNK-0144` | `RETAIN` | Rewind-specific inherited boundary is 14 API-bearing additions across paired `d1`/`d2` `game.c` and `state.c`, four paths and ten hunks. Calls remain at their natural lifecycle/state owners; wrapper alternatives remove no material hunk and obscure ordering or state ownership |
| `GQD-0038` | `GQ1-CHUNK-0145` | `NO_INHERITED_EFFECT` | UTF-8 codec files, wrapper, six production consumers, two build owners and focused test are all branch-added. Measured inherited impact and removable surface are 0 paths, 0 hunks and +0/-0 lines |
| `GQD-0039` | `GQ1-CHUNK-0146` | `RETAIN` | Assigned policy headers are branch-added +60. The remaining unique paired co-op surface spans 21 inherited paths, 68 attributed hook/extraction hunks and +179/-11 after a prior shared restore extraction already removed 166 intermediate inherited lines. Remaining sites own game-local state, packet and lifecycle placement; another wrapper would not materially shrink the merge surface |
| `GQD-0040` | `GQ1-CHUNK-0148` | `RETAIN` | Assigned control owner is branch-added. Paired inherited `gamecntl.c` retains 14 body additions plus four related includes at natural control sites; a wrapper removes at most those additions but no path or hunk and would duplicate or couple game-local policy |
| `GQD-0041` | `GQ1-CHUNK-0149` | `RETAIN` | Input-demo start integration spans six inherited paths, 12 hunks and +22/-1. Sites bind game-specific start, restore and lifecycle state; moving them behind another facade does not remove a path or material hunk and would hide required ordering |
| `GQD-0042` | `GQ1-CHUNK-0147` | `RETAIN` | The GLES shim is branch-added; direct paired inherited integration is 18 additions across four paths and 14 identifier-bearing hunks. These are compact include/build or GL-call compatibility sites; another wrapper cannot remove a path or material hunk and would obscure API parity |
| `GQD-0043` | `GQ1-CHUNK-0150` | `RETAIN` | Shared recorder is already branch-added. Its conservative token-bearing inherited boundary is 22 paths, 35 hunks and 64 additions across local simulation/event/build observation sites. Callback or facade replacement retains those observation points and adds dispatch state; no material inherited path or hunk reduction is supported |
| `GQD-0044` | `GQ1-CHUNK-0151` | `RETAIN` | Both shared owners are branch-added. Their direct paired inherited seam spans 12 paths and 18 identifier-bearing hunks at about +38/-50: compact audio declarations/polls plus effect declarations, calls, state reconstruction and build registration. The shared extraction already removed 50 paired effect-body lines; moving the remaining lifecycle/state sites would obscure ordering or add callback/serialization machinery without materially reducing inherited paths or hunks |
| `GQD-0045` | `GQ1-CHUNK-0152` | `RETAIN` | The branch-added shared replay owner already centralizes 817 implementation lines. Its conservative API-bearing inherited seam is 32 paths, 75 hunks and +106/-0 across game-local frame, cursor, checkpoint, result and build observation sites; six branch-added adapters hold another 129 lines. Moving these sites behind callback tables or another facade retains the local observations while adding dispatch state, so no material inherited path or hunk reduction is supported |
| `GQD-0046` | `GQ1-CHUNK-0154` | `RETAIN` | The branch-added automation owner reaches six inherited paths, 12 relevant hunks and 36 attributable additions at event, joystick and configuration ordering points. Guard or stub consolidation would retain those natural observation sites and only reshuffle cosmetic lines, so no material inherited path or hunk reduction is supported |
| `GQD-0047` | `GQ1-CHUNK-0153` | `CANDIDATE` | Paired inherited `d1/include/rbaudio.h` and `d2/include/rbaudio.h` contain identical 11-line Android-only extension blocks. Move the block into one branch-added shared header and leave one include per inherited header, removing an estimated 22 inherited lines across two paths and two hunks. Retain 11 one-line rewind compatibility includes as the narrow local macro seam |
| `GQD-0048` | `GQ1-CHUNK-0155` | `CANDIDATE` | Paired inherited `d1/misc/hmp.c` and `d2/misc/hmp.c` each retain a three-line Android include hunk and nine-line `hmp2mid_mem` wrapper. Since the bounded shared converter now needs only identical fixed tempo bytes, implement the public wrapper in the branch-added shared owner and remove all 24 inherited additions across four hunks |
| `GQD-0049` | `GQ1-CHUNK-0157` | `CANDIDATE` | Four inherited D1/D2 `game.c` and `input_demo_hooks.c` paths include `input_demo_fp_env.h` without consuming any of its declarations. Remove the four unused lines; the broader live FP API remains 49 token-bearing additions across 12 paths and 19 hunks, but no complete hunk disappears |
| `GQD-0050` | `GQ1-CHUNK-0156` | `RETAIN` | Shared debug logging and direct-command policy already own the common behavior. Their remaining identifier-bearing inherited seam is 23 D1/D2 paths, 85 hunks and 111 additions at local state capture, validation and apply sites; seven branch-added sinks hold another 105 lines. Another facade would preserve these observations and add indirection without material inherited reduction |
| `GQD-0051` | `GQ1-CHUNK-0158` | `CANDIDATE` | Duplicate confirmation of existing `DMR1-CHUNK-003`: move paired approximately 33-line texture-label draw blocks from inherited `d1/main/gamerend.c` and `d2/main/gamerend.c` into the existing branch-added texture-debug owner, leaving one compact call per game. Estimated reduction is about 64 inherited additions across two body hunks; no duplicate finding or remediation |
| `GQD-0052` | `GQ1-CHUNK-0159` | `CANDIDATE` | Consolidate residual Android mixer-init diagnostics from inherited `d1/arch/sdl/digi_mixer.c` and `d2/arch/sdl/digi_mixer.c` into the existing branch-added audio-diagnostics owner. Keep requested-rate and buffer arguments plus one call per game; remove raw Android log includes/macros, query locals and duplicate success/failure logs. Estimated net reduction is 48 inherited lines across six regions, extending completed catalog item C10 |
| `GQD-0053` | `GQ1-CHUNK-0160` | `RETAIN` | Route snapshot's complete paired build footprint is four inherited paths, four coalesced hunks and +22/-0, with only eight direct identifier lines; the build/source attachments are already the narrow target boundary. SAF contract paths have zero inherited effect. Moving either boundary removes no material inherited hunk |
| `GQD-0054` | `GQ1-CHUNK-0161` | `RETAIN` | Branch-added lifecycle diagnostics/loading owners reach four inherited D1/D2 paths, ten zero-context hunks and 24 additions at natural load, progress and teardown observation points. Another wrapper or callback would retain those ordered sites and add state, so no material inherited path or hunk reduction is supported |
| `GQD-0055` | `GQ1-CHUNK-0162` | `RETAIN` | Branch-added level-preview and lifecycle-action owners have four inherited D1/D2 paths, eight startup/event sites and +30/-0. The paired `inferno.c` and `event.c` integrations are natural startup and event-thread seams; wrappers save negligible lines or invert lifecycle ownership |
| `GQD-0056` | `GQ1-CHUNK-0163` | `RETAIN` | Shared route edge and C projection owners have a direct inherited footprint of four paths, four coalesced hunks and +6/-0, within the same complete route target boundary already measured at +22/-0 by `GQD-0053`. Source attachments and game-specific focused targets are the narrow build boundary; indirection removes no complete hunk |
| `GQD-0057` | `GQ1-CHUNK-0164` | `CANDIDATE` | Paired inherited `d1/main/state.c` and `d2/main/state.c` each define the same approximately 20-line secret-area runtime writer/reader block. Move both definitions into the existing branch-added, per-game-compiled secret-area adapter and publish declarations there, removing an estimated 40 inherited additions across two body hunks while retaining save ordering/version gates locally |
| `GQD-0058` | `GQ1-CHUNK-0165` | `CANDIDATE` | Paired inherited `d1/main/CMakeLists.txt` and `d2/main/CMakeLists.txt` hold 185 attributable lines of headless metadata/replay executable construction in two hunks. Move common target policy to a branch-added CMake owner and leave explicit 4-8 line per-game invocations, removing an estimated 169-177 inherited lines while preserving D1/D2 library and definition differences |
| `GQD-0059` | `GQ1-CHUNK-0166` | `RETAIN` | Shared introspection tail/API has 24 additions across four inherited paths and eight hunk locations at local state publication/build boundaries. The GLES array-source helper has zero marginal inherited effect. Another facade would retain the same observation sites and add indirection without material reduction |
| `GQD-0060` | `GQ1-CHUNK-0167` | `NO_INHERITED_EFFECT` | The assigned adapter/item-name paths and paired `d1/main/secretarea.h` / `d2/main/secretarea.h` declarations are all branch-added, so consolidating those 72 declaration lines between new files does not reduce any base-existing/original path, hunk or line. The only material original-file move remains the separately measured 40-line paired `state.c` extraction under `GQD-0057` / `GQF-0174`; other original-file hooks are compact local observations |

### Weighted tranche 0138-0167 summary

- Thirty decisions are terminal: seven `CANDIDATE`, 18 `RETAIN`, zero `DEFER`, and five `NO_INHERITED_EFFECT`
- The seven candidate decisions are seven unique ownership changes. Six are new GQ minimization owners, while the debug texture-overlay candidate confirms existing `DMR1-CHUNK-003`
- `GQF-0169` / `GQR-0156`: consolidate identical Android Redbook declarations from inherited `d1/include/rbaudio.h` and `d2/include/rbaudio.h`; estimated reduction 22 inherited lines and two hunks
- `GQF-0170` / `GQR-0157`: finish the HMP extraction by removing include/wrapper residue from inherited `d1/misc/hmp.c` and `d2/misc/hmp.c`; estimated reduction 24 inherited lines and four hunks
- `GQF-0171` / `GQR-0158`: remove four unused FP-environment includes from paired inherited `game.c` and `input_demo_hooks.c`; estimated reduction four lines with no complete hunk removed
- Existing `DMR1-CHUNK-003`: move paired texture-label draw blocks from inherited `d1/main/gamerend.c` and `d2/main/gamerend.c` into the branch-added texture-debug owner; estimated reduction about 64 inherited lines and two body hunks
- `GQF-0172` / `GQR-0159`: move residual paired mixer-init diagnostics from inherited `d1/arch/sdl/digi_mixer.c` and `d2/arch/sdl/digi_mixer.c` into the existing diagnostics owner; estimated reduction 48 inherited lines across six regions, with no whole hunk guaranteed to disappear
- `GQF-0174` / `GQR-0161`: move secret-area serialization bodies from paired inherited `state.c` files into the branch-added adapter; estimated reduction about 40 inherited lines and two body hunks
- `GQF-0175` / `GQR-0162`: move headless metadata/replay executable policy from inherited `d1/main/CMakeLists.txt` and `d2/main/CMakeLists.txt` into a branch-added module; measured 185-line blocks, estimated reduction 169-177 inherited lines and two hunks
- Aggregate estimated reduction is about 371-379 inherited lines. About 12 complete/body hunks are expected to disappear; the FP cleanup and mixer-log consolidation reduce original-file line/conflict surface without guaranteeing additional hunk removal
- Retained decisions preserve compact event, lifecycle, state-observation, renderer, route/build, replay, automation, audio/effect and platform seams after rejecting wrappers or callbacks that would not remove a material inherited path or hunk. The five no-effect decisions cover branch-added WAV/config, input-demo declarations, UTF-8, and secret-area declaration owners with zero attributable original-file churn
- Closure audit: all 30 queue rows are `DONE`; all 30 have one unique `GQD-*` decision and one unique `GQC-*` record; all 30 worker reports occur exactly once as imported evidence; the tracked inbox and tranche-specific temp-report counts are zero; all workers are terminal; and the ledger, evidence ledger, process and plan are ASCII/no-BOM and pass `git diff --check`

### Weighted tranche 0108-0137 summary

- Thirty decisions are terminal: seven `CANDIDATE` decisions, zero `RETAIN`, one `DEFER`, and 22 `NO_INHERITED_EFFECT`
- The seven candidate decisions collapse to three unique ownership changes. Repeated consumers confirm validation scope but do not create duplicate findings or remediation chunks
- Completed `GQF-0155` / `GQR-0142`: shared Android PhysFS initialization moved out of `d1/misc/physfsx.c` and `d2/misc/physfsx.c`; the measured surface fell from 40 additions in six relevant hunks to 14 additions in four relevant hunks, exactly removing 26 inherited lines and two hunks
- Completed `GQF-0156` / `GQR-0143`: ordinary shared accessor sources and narrow paired private-layout adapters reduce the eight inherited menu/window paths by 82 net lines while preserving the public API and opaque layouts
- Completed externally at commit `7f0f7c20`: move the host-test graph from inherited `d1/maths/CMakeLists.txt` and `d2/maths/CMakeLists.txt` to branch-added `android/tests/CMakeLists.txt`; 559 inherited lines were removed from the two paths, including 12 assigned registration lines across four logical sites
- The final regular-source accessor boundary realizes 82 inherited lines of reduction. Separately, the post-freeze CMake ownership move has already realized 559 inherited lines of reduction
- The one `DEFER` is a roughly 16-line paired songs post-init seam that removes no hunk and remains below the standalone payoff threshold. Natural retained boundaries observed inside other decisions include four paired songs consumer lines and 22 event-loop additions across six hunks

## Findings

The initial broad live survey seeded the following evidence-backed findings. These are atomic observations, not the 819-call coverage queue. Later coverage may narrow, extend, duplicate, or independently verify them

| ID | State | Severity/confidence | Category | Location or root | Evidence and next boundary |
|---|---|---|---|---|---|
| `GQF-0001` | `OPEN` | P1/high | pr-hygiene | Root `dxx_matchmaking.db`, `.db-wal`, `.db-shm` | Runtime SQLite state is tracked. Remove it, establish ignore/allowlist policy, and verify no required fixture depends on it. Regrowth of BR-0001 |
| `GQF-0002` | `OPEN` | P1/high | pr-hygiene | `game_data_to_copy_to_emulator/debuglog_20260520_211753.txt` | An 18.6 MB runtime debug log is tracked. Remove it, preserve no private data, and prevent recurrence |
| `GQF-0003` | `OPEN` | P2/high | pr-hygiene | `android/test_output.txt`, `android/build_output.txt` | Stale transcripts are tracked as source. Remove or replace with reproducible test evidence and ignore policy |
| `GQF-0004` | `OPEN` | P3/high | pr-hygiene | Root `.tmp`, `test.txt`, `d1_d2_ogl_diff.txt` | Scratch artifacts create review and whitespace noise. Classify, remove, and prevent recurrence without deleting maintained fixtures |
| `GQF-0005` | `FIXED` | P1/high | security | `DynamicReceiverPolicy.kt` | `GQR-0006` centralizes dynamic receiver exposure. Release setup command and introspection are app-internal, host migration is always app-internal, and ADB-only setup, multiplayer, game, and preview receivers register only in debug builds. Live BR-0005 is fixed |
| `GQF-0006` | `FIXED` | P1/high | correctness/bounds | `ogl_texture_android.c:android_ogl_read_texture_with_extensions` | `GQR-0001` added an explicit capacity contract and portable checked filename builder. Oversized candidates are rejected without modifying the destination or calling `read_png`; the final API adds zero inherited lines |
| `GQF-0007` | `FIXED` | P1/high | correctness/bounds | `ogl_texture_android.c:android_ogl_load_dxa_mask` | DXA mask construction now uses the existing checked shared filename builder. A complete basename, `_mask.png`, and NUL must fit before any write or `read_png` call; exact-fit and one-byte-over runtime cases are covered |
| `GQF-0008` | `OPEN` | P2/high | test-gap/false-pass | `android/tests/test_hud_layout.c` | All behavioral checks use standard `assert` and disappear under `NDEBUG`. Make the registered optimized target retain its oracle. Live BR-0662 |
| `GQF-0009` | `OPEN` | P2/high | test-gap/false-pass | `android/tests/test_escort_exit_policy.c` | Four policy assertions disappear in optimized registered builds. Replace with always-active checks and prove failure behavior |
| `GQF-0010` | `OPEN` | P3/high | maintainability/dead-code | `ActiveGamesTab.kt:ActiveGamesTab` | Symbol search finds no production caller; the live UI uses `ActiveGameCard` directly. Remove or restore one clear owner. Live BR-0673 |
| `GQF-0011` | `OPEN` | P2/high | test-gap/correctness | `test_saf_redbook.json5` optional `player` selector | Substring selection can choose `Multiplayer`; optionality does not prevent a wrong positive match. Use an unambiguous selector and regression fixture. Live BR-0671 |
| `GQF-0012` | `OPEN` | P2/high | build-release | `android/docker/nat-testbed/Dockerfile` | `python:3.12-alpine` is mutable. Pin exact version and digest and validate the NAT testbed build. Live BR-0672 |
| `GQF-0013` | `OPEN` | P2/high | documentation/license | `server/Cargo.toml` | The public server package has no license or license-file field. Establish and validate the server licensing contract. Live BR-0003 |
| `GQF-0014` | `OPEN` | P1/high | security/configuration | `server/src/config.rs` | Parse/read failure silently selects defaults and TLS paths default independently. Fail closed for broken production configuration and partial TLS. Live BR-0107/BR-0108 |
| `GQF-0015` | `OPEN` | P1/high | security/resource-exhaustion | `server/src/ws_handler.rs`, protocol client schema | Text frames enter deserialization without aggregate and per-field budgets. Reject oversized frames/fields before expensive work. Live BR-0115/BR-0116 |
| `GQF-0016` | `OPEN` | P2/high | resource-lifetime | `game_data/extract_all_gog.ps1` | The script kills every process named `cl`, including unrelated work. Track and terminate only owned children. Live BR-0171 |
| `GQF-0017` | `OPEN` | P2/high | maintainability/whitespace | Branch-modified D1 sources and plans/artifacts | `git diff --check` reports branch-owned whitespace. Fix only attributable source sites and classify artifact/plan failures without broad inherited formatting |
| `GQF-0018` | `OPEN` | P2/medium | compatibility/capability | `extract/inno_reader.c` BZip2 and EXE filters | Runtime paths report unsupported features while tests only check the diagnostic. Prove fail-closed transactional behavior and surface capability to callers/UI |
| `GQF-0019` | `OPEN` | P3/high | maintainability/diff-minimization | Paired OGL ETC2 GPU self-test bodies | Large duplicated inherited-file bodies perform FBO setup, finish, readback, and logging. Evaluate shared Android ownership and release/debug gating under DMR1 |
| `GQF-0020` | `OPEN` | P2/medium | correctness/overflow | `ogl_texture_android.c:android_ogl_get_texture_bytes` | Signed `int` accumulation can wrap for a large texture/mod corpus. Carry checked 64-bit or `size_t` accounting through consumers |
| `GQF-0021` | `OPEN` | P2/medium | correctness/overflow | `ogl_texture_android.c:android_ogl_texture_elapsed_us` | Microseconds narrow to signed `int` before callers widen them. Preserve 64-bit elapsed accounting and test boundary math |
| `GQF-0022` | `OPEN` | P2/high | test-gap/data-format | `android_resume_pilot.c` save callsign boundary | `strcpy` relies on the state reader's size/NUL contract. Add malformed-save tests proving both games enforce NUL termination and engine limits before deciding whether code changes are needed |
| `GQF-0023` | `OPEN` | P2/medium | test-gap/api-data-format | `net_udp_android_autonet_shared.c` Netgame fields | Unbounded copies rely on upstream constants and imported metadata constraints. Add compile-time layout assertions plus rejection/truncation tests before altering wire-visible behavior |
| `GQF-0024` | `FIXED` | P1/high | security/supply-chain | Android production and matching host native dependency fetches | One repository-owned helper now resolves immutable manifest URL/SHA-256 pairs, rehashes cached bytes before every reuse, locks concurrent cache population, verifies TLS downloads before atomic publication, and supports offline reuse. Hostile bodies, corrupt cache entries, changed pins, production owner coverage, Windows D1/D2, the extract-host compile, and all Android ABIs passed |
| `GQF-0025` | `OPEN` | P2/high | test-gap/partial-migration | `android/run_quick_tests.ps1:90-102` | Test consolidation deleted `test_launcher_graphics_debug_prefs.json5` and `test_menu_scale_d2.json5`, but the quick catalog still dispatches both. Replace them with current unified coverage or remove redundant rows, add catalog-integrity validation, and execute the full quick suite |
| `GQF-0026` | `OPEN` | P3/high | maintainability/abandoned-compatibility | `FileSetManager.kt`, startup migration call, and `FileSetMigrationTest.kt` | The pre-release launcher still negotiates and tests two legacy file layouts despite the repository reset/no-backward-compatibility policy. Remove obsolete migration/cleanup state or explicitly change policy with a version matrix and retirement milestone |
| `GQF-0027` | `OPEN` | P3/high | api-data-format/partial-migration | `AudioSourceManager.kt` persistence and focused test | Every save still emits obsolete scalar `bin_content_uri` beside current plural `bin_content_uris`, preserving conflicting representations. Keep one current schema or define a bounded read-time migration that stops writing the old field |
| `GQF-0028` | `OPEN` | P3/high | maintainability/dead-code | `CrashLog.install`, `installed`, and Activity call sites | Application-owned xCrash initialization left a no-op Activity compatibility API and two misleading calls. Remove the shim and retain only live Java/native crash initialization, with launch and report validation |
| `GQF-0029` | `OPEN` | P2/high | generated-fixture/data-integrity | Anniversary ISO `track_fingerprints.json` | Incomplete remediation/regrowth linked to archived `BR-0184`: the tracked MODE1/2048 fingerprint retains the old `d95f...` digest while corrected sector geometry and the canonical complete ISO identity imply `cc63...`. Regenerate from the exact source, require canonical agreement, and add post-geometry corpus validation |
| `GQF-0030` | `OPEN` | P2/high | correctness/data-integrity/reproducibility | Physical-disc and mission fingerprint generation and cache reuse | Valid fingerprint artifacts omit source-inventory, tool/native source, decoder/extractor, Chromaprint, algorithm, schema, and policy generation identities; default workflows can skip them after implementation changes. Version and bind outputs to exact identities, reject or regenerate mismatches, and test each identity change plus legacy migration |
| `GQF-0031` | `OPEN` | P2/high | test-gap/false-pass | CD regression `audio_tracks` fields and `test_extract.ps1` | Per-release audio-track counts are generated but never validated or asserted; seven assigned imports can pass after losing audio registration or tracks. Validate schema, assert one observed import-generation source with matching disc ID and count, and test missing/short/extra/ambiguous cases |
| `GQF-0032` | `OPEN` | P2/high | correctness/reproducibility/data-integrity | Combined-launch staging collision ownership | D2 and Vertigo components supply colliding HAM/HOG basenames, but the runner chooses by larger byte size and incidental order while the generated spec records neither owner nor payload digest. Require an explicit reviewed owner/precedence plus digest or reject undeclared collisions before staging |
| `GQF-0033` | `OPEN` | P2/high | generated-fixture/data-integrity/test-gap | Combined-launch generated regression oracle | Ordinary validation never proves the generated combined spec still equals its helper and current component specs. Add a read-only pure derivation/equality check for every semantic input and divergent field while keeping refresh as the only writer |
| `GQF-0034` | `OPEN` | P2/high | correctness/data-integrity/generated-cache | Mission fingerprint cache admission and packaged publication | Matching source hashes plus `complete: true` can bless empty, duplicate, missing, or malformed tracks, and the packaged consumer validates even less. Define one typed schema, validate every root/track and exact ordered member inventory before reuse and publication, and test each corruption |
| `GQF-0035` | `OPEN` | P2/high | correctness/external-metadata/provenance | Mission AcoustID enrichment | Incomplete remediation/regrowth of archived `BR-0413`: maintenance accepts the first titled response without score threshold, stable recording ID, ambiguity handling, deterministic selection, or persisted evidence. Apply typed candidate policy and retain auditable score/IDs |
| `GQF-0036` | `OPEN` | P2/high | security/resource-exhaustion/performance | `saf_manifest_parser.c` retained strings and duplicate lookup | A valid sub-1MiB manifest with many short entries causes quadratic retained allocation and duplicate comparisons. Allocate measured strings, impose shared entry/field/aggregate budgets, and use bounded set/sorted duplicate validation with 32/64-bit limit tests |
| `GQF-0037` | `FIXED` | P1/high | compatibility/JNI-boundary/encoding | SAF URI and MIDI path/JSON JNI text bridges | Both bridges now use the shared strict Java UTF-16/standard UTF-8 converter exclusively. SAF rejects conversion failure before its callback; MIDI cleans partial inbound conversions and strictly converts native JSON. Raw and escaped BMP/non-BMP URI bytes, malformed codec cases, source boundary contracts, launcher tests, and all Android ABIs passed |
| `GQF-0038` | `OPEN` | P2/high | correctness/path-safety/portability | `mac_hfs_extract.c` output and scratch path joins | Two fixed 1,024-byte `snprintf` joins ignore truncation and immediately mutate the truncated path. Use one checked/dynamic platform-correct join and exact-fit/overflow/alias tests that prove failure before filesystem access |
| `GQF-0039` | `OPEN` | P2/high | concurrency/resource-lifetime/filesystem-safety | HFS `.install_descent.sti2` scratch lifecycle | Every call truncates, reopens, maps, and deletes one shared non-exclusive scratch pathname. Use attempt-owned exclusive creation and descriptor/identity-based lifetime; test concurrent calls, preexisting objects, replacement races, failures, leaks and residue |
| `GQF-0040` | `OPEN` | P2/high | correctness/data-integrity/encoding | Flattened HFS loose-file projection | Distinct catalog paths, case aliases, lossy decoded names, STi2 output, or preexisting bytes can map to one basename and silently keep the incumbent. Precompute portable identities and reject collisions or enforce one explicit payload-verified precedence independent of order |
| `GQF-0041` | `OPEN` | P2/high | correctness/data-loss/rollback/api-contract | SOW append failure handling | A failed later append removes the entire destination, including valid caller-owned and earlier split-archive bytes. Publish an attempt-owned complete file or truncate back to the exact pre-append size on every write/flush/close failure |
| `GQF-0042` | `OPEN` | P2/medium | security/filesystem-safety/race | SOW destination opens and cleanup | Path-based overwrite/append follows links or special nodes and cleans by pathname without claiming a regular-file identity. Require an owned root, no-follow relative opens, regular-file verification, exclusive attempt names and handle-based cleanup |
| `GQF-0043` | `OPEN` | P3/high | correctness/api-contract/diagnostics | SOW filtered progress | Total counts only selected output while done advances for skipped entries, allowing done far above total. Define one progress population and use it consistently with checked arithmetic and monotonic boundary tests |
| `GQF-0044` | `OPEN` | P2/high | correctness/storage/api-contract | PKG no-audio free-space admission | Native free-space checking reserves the audio-inclusive manifest even when audio is skipped, rejecting devices with enough space for every requested output. Compute selected publication bytes separately while retaining full integrity/work budgets |
| `GQF-0045` | `OPEN` | P2/high | correctness/diagnostics/ui | PKG JNI progress aggregation | PKG callbacks are per-file, but JNI cumulative totals remain zero, so overall progress reaches 100 percent then resets with a changing denominator. Publish one stable selected total and monotonic cumulative done across files and audio modes |
| `GQF-0046` | `OPEN` | P2/high | correctness/publication/resource-lifetime | Direct PKG and standalone ISO extraction APIs/callers | Incomplete closure/regrowth linked archived `BR-0020`: later/terminal failures leave earlier final-path outputs and can destroy prior bytes. Require transaction-owned staging/atomic publication or an explicit staging-only API contract honored by every maintained caller |
| `GQF-0047` | `OPEN` | P1/high | security/path-containment/symlink-race | ISO output directory traversal and final opens | Incomplete physical-containment remediation of archived `BR-0061`: lexical safe paths still follow intermediate/final symlinks, junctions or replacement races. Walk/create relative to canonical owned directory handles, reject reparse/link objects and test outside sentinels |
| `GQF-0048` | `OPEN` | P2/high | correctness/data-integrity/input-validation | ISO normalized destination identities | Case folding, version suffix removal and trailing-dot trimming can collapse distinct files/directories; record order silently selects bytes. Precompute a portable map and apply explicit version/collision policy independent of order |
| `GQF-0049` | `OPEN` | P2/high | compatibility/parser-correctness/data-integrity | ISO extended-attribute and interleaved records | Reader accepts nonzero layout fields but reads data contiguously from unadjusted LBA, silently corrupting valid files. Implement checked layouts or reject them explicitly before catalog publication, with raw/CUE boundary fixtures |
| `GQF-0050` | `OPEN` | P2/high | compatibility/parser-correctness | ISO volume descriptor discovery | Reader requires a PVD at sector 16 instead of scanning the bounded descriptor sequence through its terminator, rejecting valid boot-record-first images. Implement explicit sequence scanning and missing/duplicate/bad descriptor tests |
| `GQF-0051` | `OPEN` | P1/high | security/resource-exhaustion/memory-budget | STi2 method-15 decode scratch | Incomplete remediation/regrowth linked archived `BR-0018`: the 128 MiB output budget excludes a simultaneous 16 MiB block and up to 64 MiB transform allocation, allowing roughly 208 MiB before archive/caller overhead. Enforce one true peak-live-memory budget and exact boundary/OOM tests |
| `GQF-0052` | `OPEN` | P1/high | stability/JNI-boundary/resource-exhaustion | Assigned JNI array/string acquisitions and MIDI byte-array creation | Fallible JNI acquisitions and `NewByteArray` results are used or followed by more JNI calls without null/pending-exception checks. Check every acquisition/allocation, release only acquired resources, stop immediately with the original exception, and test OOM/pending-exception paths under CheckJNI |
| `GQF-0053` | `OPEN` | P2/high | correctness/source-integrity/api-data-format | Inno analysis-to-extraction generation binding | PKG compares a typed expected manifest, but Inno reopens path/provider data and ignores or cannot receive analyzed entry identity; same-count substitutions can be published. Bind one typed analyzed manifest or descriptor generation and reject drift before output creation |
| `GQF-0054` | `OPEN` | P3/medium | correctness/diagnostics/cli-contract | Shared native JSON producers and output streams | Callers discard every `json_write_string`, formatting and terminal flush/error result, so a failed sink can yield truncated JSON with success status. Accumulate/check output status, flush explicitly, return nonzero and test injected failures at every phase |
| `GQF-0055` | `OPEN` | P1/high | security/protocol-authentication/replay | Reconnect request route proof across starting/waiting/migration | Incomplete/regrown archived `BR-0225`: two network states mutate address/readiness after signature only, and migration resets request counters. Preserve counters and require route-bound proof before any ownership, address, time or readiness mutation in both games |
| `GQF-0056` | `OPEN` | P1/high | security/protocol-binding/compatibility | D1/D2 reconnect signed transcript and generation token | Both titles share one domain and repeatable 32-bit Android token namespace with no game-kind, role or robust generation nonce. Define a versioned title/role transcript with checked CSPRNG nonce replicated through migration and cross-title/restart/downgrade vectors |
| `GQF-0057` | `OPEN` | P1/high | security/resource-exhaustion/network | Reconnect request verification before admission | Every small unauthenticated self-signed request forces arrays, JNI/JCA EC key construction and verification synchronously on the game thread. Add bounded per-source/global admission before crypto, cache admitted key state and load-test valid attacker signatures/routes/NAT fairness |
| `GQF-0058` | `OPEN` | P1/high | resource-exhaustion/performance/cancellation/reproducibility | Complete-track disc and compressed-file fingerprinting | Regrowth of archived `BR-0048`: complete media work is unbounded, compressed decoders allocate full PCM before budget rejection, and blocking JNI has no cancellation boundary. Define/version policy, enforce early byte/duration/memory/CPU budgets, stream decoders and propagate typed cancellation |
| `GQF-0059` | `OPEN` | P2/high | correctness/match-ranking/determinism | Fingerprint matcher score serialization and best-CD selection | Four-decimal JSON output can collapse distinct same-tier similarities, letting lexical ID select a lower raw score. Publish a lossless ranking representation separate from display formatting and test near-ties in both orders |
| `GQF-0060` | `OPEN` | P2/high | correctness/match-ambiguity/generation | Exact album-fingerprint collision duration policy | Regrowth/incomplete `BR-0040`: PowerShell exact-collision augmentation and its test use asymmetric duration ratios, making ambiguity depend on source order. Apply one shared symmetric predicate and boundary/order parity vectors |
| `GQF-0061` | `OPEN` | P1/high | security/resource-exhaustion/storage | Complete CD extraction attempt budget | Incomplete/regrown archived `BR-0018`: each track and nested SOW call receives a fresh 2 GiB budget, allowing many multiples per attempt. Carry one attempt-owned byte/entry/memory/cancellation/free-space budget through every nested operation |
| `GQF-0062` | `OPEN` | P3/medium | correctness/storage/portability/path-identity | Windows drive-relative extraction destinations | For `D:relative`, nonexistent-parent free-space fallback queries `.` on the process drive while output resolves on D. Reject drive-relative paths or resolve one wide absolute destination/volume identity before admission and creation |
| `GQF-0063` | `OPEN` | P2/high | correctness/parser-correctness/source-identity | CUE `FILE` directive grammar | Missing/unclosed/empty/truncated filenames and unsupported/missing file types still create authoritative native slots while Kotlin uses a different grammar. Parse strict complete supported records and share native identities with source ordering |
| `GQF-0064` | `OPEN` | P2/high | correctness/parser-lexing/input-validation | CUE physical line handling | Lines beyond 511 bytes are split and reparsed as synthetic directive lines. Enforce/consume one bounded physical line and fail oversized input, with exact boundary and Kotlin/native parity tests |
| `GQF-0065` | `OPEN` | P2/high | correctness/data-identity/ordering | CUE track-number policy | Duplicate, descending, gapped and track 100 identities are accepted while downstream maps collapse by number. Define and enforce one compatible unique/order/range policy across parsing, fingerprinting, registration and merged CUEs |
| `GQF-0066` | `OPEN` | P2/medium | correctness/parser-lexing/source-transaction | Exact CUE snapshots containing embedded NUL | Exact byte reads become successful prefix parses because the parser has no length and stops at NUL while Kotlin may see later references. Pass length, reject embedded NUL/disallowed controls and require complete consumption |
| `GQF-0067` | `OPEN` | P2/medium | security/filesystem-safety/privacy/resource-exhaustion | Audio fingerprint directory enumeration/input identity | CLI admits symlinks, reparse points, FIFOs, devices and replaced paths as audio inputs, allowing outside-root reads, privacy-derived output or hangs. Open relative to an owned directory, enforce regular/no-follow stable identity, or define explicit contained link policy |
| `GQF-0068` | `OPEN` | P2/high | test-gap/regression-registration | `test_fingerprint_audio_enumeration.ps1` | The focused enumeration behavior test is neither CTest-registered nor invoked by any aggregate runner, so `BR-0043` can regress while quick native tests stay green. Register a portable test or aggregate invocation with timeout and prove a seeded regression fails |
| `GQF-0069` | `OPEN` | P2/high | correctness/data-integrity/reproducibility/source-transaction | CD fingerprint BIN source generation | The CLI sizes each BIN, then separately reopens it per track without binding one immutable source generation, so same-size replacement or in-place mutation can publish one manifest assembled from inconsistent bytes. Open each source once, derive geometry from owned descriptors, and verify/stage one stable generation |
| `GQF-0070` | `OPEN` | P2/high | compatibility/portability/path-identity | Windows `fingerprint_cd` CUE and BIN paths | The maintained PowerShell caller supplies Unicode paths to a narrow `main`/`_stat64`/`_open` CLI, making non-ACP CUE or BIN identities unaddressable or lossy. Use a wide Windows entry/filesystem boundary and one explicit checked CUE filename encoding |
| `GQF-0071` | `OPEN` | P2/high | correctness/parser-validation/path-identity | HFS catalog parent graph | Orphans, file parents, CNID-1 misuse and parent cycles are repaired into flattened usable paths instead of rejecting the catalog. Validate one rooted acyclic directory graph and complete bounded paths before any public projection |
| `GQF-0072` | `OPEN` | P2/high | security/resource-exhaustion/performance/cancellation | HFS catalog metadata and ancestry work | Regrowth of archived `BR-0018`: declared catalog bytes and ignored records bypass effective metadata/work ceilings, while linear CNID/parent/path scans amplify accepted catalogs without cancellation. Carry one bounded parser/work budget and indexed graph validation through catalog open |
| `GQF-0073` | `OPEN` | P2/high | compatibility/parser-correctness | Classic-HFS B-tree allocation-map chain | The reader requires later map nodes' unused backward links to name the prior node, rejecting valid multi-map-node trees whose required `bLink` values are zero. Apply map-node semantics and add exact multi-node capacity fixtures; link the incomplete archived `BR-0050` boundary |
| `GQF-0074` | `OPEN` | P2/high | correctness/parser-validation | Classic-HFS fixed catalog records | Folder bodies of 10-69 bytes and file bodies of 98-101 bytes are accepted although the fixed schemas require 70 and 102 bytes; key/name slack is also admitted. Validate complete record/key schemas before decoding or publishing any entry |
| `GQF-0075` | `OPEN` | P2/high | correctness/parser-validation/structural-bounds | Inno PE resource offset-table leaf | Incomplete archived `BR-0052`: PE traversal validates the resource `data_size` for RVA mapping, returns only an offset, then reads 64 bytes without the leaf/section bound, allowing an undersized resource to borrow adjacent bytes. Carry a bounded leaf view and enforce layout-specific exact reads |
| `GQF-0076` | `OPEN` | P1/high | security/resource-exhaustion/memory-budget | Inno metadata LZMA peak-live memory | Incomplete archived `BR-0018`: near-64-MiB raw metadata, decompressed output and archive-selected dictionary can coexist at roughly 192 MiB despite the 128-MiB policy. Carry one live-allocation budget through buffers and every decoder allocation |
| `GQF-0077` | `OPEN` | P2/high | correctness/data-integrity/parser-validation | Inno setup-header LZMA terminal state | Incomplete archived `BR-0059`: metadata decode returns partial output on no progress or input exhaustion and accepts terminal streams with trailing bytes. Share the strict payload terminal predicate, require exact input consumption, and discard all partial archive state |
| `GQF-0078` | `OPEN` | P1/high | security/undefined-behavior/parser-validation | Inno version admission arithmetic | Individually representable components reach signed `major * 10000 + minor * 100 + patch` overflow before unsupported-version rejection. Validate the supported tuple directly or use checked arithmetic before every layout selection, with complete archive and UBSan boundary fixtures |
| `GQF-0079` | `OPEN` | P2/high | compatibility/correctness/data-integrity | Galaxy multipart Inno assembly | Incomplete archived `BR-0057`: validated multipart count and `AfterInstall` grouping metadata are discarded, so documented multipart archives cannot be bound or atomically reassembled. Preserve typed grouping/order and validate complete sets before extraction |
| `GQF-0080` | `OPEN` | P2/high | compatibility/data-integrity/path-identity | Inno Windows destination namespace | Incomplete archived `BR-0054`: preflight omits reserved device components and models only ASCII case, allowing invalid names or Unicode case aliases to collide at wide-path output. Canonicalize against actual Windows component/case rules before writes |
| `GQF-0081` | `OPEN` | P2/high | correctness/compatibility/api-data-format | Inno split-volume source identity | Parsed first/last-slice metadata is ignored and every chunk is resolved against one owned descriptor. Reject unsupported split volumes or bind every location to the correct validated source set |
| `GQF-0082` | `OPEN` | P2/high | correctness/compatibility/resource-policy | Inno solid-chunk decoded prefix and terminal routing | Buffered/stream routing considers compressed and file size but not required `file_offset + file_size`, rejecting valid small high-offset files and mishandling exact-capacity zlib termination. Route on checked decoded prefix and share strict terminal semantics |
| `GQF-0083` | `OPEN` | P1/high | security/resource-exhaustion/performance | Inno aggregate repeated solid-chunk decoding | Incomplete archived `BR-0018`: up to 4,096 selected aliases can restart decode of the same near-limit solid chunk with no attempt-wide input or decode-work budget. Cache validated chunks or charge every restart to one cumulative work budget |
| `GQF-0084` | `OPEN` | P2/high | correctness/data-integrity/transaction | Galaxy external-size validation and publication | Incomplete archived `BR-0020`/`BR-0034`: a size mismatch is detected only after a temporary leaf is committed to its final name, creating an observable and process-death invalid-publication window. Validate all external expectations before commit |
| `GQF-0085` | `OPEN` | P2/high | security/resource-exhaustion/performance | Inno output-name uniqueness preflight | Pairwise comparison of 4,096 legal long common-prefix basenames can exceed four billion byte comparisons with no work or cancellation bound. Build one bounded canonical-name map and reject duplicates in near-linear work |
| `GQF-0086` | `OPEN` | P3/high | correctness/progress/observability | Inno solid-chunk progress accounting | Progress charges unread prefixes and suffixes and may require complete chunk decode for sparse selections, so callbacks do not represent selected output work. Define cumulative source-versus-selected-output semantics and test sparse solid chunks |
| `GQF-0087` | `OPEN` | P2/high | correctness/compatibility/data-selection | ISO9660 directory named `zero` | The generic parser suppresses every directory named `zero` at any depth and on every medium based on one D2 placeholder-corpus rule, silently omitting ordinary content. Move the exception to an explicitly identified release policy or remove it |
| `GQF-0088` | `OPEN` | P2/high | correctness/parser-validation/structural-bounds | ISO9660 declared volume-space containment | Root, directory and file extents are checked against the outer track/file but not the PVD Volume Space Size, so bytes outside the declared filesystem can be extracted. Bound all logical blocks to the declared volume before traversal |
| `GQF-0089` | `OPEN` | P2/high | compatibility/JNI-boundary/encoding | CUE title UTF-8 capacity boundary | Valid multibyte titles can be truncated at byte 63 inside a scalar; strict JNI conversion then rejects the internally corrupted row and aborts an otherwise valid disc import. Preserve complete scalar prefixes, reject explicitly, or use dynamic title storage |
| `GQF-0090` | `OPEN` | P2/high | parser-correctness/api-data-format/source-integrity | XAR direct-field scope | Incomplete archived `BR-0068`: field lookup skips nested `file` elements but accepts matching fields under arbitrary other wrappers, allowing structurally wrong package/Scripts metadata. Use bounded structural parsing and exact reviewed child relationships |
| `GQF-0091` | `OPEN` | P2/high | correctness/source-integrity/api-data-format | PKG analysis-to-extraction generation identity | Incomplete archived `BR-0069`: same-length CRC32 collisions preserve both per-entry and concatenated Scripts identities, allowing substituted analyzed bytes before extraction. Bind the exact source or expanded stream with a collision-resistant identity |
| `GQF-0092` | `OPEN` | P3/medium | correctness/compatibility/portability | SOW zero-length entry allocation | A valid zero-length stored entry is accepted only when `malloc(0)` returns non-null, making behavior allocator-dependent. Represent empty output explicitly without requiring a non-null allocation |
| `GQF-0093` | `OPEN` | P2/high | test-gap/correctness/decoder | STi2 method-14 production success oracle | No registered fixture drives a successful production method-14 archive decode, leaving the repaired tree and terminal-state paths protected only by synthetic unit fragments. Add a legal archive fixture with exact output/hash and seeded regression proof; linked archived `BR-0158` |
| `GQF-0094` | `OPEN` | P2/high | resource-lifetime/stack/memory-budget | STi2 fixed entry-list stack frames | Direct extraction places at least 54 KiB of fixed entry arrays on one native frame and the maintained nested StuffIt path reaches roughly 95 KiB before decoder scratch. Move bounded catalogs to checked heap ownership and include nested low-stack validation |
| `GQF-0095` | `OPEN` | P2/high | correctness/resource-lifetime/lifecycle/failure-recovery | Level-metadata process-global initialization retry | Recoverable initialization failure returns with runtime readiness false but leaves PHYSFS, memory, audio, game, or screen globals partly initialized; retained-service retry reruns one-time setup over dirty state. Make initialization transactional with rollback or poison/replace the worker process |
| `GQF-0096` | `OPEN` | P2/high | correctness/data-identity/compatibility | Resume-save long mission identity | Incomplete archived `BR-0083`: metadata retains only eight mission bytes while save-set paths encode the complete/hash identity, so valid longer custom missions become path-mismatch orphans. Carry one complete canonical mission identity through metadata and path ownership |
| `GQF-0097` | `OPEN` | P2/high | performance/lifecycle/filesystem | Resume-save option discovery on Compose looper | Incomplete archived `BR-0444`: `findOptions` synchronously walks both complete game roots and retries up to 20 times at 500 ms when no candidate exists. Move bounded indexed discovery off the main thread and publish one cancellable snapshot |
| `GQF-0098` | `OPEN` | P2/high | correctness/data-loss/concurrency/file-identity | Resume-save conditional deletion generation | Path/slot plus embedded or whole-second timestamp does not bind the confirmed file generation, so a replaced same-slot save can be removed across the check/delete race. Delete through an owned descriptor or revalidate a collision-resistant identity atomically |
| `GQF-0099` | `OPEN` | P2/high | correctness/data-integrity/source-transaction | Seekable SAF mounted archive generation | Incomplete archived `BR-0085`: seekable provider descriptors bypass staging and a length-only native cache permits same-size or in-place mutation within one mounted archive generation. Stage or bind stable content identity for the mount lifetime |
| `GQF-0100` | `OPEN` | P1/high | security/resource-exhaustion/storage/input-validation | ZIP early-marker prompt bypass | Incomplete `BR-0093`/`BR-0018`: any early PK marker disables the 16-MiB preamble prompt bound and permits near-2-GiB staging before rejection. Validate an actual archive structure before lifting prompt limits |
| `GQF-0101` | `OPEN` | P2/high | compatibility/parser-correctness/input-validation | ZIP EOCD candidate selection | EOCD-shaped bytes in a valid comment commit parsing to an invalid later candidate without falling back to the real EOCD. Search bounded candidates and accept the one whose complete central structure validates |
| `GQF-0102` | `OPEN` | P2/high | correctness/data-integrity/parser-validation | ZIP central versus sequential entry set | Central-directory validation is followed by sequential `ZipInputStream` extraction that can expose local entries absent from the validated central set. Extract only the exact validated identities/spans or prove set equivalence |
| `GQF-0103` | `OPEN` | P1/high | security/resource-exhaustion/storage/compression | Setup direct ZIP extraction budget | A direct SetupFileImport extraction path bypasses the shared `ExtractionBudget`, allowing entry/count/expanded-byte limits to be avoided. Carry one attempt-owned budget through every ZIP path |
| `GQF-0104` | `OPEN` | P1/high | security/resource-exhaustion/memory-budget | Kotlin bounded byte-array peak memory | `readBytesBounded` can retain `ByteArrayOutputStream` storage plus `toByteArray` copy near one GiB and omits the native 128-MiB policy. Stream or include all simultaneous buffers in one peak-live-memory budget |
| `GQF-0105` | `OPEN` | P2/high | compatibility/api-data-format | Song-list token versus engine filename capacity | Launcher tokens exceed the paired engines' retained `%15s` filename contract. Enforce one shared complete identity capacity and test collisions/truncation across both engines |
| `GQF-0106` | `OPEN` | P2/high | correctness/cache/api-data-format | Container-track sidecar identity | Incomplete `BR-0091`: multiple contained tracks reuse the outer DXA/HOG as one exact sidecar path, so the strict encoder rejects the second record. Key sidecars by container plus member identity |
| `GQF-0107` | `OPEN` | P2/high | correctness/cache-completeness/retry | Partial identification sidecar freshness | A sidecar containing full source identity but failed tracks is considered current and permanently suppresses retries. Record completeness/retry policy separately from source freshness |
| `GQF-0108` | `OPEN` | P1/high | security/resource-exhaustion/compression | RAR policy rejection fallback | A native RAR policy rejection falls through to a weaker host-tar backend. Preserve typed policy failure and never retry it through a less constrained extractor |
| `GQF-0109` | `OPEN` | P1/high | security/resource-exhaustion/memory/work | RAR catalog projection admission | RAR item and projection lists are fully materialized before the entry-count budget. Apply count/work/memory limits while enumerating, before projection |
| `GQF-0110` | `OPEN` | P2/high | correctness/resource-lifetime/failure-recovery | Direct Setup RAR staged source ownership | Direct fallback deletes its own staged source archive. Separate input-stage and output-cleanup ownership and prove retry preservation |
| `GQF-0111` | `OPEN` | P2/high | correctness/input-validation/compatibility | Mission ZIP loadable-level admission | Incomplete `BR-0096`: catalog membership accepts one-byte or invalid level payloads as playable. Validate the production level contract before mission publication |
| `GQF-0112` | `OPEN` | P2/high | correctness/path-identity/compatibility | Mission descriptor qualified references | Leaf-only matching accepts qualified descriptor references that the engine cannot resolve. Preserve and validate the exact relative identity consumed by both engines |
| `GQF-0113` | `OPEN` | P2/high | correctness/input-validation/publication | Stored ZIP collision validation timing | Incomplete `BR-0105`: sub-10-MiB stored collisions bypass validation until after registration/launch. Complete portable identity/collision admission before publication |
| `GQF-0114` | `OPEN` | P3/high | maintainability/dead-code | `MissionZip.isMissionHog` | Private helper is unused after the admission refactor. Remove it or restore one explicit owner and test |
| `GQF-0115` | `OPEN` | P1/high | correctness/source-transaction/data-integrity | Mission extraction `ScanResult` generation | Incomplete `BR-0102`: the scan result is not bound to the source generation later hashed/staged, so admission and extraction can describe different bytes. Carry one immutable source identity/descriptor through scan, stage and publish |
| `GQF-0116` | `OPEN` | P2/high | performance/responsiveness/filesystem-I/O | Mission extraction freshness hashing | Regrowth of `BR-0444`: ordinary launch preparation synchronously rehashes complete source and outputs at least three times. Cache one verified snapshot and perform bounded off-main-thread freshness checks |
| `GQF-0117` | `OPEN` | P3/high | correctness/progress/storage-admission | Generated `descent.sng` alias bytes | Generated alias bytes are omitted from initial storage totals and progress, so progress reaches 100 percent before additional I/O and tight-storage failure. Include every generated output in preflight and cumulative progress |
| `GQF-0118` | `OPEN` | P1/high | security/resource-exhaustion/catalog-budget | Nested mission music container catalogs | Incomplete `BR-0018`: per-container budget resets admit up to 4,096 containers each retaining up to 4,096 tracks. Carry one attempt-owned entry/work/memory budget through every nested container |
| `GQF-0119` | `OPEN` | P1/high | security/resource-exhaustion/storage/compression | Streaming descriptor nested music budget | Streaming data-descriptor sizes become zero, negative compressed sizes disable ratio accounting, and inner DXA/HOG helpers get fresh budgets; MIDI can materialize up to 512 MiB. Preserve exact source sizes and one nested expansion budget |
| `GQF-0120` | `OPEN` | P2/medium | correctness/resource-lifetime/cache | Music staging generation directory lease | Cleanup evicts generation directories while UI retains returned `File` paths for later/asynchronous preview. Introduce explicit leases or copy/own preview inputs before eviction |
| `GQF-0121` | `OPEN` | P2/high | security/resource-exhaustion/concurrency | NAT simulator live symmetric mappings | Live mappings can create unbounded tasks/sockets and grow restricted allowlists without an aggregate concurrency/ownership ceiling. Add global/per-client caps, bounded allowlists, supervised lifetimes and teardown accounting |
| `GQF-0122` | `OPEN` | P2/high | correctness/networking/prediction | Sequential-port prediction sample admission | Predictor accepts uneven or duplicate samples using only integer average spread, fabricates predicted endpoints and changes relay choice. Require a reviewed sequence model, unique ordered samples and conservative fallback |
| `GQF-0123` | `OPEN` | P2/high | build-release/incremental-correctness | TinySoundFont dependency rebuild identity | Preserved archive timestamps can leave `test_midi_seek_timeline` using an object compiled against the previous pinned dependency after reconfigure. Use supported timestamp behavior or an explicit dependency-generation input and prove incremental and clean builds agree |
| `GQF-0124` | `OPEN` | P2/high | correctness/failure-containment/observability | Mission batch diagnostic artifact capture | A diagnostic-copy exception inside item `finally` can abort result recording, remaining items, summaries, recovery and app cleanup. Make diagnostics best-effort within a bounded item outcome while preserving the primary failure |
| `GQF-0125` | `OPEN` | P1/high | security/supply-chain/tool-identity | Bounded extraction Python runtime | The wrapper executes a PATH-selected Python interpreter without an accepted identity, regrowing archived `BR-0157`. Resolve a repository-owned or explicitly admitted runtime and test hostile PATH precedence |
| `GQF-0126` | `OPEN` | P3/high | correctness/data-preservation/HFS | Zero-length HFS output files | The fallback silently omits valid zero-length HFS files. Publish empty regular files with the same checked identity and collision policy as nonempty entries |
| `GQF-0127` | `OPEN` | P1/high | security/resource-exhaustion/process-lifetime | Bounded extractor descendant ownership | A successful extractor parent can return while leaving unbounded descendants alive. Supervise the complete process tree through success, failure and timeout, and prove no child survives |
| `GQF-0128` | `OPEN` | P1/high | security/filesystem-safety/resource-accounting | Extractor link and special-file outputs | Links and special files can bypass physical containment and byte accounting. Reject unsupported entry types and verify physical containment plus actual output accounting before publication |
| `GQF-0129` | `OPEN` | P2/high | build-release/portability/tooling | POSIX CUE/ISO test entry point | The documented shell entry point is not executable in a fresh checkout. Preserve executable mode or invoke it explicitly through a supported shell and add a checkout-level invocation oracle |
| `GQF-0130` | `OPEN` | P2/high | correctness/concurrency/transactionality | Bulk extraction destination publication | Concurrent callers publishing the same destination can race backup/final moves, allowing the loser to delete the winner or the prior backup. Serialize by canonical destination and bind rollback to the owned generation |
| `GQF-0131` | `OPEN` | P2/high | correctness/data-integrity/publication | DOS demo initial output admission | Name-only first-generation checks can bless incomplete or wrong required installer output. Validate required bytes and source-bound semantics before publishing a reusable generation |
| `GQF-0132` | `OPEN` | P2/high | correctness/media-geometry/compatibility | Legacy Mac CD CUE sector geometry | A legacy parser reads Mode 2 or unknown sectors using Mode 1 geometry, regrowing archived `BR-0184`. Share the strict production geometry contract and fail unsupported layouts closed |
| `GQF-0133` | `OPEN` | P2/high | correctness/input-validation/source-selection | Legacy Mac CD CUE INDEX fields | Missing or invalid INDEX values become usable sector positions, regrowing archived `BR-0183`. Require one strict complete INDEX 01 and bind it to the selected source |
| `GQF-0134` | `OPEN` | P2/high | correctness/reproducibility/cache-provenance | Mac CD bounded-supervisor generation | Extraction cache provenance omits the bounded supervisor implementation. Include every helper and policy generation that can change accepted outputs |
| `GQF-0135` | `OPEN` | P2/high | security/resource-exhaustion/process-supervision | Mac demo direct `unar` phases | Both external extraction phases bypass bounded child supervision. Apply process-tree time/work/output bounds and cancellation to every phase |
| `GQF-0136` | `OPEN` | P2/high | correctness/reproducibility/oracle-provenance | Mac demo tracked oracle generation | A legacy tracked oracle predates the provenance schema and its consumer ignores provenance. Regenerate from verified sources and require schema/source/tool/policy identity before use |
| `GQF-0137` | `OPEN` | P2/high | security/resource-exhaustion/stack | Mission music nested archive recursion | Nested ZIP/DXA traversal has no explicit depth or stack budget. Carry a single attempt-owned nesting ceiling and reject cycles or excessive depth |
| `GQF-0138` | `OPEN` | P2/high | correctness/data-model/determinism | Mission music tracklist selector conflicts | Multiple matching selector rows are silently resolved by input order. Define uniqueness or deterministic precedence and reject ambiguous conflicting rows |
| `GQF-0139` | `OPEN` | P2/high | correctness/cache-generation/data-integrity | Mission fingerprint no-audio transition | Successful regeneration of a source with no audio leaves the prior audio generation authoritative. Publish an explicit current empty generation or atomically retire the stale sidecar |
| `GQF-0140` | `OPEN` | P2/high | resource-lifetime/process-supervision/cancellation | Native mission fingerprint invocation | Native fingerprinting has no deadline or owned process-tree cancellation. Bound each mission invocation and continue with an explicit per-item failure after complete teardown |
| `GQF-0141` | `OPEN` | P1/high | correctness/data-integrity/transactionality | Graphics configuration rollback backup | A failed rollback deletes the only retained original backup, violating archived `BR-0200` preservation. Retain a recoverable original until replacement and durable rollback are independently proven |
| `GQF-0142` | `OPEN` | P2/high | correctness/resource-lifetime/failure-path | Graphics transaction parent directory sync | Parent-directory `fsync` failure leaks its descriptor. Give every descriptor one cleanup owner and inject sync failure before each return boundary |
| `GQF-0143` | `OPEN` | P2/high | test-gap/false-pass/parser-contract | CUE extension-filter tests | Reject-filter coverage can pass after a failed or empty listing, while null-filter coverage uses one BIN and checks only a positive count. Require successful listing plus exact accepted/rejected path identities across multiple extensions |
| `GQF-0144` | `OPEN` | P2/high | test-gap/correctness/production-parity | HMP conversion success oracle | Successful cases assert only return status, allowing null, empty or corrupt MIDI, and bypass paired D1/D2 wrapper/header/tempo/ownership seams. Validate exact MIDI semantics through both production wrappers |
| `GQF-0145` | `OPEN` | P2/high | correctness/state-lifetime/configuration | Chromaprint database invalid threshold reconfiguration | After a valid load, an invalid threshold leaves entries matchable at zero because matching ignores configuration validity. Fail closed or preserve the last complete valid generation atomically |
| `GQF-0146` | `OPEN` | P3/high | maintainability/dead-code/test-source | CUE test fixture helpers | Two fixture helpers and their supporting assertion header are unused in the branch-added test. Remove them or give them an explicit oracle owner without increasing production scope |
| `GQF-0147` | `OPEN` | P2/high | correctness/path-identity/portability | ISO Windows reserved device components | ISO output containment admits device basenames such as `NUL.HOG` and `COM1.HOG`, regrowing archived `BR-0061`. Apply one portable component-identity policy before publication |
| `GQF-0148` | `OPEN` | P2/high | test-infrastructure/resource-lifetime/portability | CUE/ISO production-sized stack catalogs | Multiple automatic `iso_file_list_t` values each exceed 1 MiB, requiring an MSVC-only 16 MiB stack workaround and bypassing heap ownership seams. Use the production allocation/destroy contract on all platforms |
| `GQF-0149` | `OPEN` | P2/high | test-gap/oracle-strength/production-parity | CD fingerprint input-byte contract | The sector test never proves the fingerprint represents supplied bytes or matches the production stream path. Change controlled input bytes and require exact, production-path-sensitive output behavior |
| `GQF-0150` | `OPEN` | P2/high | test-gap/fixture-integrity/input-validation | Fingerprint raw PCM fixture loader | Short or malformed fixture files are accepted as successful test input. Require exact header/length/read completion and reject trailing or incomplete data before fingerprint assertions |
| `GQF-0151` | `OPEN` | P3/high | test-infrastructure/diagnostics/side-effects | Fingerprint `ASSERT_EQ_INT` macro | Failed diagnostics reevaluate expressions, allowing side effects or changed values to obscure the original failure. Evaluate each operand once before comparison and printing |
| `GQF-0152` | `OPEN` | P3/high | test-infrastructure/undefined-data/fixture-integrity | GOG LZMA1 trailing-data fixture | The fixture passes 129 bytes after initializing only 41, making results depend on uninitialized stack tail. Initialize the complete buffer and assert exact intended trailing-byte semantics |
| `GQF-0153` | `OPEN` | P2/high | test-gap/correctness/oracle-strength | D2 Mac native extraction output oracle | The production-wrapper test calls 17 expected filenames and count a full oracle without checking any bytes. Bind exact source generation and validate complete file identities, sizes and hashes |
| `GQF-0154` | `OPEN` | P3/high | correctness/cache-generation/error-handling | Empty mission music sidecar generation | Empty generation ignores failed sidecar deletion and returns zero, allowing an older same-source sidecar to remain current without diagnostics. Publish explicit empty state or require verified removal |
| `GQF-0155` | `FIXED` | P3/high | diff-minimization/merge-pressure | Paired Android PhysFS initialization residue | The shared owner now performs checked PhysFS initialization, symbolic-link setup, search-path setup with exact diagnostics, and Android argument initialization. Paired inherited files retain only compact game-directory calls, removing 26 inherited lines and two hunks |
| `GQF-0156` | `FIXED` | P2/high | diff-minimization/merge-pressure | Paired menu/window debug accessors | Ordinary shared `.h/.c` owners now implement the nine public accessors; paired inherited files retain narrow opaque-layout snapshot adapters and header includes. All APIs, private layouts, behavior and consumers are preserved, with 82 net inherited lines removed |
| `GQF-0157` | `OPEN` | P2/high | test-gap/false-pass/security-resource-bounds | Bounded extractor quota reason oracle | File, per-file-byte and aggregate output quota tests accept timeout as success; disabling all tree measurements leaves them green. Require typed or exact failure reason and prompt quota termination independently of timeout |
| `GQF-0158` | `OPEN` | P2/high | test-gap/build-release/discovery | Bounded extractor Python regression registration | The focused Python supervisor regression has no maintained runner, discovery, CTest or workflow owner. Register it in the branch-added host-test graph and prove aggregate failure propagation on supported hosts |
| `GQF-0159` | `OPEN` | P2/high | test-gap/schema-validation/false-pass | Extraction regression spec validator | An ISO spec with absent or empty `source_files` passes validation because the empty projection, skipped loop and ISO branch reach readiness without a source. Require the declared source set and reject missing, empty and malformed source identities |
| `GQF-0160` | `OPEN` | P2/high | test-infrastructure/discovery/tiering | Host-only extraction provenance/publication tests | Two host-only scripts are absent from the no-infrastructure set, default into the emulator tier and require APK/emulator prerequisites. Classify them in maintained host discovery and prove they run without device infrastructure |
| `GQF-0161` | `OPEN` | P3/high | test-infrastructure/diagnostics/stale-state | `test_extract.ps1` extraction failure diagnostic | The failure path references undefined stale variable `$maxNav` after the navigation loop was removed, obscuring the primary wrong-level failure. Use current bounded context and add a failing-path oracle under strict mode |
| `GQF-0162` | `OPEN` | P2/high | test-gap/false-pass/source-contract | Extraction regression workflow source assertions | Raw-source regular expressions can pass on comments or dead tokens and fail harmless refactors without proving runtime behavior. Replace them with behavior-level fixtures or structured ownership checks that fail when the production contract is broken |
| `GQF-0163` | `OPEN` | P2/high | test-gap/correctness/data-freshness | App-private extraction source staging | `Ensure-AppPrivateFile` reuses a stable app-private CUE/BIN/IMG/ISO path solely when byte counts match, so interrupted cleanup plus equal-length replacement can run stale media bytes under the new spec. Bind reuse and publication to exact source identity and owned cleanup |
| `GQF-0164` | `OPEN` | P2/high | test-gap/false-pass/resource-lifetime | SAF archiver descriptor-only mode | Descriptor-only mode prints PASS and exits zero when game readiness never succeeds, so no process descriptor listing or manifest-release assertion occurs. Require positive readiness and an explicit completed descriptor observation before focused success |
| `GQF-0165` | `OPEN` | P2/high | test-gap/build-release/discovery | D2X-XL WAV parser regression | The only focused malformed-WAV regression is outside maintained test discovery and has no timeout owner, so archived parser-safety behavior can regress while aggregate suites remain green. Register it under a bounded branch-added host-test owner and prove pass, failure, timeout and not-run reporting |
| `GQF-0166` | `OPEN` | P3/high | documentation/api-data-format/configuration | Matchmaking server exhaustive configuration template | The template claims to show every optional field and default but omits supported `max_connections` and `force_relay`. Add both with exact defaults/purpose and enforce schema-to-template parity or explicitly document a reviewed subset |
| `GQF-0167` | `OPEN` | P2/high | correctness/audio-lifecycle/test-gap | Shared TSF/PCM producer completion ordering | Regrowth of archived `BR-0279`: non-looping producers publish source-finished before `render_thread_func` release-publishes their final ring write. A callback can drain the old ring, declare completion, stop the generation, and strand up to 2,048 final frames. Publish the final write and EOF as one ordered transaction and add barrier-controlled MIDI/PCM race coverage |
| `GQF-0168` | `OPEN` | P2/high | performance/resource-exhaustion | Replay current-frame direct-command acquisition | Direct-command policy first parses every event to count commands, then requests each index through a getter that reparses from event zero. A frame with `n` commands performs `n + n(n+1)/2` JSON parses, and no per-frame event-count boundary exists within the 128 MiB whole-file limit. Decode once into a bounded typed batch and preserve validate-before-apply behavior |
| `GQF-0169` | `FIXED` | P3/high | diff-minimization/ownership | Paired Redbook Android extension declarations | One conventional branch-added header now owns the exact type-complete declarations. Each inherited header retains one include, reducing the merge-base additions from 22 to four, an exact 18-line inherited reduction |
| `GQF-0170` | `FIXED` | P3/high | diff-minimization/ownership | Paired HMP Android wrapper residue | The branch-added shared converter now owns `hmp2mid_mem` and its canonical 19-byte tempo track. Removing the paired includes and wrappers restores both inherited HMP files exactly to merge-base content, eliminating 24 additions and four hunks while preserving allocator and link ownership |
| `GQF-0171` | `OPEN` | P3/high | diff-minimization/dead-code | Unused paired input-demo FP environment includes | Four inherited D1/D2 gameplay/hook paths include `input_demo_fp_env.h` but use none of its declarations. Remove those lines and verify paired builds; retain the necessary FP environment calls at their current local owners |
| `GQF-0172` | `FIXED` | P3/high | diff-minimization/ownership | Residual paired Android mixer init logging | The branch-added diagnostics owner now queries the actual SDL_mixer spec and owns the exact success and failure log formats. Each inherited game retains only requested-rate/buffer inputs and compact success/failure calls, removing 46 inherited additions while preserving initialization behavior |
| `GQF-0173` | `OPEN` | P1/high | correctness/resource-lifetime/concurrency | Android SDL callback entry and teardown | Regrowth/incomplete closure of archived `BR-0248`: the OpenSL callback dereferences callback-visible device and buffer state before incrementing its in-flight counter, so teardown can observe zero, destroy/free state, then let the callback resume into freed objects. Establish lifetime ownership before any dereference and serialize player publication, background calls and destruction under the same protocol |
| `GQF-0174` | `FIXED` | P3/high | diff-minimization/ownership | Shared secret-area adapter and interface | The per-game-compiled adapter owns writer/reader bodies and selects rewind-memory or PhysicsFS operations through the existing abstraction. Its matching shared header is the sole interface owner. Paired state files retain only ordered calls and both branch-added D1/D2 header copies are removed, reducing the inherited-tree branch footprint by 122 lines while preserving byte layout, swapping, count fallback and all include sites |
| `GQF-0175` | `FIXED` | P2/high | diff-minimization/build-ownership | Paired headless executable target construction | Shared construction now lives in `cmake/dxx-headless-targets.cmake`; the two inherited blocks retain explicit source-list ownership and compact D1/D2 invocations, reducing their attributable footprint from 185 to 30 lines while preserving exact generated target properties |
| `GQF-0176` | `OPEN` | P3/high | maintainability/dead-code | Orphaned `native-lib.cpp` JNI skeleton | The unregistered 16-line skeleton duplicates the live `helloFromNative` symbol in `jni_main.c` and has no build consumer. Delete it, retain the production JNI owner, and verify no generated/source inventory references the orphan and both Android game targets link |

## Investigations

Investigations hold material risks that cannot yet be asserted as defects. They are not counted as findings until the missing evidence is obtained

| ID | State | Risk if confirmed | Category | Scope | Missing evidence and next check |
|---|---|---|---|---|---|
| `GQI-0001` | `OPEN` | P0 | security/secrets | Complete 1,252-commit publication history, including deleted and renamed blobs | Extends historical `INV-0001`. Bounded frozen-head and `git log -G` patterns found only parser/example material, but do not provide entropy, provider-aware, or binary-blob coverage. Run a maintained full-history scanner without printing values; classify, purge, and rotate before publication if confirmed |
| `GQI-0002` | `OPEN` | P3 | portability/diagnostics | Windows `fingerprint_audio` stderr orientation | Frozen code calls wide then byte-oriented I/O on the same stream, which is undefined by C but needs supported MSVC/UCRT runtime confirmation. Capture exact success/failure diagnostics; if confirmed, admit/fix one consistent UTF-8 output orientation |
| `GQI-0003` | `OPEN` | P2 | correctness/data-integrity/malformed-media | HFS volume allocation and cross-fork ownership | The reader proves extent bounds but neither reads the volume allocation bitmap nor rejects physical overlap among admitted forks. Confirm strict-importer versus recovery-reader policy and supported-media sharing; if strict, promote one allocation/ownership finding with bitmap, overlap and known-media fixtures |
| `GQI-0004` | `OPEN` | P1 | security/filesystem-safety/privacy | Host-tar link and special-file extraction | Confirm Android host-tar behavior for links, devices and special entries under the actual fallback command and staging root; if exposed, admit one fail-closed entry-type/containment finding |

## Observation normalization provenance

Rows are append-only mappings from raw worker observations to canonical owners. They preserve duplicate and rejection decisions without allocating unnecessary finding IDs

| Coverage observation | Decision | Canonical owner or reason |
|---|---|---|
| `GQ1-PREFLIGHT-001-OBS-001` | `EXTENDS` | `GQF-0001` through `GQF-0004`, historical `BR-0001` |
| `GQ1-PREFLIGHT-001-OBS-002` | `EXTENDS` | Historical `BR-0001`; generated working output shares the publication-artifact root |
| `GQ1-PREFLIGHT-001-OBS-003` | `EXTENDS` | Historical `BR-0002` complete-branch reviewability root |
| `GQ1-PREFLIGHT-001-OBS-004` | `EXTENDS` | Historical `BR-0002`; detailed plan batches retain independent review |
| `GQ1-PREFLIGHT-001-OBS-005` | `DUPLICATE` | `GQF-0013`, historical `BR-0003` |
| `GQ1-PREFLIGHT-001-OBS-006` | `DUPLICATE` | Historical `BR-0073` |
| `GQ1-PREFLIGHT-001-OBS-007` | `DUPLICATE` | Historical open `BR-0007` |
| `GQ1-PREFLIGHT-001-OBS-008` | `REGROWTH` | New `GQF-0024`, linked to archived `BR-0157` |
| `GQ1-PREFLIGHT-001-OBS-009` | `EXTENDS` | `GQI-0001`, historical `INV-0001` |
| `GQ1-PREFLIGHT-002-O-001` | `EXTENDS` | `GQF-0005`, live `BR-0005` |
| `GQ1-PREFLIGHT-002-O-002` | `HISTORICAL-CLOSED` | Archived `BR-0004`; no frozen-head structural regrowth |
| `GQ1-PREFLIGHT-002-O-003` | `DUPLICATE` | Live `BR-0016`, `BR-0029`, `BR-0044`, `BR-0078`, and `BR-0080` |
| `GQ1-PREFLIGHT-002-O-004` | `EXTENDS` | `GQF-0018`, live `BR-0021` and `BR-0103` |
| `GQ1-PREFLIGHT-002-O-005` | `EXTENDS` | Live `BR-0077`; archived `BR-0076` is positive ownership evidence |
| `GQ1-PREFLIGHT-002-O-006` | `EXTENDS` | `GQF-0006`, `GQF-0007`, `GQF-0020`, and `GQF-0021` |
| `GQ1-PREFLIGHT-002-O-007` | `EXTENDS` | `GQF-0022`, `GQF-0023`, and live `BR-0082` |
| `GQ1-PREFLIGHT-002-O-008` | `EXTENDS` | `GQF-0014`, `GQF-0015`, and linked live server `BR-*` roots |
| `GQ1-PREFLIGHT-002-O-009` | `EXTENDS` | `GQF-0008`, `GQF-0009`, `GQF-0011`, `BR-0662`, and `BR-0671` |
| `GQ1-PREFLIGHT-003-OBS-001` | `EXTENDS` | Historical `BR-0002` reviewability root |
| `GQ1-PREFLIGHT-003-OBS-002` | `EXTENDS` | Historical `BR-0006` stale migrated-owner root |
| `GQ1-PREFLIGHT-003-OBS-003` | `NEW` | `GQF-0025` |
| `GQ1-PREFLIGHT-003-OBS-004` | `NEW` | `GQF-0026`; linked historical `BR-0480` as evidence, not owner |
| `GQ1-PREFLIGHT-003-OBS-005` | `NEW` | `GQF-0027` |
| `GQ1-PREFLIGHT-003-OBS-006` | `NEW` | `GQF-0028` |
| `GQ1-CHUNK-0001-OBS-001` | `DUPLICATE` | Historical open `BR-0007`; confirms `GQ1-PREFLIGHT-001-OBS-007` |
| `GQ1-CHUNK-0001-OBS-002` | `REJECT-STANDALONE` | ASCII cleanup note attached to the eventual `BR-0007` sample edit; no independent behavior impact |
| `GQ1-CHUNK-0002-OBS-001` | `REGROWTH` | New `GQF-0029`, linked archived `BR-0184` |
| `GQ1-CHUNK-0002-OBS-002` | `EXTENDS` | Open `BR-0011` |
| `GQ1-CHUNK-0002-OBS-003` | `EXTENDS` | Open `BR-0008` |
| `GQ1-CHUNK-0002-OBS-004` | `EXTENDS` | Open `BR-0188` |
| `GQ1-CHUNK-0002-OBS-005` | `EXTENDS` | Open `BR-0010` |
| `GQ1-CHUNK-0002-OBS-006` | `EXTENDS` | Open `BR-0012` |
| `GQ1-CHUNK-0003-OBS-001` | `EXTENDS` | Open `BR-0008` |
| `GQ1-CHUNK-0003-OBS-002` | `EXTENDS` | Open `BR-0010` |
| `GQ1-CHUNK-0003-OBS-003` | `NEW` | `GQF-0030` |
| `GQ1-CHUNK-0004-OBS-001` | `NEW` | `GQF-0031` |
| `GQ1-CHUNK-0004-OBS-002` | `EXTENDS` | Open `BR-0008` |
| `GQ1-CHUNK-0004-OBS-003` | `EXTENDS` | Open `BR-0188` |
| `GQ1-CHUNK-0004-OBS-004` | `EXTENDS` | Open `BR-0011` |
| `GQ1-CHUNK-0004-OBS-005` | `EXTENDS` | Open `BR-0010` |
| `GQ1-CHUNK-0004-OBS-006` | `EXTENDS` | Open `BR-0012` |
| `GQ1-CHUNK-0004-OBS-007` | `EXTENDS` | Open `BR-0009` |
| `GQ1-CHUNK-0005-OBS-001` | `EXTENDS` | `GQF-0031` |
| `GQ1-CHUNK-0005-OBS-002` | `EXTENDS` | Open `BR-0011` |
| `GQ1-CHUNK-0005-OBS-003` | `EXTENDS` | Open `BR-0008` |
| `GQ1-CHUNK-0005-OBS-004` | `EXTENDS` | Open `BR-0188` |
| `GQ1-CHUNK-0005-OBS-005` | `EXTENDS` | Open `BR-0010` |
| `GQ1-CHUNK-0005-OBS-006` | `EXTENDS` | Open `BR-0012` |
| `GQ1-CHUNK-0005-OBS-007` | `EXTENDS` | Open `BR-0009` |
| `GQ1-CHUNK-0005-OBS-008` | `EXTENDS` | Open `BR-0189` |
| `GQ1-CHUNK-0005-OBS-009` | `EXTENDS` | `GQF-0030` context |
| `GQ1-CHUNK-0006-OBS-001` | `EXTENDS` | Open `BR-0008` |
| `GQ1-CHUNK-0006-OBS-002` | `EXTENDS` | `GQF-0031` |
| `GQ1-CHUNK-0006-OBS-003` | `EXTENDS` | Open `BR-0188` |
| `GQ1-CHUNK-0006-OBS-004` | `EXTENDS` | Open `BR-0009` |
| `GQ1-CHUNK-0006-OBS-005` | `EXTENDS` | Open `BR-0010` |
| `GQ1-CHUNK-0006-OBS-006` | `EXTENDS` | Open `BR-0012` |
| `GQ1-CHUNK-0006-OBS-007` | `EXTENDS` | `GQF-0030` |
| `GQ1-CHUNK-0006-OBS-008` | `EXTENDS` | Open `BR-0619` |
| `GQ1-CHUNK-0007-OBS-001` | `NEW` | `GQF-0032` |
| `GQ1-CHUNK-0007-OBS-002` | `EXTENDS` | Open `BR-0187` |
| `GQ1-CHUNK-0007-OBS-003` | `NEW` | `GQF-0033` |
| `GQ1-CHUNK-0007-OBS-004` | `EXTENDS` | Open `BR-0008` |
| `GQ1-CHUNK-0007-OBS-005` | `EXTENDS` | Open `BR-0188` |
| `GQ1-CHUNK-0007-OBS-006` | `EXTENDS` | Open `BR-0010` |
| `GQ1-CHUNK-0007-OBS-007` | `EXTENDS` | Open `BR-0012` |
| `GQ1-CHUNK-0007-OBS-008` | `EXTENDS` | Open `BR-0009` |
| `GQ1-CHUNK-0008-OBS-001` | `REGROWTH` | New `GQF-0034`, linked archived `BR-0049` |
| `GQ1-CHUNK-0008-OBS-002` | `EXTENDS` | Broadened `GQF-0030` |
| `GQ1-CHUNK-0008-OBS-003` | `REGROWTH` | New `GQF-0035`, linked archived `BR-0413` |
| `GQ1-CHUNK-0008-OBS-004` | `EXTENDS` | Open `BR-0623` |
| `GQ1-CHUNK-0009-OBS-001` | `NEW` | `GQF-0036` |
| `GQ1-CHUNK-0009-OBS-002` | `REGROWTH` | New `GQF-0037`, linked archived `BR-0065` |
| `GQ1-CHUNK-0009-OBS-003` | `EXTENDS` | Open `BR-0084` |
| `GQ1-CHUNK-0010-OBS-001` | `NEW` | `GQF-0038` |
| `GQ1-CHUNK-0010-OBS-002` | `NEW` | `GQF-0039` |
| `GQ1-CHUNK-0010-OBS-003` | `NEW` | `GQF-0040` |
| `GQ1-CHUNK-0010-OBS-004` | `EXTENDS` | Open `BR-0021` |
| `GQ1-CHUNK-0011-OBS-001` | `EXTENDS` | `GQF-0038` |
| `GQ1-CHUNK-0011-OBS-002` | `EXTENDS` | `GQF-0040`, archived `BR-0072` |
| `GQ1-CHUNK-0011-OBS-003` | `NEW` | `GQF-0041` |
| `GQ1-CHUNK-0011-OBS-004` | `NEW` | `GQF-0042` |
| `GQ1-CHUNK-0011-OBS-005` | `EXTENDS` | Open `BR-0021` |
| `GQ1-CHUNK-0011-OBS-006` | `NEW` | `GQF-0043` |
| `GQ1-CHUNK-0012-OBS-001` | `NEW` | `GQF-0044` |
| `GQ1-CHUNK-0012-OBS-002` | `NEW` | `GQF-0045` |
| `GQ1-CHUNK-0012-OBS-003` | `EXTENDS` | Open `BR-0021` |
| `GQ1-CHUNK-0012-OBS-004` | `REGROWTH` | New shared `GQF-0046`, linked archived `BR-0020` |
| `GQ1-CHUNK-0012-OBS-005` | `DUPLICATE` | Open `BR-0070` |
| `GQ1-CHUNK-0013-OBS-001` | `REGROWTH` | New `GQF-0047`, linked archived `BR-0061` |
| `GQ1-CHUNK-0013-OBS-002` | `NEW` | `GQF-0048` |
| `GQ1-CHUNK-0013-OBS-003` | `NEW` | `GQF-0049` |
| `GQ1-CHUNK-0013-OBS-004` | `NEW` | `GQF-0050` |
| `GQ1-CHUNK-0013-OBS-005` | `EXTENDS` | Open `BR-0021` |
| `GQ1-CHUNK-0013-OBS-006` | `EXTENDS` | Shared `GQF-0046`, linked archived `BR-0020` |
| `GQ1-CHUNK-0014-OBS-001` | `EXTENDS` | `GQF-0038` |
| `GQ1-CHUNK-0014-OBS-002` | `EXTENDS` | `GQF-0039`, `GQF-0042` |
| `GQ1-CHUNK-0014-OBS-003` | `EXTENDS` | `GQF-0040` |
| `GQ1-CHUNK-0014-OBS-004` | `EXTENDS` | Open `BR-0021` |
| `GQ1-CHUNK-0015-OBS-001` | `EXTENDS` | Open `BR-0029` |
| `GQ1-CHUNK-0015-OBS-002` | `EXTENDS` | Open `BR-0044`, `BR-0078` |
| `GQ1-CHUNK-0015-OBS-003` | `REGROWTH` | Broadened `GQF-0037`, linked archived `BR-0065` |
| `GQ1-CHUNK-0015-OBS-004` | `NEW` | `GQF-0052` |
| `GQ1-CHUNK-0015-OBS-005` | `EXTENDS` | Open `BR-0016` |
| `GQ1-CHUNK-0015-OBS-006` | `EXTENDS` | Open `BR-0276` |
| `GQ1-CHUNK-0016-OBS-001` | `REGROWTH` | New `GQF-0051`, linked archived `BR-0018` |
| `GQ1-CHUNK-0016-OBS-002` | `EXTENDS` | `GQF-0038` |
| `GQ1-CHUNK-0016-OBS-003` | `EXTENDS` | `GQF-0040` |
| `GQ1-CHUNK-0016-OBS-004` | `EXTENDS` | `GQF-0046`, archived `BR-0020` |
| `GQ1-CHUNK-0016-OBS-005` | `EXTENDS` | Open `BR-0021` |
| `GQ1-CHUNK-0016-OBS-006` | `DUPLICATE` | Historical `BR-0073` |
| `GQ1-CHUNK-0017-OBS-001` | `REGROWTH` | New `GQF-0053`, adjacent archived `BR-0069` |
| `GQ1-CHUNK-0017-OBS-002` | `EXTENDS` | `GQF-0038` |
| `GQ1-CHUNK-0017-OBS-003` | `EXTENDS` | Open `BR-0021` |
| `GQ1-CHUNK-0017-OBS-004` | `EXTENDS` | `GQF-0052` |
| `GQ1-CHUNK-0017-OBS-005` | `DUPLICATE` | Open `BR-0070` |
| `GQ1-CHUNK-0017-OBS-006` | `NEW` | `GQF-0054` |
| `GQ1-CHUNK-0018-OBS-001` | `REGROWTH` | New `GQF-0055`, linked archived `BR-0225` |
| `GQ1-CHUNK-0018-OBS-002` | `NEW` | `GQF-0056` |
| `GQ1-CHUNK-0018-OBS-003` | `NEW` | `GQF-0057`, related but not duplicate `BR-0497` |
| `GQ1-CHUNK-0019-OBS-001` | `REGROWTH` | New `GQF-0058`, linked archived `BR-0048` |
| `GQ1-CHUNK-0019-OBS-002` | `EXTENDS` | `GQF-0037` |
| `GQ1-CHUNK-0019-OBS-003` | `EXTENDS` | `GQF-0052` |
| `GQ1-CHUNK-0019-OBS-004` | `EXTENDS` | Open `BR-0016` |
| `GQ1-CHUNK-0020-OBS-001` | `NEW` | `GQF-0059` |
| `GQ1-CHUNK-0020-OBS-002` | `REGROWTH` | New `GQF-0060`, linked archived `BR-0040` |
| `GQ1-CHUNK-0020-OBS-003` | `EXTENDS` | Archived `BR-0041` |
| `GQ1-CHUNK-0021-OBS-001` | `REGROWTH` | New `GQF-0061`, linked archived `BR-0018` |
| `GQ1-CHUNK-0021-OBS-002` | `NEW` | `GQF-0062` |
| `GQ1-CHUNK-0021-OBS-003` | `EXTENDS` | `GQF-0038` |
| `GQ1-CHUNK-0021-OBS-004` | `EXTENDS` | `GQF-0046`, archived `BR-0020` |
| `GQ1-CHUNK-0021-OBS-005` | `EXTENDS` | `GQF-0054` |
| `GQ1-CHUNK-0021-OBS-006` | `EXTENDS` | Open `BR-0169` |
| `GQ1-CHUNK-0022-OBS-001` | `NEW` | `GQF-0063` |
| `GQ1-CHUNK-0022-OBS-002` | `NEW` | `GQF-0064` |
| `GQ1-CHUNK-0022-OBS-003` | `NEW` | `GQF-0065`; archived `BR-0013` fixed capacity only |
| `GQ1-CHUNK-0022-OBS-004` | `NEW` | `GQF-0066` |
| `GQ1-CHUNK-0023-OBS-001` | `EXTENDS` | `GQF-0037`, `GQF-0052` |
| `GQ1-CHUNK-0023-OBS-002` | `EXTENDS` | Open `BR-0044`, `GQF-0037`, `BR-0078` |
| `GQ1-CHUNK-0023-OBS-003` | `EXTENDS` | `GQF-0052` |
| `GQ1-CHUNK-0023-OBS-004` | `EXTENDS` | `GQF-0057`, `GQF-0052` |
| `GQ1-CHUNK-0023-OBS-005` | `EXTENDS` | Open `BR-0078` |
| `GQ1-CHUNK-0024-OBS-001` | `EXTENDS` | `GQF-0053`, archived `BR-0008` |
| `GQ1-CHUNK-0024-OBS-002` | `EXTENDS` | `GQF-0038` |
| `GQ1-CHUNK-0024-OBS-003` | `EXTENDS` | Open `BR-0169` |
| `GQ1-CHUNK-0024-OBS-004` | `EXTENDS` | `GQF-0058` |
| `GQ1-CHUNK-0024-OBS-005` | `DUPLICATE` | Named owners in raw report; no independent root |
| `GQ1-CHUNK-0025-OBS-001` | `EXTENDS` | `GQF-0058` |
| `GQ1-CHUNK-0025-OBS-002` | `NEW` | `GQF-0067` |
| `GQ1-CHUNK-0025-OBS-003` | `EXTENDS` | `GQF-0054` |
| `GQ1-CHUNK-0025-OBS-004` | `NEW` | `GQF-0068` |
| `GQ1-CHUNK-0025-OBS-005` | `INVESTIGATE` | `GQI-0002` |
| `GQ1-CHUNK-0026-OBS-001` | `NEW` | `GQF-0069` |
| `GQ1-CHUNK-0026-OBS-002` | `NEW` | `GQF-0070` |
| `GQ1-CHUNK-0026-OBS-003` | `EXTENDS` | `GQF-0058` |
| `GQ1-CHUNK-0026-OBS-004` | `EXTENDS` | `GQF-0054` |
| `GQ1-CHUNK-0026-OBS-005` | `EXTENDS` | `GQF-0063`, `GQF-0067` |
| `GQ1-CHUNK-0026-OBS-006` | `EXTENDS` | `GQF-0030`, `GQF-0065` |
| `GQ1-CHUNK-0027-OBS-001` | `NEW` | `GQF-0071`; extends downstream `GQF-0040` evidence |
| `GQ1-CHUNK-0027-OBS-002` | `INVESTIGATE` | `GQI-0003`; later contiguous review narrowed policy confidence |
| `GQ1-CHUNK-0027-OBS-003` | `REGROWTH` | New `GQF-0072`, linked archived `BR-0018` and open `BR-0021` |
| `GQ1-CHUNK-0027-OBS-004` | `EXTENDS` | Archived `BR-0051` |
| `GQ1-CHUNK-0028-OBS-001` | `NEW` | `GQF-0073`, linked archived `BR-0050` |
| `GQ1-CHUNK-0028-OBS-002` | `NEW` | `GQF-0074`, linked archived `BR-0026` |
| `GQ1-CHUNK-0028-OBS-003` | `INVESTIGATE` | Corroborates `GQI-0003`; no duplicate ID |
| `GQ1-CHUNK-0028-OBS-004` | `EXTENDS` | Archived `BR-0050` |
| `GQ1-CHUNK-0028-OBS-005` | `EXTENDS` | `GQF-0040`; `GQF-0071` owns the independent topology trigger |
| `GQ1-CHUNK-0029-OBS-001` | `REGROWTH` | New `GQF-0075`, linked archived `BR-0052` |
| `GQ1-CHUNK-0030-OBS-001` | `REGROWTH` | New `GQF-0076`, linked archived `BR-0018` and adjacent `GQF-0051` |
| `GQ1-CHUNK-0030-OBS-002` | `REGROWTH` | New `GQF-0077`, linked archived `BR-0059` |
| `GQ1-CHUNK-0030-OBS-003` | `NEW` | `GQF-0078` |
| `GQ1-CHUNK-0031-OBS-001` | `REGROWTH` | New `GQF-0079`, linked archived `BR-0057` |
| `GQ1-CHUNK-0031-OBS-002` | `REGROWTH` | New `GQF-0080`, linked archived `BR-0054` |
| `GQ1-CHUNK-0031-OBS-003` | `EXTENDS` | `GQF-0078` |
| `GQ1-CHUNK-0032-OBS-001` | `NEW` | `GQF-0081` |
| `GQ1-CHUNK-0032-OBS-002` | `NEW` | `GQF-0082` |
| `GQ1-CHUNK-0032-OBS-003` | `REGROWTH` | New `GQF-0083`, linked archived `BR-0018` |
| `GQ1-CHUNK-0032-OBS-004` | `EXTENDS` | `GQF-0076` |
| `GQ1-CHUNK-0032-OBS-005` | `DUPLICATE` | Open `BR-0021` |
| `GQ1-CHUNK-0033-OBS-001` | `EXTENDS` | `GQF-0083` |
| `GQ1-CHUNK-0033-OBS-002` | `REGROWTH` | New `GQF-0084`, linked archived `BR-0020`, `BR-0034` |
| `GQ1-CHUNK-0033-OBS-003` | `EXTENDS` | Open `BR-0021` |
| `GQ1-CHUNK-0034-OBS-001` | `EXTENDS` | Open `BR-0021` |
| `GQ1-CHUNK-0034-OBS-002` | `EXTENDS` | `GQF-0084` |
| `GQ1-CHUNK-0034-OBS-003` | `NEW` | `GQF-0085` |
| `GQ1-CHUNK-0034-OBS-004` | `EXTENDS` | `GQF-0080`, archived `BR-0054` |
| `GQ1-CHUNK-0034-OBS-005` | `NEW` | `GQF-0086` |
| `GQ1-CHUNK-0034-OBS-006` | `EXTENDS` | `GQF-0083` |
| `GQ1-CHUNK-0035-OBS-001` | `EXTENDS` | Open `BR-0021` |
| `GQ1-CHUNK-0035-OBS-002` | `EXTENDS` | `GQF-0080` |
| `GQ1-CHUNK-0035-OBS-003` | `EXTENDS` | `GQF-0079`, `GQF-0081` |
| `GQ1-CHUNK-0035-OBS-004` | `EXTENDS` | `GQF-0053` |
| `GQ1-CHUNK-0036-OBS-001` | `NEW` | `GQF-0087` |
| `GQ1-CHUNK-0036-OBS-002` | `NEW` | `GQF-0088` |
| `GQ1-CHUNK-0036-OBS-003` | `EXTENDS` | `GQF-0049` |
| `GQ1-CHUNK-0036-OBS-004` | `EXTENDS` | `GQF-0050` |
| `GQ1-CHUNK-0036-OBS-005` | `EXTENDS` | `GQF-0048` |
| `GQ1-CHUNK-0036-OBS-006` | `EXTENDS` | `GQF-0047` |
| `GQ1-CHUNK-0037-OBS-001` | `REGROWTH` | New `GQF-0089`, linked archived `BR-0065` |
| `GQ1-CHUNK-0037-OBS-002` | `EXTENDS` | `GQF-0052` |
| `GQ1-CHUNK-0037-OBS-003` | `EXTENDS` | Open `BR-0021` |
| `GQ1-CHUNK-0037-OBS-004` | `EXTENDS` | `GQF-0063` |
| `GQ1-CHUNK-0037-OBS-005` | `EXTENDS` | `GQF-0066` |
| `GQ1-CHUNK-0037-OBS-006` | `DUPLICATE` | Archived SOW scanner source-manifest owner |
| `GQ1-CHUNK-0038-OBS-001` | `REGROWTH` | New `GQF-0090`, linked archived `BR-0068` |
| `GQ1-CHUNK-0038-OBS-002` | `REGROWTH` | New `GQF-0091`, linked archived `BR-0069` |
| `GQ1-CHUNK-0038-OBS-003` | `EXTENDS` | `GQF-0044`, `GQF-0045`, `GQF-0046`, `BR-0021`, `BR-0070` |
| `GQ1-CHUNK-0039-OBS-001` | `REGROWTH` | Archived `BR-0072`; extends `GQF-0067` |
| `GQ1-CHUNK-0039-OBS-002` | `EXTENDS` | `GQF-0070` |
| `GQ1-CHUNK-0039-OBS-003` | `EXTENDS` | `GQF-0042` |
| `GQ1-CHUNK-0039-OBS-004` | `NEW` | `GQF-0092` |
| `GQ1-CHUNK-0040-OBS-001` | `EXTENDS` | `GQF-0038` |
| `GQ1-CHUNK-0040-OBS-002` | `EXTENDS` | `GQF-0040` |
| `GQ1-CHUNK-0040-OBS-003` | `HISTORICAL-CLOSED` | Archived `BR-0032` |
| `GQ1-CHUNK-0040-OBS-004` | `DUPLICATE` | Archived `BR-0073` |
| `GQ1-CHUNK-0041-OBS-001` | `REGROWTH` | New `GQF-0093`, linked archived `BR-0158` |
| `GQ1-CHUNK-0041-OBS-002` | `HISTORICAL-CLOSED` | Archived `BR-0178` |
| `GQ1-CHUNK-0041-OBS-003` | `HISTORICAL-CLOSED` | Archived `BR-0033` |
| `GQ1-CHUNK-0041-OBS-004` | `DUPLICATE` | Archived `BR-0073` |
| `GQ1-CHUNK-0042-OBS-001` | `DUPLICATE` | Archived `BR-0073` |
| `GQ1-CHUNK-0042-OBS-002` | `DUPLICATE` | Current `GQF-0093`, linked archived `BR-0158` |
| `GQ1-CHUNK-0042-OBS-003` | `HISTORICAL-CLOSED` | Archived `BR-0178` |
| `GQ1-CHUNK-0042-OBS-004` | `HISTORICAL-CLOSED` | Archived `BR-0033` |
| `GQ1-CHUNK-0043-OBS-001` | `NEW` | `GQF-0094` |
| `GQ1-CHUNK-0043-OBS-002` | `EXTENDS` | `GQF-0051` |
| `GQ1-CHUNK-0043-OBS-003` | `HISTORICAL-CLOSED` | Archived `BR-0033` |
| `GQ1-CHUNK-0043-OBS-004` | `EXTENDS` | `GQF-0046` |
| `GQ1-CHUNK-0043-OBS-005` | `EXTENDS` | `GQF-0038` |
| `GQ1-CHUNK-0043-OBS-006` | `EXTENDS` | `GQF-0040` |
| `GQ1-CHUNK-0043-OBS-007` | `EXTENDS` | Open `BR-0021` |
| `GQ1-CHUNK-0044-OBS-001` | `EXTENDS` | `GQF-0051`, archived `BR-0018` |
| `GQ1-CHUNK-0044-OBS-002` | `EXTENDS` | `GQF-0038` |
| `GQ1-CHUNK-0044-OBS-003` | `EXTENDS` | `GQF-0039` |
| `GQ1-CHUNK-0044-OBS-004` | `EXTENDS` | `GQF-0040` |
| `GQ1-CHUNK-0044-OBS-005` | `EXTENDS` | `GQF-0046`, open `BR-0021` |
| `GQ1-CHUNK-0044-OBS-006` | `DUPLICATE` | Archived `BR-0182` |
| `GQ1-CHUNK-0044-OBS-007` | `DUPLICATE` | Archived `BR-0073` |
| `GQ1-CHUNK-0045-OBS-001` | `NEW` | `GQF-0095`; archived `BR-0076`, `BR-0065` remain closed |
| `GQ1-CHUNK-0046-OBS-001` | `EXTENDS` | Open `BR-0077` |
| `GQ1-CHUNK-0047-OBS-001` | `EXTENDS` | Open `BR-0077`; archived `BR-0065`, `BR-0076` remain closed |
| `GQ1-CHUNK-0048-OBS-001` | `EXTENDS` | Open `BR-0078` |
| `GQ1-CHUNK-0048-OBS-002` | `EXTENDS` | Open `BR-0029` |
| `GQ1-CHUNK-0048-OBS-003` | `EXTENDS` | `GQF-0052`; archived `BR-0065` remains closed |
| `GQ1-CHUNK-0049` | `EXTENDS` | `BR-0029`, `BR-0254`, `BR-0080`, `BR-0257`; duplicates `BR-0223`; `BR-0065`, `BR-0079` remain closed |
| `GQ1-CHUNK-0050-OBS-001` | `EXTENDS` | Open `BR-0029` |
| `GQ1-CHUNK-0050-OBS-002` | `EXTENDS` | `GQF-0089`, archived `BR-0065`; `BR-0081`, `BR-0079`, `BR-0091` remain closed |
| `GQ1-CHUNK-0051-OBS-001` | `REGROWTH` | New `GQF-0096`, linked archived `BR-0083`, open `BR-0338` |
| `GQ1-CHUNK-0051-OBS-002` | `REGROWTH` | New `GQF-0097`, linked archived `BR-0444` |
| `GQ1-CHUNK-0051-OBS-003` | `EXTENDS` | Open `BR-0082` |
| `GQ1-CHUNK-0052-OBS-001` | `EXTENDS` | Open `BR-0417` |
| `GQ1-CHUNK-0052-OBS-002` | `NEW` | `GQF-0098` |
| `GQ1-CHUNK-0053-OBS-001` | `REGROWTH` | New `GQF-0099`, linked archived `BR-0085` |
| `GQ1-CHUNK-0053-OBS-002` | `EXTENDS` | `GQF-0036` |
| `GQ1-CHUNK-0054-OBS-001` | `REGROWTH` | New `GQF-0100`, linked `BR-0093`, `BR-0018` |
| `GQ1-CHUNK-0054-OBS-002` | `REGROWTH` | New `GQF-0101`, linked `BR-0093` |
| `GQ1-CHUNK-0054-OBS-003` | `NEW` | `GQF-0102` |
| `GQ1-CHUNK-0054-OBS-004` | `REGROWTH` | New `GQF-0103`, linked `BR-0018` |
| `GQ1-CHUNK-0054-OBS-005` | `REGROWTH` | New `GQF-0104`, linked `BR-0018` |
| `GQ1-CHUNK-0055-OBS-001` | `NEW` | `GQF-0105` |
| `GQ1-CHUNK-0055-OBS-002` | `EXTENDS` | Archived `BR-0089` |
| `GQ1-CHUNK-0055-OBS-003` | `REGROWTH` | `GQF-0106`, linked archived `BR-0091` |
| `GQ1-CHUNK-0055-OBS-004` | `NEW` | `GQF-0107` |
| `GQ1-CHUNK-0056-OBS-001` | `REGROWTH` | `GQF-0108`, linked `BR-0018` |
| `GQ1-CHUNK-0056-OBS-002` | `REGROWTH` | `GQF-0109`, linked `BR-0018` |
| `GQ1-CHUNK-0056-OBS-003` | `NEW` | `GQF-0110` |
| `GQ1-CHUNK-0056-OBS-004` | `INVESTIGATE` | `GQI-0004` |
| `GQ1-CHUNK-0057-OBS-001` | `REGROWTH` | `GQF-0111`, linked `BR-0096` |
| `GQ1-CHUNK-0057-OBS-002` | `REGROWTH` | `GQF-0112`, linked `BR-0096` |
| `GQ1-CHUNK-0057-OBS-003` | `REGROWTH` | `GQF-0113`, linked `BR-0105` |
| `GQ1-CHUNK-0057-OBS-004` | `NEW` | `GQF-0114` |
| `GQ1-CHUNK-0058` | `EXTENDS` | `GQF-0030`, `BR-0099`, `GQF-0034`, `BR-0101` |
| `GQ1-CHUNK-0059-OBS-001` | `EXTENDS` | Open `BR-0103` |
| `GQ1-CHUNK-0059-OBS-002` | `REGROWTH` | `GQF-0115`, linked `BR-0102` |
| `GQ1-CHUNK-0059-OBS-003` | `REGROWTH` | `GQF-0116`, linked `BR-0444` |
| `GQ1-CHUNK-0060-OBS-001` | `NEW` | `GQF-0117` |
| `GQ1-CHUNK-0061-OBS-001` | `REGROWTH` | `GQF-0118`, linked `BR-0018` |
| `GQ1-CHUNK-0061-OBS-002` | `EXTENDS` | `GQF-0115`, archived `BR-0088` |
| `GQ1-CHUNK-0061-OBS-003` | `EXTENDS` | Archived `BR-0185` |
| `GQ1-CHUNK-0062-OBS-001` | `REGROWTH` | `GQF-0119`, linked `BR-0018` |
| `GQ1-CHUNK-0062-OBS-002` | `NEW` | `GQF-0120` |
| `GQ1-CHUNK-0063-OBS-001` | `DUPLICATE` | Open `BR-0090` |
| `GQ1-CHUNK-0064-HIST-001` | `HISTORICAL-CLOSED` | Archived `BR-0414`; explicit clean coverage |
| `GQ1-CHUNK-0065-OBS-001` | `EXTENDS` | `BR-0106`, `GQF-0014` |
| `GQ1-CHUNK-0065-OBS-002` | `EXTENDS` | `GQF-0014`, `BR-0107` |
| `GQ1-CHUNK-0065-OBS-003` | `EXTENDS` | `GQF-0014`, `BR-0108` |
| `GQ1-CHUNK-0066` | `DUPLICATE` | `BR-0149`, `BR-0136`, `BR-0128`, `BR-0108`/`GQF-0014`, `BR-0405` |
| `GQ1-CHUNK-0067` | `DUPLICATE` | `BR-0113`, `BR-0143`, `BR-0115`, `BR-0116`, `GQF-0015`, `BR-0119`, `BR-0120`, `BR-0155`, `BR-0090` |
| `GQ1-CHUNK-0068` | `DUPLICATE/EXTENDS` | Duplicates `BR-0122`, `BR-0121`, `BR-0150`, `BR-0123`, `BR-0124`, `BR-0126`, `BR-0139`, `BR-0125`; extends `BR-0135`, `BR-0125` |
| `GQ1-CHUNK-0069` | `DUPLICATE` | `BR-0127` through `BR-0131` |
| `GQ1-CHUNK-0070` | `DUPLICATE` | `BR-0114`, `BR-0117`, `BR-0118`, `BR-0132` through `BR-0135` |
| `GQ1-CHUNK-0071` | `DUPLICATE/EXTENDS` | Duplicates `BR-0137` through `BR-0142`; extends `BR-0113`, `BR-0120` through `BR-0122` |
| `GQ1-CHUNK-0072-OBS-001` | `EXTENDS` | `BR-0109` |
| `GQ1-CHUNK-0072-OBS-002` | `DUPLICATE` | `BR-0110` |
| `GQ1-CHUNK-0072-OBS-003` | `DUPLICATE` | `BR-0111` |
| `GQ1-CHUNK-0072-OBS-004` | `EXTENDS` | `BR-0112` |
| `GQ1-CHUNK-0072-OBS-005` | `NEW` | `GQF-0121` |
| `GQ1-CHUNK-0073` | `DUPLICATE` | `GQF-0015`, `BR-0113`, `BR-0115`, `BR-0117`, `BR-0119`, `BR-0125`, `BR-0127`, `BR-0137`, `BR-0141`, `BR-0143` through `BR-0147` |
| `GQ1-CHUNK-0074-OBS-001` | `EXTENDS` | `BR-0148`, `BR-0406` |
| `GQ1-CHUNK-0074-OBS-002` | `NEW` | `GQF-0122` |
| `GQ1-CHUNK-0075` | `DUPLICATE` | `BR-0151`, `BR-0129`, `BR-0150`, `BR-0152`, `BR-0133`, `BR-0135`, `BR-0115` |
| `GQ1-CHUNK-0076` | `DUPLICATE` | `BR-0116`, `GQF-0015`, `BR-0148`, `BR-0153` through `BR-0155`, `BR-0120`, `BR-0150` |
| `GQ1-CHUNK-0077` | `DUPLICATE/EXTENDS` | Seven existing WebSocket/server owners; exact details retained in durable evidence ledger |
| `GQ1-CHUNK-0078` | `HISTORICAL-CLOSED` | Clean ignore-policy coverage; reconciled `BR-0160`, `BR-0001`, `BR-0002` |
| `GQ1-CHUNK-0079-OBS-001` | `NEW` | `GQF-0123` |
| `GQ1-CHUNK-0079-OBS-002` | `EXTENDS` | `GQF-0024`, linked archived `BR-0157` |
| `GQ1-CHUNK-0079-OBS-003` | `EXTENDS` | `GQF-0068` |
| `GQ1-CHUNK-0079-OBS-004` | `EXTENDS` | `BR-0158` |
| `GQ1-CHUNK-0079-OBS-005` | `DUPLICATE` | `BR-0182` |
| `GQ1-CHUNK-0080-OBS-001` | `EXTENDS` | `BR-0160` |
| `GQ1-CHUNK-0080-OBS-002` | `EXTENDS` | `BR-0236` |
| `GQ1-CHUNK-0081-OBS-001` | `EXTENDS` | `BR-0174` |
| `GQ1-CHUNK-0081-OBS-002` | `EXTENDS` | `BR-0159`; remediation evidence, no new owner |
| `GQ1-CHUNK-0082-OBS-001` | `REGROWTH` | `GQF-0124`, linked archived `BR-0161` |
| `GQ1-CHUNK-0082-OBS-002` | `DUPLICATE` | `BR-0162` |
| `GQ1-CHUNK-0082-OBS-003` | `DUPLICATE` | `BR-0164` |
| `GQ1-CHUNK-0082-OBS-004` | `DUPLICATE` | `BR-0167`, `BR-0168` |
| `GQ1-CHUNK-0083-OBS-001` | `REGROWTH` | `GQF-0125`, linked archived `BR-0157` |
| `GQ1-CHUNK-0083-OBS-002` | `EXTENDS` | `GQF-0072`, linked archived `BR-0018` |
| `GQ1-CHUNK-0083-OBS-003` | `EXTENDS` | `GQF-0040` |
| `GQ1-CHUNK-0083-OBS-004` | `NEW` | `GQF-0126` |
| `GQ1-CHUNK-0084-OBS-001` | `REGROWTH` | `GQF-0127`, linked archived `BR-0018` |
| `GQ1-CHUNK-0084-OBS-002` | `REGROWTH` | `GQF-0128`, linked archived `BR-0018` |
| `GQ1-CHUNK-0084-OBS-003` | `EXTENDS` | `BR-0160` |
| `GQ1-CHUNK-0084-OBS-004` | `DUPLICATE` | `BR-0007` |
| `GQ1-CHUNK-0084-OBS-005` | `NEW` | `GQF-0129` |
| `GQ1-CHUNK-0085-OBS-001` | `REGROWTH` | Archived `BR-0161` |
| `GQ1-CHUNK-0085-OBS-002` | `DUPLICATE/EXTENDS` | `BR-0165` |
| `GQ1-CHUNK-0085-OBS-003` | `DUPLICATE` | `BR-0167` |
| `GQ1-CHUNK-0085-OBS-004` | `DUPLICATE` | `BR-0168` |
| `GQ1-CHUNK-0086-OBS-001` | `NEW` | `GQF-0130` |
| `GQ1-CHUNK-0086-OBS-002` | `EXTENDS` | `BR-0169` |
| `GQ1-CHUNK-0086-OBS-003` | `DUPLICATE` | `GQF-0016`, `BR-0171` |
| `GQ1-CHUNK-0087-OBS-001` | `EXTENDS` | `BR-0169` |
| `GQ1-CHUNK-0087-OBS-002` | `REGROWTH` | `GQF-0131`, linked archived `BR-0020` |
| `GQ1-CHUNK-0087-OBS-003` | `EXTENDS` | `BR-0173` |
| `GQ1-CHUNK-0087-OBS-004` | `DUPLICATE` | `BR-0174` |
| `GQ1-CHUNK-0087-OBS-005` | `EXTENDS` | `BR-0175` |
| `GQ1-CHUNK-0088-OBS-001` | `REGROWTH` | `GQF-0132`, linked archived `BR-0184` |
| `GQ1-CHUNK-0088-OBS-002` | `EXTENDS` | `GQF-0063` |
| `GQ1-CHUNK-0088-OBS-003` | `REGROWTH` | `GQF-0133`, linked archived `BR-0183` |
| `GQ1-CHUNK-0088-OBS-004` | `EXTENDS` | `GQF-0069` |
| `GQ1-CHUNK-0088-OBS-005` | `EXTENDS` | `GQF-0061`, linked archived `BR-0018` |
| `GQ1-CHUNK-0088-OBS-006` | `REGROWTH` | `GQF-0134`, linked archived `BR-0170` |
| `GQ1-CHUNK-0088-OBS-007` | `DUPLICATE` | `BR-0173` |
| `GQ1-CHUNK-0088-OBS-008` | `EXTENDS` | `GQF-0072` |
| `GQ1-CHUNK-0088-OBS-009` | `EXTENDS` | `GQF-0040` |
| `GQ1-CHUNK-0088-OBS-010` | `DUPLICATE` | `BR-0174` |
| `GQ1-CHUNK-0088-OBS-011` | `EXTENDS` | `GQF-0127` |
| `GQ1-CHUNK-0089-OBS-001` | `REGROWTH` | `GQF-0135`, linked archived `BR-0018` |
| `GQ1-CHUNK-0089-OBS-002` | `EXTENDS` | `BR-0173` |
| `GQ1-CHUNK-0089-OBS-003` | `EXTENDS` | `BR-0169` |
| `GQ1-CHUNK-0089-OBS-004` | `DUPLICATE` | `BR-0174` |
| `GQ1-CHUNK-0089-OBS-005` | `REGROWTH` | `GQF-0136`, linked archived `BR-0170` |
| `GQ1-CHUNK-0089-OBS-006` | `EXTENDS` | `BR-0233` |
| `GQ1-CHUNK-0090-OBS-001` | `REGROWTH` | `GQF-0137`, linked archived `BR-0018` |
| `GQ1-CHUNK-0090-OBS-002` | `EXTENDS` | `GQF-0128` |
| `GQ1-CHUNK-0090-OBS-003` | `NEW` | `GQF-0138` |
| `GQ1-CHUNK-0091-OBS-001` | `NEW` | `GQF-0139` |
| `GQ1-CHUNK-0091-OBS-002` | `NEW` | `GQF-0140` |
| `GQ1-CHUNK-0091-OBS-003` | `EXTENDS` | `BR-0180`, linked `GQF-0130` |
| `GQ1-CHUNK-0091-OBS-004` | `DUPLICATE` | `BR-0179` |
| `GQ1-CHUNK-0091-OBS-005` | `DUPLICATE` | `BR-0174` |
| `GQ1-CHUNK-0092-OBS-001` | `EXTENDS` | `GQF-0078` |
| `GQ1-CHUNK-0092-OBS-002` | `EXTENDS` | `GQF-0018` |
| `GQ1-CHUNK-0093-OBS-001` | `EXTENDS` | `BR-0158` |
| `GQ1-CHUNK-0093-OBS-002` | `EXTENDS` | `BR-0182` |
| `GQ1-CHUNK-0093-OBS-003` | `EXTENDS` | `GQF-0037`, `GQR-0024` |
| `GQ1-CHUNK-0093-OBS-004` | `DUPLICATE` | `BR-0597` |
| `GQ1-CHUNK-0094-OBS-001` | `REGROWTH` | `GQF-0141`, linked archived `BR-0200` |
| `GQ1-CHUNK-0094-OBS-002` | `NEW` | `GQF-0142` |
| `GQ1-CHUNK-0094-OBS-003` | `EXTENDS` | `GQF-0075` |
| `GQ1-CHUNK-0094-OBS-004` | `DUPLICATE` | `BR-0158` |
| `GQ1-CHUNK-0095-OBS-001` | `NEW` | `GQF-0143` |
| `GQ1-CHUNK-0095-OBS-002` | `EXTENDS` | `BR-0160` |
| `GQ1-CHUNK-0095-OBS-003` | `EXTENDS` | `GQF-0061`, `GQF-0062`, linked archived `BR-0018` |
| `GQ1-CHUNK-0096-OBS-001` | `EXTENDS` | `GQF-0090` |
| `GQ1-CHUNK-0096-OBS-002` | `EXTENDS` | `GQF-0091`, `GQF-0046` |
| `GQ1-CHUNK-0096-OBS-003` | `HISTORICAL-CLOSED` | Archived `BR-0024` |
| `GQ1-CHUNK-0097-OBS-001` | `NEW` | `GQF-0144`, linked archived `BR-0198` |
| `GQ1-CHUNK-0097-OBS-002` | `EXTENDS` | `GQF-0054` |
| `GQ1-CHUNK-0097-OBS-003` | `EXTENDS` | `BR-0236` |
| `GQ1-CHUNK-0098-OBS-001` | `REGROWTH` | `GQF-0145`, linked archived `BR-0038` |
| `GQ1-CHUNK-0098-OBS-002` | `EXTENDS` | `BR-0046`, `GQF-0069` |
| `GQ1-CHUNK-0099-OBS-001` | `EXTENDS` | `BR-0160` |
| `GQ1-CHUNK-0100-OBS-001` | `EXTENDS` | `BR-0160` |
| `GQ1-CHUNK-0100-OBS-002` | `NEW` | `GQF-0146` |
| `GQ1-CHUNK-0101-OBS-001` | `REGROWTH` | `GQF-0147`, linked archived `BR-0061` |
| `GQ1-CHUNK-0101-OBS-002` | `EXTENDS` | `BR-0160` |
| `GQ1-CHUNK-0102-OBS-001` | `NEW` | `GQF-0148`, linked `GQF-0094` |
| `GQ1-CHUNK-0102-OBS-002` | `EXTENDS` | `GQF-0046`, linked archived `BR-0020` |
| `GQ1-CHUNK-0102-OBS-003` | `EXTENDS` | `BR-0160` |
| `GQ1-CHUNK-0103-OBS-001` | `EXTENDS` | `GQF-0063` |
| `GQ1-CHUNK-0103-OBS-002` | `EXTENDS` | `BR-0160` |
| `GQ1-CHUNK-0104-OBS-001` | `EXTENDS` | `BR-0181` |
| `GQ1-CHUNK-0104-OBS-002` | `NEW` | `GQF-0149` |
| `GQ1-CHUNK-0104-OBS-003` | `NEW` | `GQF-0150` |
| `GQ1-CHUNK-0104-OBS-004` | `NEW` | `GQF-0151` |
| `GQ1-CHUNK-0105-OBS-001` | `EXTENDS` | `BR-0662` |
| `GQ1-CHUNK-0105-OBS-002` | `EXTENDS` | Archived `BR-0041` |
| `GQ1-CHUNK-0106` | `EXTENDS` | `GQF-0076` through `GQF-0080`, `GQF-0085` |
| `GQ1-CHUNK-0107-OBS-001` | `EXTENDS` | Archived `BR-0034` |
| `GQ1-CHUNK-0107-OBS-002` | `NEW` | `GQF-0152` |
| `GQ1-CHUNK-0107-OBS-003` | `EXTENDS` | Archived `BR-0058` |
| `GQ1-CHUNK-0107-OBS-004` | `EXTENDS` | `GQF-0082` |
| `GQ1-CHUNK-0108-OBS-001` | `EXTENDS` | `GQF-0074` |
| `GQ1-CHUNK-0108-OBS-002` | `EXTENDS` | `GQF-0040` |
| `GQ1-CHUNK-0108-OBS-003` | `EXTENDS` | `GQI-0003` |
| `GQ1-CHUNK-0108-OBS-004` | `DUPLICATE` | `BR-0158` |
| `GQ1-CHUNK-0109` | `DUPLICATE/EXTENDS` | Earlier HFS observations, `BR-0158`, `BR-0160`, `BR-0027`; no new owner |
| `GQ1-CHUNK-0110` | `EXTENDS` | `GQF-0090`, `GQF-0091`, `GQF-0046`; no new owner |
| `GQ1-CHUNK-0111-OBS-001` | `EXTENDS` | `GQF-0092` |
| `GQ1-CHUNK-0111-OBS-002` | `EXTENDS` | `BR-0021` |
| `GQ1-CHUNK-0111-OBS-003` | `EXTENDS` | `GQF-0043` |
| `GQ1-CHUNK-0112` | `HISTORICAL-CLOSED` | Archived `BR-0024`; explicit clean coverage |
| `GQ1-CHUNK-0113-OBS-001` | `EXTENDS` | `GQF-0150`, linked archived `BR-0046` |
| `GQ1-CHUNK-0113-OBS-002` | `EXTENDS` | `BR-0160` |
| `GQ1-CHUNK-0113-OBS-003` through `OBS-005` | `HISTORICAL-CLOSED` | Archived `BR-0032`, `BR-0031`, `BR-0033` |
| `GQ1-CHUNK-0114-OBS-001` | `NEW` | `GQF-0153` |
| `GQ1-CHUNK-0114-OBS-002` | `DUPLICATE` | `BR-0158` |
| `GQ1-CHUNK-0114-OBS-003` | `EXTENDS` | `BR-0160` |
| `GQ1-CHUNK-0114-OBS-004` | `EXTENDS` | `GQF-0093` |
| `GQ1-CHUNK-0115-OBS-001` | `DUPLICATE/EXTENDS` | `BR-0182` |
| `GQ1-CHUNK-0115-OBS-002` | `EXTENDS` | `BR-0586` |
| `GQ1-CHUNK-0116-OBS-001` | `DUPLICATE/EXTENDS` | `BR-0182` |
| `GQ1-CHUNK-0116-OBS-002` | `EXTENDS` | `BR-0160` |
| `GQ1-CHUNK-0116-OBS-003` | `HISTORICAL-CLOSED` | Archived `BR-0033` |
| `GQ1-CHUNK-0117-OBS-001` | `NEW` | `GQF-0154` |
| `GQ1-CHUNK-0117-OBS-002` | `EXTENDS` | `GQF-0106`, `GQF-0107` |
| `GQ1-CHUNK-0118-OBS-001` | `EXTENDS` | `GQF-0115` |
| `GQ1-CHUNK-0118-OBS-002` | `DUPLICATE` | `BR-0099` |
| `GQ1-CHUNK-0118-OBS-003` | `EXTENDS` | `BR-0103` |
| `GQ1-CHUNK-0119-OBS-001` | `EXTENDS` | `GQF-0100`, `GQF-0101`, `GQF-0102` |
| `GQ1-CHUNK-0119-OBS-002` | `EXTENDS` | `GQF-0103`, `GQF-0104` |
| `GQ1-CHUNK-0119-OBS-003` | `EXTENDS` | `GQF-0061` |
| `GQ1-CHUNK-0120` | `DUPLICATE/EXTENDS` | `GQF-0030`, `BR-0099`, `GQF-0034`, `BR-0101`; no new owner |
| `GQ1-CHUNK-0121-OBS-001` | `EXTENDS` | `GQF-0119` |
| `GQ1-CHUNK-0121-OBS-002` | `EXTENDS` | `GQF-0120` |
| `GQ1-CHUNK-0122-OBS-001` | `EXTENDS` | `GQF-0118` |
| `GQ1-CHUNK-0122-OBS-002` | `HISTORICAL-CLOSED` | Archived `BR-0185` |
| `GQ1-CHUNK-0123-OBS-001` | `EXTENDS` | `GQF-0111`, archived `BR-0096` |
| `GQ1-CHUNK-0123-OBS-002` | `EXTENDS` | `GQF-0112`, archived `BR-0096` |
| `GQ1-CHUNK-0124` | `DUPLICATE/EXTENDS` | `GQF-0111`, archived `BR-0096`; no new owner |
| `GQ1-CHUNK-0125-OBS-001` | `NEW` | `GQF-0155`; residual extension of completed PhysFS extraction plan and reconciled with DMR1 |
| `GQ1-CHUNK-0125-OBS-002` | `HISTORICAL-CLOSED/EXTENDS` | `BR-0469`; repair evidence, no new owner |
| `GQ1-CHUNK-0126` | `DUPLICATE/EXTENDS` | `GQF-0111`, archived `BR-0096`; no new owner |
| `GQ1-CHUNK-0127-OBS-001` | `NEW` | `GQF-0156`; reconciled with DMR1 retained geometry/frame policy |
| `GQ1-CHUNK-0127-OBS-002` | `DUPLICATE/EXTENDS` | `BR-0077` |
| `GQ1-CHUNK-0127-OBS-003` | `DUPLICATE` | `BR-0283`; runner context remains `BR-0009` |
| `GQ1-CHUNK-0128-DIFF` | `RESOLVED_EXTERNAL` | `GQD-0021`; post-freeze commit `7f0f7c20`, no new GQF |
| `GQ1-CHUNK-0128-OBS-001` | `NEW` | `GQF-0157`, validation link `GQF-0127`, `GQF-0128` |
| `GQ1-CHUNK-0128-OBS-002` | `NEW` | `GQF-0158` |
| `GQ1-CHUNK-0129` | `EXTENDS` | `BR-0187`, `BR-0008`, `BR-0189`, `BR-0169`; no new owner |
| `GQ1-CHUNK-0130` | `EXTENDS` | `BR-0162`, `BR-0160`, `GQF-0125`; no new owner |
| `GQ1-CHUNK-0131-DIFF` | `DUPLICATE/EXTENDS` | `GQD-0018`; no new minimization owner |
| `GQ1-CHUNK-0131-OBS-001` | `NEW` | `GQF-0159` |
| `GQ1-CHUNK-0131-OBS-002` through `OBS-004` | `EXTENDS` | `BR-0187`, `BR-0008`, `BR-0190` |
| `GQ1-CHUNK-0132-DIFF` | `DUPLICATE/EXTENDS` | `GQD-0020`, `GQF-0156`, `GQR-0143`; retained event-loop seam |
| `GQ1-CHUNK-0132-OBS-002` | `NEW` | `GQF-0160` |
| `GQ1-CHUNK-0132-OBS-003` | `NEW` | `GQF-0161` |
| `GQ1-CHUNK-0132-OBS-004` | `DUPLICATE` | `BR-0009` |
| `GQ1-CHUNK-0133-OBS-001` | `EXTENDS` | `GQF-0153` |
| `GQ1-CHUNK-0133-OBS-002` | `EXTENDS` | `GQF-0137` |
| `GQ1-CHUNK-0134-OBS-001` | `NEW` | `GQF-0162` |
| `GQ1-CHUNK-0134-OBS-002` | `EXTENDS` | `BR-0169` |
| `GQ1-CHUNK-0134-OBS-003` | `EXTENDS` | `GQF-0126`, `GQR-0113` |
| `GQ1-CHUNK-0135-DIFF` | `DUPLICATE/EXTENDS` | `GQD-0020`, `GQF-0156`, `GQR-0143`; retained event-loop seam |
| `GQ1-CHUNK-0135-OBS-001` | `REGROWTH/EXTENDS` | `GQF-0163`, linked archived `BR-0605` |
| `GQ1-CHUNK-0135-OBS-002` | `DUPLICATE/EXTENDS` | `BR-0190` |
| `GQ1-CHUNK-0135-OBS-003` | `DUPLICATE/EXTENDS` | `BR-0191` |
| `GQ1-CHUNK-0136` | `DUPLICATE/EXTENDS` | `GQF-0032`, `BR-0010`, `BR-0190`, `BR-0191`, `BR-0192`, `BR-0008`; no new owner |
| `GQ1-CHUNK-0137-DIFF` | `DUPLICATE/EXTENDS` | `GQD-0018`, `GQF-0155`, `GQR-0142`; no new minimization owner |
| `GQ1-CHUNK-0137-OBS-001` | `NEW` | `GQF-0164` |
| `GQ1-CHUNK-0137-OBS-002` | `DUPLICATE/EXTENDS` | `BR-0084` |
| `GQ1-CHUNK-0137-OBS-003` | `DUPLICATE/EXTENDS` | `BR-0168`, `BR-0190`, `BR-0193` |
| `GQ1-CHUNK-0138-OBS-001` | `NEW` | `GQF-0165`; coordinate runner ownership with `GQF-0158` without merging product roots |
| `GQ1-CHUNK-0138-OBS-002` | `EXTENDS` | Archived `BR-0635` incomplete-validation evidence |
| `GQ1-CHUNK-0139-OBS-001` | `DUPLICATE/EXTENDS` | `BR-0610` |
| `GQ1-CHUNK-0140-OBS-001` | `DUPLICATE/EXTENDS` | `BR-0610` |
| `GQ1-CHUNK-0141-OBS-001` through `OBS-006` | `DUPLICATE/EXTENDS` | `BR-0106`, `GQF-0014`/`BR-0107`, `BR-0114`, `BR-0126`, `BR-0131`, `BR-0194` |
| `GQ1-CHUNK-0141-OBS-007` | `NEW` | `GQF-0166` |
| `GQ1-CHUNK-0142` | `HISTORICAL-CLOSED/CLEAN` | Archived `BR-0219`, `BR-0299`; separate DMR1 residue unchanged |
| `GQ1-CHUNK-0143` | `HISTORICAL-CLOSED/CLEAN` | Archived `BR-0204`, `BR-0205`; duplicate `BR-0029`; external `BR-0394` blocker unchanged |
| `GQ1-CHUNK-0144` | `CLEAN/RETAIN` | Existing `BR-0206`, `BR-0029`, `BR-0044`, `GQF-0037`, `GQF-0052`; archived `BR-0207`, `BR-0208` remain closed |
| `GQ1-CHUNK-0145` | `HISTORICAL-CLOSED/CLEAN` | Archived `BR-0065`; open `GQF-0037` remains external bypass context |
| `GQ1-CHUNK-0146` | `CLEAN/RETAIN` | Extend `BR-0195` evidence for powerup packet identity/authority; no new finding |
| `GQ1-CHUNK-0148` | `CLEAN/RETAIN` | No new observation, finding or investigation |
| `GQ1-CHUNK-0149` | `DUPLICATE/RETAIN` | Open `BR-0300` owns dead `d1_in_d2_start_from_level` |
| `GQ1-CHUNK-0147` | `HISTORICAL-CLOSED/DUPLICATE` | Archived `BR-0196` repair intact; open `BR-0289` owns no-op point size |
| `GQ1-CHUNK-0150-OBS-001` | `DUPLICATE` | Open `BR-0222` recorder/trace transactional publication |
| `GQ1-CHUNK-0151-OBS-001` | `REGROWTH` | `GQF-0167`, linked archived `BR-0279` |
| `GQ1-CHUNK-0152-OBS-001` | `NEW` | `GQF-0168` |
| `GQ1-CHUNK-0153-DIFF` | `NEW` | `GQF-0169`, `GQR-0156`; estimated -22 inherited lines/-2 hunks |
| `GQ1-CHUNK-0153-OBS-001` | `EXTENDS` | `GQF-0167`, linked archived `BR-0279`: Redbook EOF also publishes completion before queued PCM drains |
| `GQ1-CHUNK-0154-OBS-001` | `EXTENDS` | Open `BR-0224`: permissive numeric parsing and unconditional advance for malformed difficulty, wall-snapshot and unknown-field commands |
| `GQ1-CHUNK-0155-DIFF` | `NEW` | `GQF-0170`, `GQR-0157`; final residual of completed DMR1 HMP extraction, estimated -24 inherited lines/-4 hunks |
| `GQ1-CHUNK-0155-OBS-001` through `OBS-003` | `DUPLICATE/EXTENDS` | `GQF-0141`, `GQF-0142`, `GQF-0144`; no new quality root |
| `GQ1-CHUNK-0157-DIFF` | `NEW` | `GQF-0171`, `GQR-0158`; estimated -4 inherited lines, zero complete hunks |
| `GQ1-CHUNK-0157-OBS-001` | `DUPLICATE` | Open `BR-0201` floating-point environment correctness |
| `GQ1-CHUNK-0156-OBS-001` | `EXTENDS` | `GQF-0168` quadratic direct-command parsing |
| `GQ1-CHUNK-0156-OBS-002` | `DUPLICATE` | Open `BR-0290` duplicate weapon collision probes |
| `GQ1-CHUNK-0156-OBS-003` | `HISTORICAL-CLOSED` | `BR-0230` fixed; old `BR-0231` D1 unload/skip trigger absent at frozen head |
| `GQ1-CHUNK-0158-DIFF` | `DUPLICATE/EXTENDS` | Existing `DMR1-CHUNK-003`; estimated -64 inherited lines/-2 body hunks |
| `GQ1-CHUNK-0158-OBS-001` through `OBS-003` | `DUPLICATE/EXTENDS` | `BR-0232`, `BR-0195`, `BR-0029`; no new quality root |
| `GQ1-CHUNK-0159-DIFF` | `NEW` | `GQF-0172`, `GQR-0159`; residual completed C10 ownership, estimated -48 inherited lines |
| `GQ1-CHUNK-0159-OBS-001` | `REGROWTH` | `GQF-0173`, linked archived `BR-0248` |
| `GQ1-CHUNK-0159-OBS-002` | `DUPLICATE/EXTENDS` | Open `BR-0250` diagnostic publication |
| `GQ1-CHUNK-0160-OBS-001` through `OBS-003` | `DUPLICATE/HISTORICAL-CLOSED` | `BR-0332`, `BR-0396`, archived `BR-0085`; no new root |
| `GQ1-CHUNK-0161` | `DUPLICATE/EXTENDS` | Open `BR-0254`, `BR-0078`; no new root |
| `GQ1-CHUNK-0162` | `CLEAN/RETAIN` | No new or extended observation after historical deduplication |
| `GQ1-CHUNK-0163` | `DUPLICATE/RETAIN` | Open `BR-0229`, `BR-0396`, `BR-0331`; no new root |
| `GQ1-CHUNK-0164-DIFF` | `NEW` | `GQF-0174`, `GQR-0161`; estimated -40 inherited lines/-2 hunks |
| `GQ1-CHUNK-0164-OBS-001` through `OBS-004` | `DUPLICATE/EXTENDS` | `BR-0213`, `BR-0214`, `BR-0337`, `BR-0394`; no new quality root |
| `GQ1-CHUNK-0165-DIFF` | `NEW` | `GQF-0175`, `GQR-0162`; estimated -169 to -177 inherited lines/-2 hunks |
| `GQ1-CHUNK-0165-OBS-001` | `DUPLICATE` | Open `BR-0209` headless false success |
| `GQ1-CHUNK-0165-OBS-002` | `DUPLICATE` | Open `BR-0233` metadata publication |
| `GQ1-CHUNK-0165-OBS-003` | `NEW` | `GQF-0176` |
| `GQ1-CHUNK-0166` | `EXTENDS/HISTORICAL-CLOSED` | Open `BR-0244`, `BR-0588`; archived `BR-0196` remains fixed; no new root |
| `GQ1-CHUNK-0167-DIFF` | `NO_INHERITED_EFFECT` | Proposed 72-line header consolidation is entirely among branch-added paths; `GQF-0174` / `GQR-0161` remains limited to the base-existing paired `state.c` bodies |
| `GQ1-CHUNK-0167-OBS-001` through `OBS-002` | `DUPLICATE` | Open `BR-0213`, `BR-0234`; no new quality root |

## Bootstrap coverage records

The broad live survey produced ten preliminary `PARTIAL` records. They establish inventory and prioritized leads but do not complete any deterministic `GQ1-CHUNK-*` row

| ID | Outcome | Scope | Evidence and remaining work |
|---|---|---|---|
| `GQC-0001` | `PARTIAL` | Complete diff inventory | Counted all 3,136 paths by status, domain, extension, and numstat; deterministic range review remains |
| `GQC-0002` | `PARTIAL` | Android Kotlin | Reviewed broad coroutine, null, catch, logging, receiver, and reachability leads; assigned ranges remain |
| `GQC-0003` | `PARTIAL` | Shared native C/C++ | Reviewed unsafe formatting, resource, timing, extraction, JNI, and test leads; full ranges remain |
| `GQC-0004` | `PARTIAL` | Inherited D1/D2 | Counted 355 changed paths and sampled parity, OGL, whitespace, and tests; path-level semantic review remains |
| `GQC-0005` | `PARTIAL` | Tests and automation | Reviewed false-pass, child status, selector, zero-work, and cleanup patterns; full catalog remains |
| `GQC-0006` | `PARTIAL` | PowerShell, shell, Python | Sampled process ownership, deletion, download, exit, subprocess, and temp patterns; full path batches remain |
| `GQC-0007` | `PARTIAL` | CMake, build, release | Reviewed paired targets, wrappers, pinning, Docker identity, and whitespace; complete target/release coverage remains |
| `GQC-0008` | `PARTIAL` | Server Rust | Reviewed principal security/resource domains and identified 26 targeted rechecks; frozen range review remains |
| `GQC-0009` | `PARTIAL` | Docs and executable assumptions | Reviewed instructions, credential/license/provenance and whitespace leads; mechanical plan batches remain |
| `GQC-0010` | `PARTIAL` | Generated fixtures and data | Counted 279 game-data paths; generator, normalization, uniqueness, hash, and stale-baseline batches remain |
| `GQC-0011` | `ISSUES` | Complete-diff composition preflight | Completed the assigned frozen-object review of PR shape, provenance, artifacts, credentials, licenses, and split strategy. Extended five live GQ findings and five historical owners, admitted `GQF-0024`, retained `GQI-0001`, and recorded explicit clean dimensions. Mechanical path and semantic coverage remain independently queued |
| `GQC-0012` | `ISSUES` | Complete-diff architecture preflight | Completed the assigned subsystem, intended-behavior, ownership-boundary, and highest-risk-data-flow review. Nine observations extend existing GQ/BR roots or historical closure evidence; no distinct architecture finding was admitted. Deterministic ranges retain semantic coverage |
| `GQC-0013` | `ISSUES` | Complete-diff change-history preflight | Completed the 1,252-commit cluster, supersession, partial-migration, and compatibility-path review. Two observations extend historical roots; four were admitted as `GQF-0025` through `GQF-0028`; clean supersessions were recorded without IDs |
| `GQC-0014` | `ISSUES` | `GQ1-CHUNK-0001` two Android auth configuration samples | Completed every assigned line plus necessary frozen consumers, parsers, history, ignores, and ownership context. Confirmed historical `BR-0007`, attached one cleanup note, and admitted no duplicate GQ finding |
| `GQC-0015` | `ISSUES` | `GQ1-CHUNK-0002` CD regression/fingerprint records | Completed all 12 assigned records and necessary frozen generation, validation, consumption, database, corpus, and history context. Admitted `GQF-0029` and extended five historical roots |
| `GQC-0016` | `ISSUES` | `GQ1-CHUNK-0003` fingerprint and Dimensions records | Completed both assigned records and required generation/database/corpus context. Admitted `GQF-0030`, extended two historical roots, and recorded one scope-specific prior repair |
| `GQC-0017` | `ISSUES` | `GQ1-CHUNK-0004` seven D2 CD regression specs | Completed all assigned records plus required generator, validator, runner, introspection, database, sibling, and history context. Admitted `GQF-0031` and extended six historical roots |
| `GQC-0018` | `ISSUES` | `GQ1-CHUNK-0005` eight CD regression specs | Completed all assigned records and frozen generator/consumer/corpus context. Extended two GQ findings and seven historical roots; no new root admitted |
| `GQC-0019` | `ISSUES` | `GQ1-CHUNK-0006` nine Mac and D1 release records | Completed all assigned records and format/generation/database context. Extended two GQ findings and six historical roots; recorded repaired HFS and complete Levels oracles; no new root admitted |
| `GQC-0020` | `ISSUES` | `GQ1-CHUNK-0007` combined D2 plus Vertigo launch record | Completed the assigned generated record and all helper/component/generator/consumer context. Admitted `GQF-0032` and `GQF-0033`; extended six historical roots |
| `GQC-0021` | `ISSUES` | `GQ1-CHUNK-0008` ten mission music fingerprint sidecars | Completed every assigned record and required generation/publication/corpus/provenance context. Admitted `GQF-0034`, `GQF-0035`, broadened `GQF-0030`, and extended `BR-0623` |
| `GQC-0022` | `ISSUES` | `GQ1-CHUNK-0009` SAF manifest parser and header | Completed every assigned line plus native/Kotlin/JNI/PhysicsFS ownership and tests. Admitted `GQF-0036`, `GQF-0037`, and extended `BR-0084` |
| `GQC-0023` | `ISSUES` | `GQ1-CHUNK-0010` Mac HFS extraction wrapper and header | Completed every assigned line plus HFS/STi2, caller, publication, test and history context. Admitted `GQF-0038` through `GQF-0040` and extended `BR-0021` |
| `GQC-0024` | `ISSUES` | `GQ1-CHUNK-0011` SOW extractor tail and public header | Completed all assigned lines plus parser, decoder, caller and test context. Admitted `GQF-0041` through `GQF-0043`; extended three owners |
| `GQC-0025` | `ISSUES` | `GQ1-CHUNK-0012` PKG reader tail and public header | Completed all assigned lines plus manifest, stream, caller and test context. Admitted `GQF-0044` through `GQF-0046`; extended two owners |
| `GQC-0026` | `ISSUES` | `GQ1-CHUNK-0013` ISO9660 reader tail and public header | Completed all assigned lines plus parser, CUE/standalone caller, transaction and malformed-test context. Admitted `GQF-0047` through `GQF-0050`; extended `GQF-0046` and `BR-0021` |
| `GQC-0027` | `ISSUES` | `GQ1-CHUNK-0014` HFS reader tail and public header | Completed all assigned lines plus full parser/caller/test context. Extended five current owners; admitted no new root |
| `GQC-0028` | `ISSUES` | `GQ1-CHUNK-0015` JNI main tail and MIDI preview bridge | Completed all assigned lines plus Kotlin/native owners and tests. Admitted `GQF-0052`, broadened `GQF-0037`, and extended five historical roots |
| `GQC-0029` | `ISSUES` | `GQ1-CHUNK-0016` STi2 public/terminal contract | Completed the assigned contract and necessary full implementation/caller context. Admitted `GQF-0051`, extended four owners, duplicated `BR-0073` |
| `GQC-0030` | `ISSUES` | `GQ1-CHUNK-0017` GOG JNI and shared JSON writer | Completed all assigned lines plus Inno/PKG/Kotlin/CLI consumer context. Admitted `GQF-0053`, `GQF-0054`; extended three owners and duplicated one |
| `GQC-0031` | `ISSUES` | `GQ1-CHUNK-0018` reconnect authentication contracts | Completed all assigned lines plus both-game, JNI/Kotlin crypto, migration, route and protocol context. Admitted `GQF-0055` through `GQF-0057` |
| `GQC-0032` | `ISSUES` | `GQ1-CHUNK-0019` StuffIt header and CD/fingerprint JNI | Completed all assigned lines plus native/Kotlin/decoder/database context. Admitted `GQF-0058`; extended three owners; StuffIt contract added no root |
| `GQC-0033` | `ISSUES` | `GQ1-CHUNK-0020` fingerprint matcher and game-extension policy | Completed all assigned lines plus producer/consumer/configuration/test context. Admitted `GQF-0059`, `GQF-0060`; extended archived `BR-0041` |
| `GQC-0034` | `ISSUES` | `GQ1-CHUNK-0021` extractor CLIs and shared limits | Completed all assigned lines plus complete CLI/parser/publication/script context. Admitted `GQF-0061`, `GQF-0062`; extended four owners |
| `GQC-0035` | `ISSUES` | `GQ1-CHUNK-0022` exact-read and CUE parser contracts | Completed all assigned lines plus every caller/Kotlin/source/test context. Admitted `GQF-0063` through `GQF-0066` |
| `GQC-0036` | `ISSUES` | `GQ1-CHUNK-0023` SAF/reconnect/overlay/shared-string JNI | Completed all assigned lines plus native/Kotlin/lifecycle/crypto context. Extended five existing owners; admitted no new root |
| `GQC-0037` | `ISSUES` | `GQ1-CHUNK-0024` CD extraction composition first 600 lines | Completed assigned range plus complete control-flow/caller/transaction context. Extended existing owners only; admitted no new root |
| `GQC-0038` | `ISSUES` | `GQ1-CHUNK-0025` audio fingerprint CLI and decoder owner | Completed all assigned lines plus JNI/script/database/test context. Admitted `GQF-0067`, `GQF-0068`, `GQI-0002`; extended two owners |
| `GQC-0039` | `ISSUES` | `GQ1-CHUNK-0026` CD fingerprint CLI and source transaction | Completed all assigned lines plus parser/generator/publication/database/test context. Admitted `GQF-0069`, `GQF-0070`; extended six owners |
| `GQC-0040` | `ISSUES` | `GQ1-CHUNK-0027` HFS reader first 600 lines | Completed assigned range plus full catalog/tree/extraction/caller/test context. Admitted `GQF-0071`, `GQF-0072`, `GQI-0003`; extended two historical/live owners |
| `GQC-0041` | `ISSUES` | `GQ1-CHUNK-0028` HFS reader second 600 lines | Completed assigned range plus full parser/wrapper/format/test context. Admitted `GQF-0073`, `GQF-0074`; corroborated `GQI-0003` and extended two owners |
| `GQC-0042` | `ISSUES` | `GQ1-CHUNK-0029` Inno reader first 600 lines | Completed assigned range plus full PE resource/offset-table/archive-open/test context. Admitted `GQF-0075` as archived `BR-0052` regrowth; deduplicated three open owners |
| `GQC-0043` | `ISSUES` | `GQ1-CHUNK-0030` Inno reader second 600 lines | Completed assigned range plus metadata decode/version/layout/archive/test context. Admitted `GQF-0076` through `GQF-0078`; linked two archived repair roots |
| `GQC-0044` | `ISSUES` | `GQ1-CHUNK-0031` Inno reader third 600 lines | Completed assigned range plus Galaxy/parser/format/test context. Admitted `GQF-0079`, `GQF-0080`; extended `GQF-0078` |
| `GQC-0045` | `ISSUES` | `GQ1-CHUNK-0032` Inno reader fourth 600 lines | Completed assigned range plus decoder/API/caller/test context. Admitted `GQF-0081` through `GQF-0083`; extended two owners |
| `GQC-0046` | `ISSUES` | `GQ1-CHUNK-0033` Inno reader fifth 600 lines | Completed assigned range plus decoder/Galaxy/publication/test context. Admitted `GQF-0084`; corroborated `GQF-0083` and extended `BR-0021` |
| `GQC-0047` | `ISSUES` | `GQ1-CHUNK-0034` Inno reader tail | Completed assigned range plus output/progress/Galaxy/decoder/test context. Admitted `GQF-0085`, `GQF-0086`; extended four owners |
| `GQC-0048` | `ISSUES` | `GQ1-CHUNK-0035` Inno public header | Completed all lines plus implementation/caller/test/capability context. Extended five owners; admitted no new root |
| `GQC-0049` | `ISSUES` | `GQ1-CHUNK-0036` ISO9660 reader first 600 lines | Completed assigned range plus full parser/caller/test context. Admitted `GQF-0087`, `GQF-0088`; extended four owners |
| `GQC-0050` | `ISSUES` | `GQ1-CHUNK-0037` JNI disc-import bridge | Completed all lines plus CUE/ISO/SOW/archive/Kotlin/test context. Admitted `GQF-0089`; extended four owners and one historical duplicate |
| `GQC-0051` | `ISSUES` | `GQ1-CHUNK-0038` PKG reader first 600 lines | Completed assigned range plus catalog/extraction/JNI/CLI/test context. Admitted `GQF-0090`, `GQF-0091`; extended five owners |
| `GQC-0052` | `ISSUES` | `GQ1-CHUNK-0039` SOW extractor first 600 lines | Completed assigned range plus scanner/extraction/caller/test context. Admitted `GQF-0092`; extended four owners |
| `GQC-0053` | `ISSUES` | `GQ1-CHUNK-0040` STi2 first 600 lines | Completed assigned range plus full parser/decoder/test/license context. Extended two owners, verified one closure and duplicated one owner |
| `GQC-0054` | `ISSUES` | `GQ1-CHUNK-0041` STi2 second 600 lines | Completed assigned range plus method-14 production/test/history context. Admitted `GQF-0093`; verified two closures and duplicated one owner |
| `GQC-0055` | `ISSUES` | `GQ1-CHUNK-0042` STi2 third 600 lines | Completed assigned range plus method-14/15 test/history context. Duplicated `GQF-0093` and one historical owner; verified two closures |
| `GQC-0056` | `ISSUES` | `GQ1-CHUNK-0043` STi2 tail | Completed assigned range plus high-level/nested extraction/test context. Admitted `GQF-0094`; extended five owners and verified one closure |
| `GQC-0057` | `ISSUES` | `GQ1-CHUNK-0044` complete StuffIt extractor | Completed all lines plus nested STi2/publication/corpus/license context. Extended six owners and duplicated two; no new root |
| `GQC-0058` | `ISSUES` | `GQ1-CHUNK-0045` level-metadata JNI first 600 lines | Completed assigned range plus global runtime/service/caller/test context. Admitted `GQF-0095`; verified two historical closures |
| `GQC-0059` | `ISSUES` | `GQ1-CHUNK-0046` level-metadata JNI second 600 lines | Completed assigned range plus mission/root status/automation context. Extended `BR-0077`; no new root |
| `GQC-0060` | `ISSUES` | `GQ1-CHUNK-0047` level-metadata JNI tail | Completed assigned tail plus enclosing result publication context. Extended `BR-0077`; verified two historical closures |
| `GQC-0061` | `ISSUES` | `GQ1-CHUNK-0048` JNI main first 600 lines | Completed assigned range plus engine/Activity/UI/JNI lifecycle context. Extended three owners; no new root |
| `GQC-0062` | `ISSUES` | `GQ1-CHUNK-0049` JNI main tail | Completed assigned range plus UI/engine/automation/hosting context. Extended five owners and verified two closures |
| `GQC-0063` | `ISSUES` | `GQ1-CHUNK-0050` music-control JNI | Completed all lines plus engine/music/UI/sidecar/test context. Extended two owners and verified three closures |
| `GQC-0064` | `ISSUES` | `GQ1-CHUNK-0051` resume-save first 600 lines | Completed assigned range plus path/metadata/UI/filesystem/test context. Admitted `GQF-0096`, `GQF-0097`; extended `BR-0082` |
| `GQC-0065` | `ISSUES` | `GQ1-CHUNK-0052` resume-save tail | Completed assigned range plus pointer repair/deletion/path/test context. Admitted `GQF-0098`; extended `BR-0417` |
| `GQC-0066` | `ISSUES` | `GQ1-CHUNK-0053` SAF PhysicsFS archiver | Completed all lines plus provider/mount/manifest/test context. Admitted `GQF-0099`; extended `GQF-0036` |
| `GQC-0067` | `ISSUES` | `GQ1-CHUNK-0054` ZIP import Kotlin batch | Completed both paths plus ZIP parser/staging/extraction/budget/test context. Admitted `GQF-0100` through `GQF-0104` |
| `GQC-0068` | `ISSUES` | `GQ1-CHUNK-0055` HOG/audio fingerprint Kotlin batch | Completed both paths plus engine/sidecar/container/test context. Admitted `GQF-0105` through `GQF-0107`; extended `BR-0089` |
| `GQC-0069` | `ISSUES` | `GQ1-CHUNK-0056` ArchiveFiles | Completed all lines plus native/host fallback, staging and test context. Admitted `GQF-0108` through `GQF-0110`, `GQI-0004` |
| `GQC-0070` | `ISSUES` | `GQ1-CHUNK-0057` MissionZip | Completed all lines plus engine/descriptor/admission/publication/test context. Admitted `GQF-0111` through `GQF-0114` |
| `GQC-0071` | `ISSUES` | `GQ1-CHUNK-0058` mission fingerprint cache | Completed all lines plus cache/database/projection/test context. Extended four owners; no new root |
| `GQC-0072` | `ISSUES` | `GQ1-CHUNK-0059` extraction store first 600 lines | Completed assigned range plus scan/stage/freshness/transaction context. Admitted `GQF-0115`, `GQF-0116`; extended `BR-0103` |
| `GQC-0073` | `ISSUES` | `GQ1-CHUNK-0060` extraction store tail | Completed tail plus alias-generation/storage/progress context. Admitted `GQF-0117` |
| `GQC-0074` | `ISSUES` | `GQ1-CHUNK-0061` MissionZipMusic | Completed all lines plus nested container/source/stage/test context. Admitted `GQF-0118`; extended three owners |
| `GQC-0075` | `ISSUES` | `GQ1-CHUNK-0062` music stage manager | Completed all lines plus stream/budget/cache/UI/test context. Admitted `GQF-0119`, `GQF-0120` |
| `GQC-0076` | `ISSUES` | `GQ1-CHUNK-0063` PlayGamesAuth | Completed all lines plus caller/history context. Exact duplicate of `BR-0090`; no new root |
| `GQC-0077` | `CLEAN` | `GQ1-CHUNK-0064` Android data extraction rules | Completed all lines plus manifest, legacy policy, identity owner, multiplayer, test, packaging and history context. Archived `BR-0414` remains closed |
| `GQC-0078` | `ISSUES` | `GQ1-CHUNK-0065` server config and auth batch | Completed both paths plus config/auth/TLS/test context. Extended `GQF-0014`, `BR-0106` through `BR-0108`; no new root |
| `GQC-0079` | `ISSUES` | `GQ1-CHUNK-0066` server STUN and TLS batch | Completed both paths plus listener/candidate/test context. Five exact duplicates; no new root |
| `GQC-0080` | `ISSUES` | `GQ1-CHUNK-0067` server PoW/protocol batch | Completed both paths plus consumer/test/history context. Eight exact duplicates; no new root |
| `GQC-0081` | `ISSUES` | `GQ1-CHUNK-0068` server status/auth batch | Completed three paths plus consumer/test/history context. Six duplicates and two evidence extensions; no new root |
| `GQC-0082` | `ISSUES` | `GQ1-CHUNK-0069` server session batch | Completed three paths plus consumer/test/history context. Five exact duplicates; no new root |
| `GQC-0083` | `ISSUES` | `GQ1-CHUNK-0070` server rate-limit/relay/stats batch | Completed three paths plus tests/history. Seven exact duplicates; no new root |
| `GQC-0084` | `ISSUES` | `GQ1-CHUNK-0071` server database | Completed all lines plus callers/tests/history. Six duplicates and four evidence extensions; no new root |
| `GQC-0085` | `ISSUES` | `GQ1-CHUNK-0072` NAT simulator | Completed all lines plus callers/tests/history. Admitted `GQF-0121`; extended two owners and duplicated two |
| `GQC-0086` | `ISSUES` | `GQ1-CHUNK-0073` WebSocket handler first 600 lines | Completed assigned range plus model/tests/history. Thirteen duplicate/evidence-refresh observations; no new root |
| `GQC-0087` | `ISSUES` | `GQ1-CHUNK-0074` WebSocket handler second 600 lines | Completed assigned range plus NAT/prediction/relay tests. Admitted `GQF-0122`; extended `BR-0148` |
| `GQC-0088` | `ISSUES` | `GQ1-CHUNK-0075` WebSocket handler third 600 lines | Completed assigned range plus model/tests/history. Six duplicates/extensions; no new root |
| `GQC-0089` | `ISSUES` | `GQ1-CHUNK-0076` WebSocket handler fourth 600 lines | Completed assigned range plus tests/history. Seven duplicate/evidence-refresh observations; no new root |
| `GQC-0090` | `ISSUES` | `GQ1-CHUNK-0077` WebSocket handler tail | Completed assigned range plus tests/history. Seven duplicate/evidence-extension observations; no new root |
| `GQC-0091` | `CLEAN` | `GQ1-CHUNK-0078` extraction `.gitignore` | Reviewed exact rule plus generated outputs, repository ignores, history and consumers. Rule is correctly scoped and historical owners reconcile |
| `GQC-0092` | `ISSUES` | `GQ1-CHUNK-0079` extraction CMake first 600 lines | Completed assigned range plus complete registration and dependency context. Admitted `GQF-0123`; extended three owners and duplicated `BR-0182` |
| `GQC-0093` | `ISSUES` | `GQ1-CHUNK-0080` extraction CMake tail | Completed assigned range plus registered target sources and tests. Extended `BR-0160` and `BR-0236`; no new root |
| `GQC-0094` | `ISSUES` | `GQ1-CHUNK-0081` verified 7-Zip helper | Completed all lines plus callers, dependency manifest, tests and portability history. Extended `BR-0174` and `BR-0159`; no new root |
| `GQC-0095` | `ISSUES` | `GQ1-CHUNK-0082` mission batch tail and UDP relay | Completed both paths plus callers/tests/history. Admitted `GQF-0124`; duplicated four existing roots |
| `GQC-0096` | `ISSUES` | `GQ1-CHUNK-0083` bounded extraction and HFS recovery helpers | Completed three paths plus callers/tests/history. Admitted `GQF-0125`, `GQF-0126`; extended two owners |
| `GQC-0097` | `ISSUES` | `GQ1-CHUNK-0084` auth and bounded test helpers | Completed three paths plus callers/tests/history. Admitted `GQF-0127` through `GQF-0129`; extended `BR-0160` and duplicated `BR-0007` |
| `GQC-0098` | `ISSUES` | `GQ1-CHUNK-0085` mission batch first 600 lines | Completed assigned range plus tail, parsers, tests and history. Regrowth of `BR-0161`; duplicated three owners; no new root |
| `GQC-0099` | `ISSUES` | `GQ1-CHUNK-0086` bulk CD/GOG extraction scripts | Completed both paths plus helpers/tests/history. Admitted `GQF-0130`; extended `BR-0169` and duplicated `GQF-0016`/`BR-0171` |
| `GQC-0100` | `ISSUES` | `GQ1-CHUNK-0087` DOS demo extraction | Completed all lines plus helpers/tests/history. Admitted `GQF-0131`; extended four owners |
| `GQC-0101` | `ISSUES` | `GQ1-CHUNK-0088` Mac CD extraction | Completed all lines plus parser/helper/test/history context. Admitted `GQF-0132` through `GQF-0134`; extended or duplicated eight owners |
| `GQC-0102` | `ISSUES` | `GQ1-CHUNK-0089` Mac demo extraction | Completed all lines plus tools/oracles/tests/history. Admitted `GQF-0135`, `GQF-0136`; extended four owners |
| `GQC-0103` | `ISSUES` | `GQ1-CHUNK-0090` mission music fingerprint first 600 lines | Completed assigned range plus tail, callers/tests/history. Admitted `GQF-0137`, `GQF-0138`; extended `GQF-0128` |
| `GQC-0104` | `ISSUES` | `GQ1-CHUNK-0091` mission music fingerprint tail | Completed assigned range plus preceding logic, callers/tests/history. Admitted `GQF-0139`, `GQF-0140`; extended three owners and duplicated two |
| `GQC-0105` | `ISSUES` | `GQ1-CHUNK-0092` Inno capability documentation | Completed all lines against implementation/tests/history. Extended `GQF-0078`, `GQF-0018`; no new root |
| `GQC-0106` | `ISSUES` | `GQ1-CHUNK-0093` StuffIt/UTF-8/SAF test batch | Completed all paths plus production/build/history context. Extended three owners and duplicated `BR-0597`; no new root |
| `GQC-0107` | `ISSUES` | `GQ1-CHUNK-0094` GOG and graphics transaction tests | Completed both ranges plus production/build/history. Admitted `GQF-0141`, `GQF-0142`; extended `GQF-0075` and duplicated `BR-0158` |
| `GQC-0108` | `ISSUES` | `GQ1-CHUNK-0095` CUE/ISO tail and extraction-limit tests | Completed both ranges plus production/build/history. Admitted `GQF-0143`; extended three owners |
| `GQC-0109` | `ISSUES` | `GQ1-CHUNK-0096` PKG and SOW test batch | Completed all ranges plus production/build/history. Extended three current owners and verified archived `BR-0024` remains closed |
| `GQC-0110` | `ISSUES` | `GQ1-CHUNK-0097` HMP, JSON and pilot transaction tests | Completed all paths plus production/build/history. Admitted `GQF-0144`; extended `GQF-0054`, `BR-0236` |
| `GQC-0111` | `ISSUES` | `GQ1-CHUNK-0098` CD read and Chromaprint database tests | Completed all paths plus production/build/history. Admitted `GQF-0145`; extended `BR-0046`, `GQF-0069` |
| `GQC-0112` | `ISSUES` | `GQ1-CHUNK-0099` CUE/ISO test lines 1-600 | Completed assigned range plus production/build/history. Extended `BR-0160`; no new root |
| `GQC-0113` | `ISSUES` | `GQ1-CHUNK-0100` CUE/ISO test lines 601-1200 | Completed assigned range plus production/build/history. Admitted `GQF-0146`; extended `BR-0160` |
| `GQC-0114` | `ISSUES` | `GQ1-CHUNK-0101` CUE/ISO test lines 1201-1800 | Completed assigned range plus production/build/history. Admitted `GQF-0147`; extended `BR-0160` |
| `GQC-0115` | `ISSUES` | `GQ1-CHUNK-0102` CUE/ISO test lines 1801-2400 | Completed assigned range plus production/build/history. Admitted `GQF-0148`; extended `GQF-0046`, `BR-0160` |
| `GQC-0116` | `ISSUES` | `GQ1-CHUNK-0103` CUE/ISO test lines 2401-3000 | Completed assigned range plus production/build/history. Extended `GQF-0063`, `BR-0160`; no new root |
| `GQC-0117` | `ISSUES` | `GQ1-CHUNK-0104` fingerprint C test | Completed all lines plus production/build/history. Admitted `GQF-0149` through `GQF-0151`; extended `BR-0181` |
| `GQC-0118` | `ISSUES` | `GQ1-CHUNK-0105` shared game-extension test | Completed all lines plus shared/build/history context. Extended `BR-0662` and archived `BR-0041`; no new root |
| `GQC-0119` | `ISSUES` | `GQ1-CHUNK-0106` GOG descriptor tests lines 1-600 | Completed assigned range plus production/build/history. Extended six current owners; no new root |
| `GQC-0120` | `ISSUES` | `GQ1-CHUNK-0107` GOG descriptor tests lines 601-1200 | Completed assigned range plus production/build/history. Admitted `GQF-0152`; extended three owners |
| `GQC-0121` | `ISSUES` | `GQ1-CHUNK-0108` HFS tests lines 1-600 | `GQD-0001` measured no inherited effect. Extended three owners and `GQI-0003`; duplicated `BR-0158` |
| `GQC-0122` | `ISSUES` | `GQ1-CHUNK-0109` HFS tests lines 601-1140 | `GQD-0002` measured no inherited effect. Existing HFS and fixture-owner evidence only; no new root |
| `GQC-0123` | `ISSUES` | `GQ1-CHUNK-0110` PKG TOC bounds tests lines 1-600 | `GQD-0003` measured no inherited effect. Extended three current owners; no new root |
| `GQC-0124` | `ISSUES` | `GQ1-CHUNK-0111` SOW integrity test | `GQD-0004` measured no inherited effect. Extended `GQF-0092`, `BR-0021`, `GQF-0043`; no new root |
| `GQC-0125` | `CLEAN` | `GQ1-CHUNK-0112` SOW real-media CMake test | `GQD-0005` measured no inherited effect. Explicit fixture/registration review; archived `BR-0024` repair remains effective |
| `GQC-0126` | `ISSUES` | `GQ1-CHUNK-0113` STi2 tests lines 1-600 | `GQD-0006` measured no inherited effect. Extended `GQF-0150`, `BR-0160`; verified three archived repairs |
| `GQC-0127` | `ISSUES` | `GQ1-CHUNK-0114` STi2 tests lines 601-1050 | `GQD-0007` measured no inherited effect. Admitted `GQF-0153`; extended three owners |
| `GQC-0128` | `ISSUES` | `GQ1-CHUNK-0115` StuffIt corpus lines 1-600 | `GQD-0008` measured no inherited effect. Extended `BR-0182`, `BR-0586`; no new root |
| `GQC-0129` | `ISSUES` | `GQ1-CHUNK-0116` StuffIt corpus lines 601-1120 | `GQD-0009` measured no inherited effect. Extended `BR-0182`, `BR-0160`; archived `BR-0033` remains closed |
| `GQC-0130` | `ISSUES` | `GQ1-CHUNK-0117` mission music name/progress tests | `GQD-0010` defers a roughly 16-line paired songs seam below DMR1 threshold. Admitted `GQF-0154`; extended two owners |
| `GQC-0131` | `ISSUES` | `GQ1-CHUNK-0118` mission music store/display/preview tests | `GQD-0011` measured no inherited effect. Three existing-owner observations; no new root |
| `GQC-0132` | `ISSUES` | `GQ1-CHUNK-0119` archive/CUE/limit/progress JVM tests | `GQD-0012` measured no inherited effect. Existing extraction-budget and stream-owner evidence only |
| `GQC-0133` | `ISSUES` | `GQ1-CHUNK-0120` audio fingerprint cache test | `GQD-0013` measured no inherited effect and retained four necessary paired consumer lines. Four existing-owner observations; no new root |
| `GQC-0134` | `ISSUES` | `GQ1-CHUNK-0121` music stage manager test | `GQD-0014` measured no inherited effect. Extended `GQF-0119`, `GQF-0120`; no new root |
| `GQC-0135` | `ISSUES` | `GQ1-CHUNK-0122` mission ZIP music catalog test | `GQD-0015` measured no inherited effect. Extended `GQF-0118`; archived `BR-0185` remains closed |
| `GQC-0136` | `ISSUES` | `GQ1-CHUNK-0123` MissionZip tests lines 1-600 | `GQD-0016` measured no inherited effect. Extended `GQF-0111`, `GQF-0112`; no new root |
| `GQC-0137` | `ISSUES` | `GQ1-CHUNK-0124` MissionZip test tail | `GQD-0017` measured no inherited effect. Duplicate evidence for `GQF-0111`/archived `BR-0096`; no new root |
| `GQC-0138` | `ISSUES` | `GQ1-CHUNK-0125` ModManager mission ZIP tests lines 1-600 | `GQD-0018` admitted `GQF-0155`: estimated 26 inherited lines and two hunks removable from paired PhysFS init. Capacity evidence reconciles with `BR-0469` |
| `GQC-0139` | `ISSUES` | `GQ1-CHUNK-0126` ModManager mission ZIP test tail | `GQD-0019` measured no inherited effect beyond the already-owned PhysFS seam. Duplicate `GQF-0111` evidence only |
| `GQC-0140` | `ISSUES` | `GQ1-CHUNK-0127` extraction/metadata automation scripts | `GQD-0020` admitted `GQF-0156`: estimated 144 inherited lines removable across eight menu/window files. Secondary observations deduplicated |
| `GQC-0141` | `ISSUES` | `GQ1-CHUNK-0128` reconnect and bounded-extractor tests | `GQD-0021` records 559 inherited build lines already removed externally. Admitted `GQF-0157`, `GQF-0158` |
| `GQC-0142` | `ISSUES` | `GQ1-CHUNK-0129` extraction regression spec helpers | `GQD-0022` measured no inherited effect. Four existing-owner extensions; no new root |
| `GQC-0143` | `ISSUES` | `GQ1-CHUNK-0130` bounded extraction and workflow tests | `GQD-0023` measured no inherited effect. Extended `BR-0162`, `BR-0160`, `GQF-0125`; no new root |
| `GQC-0144` | `ISSUES` | `GQ1-CHUNK-0131` extraction schema and SAF tests | `GQD-0024` confirms but does not duplicate the PhysFS remediation. Admitted `GQF-0159`; extended three owners |
| `GQC-0145` | `ISSUES` | `GQ1-CHUNK-0132` extraction provenance/publication tests and extraction tail | `GQD-0025` confirms `GQF-0156` and retains the compact event seam. Admitted `GQF-0160`, `GQF-0161`; deduplicated wrong-level success |
| `GQC-0146` | `ISSUES` | `GQ1-CHUNK-0133` mission fingerprint, Mac SAF and batch tests | `GQD-0026` measured no inherited effect. Extended `GQF-0153`, `GQF-0137`; no new root |
| `GQC-0147` | `ISSUES` | `GQ1-CHUNK-0134` extraction batch, HFS and workflow tests | `GQD-0027` measured no inherited effect. Admitted `GQF-0162`; extended `BR-0169`, `GQF-0126` |
| `GQC-0148` | `ISSUES` | `GQ1-CHUNK-0135` extraction tests lines 1-600 | `GQD-0028` confirms `GQF-0156` and retains the compact event seam. Admitted `GQF-0163`; extended `BR-0190`, `BR-0191` |
| `GQC-0149` | `ISSUES` | `GQ1-CHUNK-0136` extraction tests lines 601-1200 | `GQD-0029` measured no inherited effect. Existing `GQF-0032`, `BR-0010`, `BR-0190`, `BR-0191`, `BR-0192`, `BR-0008` evidence only |
| `GQC-0150` | `ISSUES` | `GQ1-CHUNK-0137` SAF archiver test lines 1-600 | `GQD-0030` confirms but does not duplicate the PhysFS remediation. Admitted `GQF-0164`; extended `BR-0084`, `BR-0168`, `BR-0190`, `BR-0193` |
| `GQC-0151` | `ISSUES` | `GQ1-CHUNK-0138` D2X-XL WAV parser regression | `GQD-0031` measured no inherited effect. Admitted `GQF-0165`; extended archived `BR-0635` for a short-format false-pass |
| `GQC-0152` | `ISSUES` | `GQ1-CHUNK-0139` D1 CMake preset seam | `GQD-0032` retains the one-line direct shared-toolchain integration. Duplicate/extension of open `BR-0610`; no new root |
| `GQC-0153` | `ISSUES` | `GQ1-CHUNK-0140` D2 CMake preset seam | `GQD-0033` retains paired parity with D1. Duplicate/extension of open `BR-0610`; no new root |
| `GQC-0154` | `ISSUES` | `GQ1-CHUNK-0141` matchmaking server config and nginx templates | `GQD-0034` measured no inherited effect. Admitted `GQF-0166`; six existing server/security owners extended |
| `GQC-0155` | `CLEAN` | `GQ1-CHUNK-0142` input-demo shared hooks and limits | `GQD-0035` measured no inherited effect. Archived `BR-0219`/`BR-0299` fixes remain present; no new root |
| `GQC-0156` | `CLEAN` | `GQ1-CHUNK-0143` Android surface and D2 headless runtime | `GQD-0036` retains 15 additions at seven necessary paired lifecycle/build sites. Archived surface repairs remain present; no new root |
| `GQC-0157` | `CLEAN` | `GQ1-CHUNK-0144` shared rewind implementation and header | `GQD-0037` retains 14 additions across four paired lifecycle/state paths and ten hunks. Existing owners reconciled; no new root |
| `GQC-0158` | `CLEAN` | `GQ1-CHUNK-0145` shared UTF-8 codec | `GQD-0038` measured no inherited effect. Archived `BR-0065` remains fixed; no new root |
| `GQC-0159` | `CLEAN` | `GQ1-CHUNK-0146` co-op duplication and restore-remap policies | `GQD-0039` retains the remaining game-local hook surface after the prior 166-line extraction. `BR-0195` gains authority evidence; no new root |
| `GQC-0160` | `CLEAN` | `GQ1-CHUNK-0148` input-demo controls tail and header | `GQD-0040` retains the paired game-control seam. No new observation or owner |
| `GQC-0161` | `ISSUES` | `GQ1-CHUNK-0149` input-demo start tail and header | `GQD-0041` retains six paths and 12 lifecycle hunks. Dead helper is exact duplicate of `BR-0300`; no new root |
| `GQC-0162` | `ISSUES` | `GQ1-CHUNK-0147` GLES shim tail and header | `GQD-0042` retains 18 additions across four paired paths and 14 hunks. Archived `BR-0196` remains fixed; point-size evidence extends `BR-0289` |
| `GQC-0163` | `ISSUES` | `GQ1-CHUNK-0150` input-demo recorder tail and header | `GQD-0043` retains the 22-path observation surface. Recorder/trace publication duplicates `BR-0222`; no new root |
| `GQC-0164` | `ISSUES` | `GQ1-CHUNK-0151` shared TSF music tail and effect runtime | `GQD-0044` retains the compact remaining audio/effect lifecycle seam after substantial shared extraction. Admitted `GQF-0167` as regrowth of archived `BR-0279` |
| `GQC-0165` | `ISSUES` | `GQ1-CHUNK-0152` shared input-demo replay tail and API | `GQD-0045` retains the already-shared replay owner and its game-local observation seam. Admitted `GQF-0168` for quadratic current-frame command reparsing |
| `GQC-0166` | `ISSUES` | `GQ1-CHUNK-0154` shared automation tail and API | `GQD-0046` retains 36 additions across six event/joy/config paths and 12 natural ordering hunks. Malformed command acceptance extends `BR-0224`; no new root |
| `GQC-0167` | `ISSUES` | `GQ1-CHUNK-0153` shared Redbook tail and rewind compatibility header | `GQD-0047` admits `GQF-0169` for an estimated 22-line/two-hunk paired-header reduction. Redbook queued-PCM completion extends `GQF-0167`/archived `BR-0279` |
| `GQC-0168` | `ISSUES` | `GQ1-CHUNK-0155` graphics transaction and shared HMP conversion | `GQD-0048` admits `GQF-0170` for the 24-line/four-hunk HMP residual. Existing graphics/HMP quality evidence extends `GQF-0141`, `GQF-0142`, `GQF-0144` |
| `GQC-0169` | `ISSUES` | `GQ1-CHUNK-0157` input-demo fixture and FP environment APIs | `GQD-0049` admits `GQF-0171` for four unused inherited includes. FP correctness evidence duplicates open `BR-0201`; no new quality root |
| `GQC-0170` | `ISSUES` | `GQ1-CHUNK-0156` input-demo debug logging and direct-command policy | `GQD-0050` retains the state-local observation seam. Extended `GQF-0168`, duplicated `BR-0290`, and reconciled historical `BR-0230`/`BR-0231`; no new root |
| `GQC-0171` | `ISSUES` | `GQ1-CHUNK-0158` debug texture overlay, deterministic math and shared difficulty | `GQD-0051` confirms existing `DMR1-CHUNK-003` with about 64 removable inherited lines. Secondary evidence deduplicates to `BR-0232`, `BR-0195`, `BR-0029` |
| `GQC-0172` | `ISSUES` | `GQ1-CHUNK-0159` Android SDL audio driver and config | `GQD-0052` admits `GQF-0172` for an estimated 48-line paired logging reduction. Admitted `GQF-0173` as `BR-0248` lifecycle regrowth and extended `BR-0250` |
| `GQC-0173` | `ISSUES` | `GQ1-CHUNK-0160` route snapshot and SAF I/O contracts | `GQD-0053` retains the four-path route build boundary; SAF has no inherited effect. Deduplicated `BR-0332`, `BR-0396`, archived `BR-0085` |
| `GQC-0174` | `ISSUES` | `GQ1-CHUNK-0161` lifecycle diagnostics and loading progress | `GQD-0054` retains 24 additions across four ordered lifecycle/loading paths and ten hunks. Existing evidence extends `BR-0254`, `BR-0078`; no new root |
| `GQC-0175` | `CLEAN` | `GQ1-CHUNK-0162` Android level preview and lifecycle actions | `GQD-0055` retains 30 additions across four natural startup/event paths and eight sites. No secondary issue survived deduplication |
| `GQC-0176` | `ISSUES` | `GQ1-CHUNK-0163` shared route edge and C API | `GQD-0056` retains the existing four-path route build boundary. Secondary evidence duplicates `BR-0229`, `BR-0396`, `BR-0331`; no new root |
| `GQC-0177` | `ISSUES` | `GQ1-CHUNK-0164` secret-area scanner tail/API and software renderer debug | `GQD-0057` admits `GQF-0174` for an estimated 40-line/two-hunk state serialization extraction. Quality evidence deduplicates to `BR-0213`, `BR-0214`, `BR-0337`, `BR-0394` |
| `GQC-0178` | `ISSUES` | `GQ1-CHUNK-0165` headless metadata/replay entry points and JNI skeleton | `GQD-0058` admits `GQF-0175` for a 169-177-line CMake extraction. Admitted dead-code `GQF-0176`; duplicated `BR-0209`, `BR-0233` |
| `GQC-0179` | `ISSUES` | `GQ1-CHUNK-0166` introspection tail/API and GLES array-source helper | `GQD-0059` retains 24 additions across four introspection paths; GLES helper has zero marginal inherited effect. Extended `BR-0244`, `BR-0588`; archived `BR-0196` remains closed |
| `GQC-0180` | `ISSUES` | `GQ1-CHUNK-0167` secret-area adapter tail and item names | `GQD-0060` records no additional original-file effect; the proposed header consolidation is only between branch-added paths and `GQF-0174` remains unchanged. Quality evidence duplicates `BR-0213`, `BR-0234` |

## Initial remediation queue

This queue is deliberately much smaller than the coverage queue. Each row is one product-edit chunk for a fresh Sol-medium worker after live overlap review

| ID | State | Finding links | Scope | Prerequisite or hold |
|---|---|---|---|---|
| `GQR-0001` | `DONE` | `GQF-0006` | Add a capacity-aware shared texture extension lookup and exact boundary tests | Root correction accepted in worker scope: all five inherited calls retain their original two-line shape, D1 is `+2/-2`, D2 is `+3/-3`, and focused/platform validation is recorded below |
| `GQR-0002` | `DONE` | `GQF-0007` | Bound DXA mask-name construction and add exact-boundary coverage | Existing shared builder reused; rejection precedes lookup and preserves the destination. Focused runtime/source contracts, Windows D1/D2, and all Android ABIs passed with zero inherited-file edits |
| `GQR-0003` | `TODO` | `GQF-0008` | Restore always-active HUD layout test oracles | Confirm paired CMake configuration |
| `GQR-0004` | `TODO` | `GQF-0009` | Restore always-active escort policy test oracles | D2-only focused target |
| `GQR-0005` | `TODO` | `GQF-0001` through `GQF-0004` | Remove tracked runtime/scratch artifacts and establish narrow recurrence policy | Exact target and provenance audit before deletion |
| `GQR-0006` | `DONE` | `GQF-0005` | Constrain exported automation receivers without breaking intended tests | Central policy, full launcher unit suite, debug/release compilation, all-ABI debug APK, and debug ADB introspection passed |
| `GQR-0007` | `TODO` | `GQF-0011` | Replace ambiguous optional menu selection with stable semantics | Requires focused automation reproduction |
| `GQR-0008` | `TODO` | `GQF-0012` | Pin NAT testbed base image reproducibly | Requires selected digest and testbed build |
| `GQR-0009` | `TODO` | `GQF-0016` | Limit compiler-process cleanup to owned children | Requires Windows script fixture or process-ownership test |
| `GQR-0010` | `DEFERRED` | `GQF-0019` | Evaluate paired ETC2 self-test extraction | Link to DMR1 and wait for current graphics writer overlap to clear |
| `GQR-0011` | `DONE` | `GQF-0024` | Unify and cryptographically verify production Android dependency acquisition | Central manifest/helper, hostile-cache and disconnected-reuse tests, Windows D1/D2, extract-host compile, and all Android ABIs passed |
| `GQR-0012` | `TODO` | `GQF-0025` | Repair quick-test catalog migration and add target-resolution coverage | Determine whether unified rows replace or subsume both deleted entries before editing |
| `GQR-0013` | `TODO` | `GQF-0026` | Remove unsupported launcher file-layout migration state | Reconfirm pre-release reset policy and current producer paths; preserve current-layout behavior |
| `GQR-0014` | `TODO` | `GQF-0027` | Finish audio URI persistence schema migration | Test zero, one, multiple, legacy-only, and conflicting URI records |
| `GQR-0015` | `TODO` | `GQF-0028` | Remove no-op crash Activity compatibility shim | Preserve Application Java handler and native post-load handler ownership |
| `GQR-0016` | `TODO` | `GQF-0029` | Regenerate and bind the stale Anniversary fingerprint | Requires exact source verification; do not guess or edit the digest without reproducible generator evidence |
| `GQR-0017` | `TODO` | `GQF-0030` | Version and bind physical-disc fingerprint generations | Design one compact identity contract shared by generator, checked-in output, and packaged-database publication |
| `GQR-0018` | `TODO` | `GQF-0031` | Make declared CD audio identity/counts executable regression oracles | Reuse setup introspection and add schema plus decision fixtures before emulator coverage |
| `GQR-0019` | `TODO` | `GQF-0032` | Replace size-based combined-component collision selection with explicit ownership | Requires reviewed owner and payload identity for real HAM/HOG collisions plus synthetic order/size fixtures |
| `GQR-0020` | `TODO` | `GQF-0033` | Add non-mutating combined-oracle semantic freshness validation | Share pure derivation with generation and test every helper/component input independently |
| `GQR-0021` | `TODO` | `GQF-0034` | Enforce typed complete mission fingerprint cache/publication schema | Coordinate with generation identity work without merging the independent validation root |
| `GQR-0022` | `TODO` | `GQF-0035` | Restore evidence-backed deterministic AcoustID candidate selection | Share policy with existing metadata clients and retain score/recording provenance |
| `GQR-0023` | `TODO` | `GQF-0036` | Bound SAF manifest decoded allocation and duplicate work | Requires portable entry/field/aggregate budgets and 32-bit stress coverage |
| `GQR-0024` | `DONE` | `GQF-0037` | Route SAF URI and MIDI text through strict standard-UTF-8 JNI conversion | No Modified UTF-8 API remains in either bridge; shared-codec, parser, source, launcher, Windows, and all-ABI Android validation passed |
| `GQR-0025` | `TODO` | `GQF-0038` | Replace HFS wrapper fixed-buffer joins with checked path ownership | Live file currently overlaps another writer; rebase or defer before edits |
| `GQR-0026` | `TODO` | `GQF-0039` | Give HFS installer scratch files exclusive attempt ownership | Coordinate descriptor/map lifetime and concurrency fixtures |
| `GQR-0027` | `TODO` | `GQF-0040` | Make flattened HFS collision policy explicit and order-independent | Requires synthetic catalog/encoding/STi2 collision matrix and real-media hash preservation |
| `GQR-0028` | `TODO` | `GQF-0041` | Preserve valid SOW append prefixes on later failure | Choose complete sibling publication or descriptor-owned truncation rollback and inject write/close faults |
| `GQR-0029` | `TODO` | `GQF-0042` | Give SOW output leaves no-follow regular-file ownership | Medium confidence; first unify portable root/handle policy with other extractor sinks |
| `GQR-0030` | `TODO` | `GQF-0043` | Correct SOW filtered progress population accounting | Add public API decision fixtures with skipped/stored/compressed mixes |
| `GQR-0031` | `TODO` | `GQF-0044` | Align PKG no-audio free-space checks with selected outputs | Preserve full stream integrity budgets and test storage thresholds around both totals |
| `GQR-0032` | `TODO` | `GQF-0045` | Make PKG JNI progress stable and cumulative | Test unequal multi-file and audio-filtered callback sequences |
| `GQR-0033` | `TODO` | `GQF-0046` | Make direct PKG and standalone ISO publication transactional | Coordinate shared staging contract while preserving launcher CUE transaction behavior |
| `GQR-0034` | `TODO` | `GQF-0047` | Enforce physical ISO output containment | Requires POSIX symlink and Windows reparse/race fixtures and exact owned-root policy |
| `GQR-0035` | `TODO` | `GQF-0048` | Define ISO normalized version/collision semantics | Preflight exact portable destination map before any writes |
| `GQR-0036` | `TODO` | `GQF-0049` | Implement or explicitly reject ISO extended/interleaved layouts | Prefer narrow fail-closed support decision with valid/malformed fixtures |
| `GQR-0037` | `TODO` | `GQF-0050` | Scan ISO volume descriptor sequences correctly | Bound scan and test boot-first, duplicates, missing terminator and ordinary images |
| `GQR-0038` | `TODO` | `GQF-0051` | Enforce one peak-live-memory budget for STi2 method 15 | Include output, block, transform, archive and caller overhead with exact/OOM tests |
| `GQR-0039` | `TODO` | `GQF-0052` | Make assigned JNI acquisitions and allocations exception-safe | Coordinate with `GQF-0037` text conversion only where the same boundary/test owns both |
| `GQR-0040` | `TODO` | `GQF-0053` | Bind Inno extraction to its analyzed source generation | Cover path and provider descriptor swaps, same-count substitutions and both audio modes |
| `GQR-0041` | `TODO` | `GQF-0054` | Propagate native JSON/output stream failures | Add injectable sink/flush failures while preserving strict downstream parse/publication gates |
| `GQR-0042` | `TODO` | `GQF-0055` | Require route proof before every reconnect state mutation | Pair D1/D2 and include migration counter preservation plus proxy/NAT/replay matrix |
| `GQR-0043` | `TODO` | `GQF-0056` | Version and domain-separate reconnect transcripts/generations | Protocol-visible change requires cross-title, endian, migration and downgrade vectors |
| `GQR-0044` | `TODO` | `GQF-0057` | Bound unauthenticated reconnect verification before JNI/JCA | Requires game-thread latency, heap, route and NAT fairness load tests |
| `GQR-0045` | `TODO` | `GQF-0058` | Bound and cancel complete-track fingerprint work | Coordinate generation identity `GQF-0030`; stream compressed formats and measure peak resources |
| `GQR-0046` | `TODO` | `GQF-0059` | Preserve lossless fingerprint ranking evidence | Separate machine comparison fields from presentation precision and test near-ties/order |
| `GQR-0047` | `TODO` | `GQF-0060` | Make exact-collision duration decisions symmetric | Share policy with native matchers and fix the parity oracle |
| `GQR-0048` | `TODO` | `GQF-0061` | Carry one budget through complete CD composition | Include every track, fallback and nested SOW operation with exact/one-over cleanup tests |
| `GQR-0049` | `TODO` | `GQF-0062` | Resolve or reject Windows drive-relative output identity | Medium confidence; validate across two volumes, UNC and nonexistent ancestors |
| `GQR-0050` | `TODO` | `GQF-0063` | Make CUE FILE grammar strict and source ordering shared | Coordinate raw-path and SAF flows and ordinary multi-file compatibility |
| `GQR-0051` | `TODO` | `GQF-0064` | Reject oversized CUE physical lines without synthetic parsing | Add exact byte/CRLF/no-final-newline/Kotlin parity matrix |
| `GQR-0052` | `TODO` | `GQF-0065` | Define and enforce CUE track identity rules | Requires corpus compatibility audit before choosing contiguity/gap policy |
| `GQR-0053` | `TODO` | `GQF-0066` | Make CUE parsing length-aware and reject embedded NUL | Thread exact byte length through all callers and compare Kotlin/native outcomes |
| `GQR-0054` | `TODO` | `GQF-0067` | Restrict fingerprint CLI enumeration to owned regular inputs | Requires Windows reparse and POSIX symlink/FIFO/device/replacement fixtures |
| `GQR-0055` | `TODO` | `GQF-0068` | Register audio enumeration regression coverage | Prefer portable CTest behavior with aggregate discovery and seeded failure proof |
| `GQR-0056` | `TODO` | `GQF-0069` | Bind each CD fingerprint run to one immutable BIN generation | Reuse descriptors for geometry and all tracks; inject same-size replacement and in-place mutation |
| `GQR-0057` | `TODO` | `GQF-0070` | Make Windows CD fingerprint paths Unicode-safe | Define CUE filename encoding and test BMP, supplementary, malformed and long identities |
| `GQR-0058` | `TODO` | `GQF-0071` | Validate the complete HFS catalog parent graph before projection | Cover roots, orphans, file parents, cycles, depth, path capacity and order independence |
| `GQR-0059` | `TODO` | `GQF-0072` | Bound and cancel HFS metadata and ancestry work | Share attempt budgets, index CNIDs/paths and add exact/one-over hostile catalogs |
| `GQR-0060` | `TODO` | `GQF-0073` | Accept standards-conforming multi-node HFS allocation maps | Preserve all forward/allocation/bitmap checks and add two-/three-map valid and malformed fixtures |
| `GQR-0061` | `TODO` | `GQF-0074` | Require complete classic-HFS catalog record schemas | Test every boundary around fixed folder/file sizes and exact key/name/padding rules |
| `GQR-0062` | `TODO` | `GQF-0075` | Preserve the PE resource leaf bound through offset-table parsing | Use layout-specific exact reads and short/exact/adjacent-byte PE32/PE32+ fixtures |
| `GQR-0063` | `TODO` | `GQF-0076` | Enforce one peak-live-memory budget for Inno metadata decode | Instrument stored/raw/output/dictionary/probability allocations at exact and one-over combinations |
| `GQR-0064` | `TODO` | `GQF-0077` | Require strict setup-header LZMA terminal state | Share payload completion logic and add truncated/no-progress/missing-marker/trailing-byte fixtures |
| `GQR-0065` | `TODO` | `GQF-0078` | Make Inno version admission overflow-free | Validate supported tuples before encoding and run complete-open boundaries under UBSan where available |
| `GQR-0066` | `TODO` | `GQF-0079` | Preserve and assemble Galaxy multipart groups | Validate part count/order/group metadata and publish only a complete atomic assembly |
| `GQR-0067` | `TODO` | `GQF-0080` | Model the complete Windows destination namespace | Add reserved-device and Unicode case-alias preflight fixtures using wide filesystem behavior |
| `GQR-0068` | `TODO` | `GQF-0081` | Bind Inno chunks to validated split-volume sources | Decide support versus fail-closed rejection and test missing/swapped/duplicate slices |
| `GQR-0069` | `TODO` | `GQF-0082` | Route solid chunks by complete decoded prefix | Cover high-offset small files and exact-capacity terminal behavior across decoders |
| `GQR-0070` | `TODO` | `GQF-0083` | Bound aggregate Inno solid-chunk decode work | Cache or charge alias restarts and test exact/one-over cumulative source and output work |
| `GQR-0071` | `TODO` | `GQF-0084` | Validate Galaxy external size before final publication | Inject mismatch and process-death boundaries and prove no invalid final leaf becomes visible |
| `GQR-0072` | `TODO` | `GQF-0085` | Make Inno output-name uniqueness preflight bounded | Use one canonical-name map and maximum-entry/common-prefix work tests |
| `GQR-0073` | `TODO` | `GQF-0086` | Define truthful Inno solid-chunk progress | Separate source work from selected output and cover sparse selections and cancellation |
| `GQR-0074` | `TODO` | `GQF-0087` | Scope or remove ISO `zero` directory suppression | Preserve the intended D2 corpus rule without hiding ordinary cross-media directories |
| `GQR-0075` | `TODO` | `GQF-0088` | Enforce ISO declared-volume containment | Test exact-end and outside-volume roots, directories and files within a larger outer source |
| `GQR-0076` | `TODO` | `GQF-0089` | Make bounded CUE titles preserve valid UTF-8 | Cover complete scalars around every byte boundary through parser and CheckJNI bridge |
| `GQR-0077` | `TODO` | `GQF-0090` | Parse exact XAR field structure | Reject nested/decoy/duplicate wrong-scope fields before gzip or catalog publication |
| `GQR-0078` | `TODO` | `GQF-0091` | Bind PKG extraction to a collision-resistant source generation | Include deterministic equal-length CRC32-collision and mutable-provider fixtures |
| `GQR-0079` | `TODO` | `GQF-0092` | Make zero-length SOW extraction allocation-independent | Test null-returning and non-null-returning zero allocation behavior with empty output |
| `GQR-0080` | `TODO` | `GQF-0093` | Add a successful production STi2 method-14 oracle | Register a legal archive fixture with exact bytes/hash and mutation proof |
| `GQR-0081` | `TODO` | `GQF-0094` | Move STi2 fixed entry catalogs off native stack | Preserve caps and validate direct plus nested extraction under constrained stack |
| `GQR-0082` | `TODO` | `GQF-0095` | Make level-metadata runtime initialization transactional | Inject every init failure and prove clean retry or terminal worker replacement |
| `GQR-0083` | `TODO` | `GQF-0096` | Preserve complete resume-save mission identity | Test short, long, colliding-prefix and hashed custom missions across metadata/path discovery |
| `GQR-0084` | `TODO` | `GQF-0097` | Move resume-save discovery off the Compose looper | Use one cancellable indexed scan and test empty/large roots plus lifecycle restart |
| `GQR-0085` | `TODO` | `GQF-0098` | Bind conditional save deletion to one file generation | Inject same-slot replacement and check/delete races without deleting the replacement |
| `GQR-0086` | `TODO` | `GQF-0099` | Freeze one SAF mounted-source generation | Test same-size and in-place provider mutation for seekable and staged sources |
| `GQR-0087` | `TODO` | `GQF-0100` | Require valid ZIP structure before lifting prompt limits | Cover false early markers and near-limit preambles without large staging |
| `GQR-0088` | `TODO` | `GQF-0101` | Select a fully valid ZIP EOCD candidate | Test comment decoys, multiple candidates and exact scan boundaries |
| `GQR-0089` | `TODO` | `GQF-0102` | Bind ZIP extraction to the validated central entry set | Add central-unlisted and duplicate local-entry fixtures |
| `GQR-0090` | `TODO` | `GQF-0103` | Apply one extraction budget to direct Setup ZIP import | Test all bypass paths at exact and one-over limits |
| `GQR-0091` | `TODO` | `GQF-0104` | Enforce peak-live-memory policy in Kotlin bounded reads | Include buffer growth and copy overlap with allocation instrumentation |
| `GQR-0092` | `TODO` | `GQF-0105` | Share song filename capacity with both engines | Cover exact/over-limit and colliding retained tokens |
| `GQR-0093` | `TODO` | `GQF-0106` | Key contained-track sidecars by member identity | Test two or more tracks in one DXA/HOG |
| `GQR-0094` | `TODO` | `GQF-0107` | Separate sidecar freshness from identification completeness | Retry failed tracks without repeating complete successes |
| `GQR-0095` | `TODO` | `GQF-0108` | Prevent weaker fallback after RAR policy rejection | Preserve typed failure through every backend dispatcher |
| `GQR-0096` | `TODO` | `GQF-0109` | Bound RAR enumeration before materialization | Test exact/over entry, work and memory limits |
| `GQR-0097` | `TODO` | `GQF-0110` | Preserve staged RAR source ownership | Inject fallback failure/retry and assert source retention |
| `GQR-0098` | `TODO` | `GQF-0111` | Validate loadable mission level payloads | Use production parser acceptance and malformed tiny fixtures |
| `GQR-0099` | `TODO` | `GQF-0112` | Preserve qualified mission descriptor identities | Test nested same-leaf and engine resolution parity |
| `GQR-0100` | `TODO` | `GQF-0113` | Reject stored ZIP collisions before registration | Cover below/at/above streaming threshold |
| `GQR-0101` | `TODO` | `GQF-0114` | Remove or restore `isMissionHog` ownership | Require symbol reachability and focused behavior evidence |
| `GQR-0102` | `TODO` | `GQF-0115` | Bind mission scan to one extraction source generation | Inject replacement between scan, hash and stage |
| `GQR-0103` | `TODO` | `GQF-0116` | Eliminate repeated synchronous mission freshness hashing | Measure ordinary launch on large source/output sets |
| `GQR-0104` | `TODO` | `GQF-0117` | Include generated aliases in storage and progress totals | Test large alias content and exact free-space boundary |
| `GQR-0105` | `TODO` | `GQF-0118` | Carry one catalog budget through nested music containers | Stress maximum outer/inner combinations with bounded retention |
| `GQR-0106` | `TODO` | `GQF-0119` | Preserve nested streaming size and expansion accounting | Test data descriptors, zero/negative metadata and large MIDI output |
| `GQR-0107` | `TODO` | `GQF-0120` | Lease staged music generations across preview use | Cover asynchronous preview and cleanup concurrency |
| `GQR-0108` | `TODO` | `GQF-0121` | Bound NAT simulator live mappings and tasks | Stress per-client/global caps, allowlists, socket pools and teardown |
| `GQR-0109` | `TODO` | `GQF-0122` | Make sequential-port prediction conservative | Cover duplicate, uneven, reordered, wrap and insufficient sample sets |
| `GQR-0110` | `TODO` | `GQF-0123` | Bind TinySoundFont updates to incremental rebuilds | Test two pinned generations with older/newer archive mtimes under minimum and current supported CMake, and compare incremental with clean output |
| `GQR-0111` | `TODO` | `GQF-0124` | Contain mission-batch diagnostic capture failures | Inject every diagnostic I/O failure and require primary outcome, remaining items, summary, recovery and app cleanup |
| `GQR-0112` | `TODO` | `GQF-0125` | Bind bounded extraction to an admitted Python runtime | Cover hostile PATH precedence, missing runtime and cross-platform resolution |
| `GQR-0113` | `TODO` | `GQF-0126` | Preserve zero-length HFS files | Test empty files alongside normalization collisions and ordinary data forks |
| `GQR-0114` | `TODO` | `GQF-0127` | Supervise bounded extractor process trees | Spawn descendants on success/failure/timeout and prove complete teardown |
| `GQR-0115` | `TODO` | `GQF-0128` | Reject unsafe extractor output types | Cover symlinks, hard links, FIFOs/devices where supported, physical escape and byte-accounting bypass |
| `GQR-0116` | `TODO` | `GQF-0129` | Make the POSIX CUE/ISO entry point executable | Validate direct documented invocation from a fresh checkout on supported POSIX hosts |
| `GQR-0117` | `TODO` | `GQF-0130` | Serialize extraction publication by destination | Race two successful and failing callers and prove winner/backup generation ownership |
| `GQR-0118` | `TODO` | `GQF-0131` | Validate DOS demo output before first publication | Test missing, truncated, swapped and wrong required payloads with prior-generation preservation |
| `GQR-0119` | `TODO` | `GQF-0132` | Share strict CUE sector geometry with Mac extraction | Cover supported modes and reject unknown/inconsistent layouts |
| `GQR-0120` | `TODO` | `GQF-0133` | Validate Mac extraction CUE INDEX fields | Cover missing, malformed, duplicate and out-of-order INDEX 01 records |
| `GQR-0121` | `TODO` | `GQF-0134` | Bind Mac extraction cache to supervisor policy | Change helper/policy generations and require cache invalidation |
| `GQR-0122` | `TODO` | `GQF-0135` | Bound every Mac demo external process | Exercise hang, descendant, output flood, cancellation and cleanup for both phases |
| `GQR-0123` | `TODO` | `GQF-0136` | Regenerate and enforce Mac demo oracle provenance | Reject legacy/mismatched schema, source, tool, helper and policy generations |
| `GQR-0124` | `TODO` | `GQF-0137` | Bound mission music archive nesting | Test exact/over depth, cycles and mixed ZIP/DXA chains |
| `GQR-0125` | `TODO` | `GQF-0138` | Make tracklist selector conflicts explicit | Cover duplicate identical, duplicate conflicting, overlapping and reordered rows |
| `GQR-0126` | `TODO` | `GQF-0139` | Retire stale audio generations on no-audio success | Transition audio-to-none and require current cache/UI state with atomic prior-generation handling |
| `GQR-0127` | `TODO` | `GQF-0140` | Bound native mission fingerprint processes | Exercise parent and descendant hangs, cancellation, cleanup and continuation to later missions |
| `GQR-0128` | `TODO` | `GQF-0141` | Preserve graphics transaction originals through rollback | Inject publication and rollback failure at every file and prove recoverable original ownership |
| `GQR-0129` | `TODO` | `GQF-0142` | Close graphics transaction descriptors on sync failure | Inject parent-directory sync failures and audit descriptor counts on every exit |
| `GQR-0130` | `TODO` | `GQF-0143` | Make CUE extension-filter tests exact | Require successful listing and exact identities for reject, null and mixed-extension cases |
| `GQR-0131` | `TODO` | `GQF-0144` | Validate HMP output through production wrappers | Cover D1/D2 exact structure, tempo, payload ownership, null/empty/corrupt output and cleanup |
| `GQR-0132` | `TODO` | `GQF-0145` | Make invalid fingerprint database configuration fail closed | Test valid-to-invalid transitions, matching, reload and concurrent readers |
| `GQR-0133` | `TODO` | `GQF-0146` | Remove unused CUE test helpers | Preserve active fixture coverage and verify symbol reachability plus focused test build |
| `GQR-0134` | `TODO` | `GQF-0147` | Reject reserved Windows ISO output components | Cover base and extension variants, case, trailing spaces/dots and ordinary near-matches |
| `GQR-0135` | `TODO` | `GQF-0148` | Use production heap ownership in CUE/ISO tests | Remove platform stack workaround and run under constrained-stack hosts and sanitizers |
| `GQR-0136` | `TODO` | `GQF-0149` | Prove fingerprint input-byte and stream parity | Mutate controlled sectors and compare exact direct and production streaming results |
| `GQR-0137` | `TODO` | `GQF-0150` | Validate raw fingerprint fixtures exactly | Cover short header/payload, overflow, trailing bytes and read errors |
| `GQR-0138` | `TODO` | `GQF-0151` | Make fingerprint assertions evaluate operands once | Add side-effect operands and preserve first observed values in diagnostics |
| `GQR-0139` | `TODO` | `GQF-0152` | Initialize GOG LZMA trailing-data fixtures | Make intended prefix and trailing bytes explicit and run under memory sanitizers where available |
| `GQR-0140` | `TODO` | `GQF-0153` | Make D2 Mac extraction oracle content-exact | Bind verified source and complete expected inventory, sizes and SHA-256; mutate bytes, truncate, swap, add and remove leaves |
| `GQR-0141` | `TODO` | `GQF-0154` | Make empty music generations explicit | Inject deletion failure and stale same-source sidecars; require verified removal or atomic empty generation with diagnostics |
| `GQR-0142` | `DONE` | `GQF-0155` | Finish paired Android PhysFS init extraction | Shared checked initialization, diagnostics, setup and Android argument initialization now have one owner; exact order, compact game-directory seams, Windows D1/D2, all Android ABIs and mount order are validated |
| `GQR-0143` | `DONE` | `GQF-0156` | Consolidate paired menu/window debug accessors | Regular shared `.h/.c` modules own the public API, with private-layout snapshot adapters retained in D1/D2. Source contracts, Windows D1/D2, all configured Android ABIs, and focused D1/D2 introspection-driven menu automation passed |
| `GQR-0144` | `TODO` | `GQF-0157` | Make bounded-extractor quota tests reason-exact | Independently disable each quota and timeout and require the matching test to fail promptly with the expected category |
| `GQR-0145` | `TODO` | `GQF-0158` | Register bounded-extractor Python regression | Add it to maintained branch-added host-test discovery and prove pass/fail/not-run reporting on Windows and POSIX |
| `GQR-0146` | `TODO` | `GQF-0159` | Require source-bearing extraction regression specs | Reject absent, empty and malformed source sets for every schema discriminator and test readiness cannot bypass admission |
| `GQR-0147` | `TODO` | `GQF-0160` | Put host-only extraction tests in the host tier | Register provenance and publication scripts without APK/emulator prerequisites and prove aggregate discovery and failure propagation |
| `GQR-0148` | `TODO` | `GQF-0161` | Remove stale extraction failure state | Replace `$maxNav` with current bounded context and exercise the exact-level failure path under strict mode |
| `GQR-0149` | `TODO` | `GQF-0162` | Make extraction workflow assertions behavioral | Replace comment/token regex checks with structured or executable production-contract oracles and mutation-prove each assertion |
| `GQR-0150` | `TODO` | `GQF-0163` | Bind app-private extraction staging to source identity | Verify local and device digests, publish through an owned temporary and test equal-size replacement, interruption and concurrent generations |
| `GQR-0151` | `TODO` | `GQF-0164` | Make descriptor-only SAF success observation-gated | Fail missing readiness/process/listing evidence and inject suppressed launch, dead process, unreadable descriptors, retained manifest and clean release |
| `GQR-0152` | `TODO` | `GQF-0165` | Register and bound the WAV parser regression | Add a maintained branch-added host-test entry with a strict timeout, reason-exact malformed fixtures and aggregate pass/fail/timeout/not-run evidence |
| `GQR-0153` | `TODO` | `GQF-0166` | Complete and guard the server configuration template | Add `max_connections` and `force_relay`, then require exact agreement between accepted file keys and the claimed exhaustive template |
| `GQR-0154` | `TODO` | `GQF-0167` | Publish final audio samples before producer completion | Make final ring write and source-finished one ordered producer transaction; barrier-test MIDI and PCM tail delivery, exact one-shot completion, replacement and explicit stop |
| `GQR-0155` | `TODO` | `GQF-0168` | Decode replay direct commands once per frame | Add a bounded typed batch or one-pass acquisition API, preserve validate-before-apply, and mutation-test linear parse counts plus maximum and over-limit batches in both games |
| `GQR-0156` | `DONE` | `GQF-0169` | Consolidate paired Redbook Android declarations | One conventional shared header owns all eight declarations; paired includes, exact C/C++ signatures, Windows D1/D2 and all configured Android ABI links passed with 18 inherited lines removed |
| `GQR-0157` | `DONE` | `GQF-0170` | Finish shared HMP wrapper extraction | The shared owner now implements the public wrapper with canonical tempo data; exact successful MIDI bytes, focused failure paths, Windows D1/D2, and all configured Android ABIs passed with zero remaining inherited HMP diff |
| `GQR-0158` | `TODO` | `GQF-0171` | Remove unused paired FP environment includes | Delete the four unconsumed inherited includes and run focused input-demo tests plus paired D1/D2 Windows and Android builds |
| `GQR-0159` | `DONE` | `GQF-0172` | Consolidate Android mixer init diagnostics | Shared diagnostics now own actual-spec queries and exact raw/detailed messages; focused ownership contracts, Windows D1/D2 and all configured Android ABIs passed with 46 inherited lines removed |
| `GQR-0160` | `TODO` | `GQF-0173` | Make SDL audio callback lifetime entry-safe | Acquire lifetime ownership before any callback dereference; serialize player publication, pause/resume and teardown; barrier-test close/reopen, enqueue failure and callback entry under race instrumentation |
| `GQR-0161` | `DONE` | `GQF-0174` | Move secret-area serialization and interface into its adapter | The shared C/header pair preserves rewind/PhysicsFS ownership, exact layout, version ordering, swapped reads, fallback and unchanged include spelling; 28 focused contracts, Windows D1/D2 and all Android ABIs passed with 38 original-file lines plus both 42-line branch-added D1/D2 headers removed |
| `GQR-0162` | `DONE` | `GQF-0175` | Extract paired headless CMake target policy | Root correction accepted in worker scope: exact normalized CMake File API properties match frozen HEAD for all three targets; Windows default and option-matrix validation plus maintained metadata/replay fixtures passed, with Linux/Apple execution unavailable on this Windows host |
| `GQR-0163` | `TODO` | `GQF-0176` | Remove the orphan JNI skeleton | Delete `native-lib.cpp`, verify inventories have no reference, link both Android game targets and prove `helloFromNative` resolves through `jni_main.c` |

### GQR-0142 completion

- Status: `DONE`; linked `GQF-0155` is `FIXED`. Historical impact remains 57 (`12/21/7/10/7`, `MEDIUM-HIGH`) with terminal state removing it from dispatch
- Live boundary: pre-edit and final HEAD remained `339a101b5fbdcbea79989168c0378af985c0cdbe`; overlap was `SAFE_DISJOINT`. The concurrent GQR-0143 menu/window paths, shared introspection files, and contract test remained untouched
- Changed paths: `android/app/src/main/cpp/shared/physfsx_android_shared.c`, `physfsx_android_shared.h`, `test_physfsx_android_setup.c`, new `android/tests/test_physfsx_android_init_contracts.py`, and paired `d1/misc/physfsx.c` / `d2/misc/physfsx.c`
- Boundary and behavior: `physfsx_android_init(argc, argv, game_dir)` now owns checked `PHYSFS_init`, `PHYSFS_permitSymbolicLinks`, the existing transactional search-path setup and exact failure diagnostics, then `InitArgsAndroid`. D1 and D2 retain only `d1x-redux` / `d2x-redux` calls and early returns. Desktop `PHYSFS_init`, symbolic-link setup and all later flow are unchanged; game-independent mount policy remains in `physfsx_android_setup.c`
- Exact metrics against merge base `fb555eec75e1ed12c8348805ab335afb4c721b06`: before, each inherited path had 20 relevant Android additions across three hunks, 40 lines/six hunks total. After, each retains seven additions across two hunks, 14 lines/four hunks total. This exactly removes 26 inherited lines and two hunks. Full paired numstat is now `+14/-0` per path because each also retains an unrelated seven-line Windows absolute-path fix
- Validation: `python -m unittest android.tests.test_physfsx_android_init_contracts` passed two focused source-contract tests. Rebuilt `test_physfsx_android_setup` and `ctest -R '^physfsx_android_setup_tests$'` passed, covering exact mount order, rollback/failure diagnostics, optional fallbacks, and both game-directory parameters. `run-windows-build.ps1 -Target both` built and linked D1 and D2. With JDK 21, `:app:externalNativeBuildDebug` built both affected targets for `arm64-v8a`, `armeabi-v7a`, and `x86_64`
- Quality and audit: scoped `run-code-quality.ps1 -Fix` passed for all four changed branch-added code/test files. Final scoped `git diff --check` passed; all six product/test files are printable ASCII without BOM. No additional emulator mission-import run was needed because the maintained compiled PhysFS setup test directly exercises initialization mount order and failure behavior without launcher state. Final status contains only this six-path chunk, the campaign records, and the preserved unrelated GQR-0143 work

### GQR-0143 completion

- Status: `DONE`; linked `GQF-0156` is `FIXED`. Historical impact remains 75 (`23/28/7/10/7`, `HIGH`) with terminal state removing it from dispatch
- Worker and overlap: one fresh `gpt-5.6-sol` medium worker. Pre-edit `HEAD` was `339a101b5fbdcbea79989168c0378af985c0cdbe`; overlap was `SAFE_DISJOINT`. Only this ledger and the current campaign plan were dirty from the root claim. All eight product paths were clean and hashed before edits: D1 `newmenu.c` `9d51dedb6c952e1a566b22a87a1105bb1ea87d81ded3bf441e6c0ac67467817e`, D2 `newmenu.c` `2fadb0440d6feb36bb8e8bb2bfea14ad29432c66ed3bae57ae22a51e6b465c97`, D1 `newmenu.h` `8838339ad5d15c875a1d45ab857cd441ac3ea24af00a781accb8ce04dd6c5020`, D2 `newmenu.h` `19510cf82ceb177986c54e44ee048c582e33fde6416c105f2a6025ac6570bc14`, paired `window.c` `e94d4a8486daa52bd3674325c420273e86ed168bd6552463fe840c65a6d3a183`, and paired `window.h` `912235476a32daca3c685222787dc15eba9d11d333be6f48566bdf7ec7bbcfa1`
- Boundary: the user-requested regular-source follow-up replaces all three type-complete `.inc` fragments with `game_menu_introspect_accessors.h/.c` and `game_window_introspect_accessors.h/.c`. The ordinary shared sources implement the same nine public accessors. Paired `newmenu.c` and `window.c` retain small snapshot adapters because only those translation units can legally see the opaque private layouts; no private struct is moved, mirrored, or exported. No callback table, menu geometry, selection policy, frame orchestration, public API, C linkage, guard, or consumer changed
- Before metrics against `upstream/main`: eight inherited paths at `+2686/-262`: D1 window header `+6/-0`, window source `+16/-4`, newmenu source `+1310/-119`, newmenu header `+11/-0`; D2 `+6/-0`, `+16/-4`, `+1301/-126`, and `+20/-9`. The precise live removable accessor/declaration bodies were 144 inherited lines across 12 hunks
- After regular-source metrics against `upstream/main`: the same paths are `+2604/-262`: D1 `+2/-0`, `+12/-4`, `+1287/-119`, `+1/-0`; D2 `+2/-0`, `+12/-4`, `+1278/-126`, `+10/-9`. The isolated inherited worktree delta is `+30/-112`, a net reduction of 82 lines; branch-attributed additions against upstream also fall by 82. This intentionally gives back 52 lines versus the fragment implementation so private layouts remain private while all implementation files use conventional extensions
- Validation: `python -m unittest android.tests.test_introspection_accessor_contracts` passed two focused tests that reject `.inc` files and inherited public bodies, enforce the narrow adapters, ordinary source registrations, guards, signatures and field mappings. `run-windows-build.ps1 -Target both` built and linked D1, D2 and both headless targets successfully. With JDK 21, `:app:externalNativeBuildDebug` built both `INTROSPECT_ON` game targets for `arm64-v8a`, `armeabi-v7a`, and `x86_64`. The maintained `test_newmenu_render_paths_unified.json5` had already passed separately for D1 and D2 on `emulator-5554`, exercising the unchanged public API through introspection. Scoped quality passed for the regular shared modules, test and Android CMake owner
- Limitations: platform builds emitted pre-existing unrelated inherited warnings. The focused automation used the available provisioned emulator and proprietary game-data dependencies, so there is no emulator prerequisite gap. No Linux or Apple build was run because the requested paired Windows and Android validation covered the moved C include boundary on this Windows host
- Out-of-scope audit: the two ordinary shared sources are registered only in the branch-added Android CMake owner for both game targets. No inherited build file, unrelated accessor, or policy was edited. Build and emulator artifacts remain ignored. Final ASCII/no-BOM, focused test rerun, `git diff --check`, exact numstat, allowed-path and worktree audits are recorded in the terminal validation

### GQR-0156 completion

- Status: `DONE`; linked `GQF-0169` is `FIXED`. Historical impact remains 57 (`12/21/7/10/7`, `MEDIUM-HIGH`) with terminal state removing it from dispatch
- Live boundary: claim began at `339a101b5fbdcbea79989168c0378af985c0cdbe` and continued at `bbedca85805246879ce7d31d69f54ce5edddd3fc` after a user-owned cleanup commit landed. This was `DRIFT_SAFE_EXTERNAL_COMMIT`: it committed the already validated GQR-0142/GQR-0143 work, did not overlap the clean paired Redbook headers, and was preserved without reset, checkout, clean, stash or commit by this worker
- Boundary: new conventional `android/app/src/main/cpp/shared/rbaudio_bin.h` matches its `rbaudio_bin.c` implementation owner and is the sole owner of the exact eight type-complete track-control declarations. D1 and D2 each retain one `#include "rbaudio_bin.h"` at the former block position. Existing unconditional visibility, C and C++ linkage context, function-pointer types, include ordering, consumers and non-Android behavior are unchanged. No `.inc`, generated include, layout mirror, source wrapper or CMake edit was added
- Exact metrics against campaign merge base `fb555eec75e1ed12c8348805ab335afb4c721b06`: before, each inherited header had 11 additions in one hunk, 22 additions/two hunks total. After, each has two additions in one hunk, four additions/two hunks total, an exact 18-line reduction. The isolated worktree edit is `+1/-10` per header, `+2/-20` total. This corrects the historical estimate of 22 removed inherited lines; the estimate counted blank/context lines, and Git still presents one replacement hunk per header
- Validation: `python -m unittest android.tests.test_rbaudio_bin_header_contracts` passed three tests, including exact C and C++ signature compilation through both game headers with `-Werror`, single-owner/include checks and production-consumer checks. `run-windows-build.ps1 -Target both` built and linked D1, D2 and their headless targets. With JDK 21, `:app:externalNativeBuildDebug` built both game targets for `arm64-v8a`, `armeabi-v7a` and `x86_64`. Scoped code quality passed for the new header and focused test
- Limitation: maintained `test_saf_redbook.ps1` ran on `emulator-5554` but stopped before Redbook playback at step 21 because the automation remained on the pilot list and could not select `New game`. This pre-playback navigation failure is unrelated to the declaration-only boundary and provides no additional runtime evidence; compile and link parity remain the primary behavior proof

### GQR-0157 completion

- Status: `DONE`; linked `GQF-0170` is `FIXED`. Historical impact remains 57 (`12/21/7/10/7`, `MEDIUM-HIGH`) with terminal state removing it from dispatch
- Live boundary: work began at `5701fcd2866689d7f8103e07dc7a51558197832b` with a clean product scope. The only initial worktree path was this chunk's new durable plan; no unrelated product changes were present or modified
- Boundary: `android/app/src/main/cpp/shared/hmp_android_shared.c` now owns the public `hmp2mid_mem` symbol and the canonical 19-byte tempo track. Its parameterized converter is private to that translation unit. Existing callers, allocation through `d_malloc`/`d_realloc`/`d_free`, `hmp_close` linkage, output ownership, original per-game file converter and its tempo arrays remain unchanged
- Exact metrics against campaign merge base `fb555eec75e1ed12c8348805ab335afb4c721b06`: before, paired `d1/misc/hmp.c` and `d2/misc/hmp.c` each carried 12 additions in two hunks, 24 additions/four hunks total. After, both paths are byte-equivalent to merge base with empty numstat and zero hunks, an exact 24-line/four-hunk inherited reduction
- Focused validation: `hmp_android_shared_tests` built with MSVC and passed through CTest. The maintained test now calls the public wrapper and compares all 49 emitted MIDI bytes, including header, division, canonical tempo event, track length and converted payload; malformed-input and allocation-failure coverage continues through the same API
- Platform validation: `run-windows-build.ps1 -Target both` configured, built and linked the full D1 and D2 trees. With JDK 21, `:app:externalNativeBuildDebug` built `arm64-v8a`, `armeabi-v7a` and `x86_64`. Scoped code quality and final `git diff --check` passed
- Scope audit: changed product/test paths are the shared HMP source/header, its focused host test, and a corrected ownership comment in `midi_preview.c`; paired inherited files contain no remaining branch diff. `GQR-0131` remains independently open for any broader production-wrapper validation beyond this exact shared public API oracle

### GQR-0159 completion

- Status: `DONE`; linked `GQF-0172` is `FIXED`. Historical impact remains 57 (`12/21/7/10/7`, `MEDIUM-HIGH`) with terminal state removing it from dispatch
- Live boundary: work began at `9b79b8de889ab1920a2d5b9216e4e508be6afbcb` with a clean product scope. The only initial worktree path was this chunk's new durable plan; no unrelated changes were modified
- Boundary: `android_audio_diagnostics.c` now owns `Mix_QuerySpec`, the exact `Mix_OpenAudio ok: requested=...` raw message, the existing detailed `[audio] init: ...` message and the exact open-failure raw message. Its public init API accepts only requested rate and mixer buffer frames. D1 and D2 retain the existing `Mix_OpenAudio`, urgent console error, no-sound state, requested-rate selection, buffer size and one compact shared call per outcome
- Exact metrics against campaign merge base `fb555eec75e1ed12c8348805ab335afb4c721b06`: D1 falls from `+87/-8` to `+64/-8`; D2 falls from `+84/-9` to `+61/-9`. The isolated paired worktree edit is `+8/-54`, an exact 46-line reduction in inherited branch additions. The survey's 48-line estimate counted two presentation lines that remain necessary for the paired failure calls
- Focused validation: `python -m unittest android.tests.test_android_mixer_diagnostics_contracts` passed two tests. The contracts require the narrow declarations, one shared actual-spec query, exact success/failure format strings and detailed fields, prohibit inherited raw Android logging/query locals, and require paired requested-rate/buffer calls
- Platform validation: `run-windows-build.ps1 -Target both` configured, built and linked full D1 and D2 trees. With JDK 21, `:app:externalNativeBuildDebug` built `arm64-v8a`, `armeabi-v7a` and `x86_64`. Scoped code quality and `git diff --check` passed
- Non-goals: mixer open/close ordering, native-rate selection, sample conversion, SFX latency probes and the separate open `GQF-0173` callback-lifetime problem are unchanged

### GQR-0161 completion

- Status: `DONE`; linked `GQF-0174` is `FIXED`. Historical impact remains 57 (`12/21/7/10/7`, `MEDIUM-HIGH`) with terminal state removing it from dispatch
- Live boundary: work began at `4891e3dd6decae1e679eaa2a18ea923ab7a5b3f4` with a clean product scope. The only initial worktree path was this chunk's new durable plan; no unrelated change was modified
- Boundary: the per-game-compiled `secretarea.c` now owns the runtime writer and reader. A platform-selected `rewind_file` API preserves Android memory-backed checkpoint and PhysicsFS behavior; local helpers select `rewind_file_*` only when the wrapper is active and ordinary PhysicsFS operations otherwise. The byte-identical, branch-added D1/D2 `secretarea.h` files were then replaced by the matching `android/app/src/main/cpp/shared/secretarea.h`; all consumers retain their existing include spelling and resolve it through established shared include paths. The first Android build exposed and rejected a raw-PhysicsFS pointer boundary; the corrected build is warning-free for all changed paths
- Exact behavior: the writer still emits host-format `int total` followed immediately by exactly `SECRET_AREA_MAX_GENERATED` found bytes, using zeros when no state exists. The reader still applies `swap` to the total, reads the fixed array, restores it only on matching totals and otherwise derives found state from `Automap_visited` through `Highest_segment_index + 1`. State version gates and effect-before-secret ordering are unchanged
- Exact metrics against campaign merge base `fb555eec75e1ed12c8348805ab335afb4c721b06`: D1 `state.c` falls from `+1545/-173` to `+1526/-173`; D2 falls from `+2086/-107` to `+2067/-107`. The isolated state edit is `+2/-21` per path, an exact 38-line original-file reduction. Deleting the two 42-line branch-added headers removes another 84 lines and both paths from the inherited trees, for 122 inherited-tree lines removed. One 45-line shared header replaces them, so the consolidation itself reduces duplicated authored source by 39 lines
- Focused validation: `python -m unittest android.tests.test_native_file_naming_contracts android.tests.test_secret_area_serialization_contracts android.tests.test_save_runtime_validation android.tests.test_state_persistence_contracts` passed 28 tests. The contracts enforce the one-to-one shared C/header pair, reject return of either D1/D2 header copy, and preserve exact field order and width, version gates, swapped read, automap fallback and absence of inherited bodies
- Platform validation: the final `run-windows-build.ps1 -Target both` rebuilt and linked D1, D2 and headless variants after the rewind boundary correction. With JDK 21, `:app:externalNativeBuildDebug` rebuilt `arm64-v8a`, `armeabi-v7a` and `x86_64` without changed-path warnings. Scoped quality and final `git diff --check` passed
- Non-goals: state validation size checks, old-version admission, unrelated checkpoint fields and scanner policy remain unchanged

### GQR-0011 terminal claim

- Status: `DONE`; `GQF-0024` is `FIXED`. Historical impact remains 56 (`32/0/10/10/4`, `MEDIUM-HIGH`) with terminal state removing it from dispatch
- Live boundary: work began at `5f3a4f7685edfcfec6a775b0eaedd0f246a61512`. Product scope was disjoint from the completed GQR-0006 work present at claim time; an unrelated performance-plan file remained untouched
- Root-cause fix: new `cmake/dxx-verified-dependencies.cmake` is the common acquisition owner for production Android, extraction-host tests, and input-demo codec dependencies. It resolves repository-owned manifest URL/SHA-256 pairs, names cache objects by digest, locks per dependency during population, rehashes every existing cache object, downloads through TLS with `EXPECTED_HASH`, removes rejected temporary bodies, and atomically renames accepted bytes before FetchContent extraction or source copying
- Dependency boundary: SDL 1.2, SDL_mixer 1.2, TinySoundFont, nlohmann/json, PhysicsFS, LZMA SDK, zlib, Chromaprint, KTX-Software, PicoSHA2, cpp-base64, the StuffIt corpus, minimp3, stb_vorbis, dr_flac, and stb_image now have explicit reviewed URL/SHA-256 identities in `tool_versions.conf`. Production no longer contains direct Git acquisition, mutable `master`, raw unverified downloads, or existence-only cache admission. Android retains Chromaprint 1.5.1 because its existing source list requires that release's bundled KissFFT layout; extraction tooling retains its existing 1.6.0 identity
- Focused validation: `python -m unittest android.tests.test_verified_native_dependencies` passed three tests covering complete production manifest ownership, hostile response rejection, corrupt cached-byte rejection, changed-pin acceptance only with matching bytes, and disconnected reuse after the source disappears. A final source audit found no direct Git repository, direct HTTP URL declaration, mutable branch, or local `file(DOWNLOAD)` path in the production owners. Scoped CMake formatting/lint, BOM lint, `git diff --check`, and final focused reruns passed
- Platform validation: `run-windows-build.ps1 -Target both` configured, compiled, and linked full D1/D2 game and headless targets through the verified input-demo dependencies. The extract-host CMake project configured and compiled every target through the same verified helper. With JDK 21, `:app:assembleDebug` configured, compiled, linked, and packaged production native code for `arm64-v8a`, `armeabi-v7a`, and `x86_64`; a post-format rerun passed for all three ABIs
- Diff scope: changes are limited to branch-owned Android/CMake dependency owners, the central manifest, a new helper, a new focused Python test, this ledger, and the durable plan. No D1/D2 source changed, so inherited-game-file impact is zero. The production CMake owner shrank substantially by replacing repeated acquisition branches with the shared contract
- Validation limitation: the configured extract CTest suite was stopped after the unrelated existing `test_graphics_config_transaction` executable remained idle with near-zero CPU for more than ten minutes. This occurred after the complete extract project compiled successfully and does not exercise dependency acquisition; dependency-specific tests and both platform build paths are green

### GQR-0006 terminal claim

- Status: `DONE`; linked `GQF-0005` and adversarial `BR-0005` are `FIXED`. Historical impact remains 56 (`32/0/10/10/4`, `MEDIUM-HIGH`) with terminal state removing it from dispatch
- Live boundary: work began at `5f3a4f7685edfcfec6a775b0eaedd0f246a61512` with a clean worktree. The durable plan was created before product edits
- Root-cause fix: new `DynamicReceiverPolicy.kt` is the only owner of dynamic receiver registration in the app package. It uses AndroidX compatibility registration on every supported API. `SETUP_COMMAND` and `SETUP_INTROSPECT` remain available to package-scoped senders but become `RECEIVER_NOT_EXPORTED` in release. `HOST_MIGRATION` is always `RECEIVER_NOT_EXPORTED`. `MP_COMMAND`, `SETUP_AUTOMATE`, game introspection/automation/commands, and level-preview debug commands register as exported only when `BuildConfig.DEBUG` is true
- Compatibility: the game-process skip-intro preference broadcast, launcher-script setup broadcasts, and game-process host-migration broadcast retain their package-scoped paths. Existing ADB automation retains the exported debug boundary. Release builds expose no automation-only registration and no permissionless external command surface
- Diff scope: product changes are three branch-added Activity paths plus new branch-added policy and unit-test paths. Existing-file numstat is `+54/-76`; the new policy and test are 44 and 33 lines. No D1/D2 or other inherited game file changed, so original-file impact is zero
- Validation: the focused `DynamicReceiverPolicyTest` and complete `:app:testDebugUnitTest` suite passed. `:app:assembleDebug` built the APK and native libraries for `arm64-v8a`, `armeabi-v7a`, and `x86_64`; `:app:compileReleaseKotlin` and `:app:processReleaseMainManifest` passed. On emulator-5554, a package-targeted external ADB `SETUP_INTROSPECT` broadcast produced a fresh `setup_introspect.json`. The merged manifest contains none of the dynamic action strings. Scoped ktlint/BOM checks and `git diff --check` passed
- Validation boundary: a separately signed hostile release test application was not introduced. Release rejection is delegated to the platform's `RECEIVER_NOT_EXPORTED` contract and covered by the unit-tested release flag selection. This project defines no `testReleaseUnitTest` task, so release validation used `compileReleaseKotlin`, release manifest processing, and the pure `debug = false` unit case. Windows D1/D2 validation was not rerun because no native, build-system, or inherited game path changed

### GQR-0001 live claim

- Claimed: 2026-08-11 by one fresh `gpt-5.6-sol` worker at medium reasoning effort with no inherited turns
- Live HEAD before edits: `7877ad30d05887b8e19869ed4c50075e41e2f88e`
- Overlap: `SAFE_DISJOINT`; all allowed existing paths were clean, while unrelated launcher/import worktree changes remain user-owned and out of scope
- Existing allowed paths and pre-edit Git hashes: `android/app/src/main/cpp/shared/ogl_texture_android.c` `4e42cc962697b817541d7b8093ac6866feadd127`; `ogl_texture_android.h` `4f3750313dc4278578bb053e25c491df98da896c`; `android/app/src/main/cpp/CMakeLists.txt` `2ba9cfaa5d6e00fe8bf8bd22c91c8dd0bd3d61b8`; `android/app/src/main/cpp/extract/CMakeLists.txt` `47992190e8edccf31ac9c21985029434b8f44b31`; `android/tests/test_android_renderer_contracts.py` `296cc293ac0ce19e78fb47fb5b5a095f85fc255b`; `d1/arch/ogl/ogl.c` `cda1161ddf0e99e013b1526029882b3e1e1ae483`; `d2/arch/ogl/ogl.c` `a638868ecbe463bce270a305780d8a0d083d7996`
- Optional exact new paths: `android/app/src/main/cpp/shared/ogl_texture_filename.c`, `android/app/src/main/cpp/shared/ogl_texture_filename.h`, and `android/app/src/main/cpp/extract/test/test_ogl_texture_filename.c`
- Acceptance boundary: the API receives capacity; every candidate either fits with NUL or is rejected before `read_png`; no truncation or partial candidate is treated as a lookup; exact-fit, one-byte-short, zero-capacity, empty/base cases and unchanged lookup ordering/metrics are covered; inherited D1/D2 edits remain call-site-sized
- Required validation: focused source contract and any new host C test, D1 and D2 Windows game targets, Android native build for supported ABIs, scoped quality, `git diff --check`, and final allowed/out-of-scope audit

## Disposition log

Append a dated entry whenever a finding becomes fixed, dismissed, deferred, or a duplicate. Include evidence and the deciding person or call

### 2026-08-12: GQR-0011 / GQF-0024 completion

- Status: `DONE`; `GQF-0024` is `FIXED`
- Result: one repository-owned verified-download/cache helper and reviewed manifest now own production Android and matching host native dependency bytes. Hostile response, corrupt cache, changed pin, and disconnected reuse tests passed, as did Windows D1/D2, the complete extract-host compile, and all Android ABIs, with zero inherited-game-file changes
- Limitation: the unrelated existing `test_graphics_config_transaction` executable stalled the extract CTest suite after its complete compile

### 2026-08-12: GQR-0006 / GQF-0005 completion

- Status: `DONE`; `GQF-0005` and adversarial `BR-0005` are `FIXED`
- Result: one AndroidX-backed policy makes production command and migration receivers app-internal and omits automation-only registrations from release while retaining exported ADB access in debug. The full launcher unit suite, debug APK and all native ABIs, release Kotlin and manifest processing, emulator ADB introspection, scoped quality, and final repository audits passed with zero inherited-game-file impact

### 2026-08-12: GQR-0161 / GQF-0174 completion

- Status: `DONE`; `GQF-0174` is `FIXED`
- Result: secret-area runtime serialization and its interface now have one shared C/header owner with the rewind/PhysicsFS abstraction preserved. Paired original state additions fall by 38 lines and both 42-line branch-added D1/D2 header paths are removed, a 122-line inherited-tree reduction; 28 focused contracts, Windows D1/D2, all Android ABIs, scoped quality and final repository audits passed

### 2026-08-12: GQR-0159 / GQF-0172 completion

- Status: `DONE`; `GQF-0172` is `FIXED`
- Result: actual SDL_mixer queries and exact success/failure diagnostics now have one shared owner. Paired inherited additions fall by 46 lines; focused contracts, Windows D1/D2, all Android ABIs, scoped quality and final repository audits passed

### 2026-08-12: GQR-0157 / GQF-0170 completion

- Status: `DONE`; `GQF-0170` is `FIXED`
- Result: the branch-added shared HMP owner now provides the public wrapper and canonical tempo track. Both inherited HMP files are identical to merge base, removing 24 additions across four hunks; exact-byte host coverage, Windows D1/D2, all Android ABIs, scoped quality and final repository audits passed

### 2026-08-12: GQR-0156 / GQF-0169 completion

- Status: `DONE`; `GQF-0169` is `FIXED`
- Result: one conventional shared header replaces the paired declaration blocks with one include per inherited header. Exact merge-base additions fall from 22 to four, reducing inherited additions by 18; focused C/C++ contracts, Windows D1/D2, all Android ABIs, scoped quality and final repository audits passed
- Limitation: the maintained SAF Redbook automation failed at pre-playback pilot-menu navigation and did not exercise the runtime playback path

### 2026-08-12: GQR-0162 / GQF-0175 completion

- Status: `DONE`; `GQF-0175` is `FIXED`
- Live boundary: HEAD `67d57e1ebd0362763643b819a528ea359da42c44`; overlap `SAFE_DISJOINT`. Pre-edit SHA-256 was `2E8A46EF501241E00AF8B9064064C0F3B4C3D1E9E8702B5E61AA4CA054CBD7EC` for D1 CMake and `9F749D94F283FFAC0F539F2E707D092EC266F6B2335F920FC55B5AF0CEB2B457` for D2 CMake; the module was absent. Concurrent ledger and plan edits were the same-root claim and were preserved
- Root-cause fix: new `cmake/dxx-headless-targets.cmake` owns common non-Android executable construction. D1/D2 retain `D1X_MAIN_SOURCES`/`D2X_MAIN_SOURCES` derivatives and explicit calls naming each target, entry point, D2 definition, D2 `libmve`, and metadata-only game include. Call-site scope still resolves `vers_id.c` and conditional `net_udp.c`; codec dependencies and build metadata remain applied once per target
- Inherited metrics: attributable target blocks fell from D1 62 plus D2 123 lines (185 total in two logical construction hunks) to D1 11 plus D2 19 lines (30 total), a 155-line reduction. Isolated Git numstat is D1 `+5/-56`, D2 `+12/-116`, total `+17/-172`; zero-context presentation splits retained entry-point lines into seven display hunks but changes the same two original construction blocks
- Exact parity: frozen-HEAD and live CMake File API codemodel projections matched byte-for-byte after root normalization for ordered sources, definitions, includes, compile fragments and link fragments, plus target name/type/output. SHA-256 values were D1 metadata `5A574F09A778D11F58EEEE4CF0C7980B9D863E27AF0149711668A1895549162F`, D2 replay `4E16E4EF136C51D29A6ED2A2F40A64F8C825E033620E66C85240F971210091E7`, and D2 metadata `16D79FF83A80F317AADBF9773E31B9DC6B1BB1314A80AD74EEDC0998E11172FA`
- Validation: scoped `cmake-format --check`, `cmake-lint`, and `git diff --check` passed. `run-windows-build.ps1 -Target both` configured and built the full D1/D2 trees with `OPENGL=ON`, `SDLMIXER=ON`, `UDP=ON`, including all three headless targets. D1 metadata also built with OpenGL/mixer disabled and UDP enabled; both D2 targets built with OpenGL disabled and mixer/UDP enabled. Both games configured with all three options disabled. Maintained secret-area metadata baselines matched at D1 181 and D2 234 records, and D2 level-10 headless replay passed 774 frames
- Limitations: Linux and Apple execution was unavailable on the Windows host; the unchanged module conditionals retain the exact `arch_x11` and `arch_cocoa` links. Building with `UDP=OFF` reaches an inherited global-`NETWORK` `_sockaddr` compile failure in D1, and building D2 with `SDLMIXER=OFF` reaches inherited undeclared `mve_audio_resample_needed`/`mve_audio_output_frame_size` symbols in `libmve`; both matrices configured successfully and neither failure is caused by the extracted target policy

### 2026-08-11: GQR-0001 / GQF-0006 completion

- Status: `DONE`; `GQF-0006` is `FIXED` after root review correction
- Worker: one fresh `gpt-5.6-sol` worker at medium reasoning effort, processing only `GQR-0001`
- Live boundary: HEAD remained `7877ad30d05887b8e19869ed4c50075e41e2f88e`. All seven existing allowed paths matched their recorded pre-edit hashes and were clean at claim time, all three optional new paths were absent, and overlap remained `SAFE_DISJOINT`
- Root-cause fix: `android_ogl_read_texture_with_extensions` now receives `size_t filename_capacity`. The portable `android_ogl_texture_filename` helper checks the complete basename, extension, and terminating NUL before either write, so a rejected candidate cannot become a truncated or partial lookup. The existing `.png`, `.jpg`, `.tga` order, one `png_attempts` increment per slot lookup, per-slot and per-extension read timing, hit fields, and early-success behavior remain unchanged
- Paired inherited boundary: capacity is the final API argument so each of the five pre-existing calls keeps its original first line and two-line shape. Final isolated inherited numstat is D1 `+2/-2` and D2 `+3/-3`, with zero inherited line-count growth. No lookup body, renderer policy, upload transaction, mask handling, or unrelated cleanup moved into inherited sources
- Changed paths: `android/app/src/main/cpp/shared/ogl_texture_android.c`, `android/app/src/main/cpp/shared/ogl_texture_android.h`, new `ogl_texture_filename.c` and `.h`, `android/app/src/main/cpp/CMakeLists.txt`, `android/app/src/main/cpp/extract/CMakeLists.txt`, new `android/app/src/main/cpp/extract/test/test_ogl_texture_filename.c`, `android/tests/test_android_renderer_contracts.py`, `d1/arch/ogl/ogl.c`, and `d2/arch/ogl/ogl.c`
- Focused validation: `python -m unittest android.tests.test_android_renderer_contracts` passed 6 tests. The configured host target and `ctest -R '^ogl_texture_filename_tests$'` passed, covering exact fit including NUL, one-byte-short rejection with unchanged output, zero capacity with unchanged output, empty basename, and empty extension/base preservation. The source contract verifies the checked builder and rejection branch precede `read_png`, and both game sources pass `sizeof(filename)`
- Platform validation: the initial `run-windows-build.ps1 -Target both` and all-ABI Android build passed. After the final-argument correction, an incremental Windows rerun rebuilt and linked D1 and completed D2 successfully. The JDK 21 Android umbrella rerun rebuilt arm64-v8a, then unrelated concurrent Chromaprint configuration failed on armeabi-v7a because its generated `config.h` was absent. Isolated Ninja object targets for both inherited OGL call sites plus both shared filename and lookup sources compiled successfully for armeabi-v7a and x86_64; arm64-v8a had already completed the same corrected sources in the umbrella run
- Quality and audit: the initial mixed scoped quality pass covered all ten product/test/build paths and passed clang-format, BOM lint, cmake-format, and cmake-lint. A correction-scoped pass covered the shared declaration/implementation, source contract, and paired inherited calls; inherited sources remained excluded by policy. Final scoped `git diff --check` passed. The full final status retained the unrelated launcher/import/native dirty paths and concurrent unrelated changes; this worker did not edit, format, revert, stage, or delete them. The final chunk paths are exactly the seven allowed existing paths, three allowed new paths, and this active ledger
- Non-goals preserved: `GQF-0007` DXA mask construction, `GQF-0020` texture byte accounting, `GQF-0021` elapsed accounting, and all unrelated formatting or cleanup remain open and unchanged
- Root acceptance: reviewed every scoped diff and new file after the correction; independently reran all six renderer contract tests and the focused host test executable; confirmed scoped `git diff --check`; verified only five inherited lines are substituted (`d1` `+2/-2`, `d2` `+3/-3`) with zero inherited line-count growth; accepted the recorded Android umbrella limitation because corrected affected objects compiled for every configured ABI

### 2026-08-12: GQR-0002 / GQF-0007 completion

- Status: `DONE`; `GQF-0007` is `FIXED`. Historical impact remains 56 (`32/0/7/10/7`, `MEDIUM-HIGH`) with terminal state removing it from dispatch
- Live boundary: work began and ended at HEAD `52ca652d58ea70e62e1f28ee24e2455194be11e1` with a clean product scope. Only the durable plan was added before product edits
- Root-cause fix: `android_ogl_load_dxa_mask` now calls the existing `android_ogl_texture_filename` helper with `sizeof(maskname)` and `_mask.png`. The helper checks the complete basename, suffix, and terminating NUL before writing. Rejection logs the source name and returns before `read_png`; no truncated or partial candidate can be queried
- Boundary behavior: a 246-byte basename produces the exact 255-character filename accepted by the 256-byte buffer. A 247-byte basename is rejected and leaves the destination unchanged. Ordinary names retain the exact `<bitmapname>_mask.png` lookup. Existing null/empty argument guards, not-found logging, PNG conversion, texture upload, and memory cleanup are unchanged
- Diff scope: three branch-added Android/test paths changed: `shared/ogl_texture_android.c`, `extract/test/test_ogl_texture_filename.c`, and `test_android_renderer_contracts.py`. Isolated numstat is `+42/-1`. No D1/D2 or other inherited path changed, so inherited diff impact is exactly zero
- Focused validation: `python -m unittest android.tests.test_android_renderer_contracts` passed seven tests. The configured MSVC `test_ogl_texture_filename` target rebuilt and `ctest -R '^ogl_texture_filename_tests$'` passed, including ordinary DXA naming, exact capacity, one-byte-over rejection with unchanged output, and the pre-existing general filename boundaries
- Platform validation: `run-windows-build.ps1 -Target both` configured, compiled, and linked full D1/D2 game and headless targets. With JDK 21, `:app:externalNativeBuildDebug` built both Android games for `arm64-v8a`, `armeabi-v7a`, and `x86_64` with no new warnings
- Quality and audit: scoped `run-code-quality.ps1 -Fix` passed clang-format and BOM checks; final focused tests were rerun after formatting. `git diff --check`, ASCII/no-BOM inspection, allowed-path audit, and clean build-artifact exclusion passed

### 2026-08-12: GQR-0024 / GQF-0037 completion

- Status: `DONE`; `GQF-0037` is `FIXED`. Historical impact remains 56 (`32/0/7/10/7`, `MEDIUM-HIGH`) with terminal state removing it from dispatch
- Live boundary: work began and ended at HEAD `d1ad925cbbb76ddc964e9c6db05a947c4cfa5596` with a clean product scope. Only the durable plan was added before product edits
- Root-cause fix: `jni_saf.c` converts native standard UTF-8 content URIs with `dxx_jni_string_from_utf8` and rejects conversion failure before `CallIntMethod`. `jni_midi_preview.c` converts Java paths and HOG entry names with `dxx_jni_string_to_utf8`, converts state and enumeration JSON with `dxx_jni_string_from_utf8`, and frees partial conversion storage. Neither bridge retains a Modified UTF-8 API
- Encoding behavior: the established shared converter reads Java UTF-16 through `GetStringChars`, writes Java strings through `NewString`, validates scalar structure through `utf8_codec`, preserves supplementary scalars, rejects malformed standard UTF-8 and invalid/unpaired UTF-16, and rejects embedded NUL on native path boundaries. SAF callback exceptions retain the bridge's existing logged-and-cleared descriptor-failure policy; broader JNI exception ownership remains independently assigned to `GQF-0052` / `GQR-0039`
- Focused validation: `python -m unittest android.tests.test_strict_jni_utf8_contracts` passed three contracts enforcing the shared converter internals, checked SAF order, strict MIDI ownership, and absence of Modified UTF-8 APIs. MSVC builds of `test_utf8_codec` and `test_saf_manifest_parser` passed through CTest. The codec covers ASCII, BMP, supplementary, malformed continuation/overlong/truncated/surrogate/out-of-range sequences, invalid UTF-16, embedded NUL policy, and capacity failures. The parser now proves raw and JSON-escaped BMP/non-BMP content URIs produce identical canonical standard UTF-8 bytes
- Platform validation: with JDK 21, `:app:testDebugUnitTest` passed the complete launcher unit suite and `:app:externalNativeBuildDebug` compiled and linked D1/D2 for `arm64-v8a`, `armeabi-v7a`, and `x86_64` without changed-path warnings. `run-windows-build.ps1 -Target both` configured, compiled, and linked full D1/D2 game and headless targets
- Diff scope: product/test changes are limited to branch-added `jni_saf.c`, `jni_midi_preview.c`, `test_saf_manifest_parser.c`, and new `test_strict_jni_utf8_contracts.py`; inherited D1/D2 impact is zero. Tracked-path numstat before records is `+49/-9`. Scoped quality, final focused reruns, `git diff --check`, no-BOM, added-line ASCII, and allowed-path audits passed. Existing non-ASCII comment rulers in the two JNI files were preserved to avoid unrelated churn
- Validation limitation: no standalone device-side CheckJNI fault injector exists for these callbacks. The real Android JNI translation units compiled for every ABI, while malformed and non-BMP behavior is proved by the shared compiled codec/parser tests and source-order contracts. A future generic CheckJNI failure-injection harness belongs with GQR-0039 rather than reintroducing bridge-local conversion code

## Campaign closure

- [ ] Every queue item is `DONE` or has an approved `SKIP` disposition
- [ ] No queue item remains `ACTIVE` or `BLOCKED`
- [ ] Every P0 and P1 finding received an independent verification call
- [ ] Every finding has a final disposition or an explicitly accepted deferral owner
- [ ] Required build, lint, unit, integration, emulator, and server validations are recorded
- [ ] `git diff 7877ad30d05887b8e19869ed4c50075e41e2f88e..HEAD` was reviewed through a delta campaign or proven empty
- [ ] A human maintainer reviewed open risks and AI-generated dispositions
- [ ] The closing summary records counts by severity, category, and disposition
