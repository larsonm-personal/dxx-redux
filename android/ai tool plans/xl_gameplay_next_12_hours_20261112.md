# Next 12 hours: XL gameplay continuation

Proposed follow-up outline, revised November 12, 2026 after source and evidence
review. This document does not start another worker or schedule an automatic run.
The wind-down instruction remains in effect until the user resumes implementation.
Start from the current continuation branch and the
[wind-down report](xl_gameplay_what_was_done_20261112.md), not an old scratch checkout.
The recorded starting implementation is `3660e9a14`, documentation handoff
`013b7234`. Preserve and inspect any later changes before using those identities.
Read the [original goal](xl_gameplay_goal_20261110.txt) and the G2-G7 acceptance
sections of the [execution ledger](xl_gameplay_execution_20261110.md).

## Objective and expected outcome

Advance the critical path to the user's 90% playable-level standard. First make
that target measurable, then complete the missing restore transaction as far as a
coherent, verified chunk allows. Avoid further observer/schema expansion unless
a real comparison exposes missing gameplay state.

The revised G2 allowance is **18-32 hours**, within **63-120 hours of focused
remaining engineering**, both low-confidence judgments explained in the report.
Catalog-wide 90% playability qualification is additional and unestimated.

The expected deliverable is one coherent, tested target-definition/resource
preparation milestone, an initial catalog accounting, and an exact map of the
remaining restore transaction. Prefer a working vertical path through the real
reader to a collection of disconnected staging APIs. Full G2 closure is a stretch
outcome, conditional on all of its acceptance gates. Neither outcome implies
90% playable, Android acceptance or G3-G7 completion. A new observer schema is not
a deliverable by itself, and passing the energy assertion alone does not prove
whole-save atomicity.

## Sequential outline

| Budget    | Work                                                                                                                         | Concrete output / decision                                                                                                                       |
| --------- | ---------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------ |
| 0-0.5 h   | Verify source/tree/diff, constraints, cache roots, assets, running jobs and free space                                       | Exact resume identity and owned paths; preserve unrelated work, `.codex/`, stashes and authentic inputs                                          |
| 0.5-1.5 h | Recount the fixed catalog and join existing surveys; document eligibility, package mix and overlapping blockers              | Initial accounting with unresolved cases visible; do not promise full classification of 1,111 entries in one hour                                |
| 1.5-3 h   | Reproduce `birth-atomic` on a current build; trace every late rejection, live write and retained pointer in the restore path | Failing receipt and transaction/ownership map; select the smallest coherent implementation slice and its tests before editing                    |
| 3-7 h     | Implement that slice through existing native codecs and resource owners, normally incoming HAM/definitions/resources         | A genuine target-mission path prepares independently of outgoing definitions; malformed/discard controls preserve live state                     |
| 7-9 h     | Integrate further only if the slice is stable and commit-time operations are understood                                      | Either bounded whole-restore integration or a stabilized partial milestone. At hour eight, drop stretch scope if preparation is still incomplete |
| 9-12 h    | Complete affected Windows/Linux builds/tests, ordinary playback, quality and evidence review; update the handoff             | Terminal results on final source, exact open failures and canonical coverage; publication follows the procedure below                            |

These budgets include local build/test time. If setup or reproduction overruns,
reduce implementation breadth and preserve the verification reserve. Finish an
already-started coherent operation and report an overrun; do not start a long new
build, Android owner or CI dispatch at the end merely to leave it running. Work
sequentially in the main checkout, without parallel agents. Run Android owners
serially. This bounded proposal does not reinstate automatic G0-G7 continuation.

## Make the catalog accounting useful

The fixed inventory contains 1,111 IDs: 1,020 normal and 91 secret entries across
23 archives and 356 archive/descriptor pairs. `levelpack.7z` contributes 866
entries. It includes classic/imported, multiplayer and XL content and has no
current playability or per-entry single-player eligibility flag.

Use the original single-player scope as the working rule. Preserve every raw ID,
prove exclusions and keep unresolved eligibility in a conservative interim
denominator. If all 1,111 entries are intended, 1,000 successes are required.
Report package and XL-active breakdowns alongside the aggregate so stock-heavy
loading results cannot stand in for XL support.

Reuse existing inventory/reporting machinery. Rows need archive/descriptor/level
identities, ordinal/resource profile, eligibility evidence, all known blockers,
platform/build, receipt and assists. Track parse/admit, render/control, connected
play, save/continue, completion/transition and replay separately. Direct evidence,
feature inference and untested status must remain distinct. Do not deduplicate
levels solely by filename or bytes when mission definitions or progression differ.

Rank marginal entries unblocked by a **set** of fixes, not the sum of blocker
frequencies. One level with three blockers is still blocked after fixing one.
Keep unknown later blockers behind a first rejection visible. The census slot is
a bounded baseline; do not spend this window regenerating every metadata report
or starting 1,000 playthroughs. See the report for the counting formula and the
distinction between G0-G7 acceptance and a catalog-wide 90% claim.

## Define the restore transaction before expanding it

In [state.c](../../d2/main/state.c), `state_restore_all_sub` selects the mission,
assigns clocks, initializes the player and runs `StartNewLevelSub` before late
runtime/XL validation. Android adaptation/metadata work also follows the XL read.
Moving only the checksum check earlier cannot establish the whole contract.

Before editing, put a concise ownership table in the ledger. For each category,
record live writes, candidate storage, validation dependencies, fallible work,
commit operation and discard owner:

- Save bytes, descriptor/archive/search-path leases and source identity.
- Incoming HAM/VHAM/HXM, model payloads, PIG/POG/material/sample metadata and
  other gameplay resources.
- Target geometry/topology, walls/triggers, objects/segment/free lists,
  signatures, players, AI/paths and the saved epoch used for relative times.
- Native runtime, XL events/producers/weapons/time/birth state, Guide-Bot state,
  SIM RNG and pending allocator work.
- Secret-return/lifecycle and relevant Android metadata. Preserve existing
  stock, rewind and multiplayer behavior without expanding XL multiplayer scope.

**Prepare and validate:** use existing codecs to decode into owned storage against
the incoming mission/geometry/epoch; resolve origins, reserve necessary storage
and complete semantic checks before publication. Keep the old world usable.
Retain the actual source or a verified identity/lease strategy through commit.
A second unchecked decode or a hash check alone is not sufficient. A different
target with equal table counts must not accidentally borrow live definitions.

**Commit and discard:** transfer validated owners at the existing safe lifecycle
boundary, rebind views, publish clocks/RNG/state consistently, then release old
owners. Account for recoverable content/I/O/allocation failure before publication;
`install_pog` currently can still fail allocation. Discard closes candidate handles
without changing live lookup, consuming SIM RNG or stopping live owners. Process
termination is a different failure model; do not build a general rollback system.
No serialized rollback, temporary live-table swapping, guessed trailer search,
second parser, copied game loop or replay-only compensation.

## Concrete implementation starting points

Apply the [layer-03 boundary review](xl_layer03_architecture_review_20261112.md)
before extending this work. Put new XL policy/resource/runtime implementations
and developer configuration in `android/XL_compat`; keep `d2/` changes to narrow
hooks and necessary native codec/state-owner changes. The mission-preparation
header now belongs in `XL_compat/runtime`, alongside the isolated resource scope.
Do not split or duplicate a native decoder or expose private globals merely to
move lines. Explain substantial native changes in the ledger and PR description.

| Existing piece                                                                                                          | Required next responsibility                                                                                                                                                                                                |
| ----------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `mission_prepare_*` in [mission.c](../../d2/main/mission.c)                                                             | Reuse retained descriptor and origin-checked candidate reads. `mission_commit_prepared` still calls the mutating loader; do not select the mission just to obtain its definitions                                           |
| `bm_read_all_internal` in [bm.c](../../d2/main/bm.c)                                                                    | Factor destination binding in the existing typed reader; preserve HAM versions, counts, model ownership and source-required initialization. No copied parser                                                                |
| Native model initialization                                                                                             | Audit allocation, alignment/endian handling, model-data pointers and global scratch such as `highest_texture_num`; account for those writes during preparation                                                              |
| `MissionDefinitions` / `XlPreparedResources` in [resource_validation.cpp](../XL_compat/runtime/resource_validation.cpp) | Supply genuinely incoming definitions/source context. `mission_base` starts after live HAM initialization; companion lookup, ordinals, bitmap/sample banks and some GL preparation still depend on the current installation |
| Native object/player and AI candidate decoders                                                                          | Bind to target geometry/capacities/signatures/epoch, not the outgoing world                                                                                                                                                 |
| Existing runtime and `trigger_save.cpp` owners                                                                          | Retain validated data through commit; any check/apply split must use the same source/context and enforce required capabilities                                                                                              |

The HAM destination inventory must cover all fields the native reader writes,
including ship/reactor data, gauges/cockpits and model/bitmap indices, rather than
stopping at the subset currently copied by `MissionDefinitions`. Preserve existing
version-specific defaults and patch/Android hooks at their correct lifecycle
boundary. Prove the design by preparing target B while a different A remains
active, then discarding B and continuing A, as well as committing B successfully.

The recent sound-map lifetime failure must remain covered. A diagnostics-only
rebuild changed heap layout and merely appeared to fix it; rebinding views after
moving camera definitions was the actual fix. Exercise move/copy/destruction and
discard paths where views escape, including model-data pointers and inline arrays.

At hour three choose a bounded slice with meaningful acceptance. If the complete
target resource context cannot fit, finish native HAM destination binding and
its real integration tests, document every remaining live dependency, and leave
whole-save atomicity explicitly open. Do not turn this into a rewrite of every
resource loader.

## Required negative and continuation evidence

The existing `birth-atomic` mode in
[projectile_test.h](../XL_compat/runtime/projectile_test.h) asserts rejection and
unchanged energy. The [canonical projectile owner](../XL_compat/tests/projectile.cmake)
runs `birth` and `birth-resume`, **not `birth-atomic`**. Reproduce it, fix its cause,
then make the repaired regression execute inside that same owner before accepting
the restore fix. Preserve the 113 required top-level private tests and deadlines.

| Case                  | Required proof                                                                                                                                                                                                  |
| --------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Invalid ordinary save | Bad XL checksum, valid-checksum semantic corruption, truncated native/runtime records and wrong source/signature/epoch reject before publication                                                                |
| Unchanged live game   | Pure before/after state covers mission/search paths, definitions/resources, player, objects/pool/lists, geometry/walls, native/XL owners, clocks and SIM RNG; then compare future inputs with a no-load control |
| Target ownership      | Stock -> XL, XL -> stock and distinct XL contexts; retained descriptor commit, corrupt/changed archive, busy-handle/discard and view lifetime controls                                                          |
| Cold continuation     | Uninterrupted versus disk save, process exit and ordinary load; same future inputs, first-frame through terminal semantic state and SIM RNG                                                                     |
| Combined state        | Pending trigger/producer, active time transition, guided/projectile and earned progression through existing owners; isolated assisted fixtures remain labeled                                                   |
| Ordinary build        | Developer-OFF save-start playback and a useful sustained continuation, not solely metadata probes or the retained 12-frame regression                                                                           |
| Compatibility         | Affected D2 checks and stock D1/D2 save/replay controls where shared hooks change; report the actual selected CTest scope                                                                                       |

Mutate derived saves only. Semantic corruption with a recomputed checksum prevents
a checksum-only fix from passing as transaction correctness. Extend existing
controls at meaningful failure boundaries, without inventing a general framework.

First ensure observations are pure. `named_audio_snapshot` destroys stale object
audio, reconciles owners and can clear loops. Identical consecutive snapshots do
not prove no mutation; compare underlying state and future behavior with an
unobserved control. Move necessary maintenance to a defined runtime boundary.
If this needs a small part of G3's logical-audio work, treat it as a documented
dependency rather than accepting G2 with an observer that repairs its subject.

For Guide-Bot, trace whether async completion, warm caches or the Android 2,000 us
/ 512-work budget changes when a route affects simulation. Exercise pending/cold
and completed/warm cases. A deadline alone does not prove a divergence. If
simulation-visible, define deterministic work/publication through the existing
owner; do not serialize arbitrary pointers or declare it cosmetic without tracing
consumers.

## If G2 closes early

Use the remaining budget for the highest-frequency real gameplay blocker from the
census, following the original G3/G4 dependency order. The likely first cluster is
logical named-audio/event state and optional-output independence, followed by the
source-defined smoke 20-24 dispatch leaves. Do not assume these are the largest
coverage gains until the counts exist.

Audio requires special care: output success currently affects future wall-trigger
disablement, and snapshot collection can mutate stale owner/link state. Preserve
logical event order, flags and timers when output is unavailable, then prove the
same simulation in rendered and genuine no-device replay. Missing gameplay assets,
POG transparency/material data, hitboxes and gun transforms remain mandatory.
Test output present, absent and voice-allocation failure. Missing metadata used
to determine a logical duration needs an explicit source-backed policy; having
no sound device does not authorize inventing that duration. GL library linkage
alone does not disqualify true headless execution; dependence on a graphics/audio
device does. Keep normal geometry, visibility, aiming and controls usable.

Do not start broad G4 implementation, a new campaign route or an Android package
build simply to consume the clock while G2 has a known retained regression.

## Validation and scope rules

Use current root-bound caches only after checking source roots/configuration:
Windows ordinary `temp/xl-day-20261110/build-d2-reference`, developer
`temp/xl-overnight-20261104/boost-crossing-native`; Linux
`/var/tmp/dxx-pr464-20261111/private/{stock,preview}` from
`/mnt/c/local/dxx-redux/d2`. Use root helpers or verified invocation receipts.
Targets include `dxx-redux-d2`, `test_upstream_compat` in its configured cache and
`dxx-redux-d2-headless-metadata`; `xl-preview-lifecycle` is an output basename,
not that build target. Quote PowerShell CMake `-DNAME=path` arguments.

- Reuse the existing native/reference/Pumo/Android/replay owners. Keep the pinned
  XL reference at `2576a98b5de310e3ac7c9777df2677d1627b8bae`, authentic bytes,
  immutable stock fixtures, required private membership and timeouts unchanged.
- Retain the latest real Windows failure and its fix as evidence. Camera
  preparation must not leave table views referring to temporary inline arrays.
  Run actual ordinary developer-OFF save-start playback, not just metadata tests.
- Compare new or retained live captures with immediate and clean-process playback
  through detailed world/object/XL state and SIM RNG. Use the existing saved-origin
  and source/asset guards. Do not repair state only during replay or replace an
  oracle merely to erase a mismatch. A fresh candidate recording after a real
  engine fix is valid if failed evidence is retained and the new live capture
  independently matches playback. Immutable stock fixtures stay unchanged;
  terminal summaries do not substitute for detailed fidelity.
- Retain the frozen 4/8 headless series unchanged and report it separately from a
  new per-entry playability matrix. Loaded, rendered, assisted, directly played,
  feature-inferred and untested entries need distinct statuses. A green workflow
  with skipped dependents is not gameplay evidence.
- Close required negative cases and affected compatibility checks before accepting
  an implementation. Do not repeatedly broaden testing after it passes unless a
  new edit, failure or unresolved concern justifies it.
- After the last source edit, complete affected builds and tests on both hosts,
  then ordinary playback. `^test_upstream_compat$` selects one combined owner,
  not the whole native D2 suite. Xvfb/software GL is rendered evidence. A final
  12-frame replay cannot stand in for sustained gameplay or all earlier owners.
- Run one scoped quality pass on changed paths and the automation catalogs when
  registration changes. Wait for formatters to exit; materially changed tested
  source needs its affected rebuild/retest. Never test a stale executable while
  its replacement build is running. Preserve exact source/patch/binary hashes.
- Use official cleanup with preview, explicit exclusions and emulator preservation
  before large generation. At wind-down the protected workspace had about 15.5 GiB
  free and no eligible cleanup sufficient to reach 50 GiB. Recheck current space;
  preserve evidence and genuine inputs, and do not hand-delete caches. If space
  prevents a build, record the exact unmet requirement and finish independent work.

If the original goal's fork-publication authorization is active for the resumed
run, commit coherent work with explicit owned paths. Incomplete G2 remains a local
checkpoint with its failures stated. Completed G2 follows this order: **local
acceptance -> commit/push layer 03 -> immediate-parent PR/native stack update ->
exact-candidate public/private CI -> accepted status**. Do not require nonexistent
hosted checks before the first push or claim hosted acceptance before they finish.
Record the actual tested candidate in private receipts, not just the controller
SHA. If that controller is unavailable/ineligible, name the external gate without
weakening its security, cohort or timeouts. Never merge, enable auto-merge, push
upstream or rewrite unrelated branches.

## Handoff at the end of the next window

Record the implementation and documentation commits, parent/tree, push/PR/stack
state, actual time and revised estimates; exact tests and pass/fail/skip reasons;
binary/input/source hashes; original-level outcomes and every assist; raw and
eligible cohort counts with unknowns visible; Android restoration if exercised;
and the first unfinished causal defect. Packages and 90% readiness must only be
claimed when their own evidence exists.

Also record the reached transaction boundary and every remaining live dependency;
whether `birth-atomic` still fails and whether the canonical owner executes it;
the catalog's raw/eligible/unknown and package/XL-active breakdowns; overlapping
blocker sets; measured owner throughput if available; and the assumptions behind
the revised estimate. Separate historical revision evidence from final-source
validation. Preserve failed receipts. The commit hook may amend timestamps, so
record post-hook HEAD/tree and actual wall-clock time independently.

If G2 remains open, say so and give the precise failing transaction boundary. If it
closes, identify the remaining G3 presentation independence, G4 ranked authored
predicates, G5 connected play, G6 platform/corpus and G7 CI/package gates.
The full remaining list and estimates are in the wind-down report.
Stop after the requested window's coherent checkpoint and handoff; further work
requires the user's subsequent direction.
