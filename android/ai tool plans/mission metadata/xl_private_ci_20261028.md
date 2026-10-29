# Fast XL and Redux regression coverage

Continue the authorized run through 2026-10-29 04:54:16 UTC. The user requested
fast, broad XL CI using the existing private asset repository, plus controls
against changing Redux behavior. Propose additional original assets where useful;
do not add or repack private assets as part of this change.

## Existing inputs and gaps

The private checkout is clean at 8c9988a96a7e306f8aac04c85e2610492917b7ac.
It contains pinned retail D1/D2/Vertigo data, thirteen small derived XL bundles,
four original standalone archives, the original 102161128-byte levelpack, and
nine original format witnesses. The manifest stops at 23/40, but its pinned
original Boiling Point archive already contains modern levels and can supply
original 27/40 witnesses without adding an asset. Pumo's original 27/40
distribution is available locally outside that repository.

The existing retail-ci environment, immutable asset revision, trusted controller,
hash checks and sanitized receipts already supply the necessary access boundary.
Reuse them. Private archives or raw engine output must not become public artifacts.
Public PR checks retain no dependency on private credentials.

Current coverage has drifted: the original-format runner still expects 15/34 to
be rejected; its catalog description says 40 native tests while the required set
has grown to 85. Newer runtime tests exist but some important feature/stock
controls are absent from that required set. Requiring source comparisons without
checking the exact selected tests can leave newly added oracles unexecuted.

## Implementation order

1. Complete and publish the fog increment, including public offscreen GPU checks
   for actual shaders, depth transfer, geometry/culling, MSAA and inactive stock
   behavior. Keep Android lifecycle evidence distinct from desktop GPU evidence.
2. Inventory available native tests and their execution times. Build a compact
   required private selection covering admitted layouts, event dispatch, ownership,
   generators/bosses, capacities, liquids, save/restore and stock transitions.
   Include real original bytes, unsupported-format rejection, and negative recovery.
   Avoid expensive route searches in the fast lane; retain separate campaign evidence.
3. Reuse native builds and run independent controls concurrently where their output
   directories and state are isolated. Record measured duration and exact test
   membership. Fail on missing tests, skips, absent inputs or unexpected format
   outcomes. Source oracle and native gameplay coverage should both be explicit.
4. Integrate the selection with the existing trusted private CI controller and
   public policy tests. Keep ordinary public checks fast and credential-free.
   Run locally against the actual private input revision, then publish and check CI.
5. Reuse the existing original Boiling Point 27/40 content and propose original
   assets with hashes and the coverage each adds. Anthology supplies a real 25/40
   witness; Lost Relics supplies a smaller 27/40 single-level witness; original Pumo supplies full container/resource
   coverage. Keep generated variants in the test generator,
   with provenance, rather than checking fabricated originals into the asset repo.

## Acceptance

Report the actual selection, time, stock controls and remaining omissions.
A green private lane proves its specific contracts and original inventories;
it does not qualify an original campaign or substitute for human playtesting.
