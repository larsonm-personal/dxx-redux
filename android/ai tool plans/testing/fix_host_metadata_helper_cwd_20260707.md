# Fix host metadata helper cwd leak

## Goal
- Fix `android\helpers\regenerate_all_mission_metadata_host.ps1` so running it from `android\helpers` does not leave the caller in the repo root
- Avoid a cleanup-only `Set-Location` back to the original directory

## Plan
- [done] Inspect the script for process-wide directory changes
- [done] Replace ambient cwd changes with location-scoped command execution
- [done] Run focused validation and code quality for the changed script
