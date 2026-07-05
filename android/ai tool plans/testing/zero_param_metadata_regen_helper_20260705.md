# Zero Param Metadata Regeneration Helper

## Goal

Add a no-parameter helper script that regenerates all mission metadata without requiring the caller to remember the build, output, install, and timeout flags.

## Plan

1. Done: Add a wrapper script under `android/helpers` with no `param` block.
2. Done: Have the wrapper set up JDK 21 when available, build the debug APK, choose a timestamped output directory, and run the existing metadata-only batch.
3. Done: Update `.github/copilot-instructions.md` so the short wrapper is the documented default.
4. Done: Validate the wrapper with PowerShell parsing and scoped code quality.

## Result

Run `android\helpers\regenerate_all_mission_metadata.ps1` from the repo root to build the debug APK and regenerate all mission metadata JSON through the existing metadata-only batch path.
