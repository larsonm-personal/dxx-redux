# Audit PowerShell cwd leaks

## Goal
- Audit project PowerShell scripts for commands that can leave the caller in a different directory
- Fix affected scripts cleanly by avoiding ambient process cwd changes where practical
- Validate the changed scripts with scoped quality checks

## Plan
- [done] Find PowerShell scripts that call location-changing commands
- [done] Inspect each hit and choose a scoped fix
- [done] Apply fixes without touching unrelated dirty files
- [done] Run focused validation and record results
