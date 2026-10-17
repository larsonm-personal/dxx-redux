# Nightly regression sampling

## Goal

Keep the existing four estimated 45-minute lanes and exact-commit receipt ledger. Include
actual mission metadata and route regressions alongside existing extraction,
fingerprint, audio, save/replay, Android import, graphics and network owners.
Never regenerate or approve golden files in CI.

## Implementation

- Discover metadata and route owners from committed mission baseline files
- Reuse the existing metadata host with NoRegressionCopy and explicit built workers
- Reuse the existing route sampler with a reproducible commit seed, exact per-level
  identities, relative-file matching, and no cursor file or WriteRegression
- Compare full metadata including geometry and semantic dates, and full selected
  route state/header including imported-content mode
- A matching known non-ok route is only an oracle comparison, not confirmed
  gameplay; its actual outcome remains explicit and receives no covered-leaf count
- Missing fresh evidence never passes. Changed outcomes/states fail. Missing
  assets, missing oracles and unsupported variant stages remain explicit exclusions
- Interleave existing owner families in each bounded lane; only executed terminal
  receipts advance the current commit's coverage cycle
- Add the independent asset-free basic-use campaign and retain strict public LAN

## Private boundary

Corpus admission requires identical controller and exact target revisions; a race
with a newer cmake commit fails planning and must retry. The existing trusted cmake scheduled/manual gate and retail-ci environment remain
unchanged. Corpus staging requires the reviewed immutable revision, verifies each
selected archive against its committed Git blob, and preserves source-relative
paths. It does not equate similarly named root and nested-source archives. Private
logs, generated metadata, route state, archive bytes and extraction outputs are
never uploaded. Only existing sanitized receipts leave the retail lane.

## Deliberate limits

Each night samples individual levels; terminal receipts traverse the full admitted
level pool on the same commit before the cycle stops. Missing/unsupported levels
remain explicitly outside confirmed coverage. Nested descriptor variants
are excluded from both metadata and route execution until their existing stages become variant-aware. CD-image source
metadata, absent archive references and physical-device behavior remain explicit.
CD regeneration/AcoustID publication are not invoked: menu T is a writing producer,
not a read-only validator. Existing extractor and strict-fingerprint owners retain
their actual public/synthetic scope and internal private-media exclusions.

## Validation

Pure Python planner, adapter, corpus and private-boundary controls run locally.
Setup overhead remains outside test-time estimates; corpus owners additionally have
bounded wall-clock watchdogs. PowerShell execution is unavailable in this executor; existing hosted tooling jobs
own parser/catalog/profile checks. Public basic-use qualification runs separately.
Private runtime qualification requires the trusted default branch and reviewed
asset pin; a pull request cannot certify that runtime path.
