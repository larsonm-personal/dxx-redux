# Plan: emulator start crash fallback

## Goal
- Make run_all_tests recover from emulator processes that crash immediately after launch and preserve startup diagnostics

## Steps
- [x] Inspect primary and secondary emulator startup helpers
- [x] Add reusable emulator launch logging and fallback flags for immediate crash cases
- [x] Integrate fallback into health and suite preflight paths
- [x] Validate PowerShell syntax and run a targeted startup smoke check if practical
- [x] Mark plan complete
