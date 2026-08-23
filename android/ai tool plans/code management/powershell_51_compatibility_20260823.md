# PowerShell 5.1 compatibility audit

## Goal

Determine whether the repository PowerShell scripts can support Windows PowerShell 5.1 without losing behavior, detail handling, or correctness, and make low-risk compatibility fixes where practical.

## Plan

- [ ] Inventory scripts and identify PowerShell 6/7-only syntax and APIs
- [ ] Classify findings into safe compatibility fixes and intentional modern-PowerShell requirements
- [ ] Implement focused fixes with regression coverage for nontrivial compatibility helpers
- [ ] Parse changed scripts with the Windows PowerShell 5.1 parser and run relevant tests
- [ ] Run scoped code-quality checks and update `android/outstanding_bugs.md`

## Constraints

- Preserve existing comments and user changes
- Do not trade away error reporting, encoding correctness, process handling, or structured data fidelity
- Prefer small compatibility shims or equivalent 5.1 APIs over broad rewrites
- Record any scripts that should remain PowerShell 7-only and the concrete reason

## Findings

Pending audit.
