# Check Updates Hard Lock Guard

## Goal
Keep android/get_deps/check-updates.ps1 runnable in VS Code without hard
locking the editor by avoiding live prompt paths and by leaving a durable note
of the exact attempted command before the script begins network work.

## Status
- [x] Suppress PowerShell web-request progress output
- [x] Default VS Code runs to noninteractive listing mode
- [x] Add explicit selection arguments for safe apply reruns
- [x] Write a durable pre-run note file with command and method
- [x] Validate safe noninteractive execution and note-file output
- [x] Mark this plan complete