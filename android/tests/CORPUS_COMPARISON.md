# Reviewed paired corpus comparison

This manual experiment compares base `3bab162341dd85dab3bb7978a68d5ab17b99ff4d`
with PR #96 candidate `cecd63fe4951d7134786ae18e24d6335cd70672b`. Its reviewed
configuration pins asset revision `5fb5b199f9f8c670cff7b0b4ea7086c8505bbefa`.
Preparing this workflow does not qualify PR #96: no paired corpus run has occurred.

## Activation

The draft cannot run against private inputs from its feature branch. Separately
review and approve the tuple and this workflow, merge the tooling to `cmake`, then
explicitly authorize and dispatch **Reviewed corpus comparison** on `cmake`.
There are no dispatch SHA inputs. The controller checks repository, event, ref,
workflow identity, configured asset pin and all three checkout revisions. The
existing `retail-ci` environment, deployment restrictions, read token and repository
variable must already support that trusted run. A mismatch stops execution; this
change does not adjust any of those settings or the existing nightly gate.

The configuration permits only collection and artifact reporting for that exact
base/candidate/asset tuple and fixed settings. It authorizes no baseline update,
merge or deployment. For a future comparison, review a small PR replacing the
three pins and documenting its purpose; review any required setting or asset-pin
change separately. The same trusted activation procedure applies. There is no
campaign registry, arbitrary-ref input or new scheduling framework.

## Collection and coverage

Four shards build both revisions with the same dependencies before private checkout.
The existing retail verifier stages identical edition files. The existing corpus
catalog and exact-pin archive staging determine the selection. Each available
archive and both built-ins run through the existing native metadata producer and
headless GuideBot producer with full sampling, two repeats, speed 160 and two route
workers. Native watchdogs and simulation-budget formulas are unchanged. Per-head
budgets are retained because a route-length change can change the computed budget.
Nested mission variants are passed directly to the maintained producers; a producer
skip, failure or changed selection is incomplete coverage, never a successful skip.

The manifest includes every catalog level, SHA-256 of committed metadata/simulation
inputs and staged archives, the retail inventory digest, revisions and settings.
Missing archives/CD inputs remain explicit uncovered rows. Identical archive hashes
and repeated existing route-input hashes are grouped as duplicate evidence; all
logical entries still run. These groups are not a complete asset-content deduplication
and do not claim that every unmatched catalog entry is unique missing content.
Committed corpus inputs must match between revisions. Their hashes and staged input
hashes are checked again after collection. No producer receives WriteRegression;
metadata uses NoRegressionCopy and fresh scratch inputs.

## Evidence and interpretation

Only controller-produced JSON is uploaded: generated metadata, normalized level
results, selected pre-normalization native status/problem/frames/objectives/RNG,
process exit/timeout receipts, per-head budgets, manifest, phase receipts and paired
rows. These are structured raw outcomes, not unrestricted stdout/stderr logs.
Archives, game files, executables, launch files and logs are not uploaded. Local
producer scratch directories are removed after evidence extraction. A process that
emits no native JSON retains its process failure and normalized result; metadata
producer failures are recorded as incomplete phase outcomes.

Improvements, regressions, other semantic changes, repeat disagreement, unsupported
results, uncovered entries and incomplete collection remain visible separately.
Unbound matcen history is retained in metadata artifacts but the existing semantic
projection excludes its accidental masks/models. Metadata changes are counted even
when routing is unchanged. Missing shards or assigned rows fail aggregation. Regressions,
nondeterminism and incomplete rows fail the report job. An otherwise green report
can still contain uncovered rows and requires manual review; it is not blanket
baseline acceptance. All generated results are artifacts, never committed references.

## Local verification limits

Run `python3 android/tests/test_corpus_comparison.py` for asset-free contract tests.
The normal tooling CI includes these tests on Linux and Windows. Workflow lint and
scoped repository quality checks also apply. No private/native corpus is run locally:
the previously denied supervisor /proc access is neither retried nor disabled.
The first approved hosted run must validate producer integration, duration and actual
asset coverage; unit tests cannot establish those properties.
