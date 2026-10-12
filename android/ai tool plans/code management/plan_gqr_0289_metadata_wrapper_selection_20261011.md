# Candidate GQR0289 metadata wrapper selection, 2026-10-11

Diagnosis-only candidate for GQF0303/GQR0289; neither identifier is admitted until chunk0386 canonical publication. Source-backed P2/high correctness/request-scope defect; no runtime reproduction claimed. Proposed47 MEDIUM (H23/M0/B7/C10/R7): ordinary explicit metadata request, authored paths absent1996, common metadata wrapper, exact branch predicate evidence, moderate host/emulator fixture work.

## Trigger and current behavior

Invoke android/helpers/regenerate_all_mission_metadata.ps1 -Engine Emulator -ArchiveNames selected.zip in a source set containing selected.zip and unrelated.zip, with default SampleFraction1 and MissingOnlyfalse. The parameter is accepted, but only the Host/Windows branch checks ArchiveNames. Emulator patterns resolve to *.zip, *.7z, *.rar for every available source. Its batch MetadataOnly mode therefore receives unrelated archives as well. The unfiltered CD host phase runs afterwards. The focused invocation cannot constrain its publication scope.

RoutingDevelopmentSet likewise computes routingArchivePaths, built-in flags and routing mission identities before dispatch, but Emulator ignores those archive paths and routing CD IDs. Its source loop is still full or sampled inventory, and the CD phase receives all CD sources unless sampling narrowed them. Built-in request flags are not passed into this phase. These are one selection-decision defect at the public engine dispatch boundary, not separate archive/CD/built-in findings.

NoBuild also remains ignored by Emulator build and unforwarded to its host CD phase; continue existing BR0536 build-policy acceptance separately rather than conflating build and output-selection ownership.

## Ownership reconciliation

Compared actual current public wrapper208lines against the complete current BR0162 and BR0536 definitions. BR0162 covers zero-pass/all-skipped sets, omitted explicitly missing members and stale skipped output. It does not cover an accepted explicit filter broadening a successful run to unrelated input. BR0536 covers child build-policy dispatch. BR0167 covers artifact generation collisions, BR0233 verified publication and BR0582 subprocess lifetime; these remain independent shared acceptance obligations.

Current keyword census covered461 active and212 archived BR headings/blocks plus302 complete general finding rows. Matching roots are enumerated in gq2-selection-checkpoint-0386-20261011.json. Initial verbose output truncated; complete heading census and all302 correctly parsed first-cell finding rows were recovered. Search evidence alone is not a complete semantic review of every ledger block. Relevant direct owner definitions are complete or previously qualified with exact PASS bindings. No matching existing request-selection root was identified. Existing generation/cache/source identity findings require different corrections and do not replace dispatch filter enforcement.

## Implementation boundary

Compute one source-qualified selection before choosing Host/Windows or Emulator. Preserve routing order, current hash-ring sampling names/seeds/state, MissingOnly semantics and the Windows alias. Specify precedence/intersection or reject incompatible simultaneous options before build/install/publication. Forward exact archive identities, selected CD IDs and requested built-ins to supported child entries. If an engine lacks an advertised capability, fail explicitly before touching output rather than silently expanding the request.

Do not introduce another metadata projection or move game-format knowledge into PowerShell/Kotlin. Keep native CD analysis and current host worker/pool, atomic publication, workspace containment, nonzero aggregation and first-install controls. Avoid basename-only filtering where distinct source archives have the same name.

## Maintained actual-entry acceptance

Extend a maintained fixture to invoke the real public wrapper against controlled source inventory and synthetic child build/batch commands, recording exact selected identities and arguments. Do not assert only source substrings.

Cover Host, Windows and Emulator with one explicit archive among unrelated ZIP/7z/RAR inputs and multiple source directories; routing archives with its CD IDs and both built-ins; repeated basenames; missing explicit members; MissingOnly; seeded sampling; conflicting filters; empty selection; and child failures. Seed unrelated archive/CD regression outputs and prove their bytes and timestamps remain unchanged. Require requested sources each to receive one truthful disposition, supported selections to agree across engines, and unsupported/rejected requests to launch and publish nothing. Preserve existing BR0162 minimum-success and stale-skip policy checks.

Build-count controls for NoBuild/default belong to the shared continuation. This diagnosis does not run builds, tests, devices or generators and does not close any implementation or runtime gate.

## Canonical admission, 2026-10-11

GQF0303 OPEN/GQR0289 TODO admitted with chunk0386 GQC1215/GQD1095. Independent publication audit PASS5763 SHA0f7dd6c30ca30580cf489b70b61188b04c461135f039e14ff0d2e606f43afa09. Earlier candidate text is preparation history; implementation and maintained actual-entry execution remain pending. No runtime pass or product change.
