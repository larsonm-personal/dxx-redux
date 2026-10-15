# GQR-0303 proposed metadata-only verdict repair, 2026-10-14

Proposed for final GQ2-0441 canonical admission with GQF-0317; not implemented or yet imported into canonical tables.

Problem: the direct imported-D1 Metadata helper mode enables GameLog for diagnostics, replaces the gameplay script with startup plus metadata/automap checks, then requires FLYOUT samples through its general GameLog branch. In a clean metadata-only run, no selected step executes that native flythrough. A valid correlated metadata PASS can therefore be rejected before the helper reports metadata success. Frozen and current control flow retain this mismatch; the default registered wrapper does not select Metadata.

Change the test owner to distinguish diagnostic collection from scenario-specific flyout validation. Preserve diagnostic logging, checked ADB staging, unique run ID, process/crash checks, native engine/profile/cache/readiness/adoption assertions and cleanup. Apply the existing flyout record/geometry requirement only when the selected scenario deliberately exercises it. Do not inject an unrelated exit or weaken metadata correctness.

Validation to implement later: metadata-only successful native assertions and matching PASS with no flyout records must pass the metadata verdict. Wrong engine/profile/cache identity, failed adoption, native FAIL and absent/wrong run ID must still fail. A deliberate flyout scenario must still reject absent/invalid flyout records and accept valid records. Cover diagnostic logging independently of scenario selection and preserve isolated data on success/failure. No runtime acceptance is claimed.

Direct rating47MEDIUM H/M/B/C/R23/0/4/10/10: focused regression falsefailure, no inherited saving, one helper mode, exact source witness, localized test-owner policy repair. Existing package/default-renderer/FM-fixture/staging/schema/replacement/output/native owners remain distinct. No product code or shared engine-format changes are needed for this repair.


## Canonical admission, 2026-10-14

GQF-0317 OPEN and GQR-0303 TODO were admitted by GQ2-CHUNK-0441/GQC1270/GQD1150, independently replayed PASS4517 and mounted Drive verified. The proposal wording above records the prepublication phase. The repair remains unimplemented; no runtime or inherited saving is claimed. Exact immutable report/snapshot and proof receipts are linked in the controlling0441/resume handoff.
