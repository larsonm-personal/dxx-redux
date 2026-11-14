# Nightly tooling

The scheduled/manual `nightly` workflow verifies a roughly one-hour sample on the
latest authenticated authorized revision. Five quick core owners run first; the
remaining admitted unattended owners rotate in a persistent pseudorandom order.
The trusted catalog and policy define admission, estimates and explicit exclusions.

PR validation remains a read-only representative sample using shareware/generated
fixtures. Named manual campaigns remain bounded. A passing owner does not certify
skipped or unreported child cases. Detailed results and logs are workflow
artifacts, not committed inventory reports.

For host adapter arguments and prerequisites, see
[`HOST_TEST_SUITES.md`](../android/tests/HOST_TEST_SUITES.md).

## Public network coverage

The default PR sample runs the existing two-emulator public gameplay owner:
native D1, native D2, and imported D1 level-one starts with matching peers and a
cold role swap. Network engine, shared networking/co-op, Android lobby/multiplayer,
and LAN-owner changes trigger the same sample. Its dual-lane estimate is 1500
seconds; fixture-free discovery and broadcast are included in the nightly sampling pool.

The fixed public D1 host-migration variant reuses `test_lan.ps1`: graceful host
leave, rejoin, forced host process loss, and a second migration/rejoin. It is
admitted to nightly and the existing opt-in first-play selection, not the routine
PR sample. Its 900-second estimate brings that selection's dual lane to 2400 of
2700 seconds. Hosted completion within the owner timeout is required. This uses
debug launch/automation controls and public shareware, not ordinary menu input or
retail qualification. Retail defaults remain separate.

## Private retail fixtures

The `retail-ci` Environment must allow only the `cmake` branch. Its
`RETAIL_ASSETS_READ_TOKEN` secret is read-only Contents access to
`larsonm-personal/dxx-redux-test-assets`; the repository variable
`RETAIL_ASSETS_COMMIT` pins an immutable forty-character commit.

Only the exact scheduled/manual `cmake` coordinator can select that Environment.
`retail_asset_sources.json` maps supplied paths to isolated staging;
`standard_game_data.ps1` defines required file hashes. Missing configuration or
mismatched files remain pending. PRs never receive these fixtures. Retail lanes
upload only bounded result/verification metadata, exclude broad logs/screenshots,
and do not save caches. Token functionality and retail tests require a trusted
post-merge run; source admission alone is not runtime qualification.

The mission-enabled profile additionally verifies Vertigo and Obsidian. Its three
source/staging identities are fixed in `retail_ci_assets.py`; only the pinned
Vertigo descriptor receives explicit LF-to-CRLF normalization in staging. The currently reviewed XL profile uses asset commit
`8c9988a96a7e306f8aac04c85e2610492917b7ac`; older configured commits can leave
newer private owners unavailable. Supplied originals are never modified.

## Nightly sampling and revision provenance

Scheduled runs (02:15 America/Los_Angeles) and manual `nightly` runs use
`nightly_revision.py` and `nightly_sample.py`. This starts only after these changes
reach the scheduler's default branch, `cmake`. PR samples retain their selections.

Revision selection paginates GitHub repository activity and branches, checking
immutable repository ID 1171374860 and authorized account ID 265118235. It chooses
the latest server-timestamped authenticated push/merge/branch creation whose SHA
is still reachable from its branch, including cmake. Author text and commit dates
are not credentials. Ties use activity ID, ref and SHA. Deleted/unreachable refs,
no candidate and unstable snapshots fail closed. A stable snapshot pins one SHA;
private gates revalidate the activity and reachability. `revision.json` retains
this evidence. Later pushes do not retarget an active run.

Core owners are the master catalog check, verified-download check, D1 CTest,
Gradle unit tests and controller overlay check. They execute before the sampled
phase in their lanes. Missing or failing core receipts fail the summary. The
remaining pool includes admitted host adapters, generated mission corpus owners,
private XL owners, public online D1/D2, transports and three-player tests. At most
one supplemental Pumo staging or developer Release compile is selected. Unavailable
fixtures/capabilities, physical/manual and unadmitted tests remain machine-readable
in `plan.json` and `summary.json`. Pumo staging is not gameplay qualification.

Ordering restores menu T's `Get-RuntimeHashOrder`: SHA256 of owner names within
SHA256-ordered lane/category groups, interleaved across groups. It never invokes
baseline regeneration. `nightly-sample.json` on `ci/nightly-tooling-state` records
completed owner attempts and failures separately from exact-SHA coverage. Each
night starts at the first undone owner in each lane's stable order. A missing or
setup-blocked receipt stays undone even if later selected owners completed.
Executed failures advance traversal but remain explicit historical failures.
Supplemental progress uses the trusted execution-step outcome: a failed compile
or staging verification advances and retains its failure; a prerequisite failure
leaves execution skipped and undone.
A SHA change does not reset traversal. Removed or changed owner definitions lose
credit; additions enter the same order. After every available owner has an
execution receipt, the cycle wraps. Quick core checks intentionally repeat.

Selected sampled master owners use their existing bounded case sampling with a
reproducible cycle seed, not FullSuite/FullExtracts/FullRouteCorpus. Owner traversal
is not exhaustive child-case coverage: only actual current-SHA receipts and their
coverage_complete fields describe this run's coverage. Historical failures are
labelled separately. A passing sample does not certify unselected work.

A configured-assets dry run admits 2,762 owners: 2,596 host, one JVM, 149
Android-single and 16 Android-dual. Their estimates are respectively 873,060,
600, 71,130 and 14,760 seconds (266.54 runner-hours total). Generated corpus
entries account for 2,406 host owners and 846,540 seconds; aggregate owners can
contain many leaf tests, so owner counts are not leaf coverage percentages.

The sampler configures up to 16 parallel host shards, three Android/JVM workers
and one supplemental (20 workers maximum). Public/private host owners use separate
shards; the same deterministic prefix is partitioned without duplicating owners.
A missing receipt from an early shard remains the next undone owner even when
later shards finish. Each receipt is validated against its exact shard before
aggregation. Quick host core runs once in its shard before that shard's sample;
other host shards can sample concurrently.

The current dry run selects 229 owners: 220 host (88 corpus), one JVM, six
Android-single and two Android-dual. Estimates are 41,370/600/2,400/2,700 seconds,
or 13.075 runner-hours plus supplemental staging. There are 377 unavailable owners
and 235 explicit catalog/policy exclusions. These are estimates, not measured
execution or leaf coverage. The earlier 15.16-hour host lower bound was
242.52 host runner-hours divided by 16 workers; a one-hour sampler retains that
parallel capacity rather than additionally reducing it to one host worker.

Host shards each select at most 45 estimated test minutes, including core. Other
lanes normally have the same allowance; their first sampled owner can exceed the
estimate behind repeated core to avoid starvation, but the real deadline wins.
Execution stops 48 minutes after planning, reserving two minutes for receipts;
setup consumes that same allowance. Test jobs have 55-minute ceilings and the
supplemental 45 minutes. Planning/reporting/publication and GitHub queues add
overhead. Job timeouts start when runners start jobs, so late queued provisioning
can extend elapsed duration despite the absolute test-execution cutoff.

Effective 16-host concurrency is unverified. The connected account reports no
plan; the runner API denies access, and no spending limit is visible. GitHub's
[standard concurrency limits](https://docs.github.com/en/actions/reference/limits)
are 20 on Free and 40 on Pro, shared with other workflows. Recent November 11-13
nightlies used three/four test workers; host jobs lasted 8m21s, 21m11s and 39m57s,
Android single 20m56s-23m20s, dual 18m31s-27m39s, JVM about 4m26s. Those different
samples do not establish runtime or capacity for this design. No trial run was
launched. Full job ceilings permit 18h10m of worker allocation plus controllers;
the planned test estimate is lower, and neither is an actual billing measurement.

Core completeness is explicit producer evidence, never inferred from exit zero.
Fixed core assertion suites emit a completion marker after all required assertions;
Gradle inspects each required XML report and marks skipped tests incomplete. The
master observation carries those cases into the collector and strict report.
Missing, skipped or unattempted evidence fails closed. Older candidate code without
these producers cannot claim complete core coverage.

Overlap uses one active workflow and GitHub's single pending slot, with
cancel-in-progress false. An unstarted successor can be superseded; running work
is not silently cancelled. Receipt publication runs even after test failures,
using a trusted isolated writer, bounded JSON and non-force updates; the older
ledger file is preserved. This prevents both unbounded queues and SHA resets.

The controller is pinned to the workflow SHA. Candidate checkout credentials are
not persisted. Private inputs are gated/staged by trusted code before candidate
code runs, and private lanes upload sanitized receipts only. Existing retail-ci
branch protection, read-only RETAIL_ASSETS_READ_TOKEN and immutable
RETAIL_ASSETS_COMMIT remain prerequisites. New branch-only owners require trusted
policy admission. Opening this draft PR does not activate the scheduler.

## First scheduled run follow-up

Run 43584661983 selected merge 755b368bffd2ed22fd1cf86eb5ebd549bdaa2b21 and
reported about 51 minutes after planning. It recorded 211 terminal owners:
179 PASS and 32 FAIL, with 18 selected owners still pending. Only 83 owners
explicitly proved complete coverage; PASS alone must not be described as full
coverage. The writer retained 206 sampled attempts plus Pumo, including the 32
failures, excluding all repeating cores and all unrun selected owners.

The Gradle core is explicitly `fixture-independent-gradle-v1`. The exact optional
integration leaves and missing prerequisites are declared in
`android/tests/gradle_core_policy.json`. They still run when their fixtures are
present. When absent, their exact IDs/reasons and actual XML PASS/SKIP leaf counts
remain in the receipt. Only that declared eligible core can be complete; whole
owner `coverage_complete` remains false whenever a leaf is skipped. An unknown
skip, missing module, zero executed tests or failed assertion cannot satisfy core.
The first run executed 1,322 leaves and skipped 12 (app: 1,240/8; metadata: 29/0;
proxy: 53/4); prior PASS:1/SKIP:2 counts described modules, not leaf tests.

Runtime `target-capability` exclusions are retained separately from `done` and
failures, never counted as attempted/pass coverage. Their scope includes the
owner definition, exact candidate revision, trusted capability-controller hashes
and immutable retail fixture identity.
They are requalified when any scope component changes and at cycle rollover;
ordinary SHA changes still do not reset completed traversal. Transient setup and
wall-budget blockages remain next-undone. This prevents a permanently unavailable
archive from blocking wrap without hiding newly available coverage.

Dual-device server setup reads the stable rust-toolchain.toml channel, using the
same parse as the repository Rust bootstrap. rust_version.txt is a multi-tool
inventory containing stable and nightly compiler lines, not a single-version pin.
Public artifacts exclude build trees under both core/sample phases; receipts and
owner diagnostics remain available. Geometry/baseline mismatches and execution
failures are not waived or regenerated by these orchestration fixes.
