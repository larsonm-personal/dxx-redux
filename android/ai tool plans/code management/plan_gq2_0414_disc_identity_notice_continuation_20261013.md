# Chunk0414 disc identity and notice continuation, 2026-10-13

Diagnosis and plans only under plan_general_diagnosis_resume_20261009.md. Preserve existing BR-0643 identity ownership, BR-0006 documentation currency and GQF-0024/GQR-0011 acquisition acceptance. This plan adds no product change, new finding or separate remediation.

- [x] Read assigned guide hunks and complete supplied notices
- [x] Trace current matching, both import callers, host maintenance and synth packaging source
- [x] Reconcile prior direct references and current/archived owners
- [x] Publish exact canonical evidence and independently replay

## Fix identity admission before claiming the guide's contract

BR-0643 remains OPEN. Current identify can select later-track matches when its beginning prefix is zero; both path and SAF callers submit only the first audio hash. Require a beginning-track match and collect enough exact hashes to distinguish the maintained releases. Represent equally best candidates explicitly instead of allowing array order to choose an identity. Preserve complete/canceled/failed hash results and the transactional physical-record parser; archived BR-0410 stays FIXED.

Later implementation acceptance must exercise empty input, only track2, mismatched track1 with matching later audio, a valid beginning prefix, complete distinguishing discs, missing middle tracks and tied candidates. Cover identify plus both registration routes after successful database load. Known IDs, labels and legacy IDs must remain unset for ambiguous/zero-prefix input. Test maintained shared-first-audio aliases with reversed database order. Existing exact-read tests alone do not cover identity admission.

## Simplify the guide around actual producers and consumers

Under BR-0006, name known_discs.jsonc consistently and describe SHA1 as a160-bit digest used for preservation matching, not160-bit collision resistance or authentication. Preserve the compatibility hash format.

Separate physical hash generation from identity admission. Native extract_cd hashes each declared track's sector stride and handles standalone ISO as whole-file data-only. extract_all_cds.ps1 produces extraction-bound .track_hashes.json and the folder-level track_hashes.json. hash_disc_tracks.ps1 consumes those complete manifests, validates count/type/unique numbers/digest format against CUE or ISO, and updates the catalog. Existing records skip unless Force; review curated labels, IDs, inferred game, names and role mappings separately. update_known_discs_fingerprints.ps1 enriches existing audio records and is not the physical manifest producer.

Replace the claim that app import supplies every catalog hash with the actual host workflow, or a later verified complete app export if implemented. Explain current limitations until BR-0643 acceptance passes; do not present a documentation edit as repairing identity. Keep the digest/geometry/maintenance explanation concise and remove redundant blank spacing only as ordinary authored guide cleanup. Do not generalize all accepted CUE tracks to2352-byte stride.

Before publishing commands as a verified maintenance recipe, later run the supported host workflow on representative single-file, multi-file and ISO inputs, inspect complete schema-valid output and exact identity roundtrip, and check curated metadata preservation. Existing source-admission/cache/publication owners retain their separate acceptance; source inspection here does not validate current payloads or run a generator.

## Retain supplied notices and component identities

Keep all FluidSynth LGPL, GCEM Apache and nitro-shoe notice bytes intact. Current shared CMake selects reviewed FluidSynth2.6.1/GCEM pins, builds a shared libfluidsynth and links it into tsf. package_sources.py packages operator-selected configured source trees, the checked numeric cast, build settings and offline rebuild inputs. Corresponding-source identity must refer to the actual distributed build generation; source recipe inspection is not proof that any release published the matching archive.

GQF-0024/GQR-0011 already owns FluidSynth/GCEM verified acquisition cache/deadline/publication and retained patched-source acceptance. Preserve checked upstream patches and existing verifier work. Do not add a duplicate supply-chain finding or infer a missing release hook from a search.

The bundled SC55v1.34 asset and its dedicated catalog notice remain separate from optional TimGM. Current local gm.sf2 size10375822/SHA2f68d824f456e3367fe24105d590ee77b7e28faa9f622723b478fa90647d8a1a matches its configured source identity. Preserve the author-declaration and borrowed-sample caveat. No independent sample-rights, legal, release-package or source-offer certification is claimed. BR-0002/0557/0577 retain current statuses and distinct acceptance.

## Disposition and remaining gates

ISSUES/RETAIN; direct BR-0643 REFERENCE47 MEDIUM, components23/0/7/10/7, unpooled. This matches earlier GQ1-CHUNK-0632 and GQ2-CHUNK-0305 references without adding root severity. All five assigned Android paths are absent from the1996 original: NO_INHERITED_EFFECT, zero applied saving. No notice shortening/removal, hash migration or d1/d2 edit is proposed.

EntryPASS183 binds25current selections1640lines/sevenwhole,six full owners and11prior rows. SourcePASS94 binds all750assigned review lines/16hunks/current807lines. Canonical publication/replay/final gate/Drive receipt remain required before0414DONE. Missing global TSV/full older ancestry and broader queue/preflights/sweeps/investigations/finalHEAD/dirty-untracked/closure remain pending.

Publication PASS2722; future product work remains unapplied
