# Plan: Add launcher stale file cleanup debug logging

## Goal
Instrument the launcher path that cleans up stale file references so we can see:
- the full configured file list at launcher startup
- each stale entry being removed
- the stored path / SAF URI / related metadata for each removed entry
- the full file list again when the cleanup popup is triggered

## Investigation
- [x] Find the stale cleanup code and popup text
- [x] Identify the launcher file table / persisted file reference model
- [x] Inspect existing debug log categories and launcher-side logging hooks
- [x] Decide whether to add a dedicated Launcher debug log category

## Changes
- [x] Add structured launcher debug logging for startup file-table dump
- [x] Add structured launcher debug logging around stale cleanup detection and removal
- [x] Add structured launcher debug logging when the user-visible cleanup popup is shown
- [x] Add or extend a launcher-specific debug log category if useful

## Validation
- [x] Android debug build passes
- [x] Code quality checks pass for touched files; remaining ktlint failure is the pre-existing `BuildInfo.kt` whitespace issue

## Notes
- Added a new `Launcher` debug log category in the Advanced tab
- Startup now logs a full dump of root files, active set files, asset manifest entries, SAF manifest entries, and resolved D1/D2 file statuses
- Asset and SAF pruning now log each stale entry with absolute path or SAF URI plus prune reason
- When the stale popup is triggered, the launcher logs the popup file list and another full post-prune dump
