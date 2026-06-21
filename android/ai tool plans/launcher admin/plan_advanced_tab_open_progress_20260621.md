# Plan: Advanced Tab Open Progress

## Goal
Show visible progress while the launcher Advanced Settings page performs its startup scans, especially on devices with many demos, crash reports, logs, and imported files.

## Scope
- Keep the change inside the Android launcher Advanced page where possible.
- Surface progress for every known startup scan on the page, not just one likely slow section.
- Keep existing section behavior and results unchanged after loading.
- Add or update focused tests for progress formatting or scan state if practical.

## Steps
- [x] Identify all Advanced page work that can run while opening the page.
- [x] Add a small page-level progress model and UI.
- [x] Move startup scans behind the progress model so each section reports in order.
- [x] Add focused tests where the logic can be isolated without Compose UI test overhead.
- [x] Run scoped formatting and relevant tests.
