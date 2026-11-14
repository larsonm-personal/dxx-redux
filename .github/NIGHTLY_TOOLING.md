# Nightly tooling

The scheduled/manual `nightly` workflow runs all admitted unattended owners on
one revision selected using authorized GitHub push provenance. The trusted
`android/run_all_tests.ps1 -ListTests` catalog discovers master owners;
`android/tests/nightly_tooling_policy.json` admits supported owners and estimates,
and `host_test_suites.json` defines fixed host adapters. The complete campaign
uses independent bounded shards and starts fresh each night, without advancing
or consuming the old rotating ledger on `ci/nightly-tooling-state`.

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
seconds; fixture-free discovery and broadcast are included in full nightly coverage.

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

## Every-night coverage and revision provenance

Scheduled runs (02:15 America/Los_Angeles) and the manual `nightly` campaign use
`nightly_revision.py` and `nightly_complete.py`. This behavior starts only after
these changes reach the default branch, `cmake`. PR samples and explicitly named
manual campaigns retain their selections.

Revision selection uses GitHub's repository activity API, with full Link-cursor
pagination, immutable repository ID 1171374860 and authorized account ID 265118235. It chooses the latest server-timestamped push, force push, branch
creation or merge whose resulting SHA remains reachable from that same-repository
branch. Commit author names, email addresses and commit timestamps are not
credentials. Ties use activity ID, ref and SHA. Deleted branches and unreachable
revisions are ineligible; no candidate or unstable snapshots fail closed without
falling back to cmake. A stable snapshot pins one SHA for all shards; later pushes
do not retarget an active run. Private lanes revalidate the recorded activity and
reachability before accessing assets. `nightly-plan/revision.json` records the
selection evidence.

The trusted workflow catalog selects all admitted unattended owners every night,
including online-preview, D2 online-preview, the three transport owners, and the
explicit three-player owner. Exact master selection uses FullSuite, FullExtracts,
FullRouteCorpus, ExtendedGraphics and ExtendedMultiplayer. FullSuite alone is not
a claim of exhaustive coverage. The host XL originals/fast and Android originals
owners run when the reviewed immutable private fixture is available. Pumo staging
and the asset-free XL developer Release compile run as supplemental jobs. Pumo
staging verifies the pinned inputs; it does not prove candidate Pumo gameplay.

`nightly-plan/complete.json` contains every bounded shard and machine-readable
exclusions/unavailable reasons. Public and private work use separate shards so
public artifacts cannot contain private lane output. Host shards have a 90-minute
estimated test budget; other shards have 45 minutes. Job limits include setup and
allow a single retained long-running owner to finish. At most 16 host shards and
three Android/JVM shards run concurrently, subject to GitHub account limits.
A matrix exceeding 256 shards fails explicitly instead of dropping work.

At the initial cmake catalog with the reviewed XL asset revision, planning yields
2,762 selected owners in 198 shards (162 host, 29 Android single, six Android
multi-device, one JVM), 377 unavailable owners and 235 catalog/policy exclusions.
These are planning counts, not executed test results. Estimates total roughly
267 runner-hours before setup; account concurrency, queues and actual runtime can
extend the run beyond one night. This deliberately replaces the old 45-minute
per-lane rotating sample and needs corresponding Actions capacity/budget. No
progress from an earlier SHA suppresses a later night's tests.

Only exact-SHA, exact-batch execution receipts retire selected work. The summary
requires PASS plus complete coverage evidence for every selected owner; missing,
skipped, failed or partially covered owners and unavailable configuration make
the result incomplete. If only documented target-capability/manual exclusions
remain, the result is `passed_with_exclusions`, never `complete_pass`. Supplemental jobs must also succeed. Stable unsupported corpus levels,
missing fixtures, unadmitted owners and physical/manual checks stay visible; they
are never reported as passing coverage. New branch-only catalog owners require
trusted policy admission on cmake before they can run with private fixtures.

The controller is checked out at the workflow SHA. Private inputs are gated,
checked out with `persist-credentials: false`, staged by trusted code, and removed
before candidate setup/build/test code runs. Candidate jobs have read-only GitHub
permissions and no persistent checkout credentials. Private jobs upload only the
existing sanitized owner receipts. The old progress publisher does not run for
complete nightly campaigns. Existing `retail-ci`, `RETAIL_ASSETS_READ_TOKEN` and
`RETAIL_ASSETS_COMMIT` configuration remain prerequisites; missing configuration
is recorded as unavailable coverage. No scheduler or secret change is activated
by merely opening the PR.

Overlap is bounded with GitHub's default single pending slot and
`cancel-in-progress: false`: one active sweep finishes and at most one successor
waits. A newer trigger replaces a still-pending, unstarted successor; GitHub marks
that superseded run cancelled. It never cancels already executing shard coverage.
A delayed successor selects the latest authorized revision when it actually starts.
This prevents an unbounded backlog, but does not guarantee a result for every
calendar night or every intermediate commit. With the initial catalog the ideal
host lower bound is about 15 hours at 16 workers; the three-hour per-job ceiling
allows up to 33 hours across 11 host waves before queue/setup variability. A
sub-24-hour finish is unproven and must not be assumed when approving deployment.
