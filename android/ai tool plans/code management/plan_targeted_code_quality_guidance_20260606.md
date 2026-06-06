# Plan: Targeted code quality guidance 2026-06-06

## Goal

Document how to use `android\run-code-quality.ps1 -Fix` in a targeted way so normal code changes do not require a slow full-repo cleanup pass.

## Tasks

- [x] Read the CODEX-specific repository instructions
- [x] Inspect `android\run-code-quality.ps1` and helper scripts for path scoping behavior
- [x] Update `.github\copilot-instructions.md` with scoped code quality guidance
- [x] Run a scoped validation pass on the touched instruction files
- [x] Mark this plan complete

## Validation

- `.\android\run-code-quality.ps1 -Fix -Paths .github\copilot-instructions.md` passed
- `.\android\run-code-quality.ps1 -Fix -Paths "android\ai tool plans\code management\plan_targeted_code_quality_guidance_20260606.md"` passed
