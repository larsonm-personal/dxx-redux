# Next 30 parser and correctness fixes

## Plan

- [x] Inventory all open findings and rank 30 non-network correctness candidates by severity, data-format relevance, implementation locality, and testability
- [x] Record the selected queue and dependencies without disturbing the existing dirty remediation worktree
- [x] Remediate candidates sequentially, keeping partial fixes open and archiving only complete dispositions
- [x] Add or extend focused regression coverage for every completed behavior change
- [x] Run scoped quality checks and proportionate native, Android, and script validation during remediation
- [x] Run the broad native, Android, script, and regression test suites after the tranche
- [x] Diagnose and fix test fallout attributable to the accumulated audit fixes
- [x] Validate ledger uniqueness, disposition completeness, and `git diff --check`

## Current status

Survey complete. Network, privacy-policy, deployment-only, and explicitly deferred feature work are excluded. Existing uncommitted remediation edits are preserved as the baseline for this tranche. BR-0438 and BR-0493 lead because partial guards already exist but are not complete enough to archive.

## Ranked queue

1. BR-0438, atomic combined-configuration import
2. BR-0493, launcher/native HAM patch schema parity
3. BR-0275, CD preview audio initialization (fixed and archived)
4. BR-0248, failure-safe Android OpenSL lifecycle (fixed and archived)
5. BR-0186, transactional mod-archive replacement (open: depends on BR-0103 immutable extraction generations and an atomic manifest pointer)
6. BR-0344, Android render-resolution allocation bounds - fixed and archived
7. BR-0426, selected mission level admission - fixed and archived
8. BR-0480, transactional legacy file-set migration - fixed and archived
9. BR-0604, helper-owned game-data cleanup - fixed and archived (also fixed BR-0605)
10. BR-0083, save metadata game and set-path reconciliation - fixed and archived
11. BR-0161, per-item archive preflight failure containment - fixed and archived
12. BR-0166, metadata normalization failure propagation - fixed and archived
13. BR-0207, complete rewind mission identity - fixed and archived
14. BR-0211, transactional cooperative progress inventories - fixed and archived
15. BR-0212, layout-independent cooperative metadata discovery - open blocker: historical v3 reused one version number for incompatible raw C layouts; current v5 footer prevents recurrence, but safe legacy recovery needs real v1-v4 paired fixtures to disambiguate ABI/layout variants
16. BR-0222, transactional input-demo artifact publication - open blocker: the primary, RNG trace, and optional classic demo need a committed generation manifest shared by native quick-record and Kotlin install/list consumers; isolated rename changes cannot make the three-file set crash-atomic
17. BR-0235, save failure on metadata append failure - substantially fixed but open: core save and required trailers now stage, validate, publish with rollback, and propagate failure; co-op advertising is delayed, but a crash-safe multi-artifact commit with D2 secret companion and last-save pointer still needs a generation manifest
18. BR-0236, rollback for partial all-pilot preference writes - substantially fixed for launcher engine/visual/homing/music writes; remains open for the separate gamepad/autoselect/reset/transient-callsign paths and crash/concurrent-generation atomicity
19. BR-0265, controller JSON validation before mutation - fixed and archived
20. BR-0297, replay checkpoint start-time bounds - fixed and archived
21. BR-0298, malformed replay result fields at the C boundary - fixed and archived
22. BR-0430, complete touch-layout admission - fixed and archived
23. BR-0511, ambiguous import filename collisions - fixed and archived
24. BR-0552, quoted-value-safe automation catalog JSON5 parsing - fixed and archived
25. BR-0591, strict unambiguous JSON normalization - fixed and archived
26. BR-0617, complete source-set admission before forced regeneration - fixed and archived
27. BR-0618, complete physical-disc track manifests - fixed and archived
28. BR-0626, DXA output alias rejection before source deletion
29. BR-0636, DXA entry-mapping collision rejection
30. BR-0082, metadata-backed save admission, attempted last because its verified staged-reader dependency may require deferral rather than a partial fix

## Replacement candidates

Architectural blockers and the excluded DXA-generation-script items do not consume remediation slots. The next local, non-network correctness replacement is BR-0230 (stable direct-command ABI) - fixed and archived.
BR-0387 (paired Android advanced-menu capacity) is fixed and archived as the second replacement.
BR-0369 (single-owner D2 automap failure cleanup) is fixed and archived as the third replacement.
BR-0303 (release cached merged-wall textures before owner reset) is fixed and archived as the fourth replacement.
BR-0256 (remove the cross-unit scalar texture-bind skip) is fixed and archived as the fifth replacement.
BR-0279 (drain Android music before game-thread completion dispatch) is fixed and archived as the sixth replacement.
BR-0581 (reject and quote remote updater values before Bash-manifest publication) is fixed and archived as the seventh replacement.
BR-0567 (root-aware Linux PowerShell bootstrap) is fixed and archived as the eighth replacement.
BR-0550 (nondestructive ADB install failure handling) is fixed and archived as the ninth replacement.
BR-0449 (pre-O foreground-notification constructor compatibility) is fixed and archived as the tenth replacement.
BR-0540 (single-AVD rebuild scope) is fixed and archived as the eleventh replacement.
BR-0548 (exact Play artifact identity and used-version conflict handling) is fixed and archived as the twelfth replacement.
BR-0395 (stable desktop executable output names) is fixed and archived as the thirteenth replacement, completing 30 fully resolved findings. BR-0394 received a guarded-target partial fix but remains open after the real software build exposed additional GLEW/SDL compile coupling.

## Broad-suite follow-up

The 2026-08-11 full run completed all 114 runnable groups in 02:13:02: 110 passed, four failed, seven catalog entries skipped, and none were left unrun. Focused triage fixed the D1 built-in mission save-set identity fallback and the shared D1/D2 controller-config length contract; D1 autosave/resume, paired controller comparison, paired axis routing, and the 176-step KCXF2 route test then passed. The pilot long-hold failure produced an empty suite log and passed immediately for both engines on focused rerun. The Mac SAF test now avoids an unnecessary third 719 MB copy and reports a resource skip when the emulator lacks the two-copy capacity inherently required by its nonseekable staging case. All three Android ABIs assemble, the relevant native/static/unit tests pass, ledger IDs remain unique, and `git diff --check` is clean.
