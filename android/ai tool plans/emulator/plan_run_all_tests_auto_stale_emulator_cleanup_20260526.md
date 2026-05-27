# Plan: run_all_tests automatic stale emulator cleanup

## Goal
- Add an emulator cleanup helper and run it automatically from run_all_tests preflight so users do not need to run it manually

## Steps
- [x] Add helper script under android/ to detect stale emulator states and optionally force-kill stale emulator processes
- [x] Integrate helper into run_all_tests preflight before primary emulator setup
- [x] Validate PowerShell syntax and basic helper output path
- [x] Mark plan complete
