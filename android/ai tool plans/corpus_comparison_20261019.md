# Reviewed corpus comparison

Scope: prepare a separate draft PR for the approved paired comparison tooling.
No private dispatch, baseline publication, routing change, security-setting change,
merge or cmake push is part of this work.

- Pin base, candidate and assets; reuse current metadata/routing producers
- Inventory identical inputs, retain uncovered and duplicate identities, shard all
  available archives and built-ins, and run each revision twice
- Preserve structured outcomes before normalization and report changes separately
- Require trusted manual activation through the existing retail-ci environment
- Verify gates, completeness and comparison behavior with asset-free unit tests,
  scoped formatting and workflow lint; publish a draft PR

Native corpus execution remains pending separate activation approval. The local
process-supervisor access denied previously is not retried or bypassed.
