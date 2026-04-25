# Check Updates Unrar Sort And Gradle Tracking

## Goal
Add unrar to the tracked dependency set, reorder the updater table columns to
installed/target/latest, alphabetize the printed dependency rows, tidy select
multi-tool config blocks in tool_versions.conf, and add any android/app/build.gradle
dependency versions that are not currently tracked.

## Status
- [x] Audit build.gradle and current updater/config coverage
- [x] Add missing tracked versions from build.gradle, including unrar
- [x] Reorder and alphabetize updater output
- [x] Tastefully alphabetize multi-entry config blocks in tool_versions.conf
- [x] Re-run safe updater validation and confirm output

## Notes
- android/app/build.gradle had one untracked dependency version: xcrash
- unar is tracked as a pinned package version from repo evidence because the public Windows download URL does not expose a separate latest-version feed
- check-updates.ps1 now sorts dependency rows with an explicit script-block key so pwsh and Windows PowerShell print the same alphabetical order