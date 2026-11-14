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

A dry run on the current catalog with reviewed XL assets admits 2,762 owners,
selects 32 (five core plus 27 sampled), and selects Pumo staging. Estimates are
2,670/600/2,400/2,700 seconds for host/JVM/single/dual lanes: 2.33 runner-hours
plus the supplemental estimate. There are 377 unavailable owners and 235 explicit
catalog/policy exclusions. The plan serializes to 247 KB and the report parses it
under the unchanged 8 MiB limit. These are planning measurements, not executions.

There is one host lane and at most three Android/JVM lanes, plus at most one
supplemental job. The usual planned execution allowance is 45 minutes per lane,
including core. To avoid starving a single 45-minute owner behind repeated core,
the first sampled owner may exceed that estimate; the real deadline still wins.
Execution stops 48 minutes after planning, reserving two minutes for receipts;
setup consumes this same allowance. Test jobs have a 55-minute ceiling and
supplementals 45 minutes. Planning, final reporting and publication add overhead;
GitHub queue delays can exceed the approximately one-hour elapsed target.
Job timeouts start when a runner starts the job, so late queued provisioning can
also extend elapsed time despite the absolute test-execution cutoff. This
is a proposed bound, not a measured runtime guarantee. Four test lanes consume
at most 3 hours 40 minutes of job allocation plus 45 minutes for a supplemental
and small controller overhead; actual use should be lower. No full run was
performed to validate these estimates.

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
