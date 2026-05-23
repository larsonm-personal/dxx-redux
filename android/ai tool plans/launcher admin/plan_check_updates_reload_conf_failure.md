# Check Updates Reload Conf Failure

## Goal
Fix the stale Reload-Conf failure in android/get_deps/check-updates.ps1 and
clean up the surrounding target-update path so target changes can complete
before install sync actions run.

## Status
- [x] Fix the stale Reload-Conf call in the target-update path
- [x] Re-run a safe validation that exercises target updates without mutating the real config
- [x] Re-run normal no-prompt validation on the real script

## Notes
- The failure was a stale post-refactor call to Reload-Conf after target updates; the live helper name is Refresh-ConfContext
- The user's earlier interactive run had already applied target-version updates to tool_versions.conf before the failure interrupted the install-sync phase