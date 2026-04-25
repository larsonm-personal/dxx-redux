# Check Updates Installed Versions

## Goal
Teach android/get_deps/check-updates.ps1 about a third state, the actually
installed tool versions, using commands encoded in android/get_deps/tool_versions.conf
where practical, and offer a separate path to sync installed tools to the
target versions from tool_versions.conf.

## Status
- [x] Add installed-version command metadata to tool_versions.conf
- [x] Extend the updater table with installed-version visibility
- [x] Add a separate install-sync selection path
- [x] Re-run safe validation under pwsh and Windows PowerShell
- [x] Mark this plan complete

## Notes
- JDK installed-version detection now reads the local JDK release file so the reported version matches in both pwsh and Windows PowerShell