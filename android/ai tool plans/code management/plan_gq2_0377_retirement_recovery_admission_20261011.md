# GQ2 chunk0377 retirement recovery admission, 2026-10-11

Diagnosis and future implementation plan only under plan_general_diagnosis_resume_20261009.md. Candidate remains unadmitted: no canonical finding/remediation ID, implementation or runtime reproduction. Chunk0377 remains TODO

## Source-derived failure contract

clean-dependencies.ps1 computes protected checkout names at255/289-305, then calls Complete-DependencyRetirement at306. Recovery181-227 checks record spelling, plain quarantine path, duplicate original marker, process/environment/PATH/cache use and two fresh tree scans. It never checks the retirement directory against protected names or calls Test-ManagedReplacement. New retirement314-343 performs both checks, rereads checkout pins and persists only Directory/Identity before moving the tree

After termination following quarantine, a checkout can restore its old pin or the configured replacement can disappear. A subsequent -Apply still deletes the remaining quarantine when process/environment/cache checks are clear. The computed protected list does not authorize recovery. Separately, an added file or replaced plain quarantine tree before retry becomes the newly accepted scan; journal Directory/Identity and current path/length/mtime digest do not prove that payload belongs to the original retirement. These are source-derived ordinary-file witnesses, not executed reproductions

The maintained test_managed_dependencies.ps1 forced-termination child deletes the ownership marker and dies before recursive deletion. Its unchanged retry expects successful deletion. Preserve this supported case; demanding the removed marker on every retry would break the existing recovery contract

## Ownership reconciliation and admission limits

BR-0159 owns complete usable installation/cache/replacement admission. BR-0579 owns public dependency configuration generation and concurrent updater writes. Neither inspected full block specifies changed-state authorization for a persisted destructive retirement. Broad canonical definition/title inventory found no dependency-quarantine recovery root; inventory is discovery rather than complete review of unrelated findings. Final admission still requires all relevant consumer/helper and prior-plan reconciliation in the enclosing chunk. Related SDK transaction recovery does not by itself prove versioned dependency retirement safety

## Future implementation boundary

Allowed product owner: android/get_deps/clean-dependencies.ps1, with the existing ownership publisher/helper only if required for an actual shared contract. Extend android/tests/test_managed_dependencies.ps1. Keep per-root exclusive ownership lock, cross-checkout registration, preview behavior, current/unmanaged protection, process/PATH/environment/cache protection and idempotence. Preserve native source and desktop behavior

Use one understandable admission policy for new and resumed retirement. While holding the ownership lock, reread every relevant registered checkout, reject current pins and unavailable checkout evidence, and require the same configured replacement admission before every destructive continuation. Recheck original and retired process/cache references immediately before deletion. Coordinate updater configuration generation with BR-0579 rather than assuming this ownership lock serializes independent tool_versions.conf writers

Persist evidence binding the pending retirement to the actual admitted root and payload before move/deletion. Recovery may accept deletion of already admitted members but must reject additions, changed remaining payloads or replacement of the quarantine root. Select a portable root/object and remaining-member identity strategy with explicit Windows and Linux limitations; freshly hashing the current tree alone cannot establish prior ownership. Do not discard the journal or orphan the quarantine to make retry appear successful. Treat prior journals lacking sufficient proof conservatively; the disposable launcher-format rule is not permission to delete unproven dependency payloads

A now-referenced partially deleted tree may be unusable. Protect remaining bytes and emit an actionable recovery/repair result; do not silently restore it as a valid install or delete it because repair is inconvenient. Keep truthful per-item outcomes and persist completed retirement only after successful removal

## Actual-entry acceptance for a future fix

- Extend the existing forced-death fixture through the real cleaner entry; after quarantine, restore the old pin in current and second registered checkout independently. Retry -Apply must protect remaining payload and journal
- Remove or invalidate the configured replacement after the cut. Retry must protect the pending retirement; restoring an admitted replacement permits eventual completion only if the old version is unreferenced
- Add an ordinary file, change a remaining file (including equal length/mtime where identity claims require content proof), and replace the plain quarantine directory with another tree. Each retry must preserve all unproven bytes and report protection
- Retain unchanged partial deletion with missing ownership marker as a success control. Assert exact remaining payload identity, completed journal cleanup and idempotent repeat; outside linked-file sentinel survives
- Cover preview, unreadable/unavailable checkout, original and retired process/cache references, PATH/environment/PowerShell links, two competing cleaners, publication failure and bounded lock wait using the maintained isolation. Failures must preserve journal/payload and allow a truthful later retry
- Exercise Windows and Linux actual host entry points, aggregate get_all.sh and updater install-sync retention handoff. Run the maintained fixture and required catalogs/quality during the separately authorized implementation phase. No tests, downloads, builds or device actions are authorized by this diagnosis document

## Current disposition

Provisional P2/high source candidate; no ranking or new ID assigned. Both assigned files are branch additions relative to the 1996 source tree, so proposed repair has zero inherited saving. Finish complete consumer reconciliation, exact evidence publication and independent canonical audit before chunk0377 DONE
