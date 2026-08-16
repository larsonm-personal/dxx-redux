# Regression regeneration resilience, menu, and ETA

Date: 2026-08-15
Status: implemented and validated

## Requests

- Preserve detailed failures from regression regeneration in durable artifacts
- Continue later major stages after an earlier stage fails
- Show remaining-time estimates comparable to the run-all-tests runner
- Share timing implementation and file formats where practical
- Add an initial menu for full or category-specific regeneration
- Update timing history only for complete full runs, while still showing ETA on
  partial runs

## Plan

- [x] Inventory regeneration stages, current CD artifacts, and run-all timing code
- [x] Diagnose the reported CD failures from existing durable logs
- [x] Extract reusable stage timing/history helpers and adopt them in both runners
- [x] Make regeneration stages failure-tolerant with a durable summary artifact
- [x] Add interactive category selection plus noninteractive testability
- [x] Add or extend high-level regression tests
- [x] Run focused tests, scoped quality checks, and a safe runner smoke test

## Results

- Full and partial regeneration runs show weighted remaining-time estimates via
  the shared test-suite progress helper. Full runs write the same Markdown
  timing-table format consumed by `run_all_tests.ps1`; partial runs do not add
  timing history.
- Zero-parameter runs offer All, CD extraction/import/launch, fingerprints, and
  mission metadata. `-Category` provides the same choices noninteractively.
- Every selected top-level stage runs even after another stage fails. Each run
  saves `summary.json` and one complete log per stage below
  `temp/regression_data_reports`.
- The extraction suite saves a per-source log, a failed-source logcat capture,
  and a machine-readable summary below `temp/extract_regression_reports`.
- Immediate import failures were caused by byte-identical mission files in
  multiple disc directories being rejected as ambiguous. Identical files are
  now deterministically deduplicated while conflicting files remain rejected.
- D2 automation timeouts were launch preflight failures caused by inherited CD
  Audio preferences with no enabled source in the isolated test set. Extraction
  launch automation now explicitly selects built-in MIDI.
- Anniversary USA and Destination Quartzon USA both pass end-to-end after the
  fixes. Focused PowerShell tests, the disc-import Kotlin test, Android assembly,
  APK installation, and scoped quality checks pass.
- A filtered global test-runner smoke was blocked before execution by the
  unrelated dirty `test_base_robot_preview.json5` catalog owner.

## Constraints

- Preserve existing CLI automation and zero-parameter full-run behavior where
  practical
- Do not overwrite timing history on partial or incomplete full runs
- Preserve unrelated working-tree changes
