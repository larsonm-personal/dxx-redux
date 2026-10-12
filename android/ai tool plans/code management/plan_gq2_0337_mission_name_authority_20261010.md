# Chunk0337 continuation: mission short-name authority, 2026-10-10

Diagnosis and plans only. This is a source-backed continuation of chunk0337, not a completed coverage record or an allocated canonical fix. No product, test or helper edits and no builds, tests or device execution.

## Confirmed source boundary

With two enabled valid managed packages containing same-game descriptors named sample.mn2 (or sample.msn for D1), MissionScanner.scan keeps the first basename and discards later candidates at MissionPicker.kt:223. The assigned frozen hunk19 introduces this managed-wrapper loop; its raw SHA256 is 3a05d8542e848eecd280d56536722370fe0ca56ac4de9bea2e2bb90a2dd022f1. This changes the actual list before resolveMissionSelection can reject ambiguity.

ModManager.buildMissionLaunchCatalog retains both enabled packages. MissionLaunchCatalog keys identities by owner/descriptor/game and explicitly rejects ambiguous legacy lookup. MissionLaunchPublication emits separate missions/_packs/<owner-key-hash>/... aliases, and writeEnabledModPaths includes their discovery directory. Paired native recursive discovery preserves both entries and derives the same leaf basename for each. Paired load_mission_by_name builds its list with anarchy_mode=1 and rejects multiple short-name matches. This native rejection must remain.

MissionCatalog.find invokes unique selection against the already deduplicated list. MissionCompatibilityResolver can therefore report MATCH for the first wrapper when its size/hash matches, even though native lookup of the advertised short name rejects competing published descriptors. Inference from source: host eligibility and local mission-ready status can be false for this configuration. No runtime reproduction or test execution is claimed.

MissionLevelAdmissionTest:24-27 rejects a manually constructed duplicate list; it does not show that actual scanner output retains a real package collision. Archived BR-0426 FIXED explicitly claims ambiguous selection rejection. Proposed normalization is regrowth of that admission boundary, preserving the historical fix, native range checks and native ambiguity rejection. Provisional impact44 MEDIUM, H/M/B/C/R=23/0/4/10/7; canonical ID and admission remain pending.

## Historical reconciliation

The existing multiplayer/third_party_coop_mission_sync_20260829.md phase0 requires basename-collision fixtures and phase1 requires authoritative prepared-catalog host selection. These are relevant unfinished acceptance requirements, not proof that the current scanner agrees with native visibility. BR-0440 concerns anarchy-only mode admission; GQR-0102 concerns immutable admitted bytes and generation; GQR-0212 concerns resource bounds. Do not duplicate those roots or claim their closure. Active/done general and adversarial ledgers, DMR1 and earlier plans were searched by scanner, legacy resolution and ambiguity/collision keys; no exact competing canonical root for this discarded-candidate mechanism was located.

## Proposed implementation boundary

- [ ] Preserve native-visible competing identities through catalog construction, before mode-filtered presentation or short-name resolution
- [ ] Reject or visibly disable ambiguous short-name selection and block MATCH before lobby publication/start; reuse typed mission catalog authority where practical
- [ ] Keep the existing protocol short-name boundary unless a separately scoped protocol migration becomes necessary; do not silently choose one package or weaken native rejection
- [ ] Reconcile D1-in-D2, readiness, built-in, loose and managed visibility against actual native discovery; an anarchy-only competitor still participates in load_mission_by_name(1)
- [ ] Preserve IO loading, cancellation, parser semantics, wrapper identity checks, native tuple/range admission and existing narrow inherited hooks

Prospective product paths: multiplayer/MissionPicker.kt, MissionCatalog.kt and MissionCompatibilityResolver.kt; CreateGameDialog.kt only if required for ambiguity feedback, MissionLaunchCatalog.kt only for a narrow shared authority adapter. Do not edit native/inherited files for this root without newly justified scope. Coordinate immutable source-generation work with GQR-0102 and resource limits with GQR-0212.

## Future validation

Use actual imported/enabled valid packages with the same short basename and distinct descriptor/resource identity, not only synthetic MissionInfo lists. Reverse package order; vary case; cover duplicate descriptors within one archive, loose/managed and built-in overlaps, mode-filtered competitors, D1/D2/D1-in-D2 and disabled competitors. Check actual scanner, host admission, compatibility status and published/native discovery against one authority. Include positive unique-mission controls and preserve native rejection. Extend maintained MissionScannerManagedArchiveTest, MissionLevelAdmissionTest and relevant native asset fixtures. Platform execution is future remediation acceptance; none was run in this diagnosis tranche.

## Diff-minimization assessment

RETAIN remains provisional for chunk0337 until remaining owner closure. Existing compiled shared autonet bodies, guarded per-game header adapters and typed scalar wire/layout/restore hooks already provide the narrow natural inherited boundary. The prior checkpoint measures eleven related containing frozen hunks, 82 additions/12 deletions including unrelated enclosing work; these are not removable savings. This short-name authority repair belongs in branch-added launcher owners, with zero inherited savings claimed and no reason to add native callback layers or duplicate policy.

## Evidence and resume gates

The supplemental JSON binds 22 freshly read source/history contexts and verifies all51 prior checkpoint contexts unchanged. HEAD and allfive canonical ledger digests still match; the product diff has changed elsewhere since the historical checkpoint. Current and historical product identities are recorded separately. Frozen-context verbatim comparisons describe only the selected context, never whole-file delta coverage.

Chunk0337 remains TODO:336DONE/291TODO/627. Complete remaining fixtures/launch/native/source ownership, final minimization, canonical normalization, report/snapshot/baseline/publication and independent audit before marking it DONE. Supplements, sweeps, investigations and final live-head closure remain required for the active goal.

Selected publication-function drift is fully read: current writeEnabledModPaths adds D1 non-level packs at the d1-mods namespace. Managed mission catalog/discovery publication is unchanged. Preserve this current isolation repair; no whole-ModManager delta coverage claim. Resume fixture onlineClientRecord539-555 and actual toGameInfoJson95-112 now read and bound; QoL values remain explicit transport projection, not source-generation validation.
