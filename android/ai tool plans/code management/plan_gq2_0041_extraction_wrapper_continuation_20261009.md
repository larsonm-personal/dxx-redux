# CD GOG and DOS extraction wrapper continuation, 2026-10-09

## Scope and owners

Diagnosis and future implementation plan only. GQ2-CHUNK-0041 covers extract_all_cds.ps1, extract_all_gog.ps1 and extract_dos_demos.ps1. No product or test changes authorized in this tranche. These paths are branch-added; preserve inherited engine ownership, desktop support and handmade comments.

Existing GQR0118/GQF0131 owns first DOS output admission; BR0169 owns required-item accounting and aggregate status; BR0173 owns DOS process/workspace lifecycle; GQR0117/GQF0130 owns destination publication serialization; BR0174 owns supported hosts; BR0175 owns package/output/version policy; GQR0009/GQF0016 owns unrelated compiler termination; GQR0252/GQF0266 owns GOG regression reader generation selection. Do not allocate duplicate findings or use source-only evidence to close these owners.

## Retain current repairs

Retain CD source-set provenance, bounded native execution and rejection of error-bearing JSON even when native exit is zero. Preserve bound track hashes inside the manifested generation and explicit UTF8/no-BOM output. Retain recursive GOG payload admission and reporting of normalized relative paths, stable full-path ordering, zero/one/many result arrays and recorded-failure exit1. Preserve DOS source/package and executable digests, unique GUID workspace, required name/collision checks and existing finally/disposal. DOS diff is indentation-only except added compression assembly import; do not count the reindent as a new cleanup or lifecycle repair.

## Complete request accounting under BR0169

Validate explicit selection before build, unrelated process work, source hashing or publication. Distinguish absent selector from bound empty/blank selector. Use one authoritative request-set admission policy for the aggregate, then pass source-specific projections so a valid CD-only request does not require a GOG installer and an unknown request is never silently filtered away. Require exact outcomes for every requested item; detect duplicates and unknown/unavailable paths. Define required versus optional default sources explicitly. CD currently filters an empty or all-unknown file to zero folders and falls through success; GOG rejects zero matching installers but silently ignores unknown members alongside valid requests. Both use truthiness of the parameter rather than binding. These are the existing batch completeness boundary, not reopening the completed primary generator/disc/mission selector scope in GQR0230.

Combined specs require their declared source_specs closure for host extraction. New-CdRegressionSampleList samples independently by type and forwards the same unexpanded list; extraction wrappers match only their own physical spec path. Resolve and validate the selected combined source graph before dispatch, retain the selected test set separately and extract each required physical source once. Current stage-shape/type/seed fixture does not certify real extractor visitation or source closure. No stale-source/device failure was executed.

Use maintained tiny host fixtures with actual copied producers and recording native children for absent/default, explicit empty/blank, duplicate, unknown-only, valid-plus-unknown, CD-only, GOG-only and combined requests, and unselected bytes/mtime. Check preflight causes no build/tool/publication and exact requested-versus-processed dispositions. Keep valid cache success. Test good-bad-good batches with child nonzero, thrown helper, hash failure and report-publication failure; GOG currently has finally without a per-installer catch, so a thrown error aborts later installers/report rather than returning the intended complete result. Do not turn a thrown error into success.

## DOS output and lifecycle under GQR0118 BR0173

Use the reviewed package model to bind source filename/hash, required outputs, canonical version and authoritative initial output sizes/hashes or native validators. Completion manifest authenticates whatever bytes were first produced; it is not an independent initial output oracle. README contains reviewed output tables, which require provenance and freshness validation before becoming executable acceptance. Do not regenerate expected bytes from the same unvalidated producer under test.

Persist installer completion/error level in an owned DOS status artifact; shell EXIT and a two-poll size plateau do not prove installer success. Stop and confirm only owned process-tree termination before scan/hash/copy; share safe acquisition/cancellation policy with GQR0114 rather than invent another broad process abstraction. Bound the complete attempt including catalog visits, DOSBox materialization, scan/copy and cleanup under existing BR0018/GQR0212/BR0582 policies; ZIP supervisor limits currently apply only to its expansion phase. Keep live storage and cumulative work separate.

Protect workspace creation itself and final sibling staging with cleanup ownership. source/target creation126-127 occurs before try130; staging260-268 lives outside tempBase and has no independent finally, so ordinary creation/copy/manifest/publication exceptions can retain an owned partial sibling. Use literal-path operations and report retained resources; ensure process cleanup failure does not skip directory cleanup or mask original error. A fixed one-second sleep is not confirmed exit; final WaitForExit occurs after publication and is unbounded. Preserve current GUID/private paths and handle disposal while completing these gates.

Future ordinary acceptance: controlled installer signal versus shell exit, size plateau, required names plus wrong bytes, prior-generation preservation, ordinary copy/publication failure and delayed termination, unique workspace/stage cleanup and owned child sentinel. Known proprietary-package reproducibility is a separate explicitly provisioned gate. Malformed-media, security, allocation/resource-pressure probes remain deferred in this tranche.

## Shared publication and downstream identity

Serialize by canonical destination across admission, commit, rollback, hash-report synchronization and backup retirement. Bind losing rollback/cleanup to the attempt's generation; never remove another publisher's completed output. Cover absent and existing destinations, same/different generations and interrupted visibility with ordinary barriered host fixtures. Existing unique fixture-root producer.lock does not lock production destination.

Keep the CD top-level track_hashes.json synchronized with its bound manifested copy; cache repair118 is a direct overwrite and directory plus sidecar are separate publication operations. Apply shared prior-preserving/no-churn writer where appropriate, with exact parsed records and bytes/mtime controls. GOG summary245 writes canonical file directly, replacing prior report even for an incomplete batch; define a generation-bound complete-versus-failed report contract and publish safely without confusing a partial report for successful extraction. Reuse existing atomic writer/publication policy, not a new serializer layer.

GOG publisher already hashes only installer/extracted, recursively. test_extract.ps1 still selects installer parent and recursively includes unfinished .extracted siblings; retain separate GQR0252 and require existing completion/source policy at the published child. Exercise actual source selection with staged siblings and complete/absent/mismatched generations; publisher recursion does not fix the reader.

## Simplification and validation order

Replace unrelated Get-Process cl termination with owned build-process control under GQR0009. Align GOG known-version reading with existing shared JSONC owner rather than maintain regex comment stripping; preserve catalog lookup/file-basename semantics and validate complete quoted/comment records. Keep completed shared JSONC reader repairs intact; no new parser failure reproduced here.

Use shared package policy under BR0175 for PowerShell/Kotlin/README/hash_assets, including missing d2demo10_extracted canonical map and alias-order determinism. Gate unsupported DOS hosts before Windows dependency/WindowStyle lookup under BR0174. Require supported-host identity and exact outputs; do not claim the shebang or assembly import supplies portability.

Future implementation sequence: request/source policy and package owner; safe lifecycle/initial output admission; shared destination publication; actual downstream generation selection; behavior fixtures plus relevant aggregate/build/native/device gates. Run only scope-appropriate maintained checks after implementation. Diagnosis tranche runs none of these fixtures/builds/probes and does not close product acceptance.
