# Binary size labels audit

## Goal

Display all Android user-facing storage sizes and transfer rates using binary powers while labeling them with the traditional `KB`, `MB`, and `GB` names.

## Plan

- [x] Inventory user-facing size and rate formatters and literal IEC labels
- [x] Centralize binary size and rate formatting where practical
- [x] Replace visible `KiB`, `MiB`, and `GiB` labels with `KB`, `MB`, and `GB`
- [x] Verify existing `KB`, `MB`, and `GB` displays use powers of 1024
- [x] Update focused formatting tests and run Android quality, tests, and build

## Constraints

- Do not rename technical API constants or third-party units whose names are fixed interfaces
- Do not alter byte limits or storage calculations, only user-facing formatting and shared display helpers
- Use locale-stable numeric formatting

## Validation

- Kotlin formatting completed through `run-code-quality.ps1 -Fix`; its unrelated PowerShell analysis stage was stopped after it ceased producing output
- Focused unit tests passed for the shared formatter, storage guard, archive metadata size, and mission transfer protocol
- `:app:assembleDebug` passed
- Final source audit found no user-facing IEC labels or decimal byte conversions; remaining IEC text is limited to manifest comments and a fixed third-party API method name
- `git diff --check` passed

Status: complete
