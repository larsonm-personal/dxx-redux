# GQR-0286 black-output integration registration, 2026-10-11

Diagnosis and future implementation plan only, under plan_general_diagnosis_resume_20261009.md. Proposed GQF-0300/GQR-0286 are reserved for chunk0380 canonical publication; no canonical admission until publication. No code/test changes or runtime validation in this tranche.

## Problem and owner boundary

The aggregate test named test_graphics_black_output resolves to the default-standalone JSONC and run_test.ps1. The parent run_graphics_black_output_tests.ps1 lives only under helpers, outside top-level android/tests/test_*.ps1 discovery. Consequently aggregate success omits independent Kotlin rollback modal shown/attached, full accepted/current/requested tuple, three persisted graphics configurations and unchanged game PID assertions. The native scenario still checks injected black frames and rollback; preserve it. A coverage bucket name does not invoke the missing parent.

This is a concrete registration/false-coverage defect, separate from quick-suite stale names GQF-0025/GQR-0012, fingerprint/WAV registration GQF-0068/0165, bounded-runtime registration acceptance GQF-0158, build-policy BR-0536, shell all-mode metadata bypass BR-0594 and completed quoted-string parser BR-0552. No earlier code-management plan names this black-output registration boundary. Existing BR-0539 owns target/package/ambient identity and bounded ADB; BR-0543 owns process/app teardown. Coordinate those prerequisites without forming duplicate findings.

Confirmed proposed rating:44 MEDIUM,23/0/4/10/7. H23: material missing integration assertions in graphics rollback release validation; M0: all affected paths branch-added, no inherited reduction; B4: focused graphics rollback workflow; C10: exact discovery/metadata/dispatch/parent witnesses; R7: clear local routing fix with moderate platform acceptance. No runtime failure reproduction is claimed.

## Implementation after diagnosis

- [ ] Add thin android/tests/test_graphics_black_output.ps1 forwarding Game, Serial and Install to the existing helper, following the maintained FOV/preview wrappers; do not duplicate observer logic or create another registry.
- [ ] Mark child test_graphics_black_output.jsonc _standalone=false and _owner=test_graphics_black_output. Retain current native functional assertions and existing coverage bucket.
- [ ] Add bounded master budget sufficient for default sequential D1/D2 cases, provisioning, child/ADB/cleanup phases. Use existing selected-target and safe process argument mechanisms; a larger watchdog cannot replace bounded calls.
- [ ] Coordinate BR-0539 package-scoped raw broadcasts, explicit selected target/artifact and restoration after initialization or diagnostic failures. Coordinate BR-0543 guaranteed owned child-tree/app stop, wait/disposal and primary-error preservation when LeaveRunning, observation timeout, cancellation or reporting fails.
- [ ] Extend actual aggregate discovery/dispatch fixture with recording boundaries: stable test resolves once to PowerShell owner, JSONC support only, missing owner/duplicate identity fail, Game/Serial/Install and Windows/Linux paths containing spaces preserved.
- [ ] Execute registered paired integration after focused checks, scoped quality and both maintained catalog validators. Capture fresh outputs and propagate failed/not-run status before implementation closure.

## Required future acceptance

Drive the registered integration while child returns native PASS but modal is hidden/detached, accepted/current/requested restoration differs, any mirrored config differs or game PID changes. Each case must fail the integration. Positive paired controls retain every observer assertion. Exercise child exits1/7, stale/missing/late files, transport hangs, denied reporting and cancellation; prove bounded owned cleanup and unrelated device/package/process sentinels unchanged. Catalog/source-regex fixtures alone cannot prove those outcomes. Historical October1 evidence is not fresh runtime evidence.

No inherited engine hook, engine format copy, new abstraction or multi-file transaction is needed for this registration. Preserve desktop behavior and both games. Do not modify this plan's reserved IDs if canonical publication encounters competing IDs; reconcile first.
