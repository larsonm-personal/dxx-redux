# Unified CD regression runner

## Goal

Provide one command that runs the complete CD extraction regression workflow and
stops immediately when any stage fails.

## Plan

- [x] Create the implementation plan
- [x] Confirm the four existing scripts and their exit-code contracts
- [x] Add a fail-fast master runner with clear stage output
- [x] Correct any child script that reports failures without a nonzero exit
- [x] Add focused orchestration coverage
- [x] Run scoped code quality and regression validation
- [x] Document the unified command

## Validation

- The orchestration test proves a stage exiting 7 prevents the following stage
  from running
- The extraction workflow self-test passed
- All 34 CD regression specs passed structural validation
- `extract_cd` rebuilt successfully
- All 13 native CTest suites passed
- Scoped code quality and project-configured PSScriptAnalyzer checks passed for
  the new scripts
