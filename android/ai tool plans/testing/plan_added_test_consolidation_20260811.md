# Added test consolidation

## Goal

Reduce source and process overhead from tests added by the current branch without weakening the regression coverage that supports the associated fixes.

## Plan

- [x] Inventory newly added tests and identify existing suites or subsystem groups that exercise the same code.
- [x] Consolidate small host source-contract tests by subsystem and fold PowerShell checks into existing script suites.
- [x] Fold small Kotlin tests into existing related test classes where this keeps the suite readable.
- [x] Preserve standalone native translation-unit tests when compiler packing or link isolation is the behavior under test.
- [x] Run focused consolidated tests, formatting checks, and diff validation; record the measured file and line-count reduction.

## Guardrails

- Preserve every distinct behavioral assertion unless it is demonstrably duplicated.
- Prefer data-driven cases and shared setup over repeated test bodies.
- Do not modify product behavior as part of this cleanup.

## Result

- Replaced 25 small Python source-contract files with six subsystem suites, eliminating 19 interpreter launches.
- Folded seven Kotlin test classes into existing configuration, launcher, import, graphics, controller, and touch suites.
- Folded two PowerShell tests into existing CD-regression and dependency-update suites.
- Kept the two input-demo packing probes as separate C translation units inside the existing replay executable because merging them would destroy the surrounding-pack invariant they verify.
- Kept focused integration tests with material setup or distinct build wiring standalone: D2 HAM patch validation, file-set migration, multiplayer mission admission, forced hash completeness, native pilot preference transactions, and D2X-XL WAV parsing.
- Focused Python, Kotlin, PowerShell, catalog, formatting, and diff checks pass.
