# Merge conflict cleanup 2026-05-26

## Scope
- Resolve current merge conflicts without reverting unrelated local work
- Prefer preserving bug fixes and test changes where they do not conflict
- Verify that no conflict markers or unmerged index entries remain

## Tasks
- [x] Inspect unmerged files and conflict stages
- [x] Resolve PowerShell test/tooling conflicts
- [x] Resolve generated/index data conflict
- [x] Check conflict markers and git status
